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

#include "RadosStorage.hpp"

#include <Digest.hpp>
#include <Logging.hpp>
#include <core/exceptions.hpp>
#include <fmtmacros.hpp>
#include <util/expected.hpp>
#include <util/string.hpp>

// clang-format off
#include <librados.hpp>
// clang-format on

#include <cstdarg>
#include <map>
#include <memory>

namespace storage::remote {

namespace {

class RadosStorageBackend : public RemoteStorage::Backend
{
public:
  RadosStorageBackend(const RemoteStorage::Backend::Params& params);
  ~RadosStorageBackend();

  nonstd::expected<std::optional<util::Bytes>, Failure>
  get(const Digest& key) override;

  nonstd::expected<bool, Failure> put(const Digest& key,
                                      nonstd::span<const uint8_t> value,
                                      bool only_if_missing) override;

  nonstd::expected<bool, Failure> remove(const Digest& key) override;

private:
  librados::Rados m_cluster;
  librados::IoCtx m_pool;
  bool m_update_mtime = false;

  inline nonstd::expected<std::optional<float>, Failure>
  mtime(const std::string& key);
  nonstd::expected<bool, Failure> utimes(const std::string key, float times);
  std::string get_key_string(const Digest& digest) const;
};

RadosStorageBackend::RadosStorageBackend(const Params& params)
{
  ASSERT(params.url.scheme() == "rados");
  if (!params.url.host().empty()) {
    throw core::Fatal(FMT(
      "invalid file path \"{}\": specifying a host (\"{}\") is not supported",
      params.url.str(),
      params.url.host()));
  }

  for (const auto& attr : params.attributes) {
    if (attr.key == "update-mtime") {
      m_update_mtime = attr.value == "true";
    } else if (!is_framework_attribute(attr.key)) {
      LOG("Unknown attribute: {}", attr.key);
    }
  }

  LOG("RADOS open {}", params.url.str());
  int err = m_cluster.init("ceph");
  if (err < 0) {
    throw Failed(FMT("Redis init error: {}", strerror(err)));
  }
  err = m_cluster.connect();
  if (err < 0) {
    throw Failed(FMT("Redis connect error: {}", strerror(err)));
  }
  m_cluster.ioctx_create("ccache", m_pool);
}

RadosStorageBackend::~RadosStorageBackend()
{
  m_pool.close();
  m_cluster.shutdown();
}

nonstd::expected<std::optional<util::Bytes>, RemoteStorage::Backend::Failure>
RadosStorageBackend::get(const Digest& key)
{
  const auto key_string = get_key_string(key);
  LOG("RADOS get {}", key_string);
  librados::bufferlist read_buf;
  int read_len = 4194304;

  // Create I/O Completion.
  librados::AioCompletion* read_completion =
    librados::Rados::aio_create_completion();

  // Send read request.
  int ret =
    m_pool.aio_read(key_string, read_completion, &read_buf, read_len, 0);
  if (ret < 0) {
    // std::cerr << "Couldn't start read object! error " << ret << std::endl;
    exit(EXIT_FAILURE);
  }

  // Wait for the request to complete, and check that it succeeded.
  read_completion->wait_for_complete();
  ret = read_completion->get_return_value();
  if (ret < 0) {
    LOG("Failed to read {}: {}", key_string, ret);
    return nonstd::make_unexpected(Failure::error);
  }
  bool found = true;
  if (m_update_mtime) {
    utimes(key_string, time(nullptr));
  }
  if (found) {
    return util::Bytes(read_buf.c_str(), read_buf.length());
  } else {
    return std::nullopt;
  }
}

nonstd::expected<bool, RemoteStorage::Backend::Failure>
RadosStorageBackend::put(const Digest& key,
                         nonstd::span<const uint8_t> value,
                         bool /*only_if_missing*/)
{
  const auto key_string = get_key_string(key);
  LOG("RADOS put {}", key_string);
  librados::bufferlist bl;
  bl.append(reinterpret_cast<const char*>(value.data()), value.size());
  int ret = m_pool.write_full(key_string, bl);
  if (ret < 0) {
    LOG("Failed to write {}: {}", key_string, ret);
    return nonstd::make_unexpected(Failure::error);
  }
  return true;
}

nonstd::expected<std::optional<float>, RemoteStorage::Backend::Failure>
RadosStorageBackend::mtime(const std::string& key_string)
{
  LOG("RADOS get {} mtime", key_string);
  return std::nullopt;
}

nonstd::expected<bool, RemoteStorage::Backend::Failure>
RadosStorageBackend::utimes(const std::string key_string, float times)
{
  LOG("RADOS put {} mtime {}", key_string, times);
  return true;
}

nonstd::expected<bool, RemoteStorage::Backend::Failure>
RadosStorageBackend::remove(const Digest& key)
{
  const auto key_string = get_key_string(key);
  LOG("RADOS remove {}", key_string);
  int ret = m_pool.remove(key_string);
  if (ret < 0) {
    LOG("Failed to write {}: {}", key_string, ret);
    return nonstd::make_unexpected(Failure::error);
  }
  return true;
}

std::string
RadosStorageBackend::get_key_string(const Digest& digest) const
{
  return digest.to_string();
}

} // namespace

std::unique_ptr<RemoteStorage::Backend>
RadosStorage::create_backend(const Backend::Params& params) const
{
  return std::make_unique<RadosStorageBackend>(params);
}

} // namespace storage::remote
