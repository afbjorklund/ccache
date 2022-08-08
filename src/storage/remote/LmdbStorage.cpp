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

#include "LmdbStorage.hpp"

#include <Digest.hpp>
#include <Logging.hpp>
#include <core/exceptions.hpp>
#include <fmtmacros.hpp>
#include <util/expected.hpp>
#include <util/string.hpp>

#include <lmdb++.h>

#include <cstdarg>
#include <map>
#include <memory>

namespace storage::remote {

namespace {

const auto k_default_mapsize = 50UL * 1024UL * 1024UL * 1024UL; /* 50 GiB */

class LmdbStorageBackend : public RemoteStorage::Backend
{
public:
  LmdbStorageBackend(const RemoteStorage::Backend::Params& params);

  nonstd::expected<std::optional<util::Bytes>, Failure>
  get(const Digest& key) override;

  nonstd::expected<bool, Failure> put(const Digest& key,
                                      nonstd::span<const uint8_t> value,
                                      bool only_if_missing) override;

  nonstd::expected<bool, Failure> remove(const Digest& key) override;

private:
  lmdb::env m_env;
  bool m_update_mtime = false;

  inline nonstd::expected<std::optional<float>, Failure>
  mtime(const std::string& key);
  nonstd::expected<bool, Failure> utimes(const std::string key, float times);
  std::string get_key_string(const Digest& digest) const;
};

LmdbStorageBackend::LmdbStorageBackend(const Params& params)
  : m_env(lmdb::env(nullptr))
{
  ASSERT(params.url.scheme() == "lmdb");
  if (!params.url.host().empty()) {
    throw core::Fatal(FMT(
      "invalid file path \"{}\": specifying a host (\"{}\") is not supported",
      params.url.str(),
      params.url.host()));
  }

  auto mapsize = k_default_mapsize;
  for (const auto& attr : params.attributes) {
    if (attr.key == "mapsize") {
      mapsize = util::value_or_throw<core::Fatal>(util::parse_unsigned(
        attr.value, std::nullopt, std::nullopt, "mapsize"));
    } else if (attr.key == "update-mtime") {
      m_update_mtime = attr.value == "true";
    } else if (!is_framework_attribute(attr.key)) {
      LOG("Unknown attribute: {}", attr.key);
    }
  }

  m_env = lmdb::env::create();
  m_env.set_mapsize(mapsize);
  m_env.set_max_dbs(2); // mtime
  auto path = params.url.path();
  LOG("LMDB open {}", path);
  m_env.open(path.c_str(), MDB_NOSUBDIR, 0644);
}

nonstd::expected<std::optional<util::Bytes>, RemoteStorage::Backend::Failure>
LmdbStorageBackend::get(const Digest& key)
{
  const auto key_string = get_key_string(key);
  LOG("LMDB get {}", key_string);
  try {
    auto rtxn = lmdb::txn::begin(m_env, nullptr, MDB_RDONLY);
    auto dbi = lmdb::dbi::open(rtxn, nullptr);
    lmdb::val k(key_string), v;
    bool found = dbi.get(rtxn, k, v);
    rtxn.abort();
    if (m_update_mtime) {
      utimes(key_string, time(nullptr));
    }
    if (found) {
      return util::Bytes(v.data(), v.size());
    } else {
      return std::nullopt;
    }
  } catch (const lmdb::error& e) {
    LOG("Failed to read {}: {}", key_string, e.what());
    return nonstd::make_unexpected(Failure::error);
  }
}

nonstd::expected<bool, RemoteStorage::Backend::Failure>
LmdbStorageBackend::put(const Digest& key,
                        nonstd::span<const uint8_t> value,
                        bool only_if_missing)
{
  const auto key_string = get_key_string(key);
  LOG("LMDB put {}", key_string);
  try {
    auto wtxn = lmdb::txn::begin(m_env);
    auto dbi = lmdb::dbi::open(wtxn, nullptr);
    lmdb::val k(key_string), v(value.data(), value.size());
    unsigned int flags = only_if_missing ? MDB_NOOVERWRITE : 0;
    bool found = dbi.put(wtxn, k, v, flags);
    wtxn.commit();
    if (found) {
      utimes(key_string, time(nullptr));
    }
    return found;
  } catch (const lmdb::error& e) {
    LOG("Failed to write {}: {}", key_string, e.what());
    return nonstd::make_unexpected(Failure::error);
  }
}

nonstd::expected<std::optional<float>, RemoteStorage::Backend::Failure>
LmdbStorageBackend::mtime(const std::string& key_string)
{
  LOG("LMDB get {} mtime", key_string);
  try {
    auto rtxn = lmdb::txn::begin(m_env, nullptr, MDB_RDONLY);
    auto dbi = lmdb::dbi::open(rtxn, "mtime");
    lmdb::val k(key_string), v;
    bool found = dbi.get(rtxn, k, v);
    rtxn.abort();
    if (found) {
      float mtime;
      auto buf = reinterpret_cast<const uint8_t*>(v.data());
      ASSERT(v.size() == 4);
      Util::big_endian_to_float(buf, mtime);
      return mtime;
    } else {
      return std::nullopt;
    }
  } catch (const lmdb::error& e) {
    LOG("Failed to read {}: {}", key_string, e.what());
    return nonstd::make_unexpected(Failure::error);
  }
}

nonstd::expected<bool, RemoteStorage::Backend::Failure>
LmdbStorageBackend::utimes(const std::string key_string, float times)
{
  LOG("LMDB put {} mtime {}", key_string, times);
  try {
    auto wtxn = lmdb::txn::begin(m_env);
    auto dbi = lmdb::dbi::open(wtxn, "mtime", MDB_CREATE);
    uint8_t buf[4];
    Util::float_to_big_endian(times, buf);
    lmdb::val k(key_string), v(buf, sizeof(buf));
    dbi.put(wtxn, k, v);
    wtxn.commit();
    return true;
  } catch (const lmdb::error& e) {
    LOG("Failed to write {}: {}", key_string, e.what());
    return nonstd::make_unexpected(Failure::error);
  }
}

nonstd::expected<bool, RemoteStorage::Backend::Failure>
LmdbStorageBackend::remove(const Digest& key)
{
  const auto key_string = get_key_string(key);
  LOG("LMDB del {}", key_string);
  try {
    auto wtxn = lmdb::txn::begin(m_env);
    auto dbi = lmdb::dbi::open(wtxn, nullptr);
    lmdb::val k(key_string);
    bool ret = dbi.del(wtxn, k);
    wtxn.commit();
    return ret;
  } catch (const lmdb::error& e) {
    LOG("Failed to delete {}: {}", key_string, e.what());
    return nonstd::make_unexpected(Failure::error);
  }
}

std::string
LmdbStorageBackend::get_key_string(const Digest& digest) const
{
  return digest.to_string();
}

} // namespace

std::unique_ptr<RemoteStorage::Backend>
LmdbStorage::create_backend(const Backend::Params& params) const
{
  return std::make_unique<LmdbStorageBackend>(params);
}

} // namespace storage::remote
