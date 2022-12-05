// Copyright (C) 2021-2022 Joel Rosdahl and other contributors
//
// See doc/AUTHORS.adoc for a complete list of contributors.
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 3 of the License, or (at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
// more details.
//
// You should have received a copy of the GNU General Public License along with
// this program; if not, write to the Free Software Foundation, Inc., 51
// Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

#include "ArchiveStorage.hpp"

#include <Digest.hpp>
#include <Logging.hpp>
#include <core/exceptions.hpp>
#include <fmtmacros.hpp>
#include <util/expected.hpp>
#include <util/string.hpp>

#include <archive.h>
#include <archive_entry.h>
#include <fcntl.h>
#include <unistd.h>

#include <cstdarg>
#include <map>
#include <memory>

namespace storage::remote {

namespace {

using Archive = std::unique_ptr<archive, decltype(&archive_free)>;

class ArchiveStorageBackend : public RemoteStorage::Backend
{
public:
  ArchiveStorageBackend(const RemoteStorage::Backend::Params& params);

  nonstd::expected<std::optional<util::Bytes>, Failure>
  get(const Digest& key) override;

  nonstd::expected<bool, Failure> put(const Digest& key,
                                      nonstd::span<const uint8_t> value,
                                      bool only_if_missing) override;

  nonstd::expected<bool, Failure> remove(const Digest& key) override;

private:
  std::string m_file;
  Archive m_archive;
  bool m_update_mtime = false;

  inline nonstd::expected<std::optional<float>, Failure>
  mtime(const std::string& key);
  nonstd::expected<bool, Failure> utimes(const std::string key, float times);
  std::string get_key_string(const Digest& digest) const;
};

ArchiveStorageBackend::ArchiveStorageBackend(const Params& params)
  : m_archive(nullptr, archive_free)
{
  ASSERT(params.url.scheme() == "archive");

  if (!params.url.host().empty()) {
    throw core::Fatal(FMT(
      "invalid file path \"{}\": specifying a host (\"{}\") is not supported",
      params.url.str(),
      params.url.host()));
  }
  m_file = params.url.path();

  for (const auto& attr : params.attributes) {
    if (attr.key == "update-mtime") {
      m_update_mtime = attr.value == "true";
    } else if (!is_framework_attribute(attr.key)) {
      LOG("Unknown attribute: {}", attr.key);
    }
  }
}

nonstd::expected<std::optional<util::Bytes>, RemoteStorage::Backend::Failure>
ArchiveStorageBackend::get(const Digest& key)
{
  const auto key_string = get_key_string(key);
  LOG("Archive get {}", key_string);
  const bool exists = Stat::stat(m_file);
  if (!exists) {
    return std::nullopt;
  }
  m_archive.reset(archive_read_new());
  auto a = m_archive.get();
  archive_read_support_format_tar(a);
  int r = archive_read_open_filename(a, m_file.c_str(), 10240);
  if (r != ARCHIVE_OK) {
    LOG("Failed to read {}: {}", key_string, archive_error_string(a));
    return nonstd::make_unexpected(Failure::error);
  }

  struct archive_entry* entry;
  while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
    if (archive_entry_pathname(entry) == key_string) {
      util::Bytes result;
      result.resize(archive_entry_size(entry));
      archive_read_data(a, result.data(), result.size());
      archive_read_close(a);
      return result;
    }
    archive_read_data_skip(a);
  }
  archive_read_close(a);
  return std::nullopt;
}

nonstd::expected<bool, RemoteStorage::Backend::Failure>
ArchiveStorageBackend::put(const Digest& key,
                           nonstd::span<const uint8_t> value,
                           bool only_if_missing)
{
  const auto key_string = get_key_string(key);
  LOG("Archive put {}", key_string);
  const bool exists = Stat::stat(m_file);
  int fd = open(m_file.c_str(), O_CREAT | O_BINARY | O_RDWR, 0644);
  if (fd == -1) {
    return nonstd::make_unexpected(Failure::error);
  }
  bool previous = false;
  if (exists) {
    m_archive.reset(archive_read_new());
    auto a = m_archive.get();
    archive_read_support_format_tar(a);
    int r = archive_read_open_fd(a, fd, 10240);
    if (r != ARCHIVE_OK) {
      LOG("Failed to read {}: {}", key_string, archive_error_string(a));
      return nonstd::make_unexpected(Failure::error);
    }

    struct archive_entry* entry;
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
      if (archive_entry_pathname(entry) == key_string && !previous) {
        if (only_if_missing) {
          archive_read_close(a);
          return false;
        }
        previous = true;
      }
      archive_read_data_skip(a);
    }
    // append
    auto pos = archive_read_header_position(a);
    lseek(fd, pos, SEEK_SET);
  }
  m_archive.reset(archive_write_new());
  auto a = m_archive.get();
  archive_write_set_format_ustar(a);
  int r = archive_write_open_fd(a, fd);
  if (r != ARCHIVE_OK) {
    LOG("Failed to write {}: {}", key_string, archive_error_string(a));
    return nonstd::make_unexpected(Failure::error);
  }

  struct archive_entry* entry;
  entry = archive_entry_new();
  archive_entry_set_pathname(entry, key_string.c_str());
  archive_entry_set_size(entry, value.size());
  archive_entry_set_filetype(entry, AE_IFREG);
  archive_entry_set_perm(entry, 0644);
  auto now = util::TimePoint::now();
  archive_entry_set_mtime(entry, now.sec(), now.nsec_decimal_part());
  archive_write_header(a, entry);
  archive_write_data(a, value.data(), value.size());
  archive_entry_free(entry);
  if (previous) {
    // TODO: delete previous entry (add .wh.* whiteout)
  }
  archive_write_close(a);
  close(fd);
  return true;
}

nonstd::expected<bool, RemoteStorage::Backend::Failure>
ArchiveStorageBackend::remove(const Digest& key)
{
  const auto key_string = get_key_string(key);
  LOG("Archive del {}", key_string);

  // TODO: delete old entry (use .wh.* whiteout)
  return nonstd::make_unexpected(Failure::error);
}

std::string
ArchiveStorageBackend::get_key_string(const Digest& digest) const
{
  const auto key_str = digest.to_string();
  const uint8_t digits = 2;
  ASSERT(key_str.length() > digits);
  return FMT("{:.{}}/{}", key_str, digits, &key_str[digits]);
}

} // namespace

std::unique_ptr<RemoteStorage::Backend>
ArchiveStorage::create_backend(const Backend::Params& params) const
{
  return std::make_unique<ArchiveStorageBackend>(params);
}

} // namespace storage::remote
