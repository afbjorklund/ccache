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

#ifdef HAVE_SQLITE3
#  include <third_party/ratarmount.h>

#  include <sqlite3.h>
#endif

#include <cstdarg>
#include <map>
#include <memory>

namespace storage::remote {

namespace {

using Archive = std::unique_ptr<archive, decltype(&archive_free)>;
#ifdef HAVE_SQLITE3
using Sqlite3 = std::unique_ptr<sqlite3, decltype(&sqlite3_close)>;
#endif

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
#ifdef HAVE_SQLITE3
  std::string m_index;
  Sqlite3 m_sqlite3;
#endif
  bool m_update_mtime = false;

  inline nonstd::expected<std::optional<float>, Failure>
  mtime(const std::string& key);
  nonstd::expected<bool, Failure> utimes(const std::string key, float times);
  std::string get_key_string(const Digest& digest) const;
};

ArchiveStorageBackend::ArchiveStorageBackend(const Params& params)
  : m_archive(nullptr, archive_free)
#ifdef HAVE_SQLITE3
    ,
    m_sqlite3(nullptr, sqlite3_close)
#endif
{
  ASSERT(params.url.scheme() == "archive");

  if (!params.url.host().empty()) {
    throw core::Fatal(FMT(
      "invalid file path \"{}\": specifying a host (\"{}\") is not supported",
      params.url.str(),
      params.url.host()));
  }
  m_file = params.url.path();
#ifdef HAVE_SQLITE3
  m_index = m_file + ".index.sqlite";
#endif

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

#ifdef HAVE_SQLITE3
  sqlite3* db;
  int rc = sqlite3_open_v2(m_index.c_str(), &db, SQLITE_OPEN_READONLY, NULL);
  if (rc == SQLITE_OK) {
    m_sqlite3.reset(db);
  } else {
    return nonstd::make_unexpected(Failure::error);
  }

  sqlite3_stmt* select;
  rc = sqlite3_prepare_v2(
    db, SELECT_FILES_TABLE.c_str(), SELECT_FILES_TABLE.length(), &select, NULL);

  std::string path = "/" + key_string.substr(0, 2);
  rc =
    sqlite3_bind_text(select, 1, path.c_str(), path.size(), SQLITE_TRANSIENT);
  std::string name = key_string.substr(3);
  rc =
    sqlite3_bind_text(select, 2, name.c_str(), name.size(), SQLITE_TRANSIENT);

  rc = sqlite3_step(select);
  if (rc == SQLITE_ERROR) {
    LOG("exec: {}", sqlite3_errmsg(db));
  }
  auto offsetheader = sqlite3_column_int64(select, 2);
  auto offset = sqlite3_column_int64(select, 3);
  sqlite3_finalize(select);

  LOG("read offsetheader: {}", offsetheader);
  LOG("read offset: {}", offset);
#endif

  int fd = open(m_file.c_str(), O_BINARY | O_RDONLY, 0644);
  if (fd == -1) {
    return nonstd::make_unexpected(Failure::error);
  }
#ifdef HAVE_SQLITE3
  lseek(fd, offsetheader, SEEK_SET);
#endif
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
#ifdef HAVE_SQLITE3
  auto offsetheader = lseek(fd, 0, SEEK_CUR);
  auto offset = offsetheader + 512; // XXX
#endif
  archive_write_header(a, entry);
  archive_write_data(a, value.data(), value.size());
  archive_entry_free(entry);
  if (previous) {
    // TODO: delete previous entry (add .wh.* whiteout)
  }
  archive_write_close(a);
  close(fd);

#ifdef HAVE_SQLITE3
  const bool hadindex = Stat::stat(m_index);

  sqlite3* db;
  int rc = sqlite3_open_v2(
    m_index.c_str(), &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
  if (rc == SQLITE_OK) {
    m_sqlite3.reset(db);
  }

  sqlite3_stmt* insert;
  rc = sqlite3_prepare_v2(
    db, INSERT_FILES_TABLE.c_str(), INSERT_FILES_TABLE.length(), &insert, NULL);

  char* errmsg;
  if (!hadindex) {
    rc = sqlite3_exec(db, CREATE_FILES_TABLE.c_str(), NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
      LOG("exec: {}", errmsg);
    }
  }

  std::string path = "/" + key_string.substr(0, 2);
  rc =
    sqlite3_bind_text(insert, 1, path.c_str(), path.size(), SQLITE_TRANSIENT);
  std::string name = key_string.substr(3);
  rc =
    sqlite3_bind_text(insert, 2, name.c_str(), name.size(), SQLITE_TRANSIENT);
  rc = sqlite3_bind_int64(insert, 3, offsetheader);
  rc = sqlite3_bind_int64(insert, 4, offset);
  rc = sqlite3_bind_int(insert, 5, value.size());     // size
  rc = sqlite3_bind_double(insert, 6, now.seconds()); // mtime
  rc = sqlite3_bind_int(insert, 7, 0644);             // mode
  rc = sqlite3_bind_int(insert, 10, 0);               // uid
  rc = sqlite3_bind_int(insert, 11, 0);               // gid

  rc = sqlite3_step(insert);
  if (rc == SQLITE_ERROR) {
    LOG("exec: {}", sqlite3_errmsg(db));
  }
  sqlite3_finalize(insert);

  LOG("write offsetheader: {}", offsetheader);
  LOG("write offset: {}", offset);
#endif

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
