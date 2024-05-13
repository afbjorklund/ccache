// Copyright (C) 2021-2023 Joel Rosdahl and other contributors
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

#include "AerospikeStorage.hpp"

#include <Hash.hpp>
#include <core/exceptions.hpp>
#include <storage/Storage.hpp>
#include <util/assertions.hpp>
#include <util/expected.hpp>
#include <util/fmtmacros.hpp>
#include <util/logging.hpp>
#include <util/string.hpp>

#include <aerospike/aerospike.h>
#include <aerospike/aerospike_key.h>
#include <aerospike/as_error.h>
#include <aerospike/as_event.h>
#include <aerospike/as_key.h>
#include <aerospike/as_policy.h>
#include <aerospike/as_record.h>
#include <aerospike/as_status.h>
#include <openssl/crypto.h>
#include <openssl/ssl.h>

#include <cstdarg>
#include <map>
#include <memory>

namespace storage::remote {

namespace {

using AerospikeContext =
  std::unique_ptr<aerospike, decltype(&aerospike_destroy)>;
using AerospikeReply = std::unique_ptr<as_record, decltype(&as_record_destroy)>;

const uint32_t DEFAULT_PORT = 3000;

// Compatibility with skyhook adaptor
const char* SET_NAME = "redis";
const char* BIN_NAME = "b";

class AerospikeStorageBackend : public RemoteStorage::Backend
{
public:
  AerospikeStorageBackend(const Url& url,
                          const std::vector<Backend::Attribute>& attributes);

  tl::expected<std::optional<util::Bytes>, Failure>
  get(const Hash::Digest& key) override;

  tl::expected<bool, Failure> put(const Hash::Digest& key,
                                  nonstd::span<const uint8_t> value,
                                  bool only_if_missing) override;

  tl::expected<bool, Failure> remove(const Hash::Digest& key) override;

private:
  const std::string m_namespace;
  AerospikeContext m_context;

  void
  connect(const Url& url, uint32_t connect_timeout, uint32_t operation_timeout);
  void authenticate(const Url& url);
  std::string get_key_string(const Hash::Digest& digest) const;
};

std::pair<std::optional<std::string>, std::optional<std::string>>
split_user_info(const std::string& user_info)
{
  const auto [left, right] = util::split_once(user_info, ':');
  if (left.empty()) {
    // aerospike://HOST
    return {std::nullopt, std::nullopt};
  } else if (right) {
    // aerospike://USERNAME:PASSWORD@HOST
    return {std::string(left), std::string(*right)};
  } else {
    // aerospike://PASSWORD@HOST
    return {std::nullopt, std::string(left)};
  }
}

AerospikeStorageBackend::AerospikeStorageBackend(
  const Url& url,
  const std::vector<Backend::Attribute>& attributes)
  : m_namespace("test"), // TODO: attribute
    m_context(nullptr, aerospike_destroy)
{
  // Need to make sure to link with libcrypto
  // "undefined symbol: GENERAL_NAME_free"
  OPENSSL_init();

  auto connect_timeout = k_default_connect_timeout;
  auto operation_timeout = k_default_operation_timeout;

  for (const auto& attr : attributes) {
    if (attr.key == "connect-timeout") {
      connect_timeout = parse_timeout_attribute(attr.value);
    } else if (attr.key == "operation-timeout") {
      operation_timeout = parse_timeout_attribute(attr.value);
    } else if (!is_framework_attribute(attr.key)) {
      LOG("Unknown attribute: {}", attr.key);
    }
  }

  connect(url,
          static_cast<uint32_t>(connect_timeout.count()),
          static_cast<uint32_t>(operation_timeout.count()));
}

tl::expected<std::optional<util::Bytes>, RemoteStorage::Backend::Failure>
AerospikeStorageBackend::get(const Hash::Digest& digest)
{
  const auto key_string = get_key_string(digest);

  LOG("Aerospike GET {}", key_string);
  as_error err;
  as_key key;
  as_key_init(&key, m_namespace.c_str(), SET_NAME, key_string.c_str());
  as_record* p_rec = 0;
  const auto ec = aerospike_key_get(m_context.get(), &err, NULL, &key, &p_rec);
  if (ec == AEROSPIKE_OK) {
    as_bytes* bytes = as_record_get_bytes(p_rec, BIN_NAME);
    return util::Bytes(as_bytes_get(bytes), as_bytes_size(bytes));
  } else if (ec == AEROSPIKE_ERR_RECORD_NOT_FOUND) {
    return std::nullopt;
  } else {
    LOG("Unknown error: {}", err.message);
    return tl::unexpected(Failure::error);
  }
}

tl::expected<bool, RemoteStorage::Backend::Failure>
AerospikeStorageBackend::put(const Hash::Digest& digest,
                             nonstd::span<const uint8_t> value,
                             bool only_if_missing)
{
  const auto key_string = get_key_string(digest);

  LOG("Aerospike PUT {} [{} bytes]", key_string, value.size());
  as_error err;
  as_policy_write pol;
  as_policy_write_init(&pol);
  if (only_if_missing) {
    pol.exists = AS_POLICY_EXISTS_CREATE;
  }
  as_key key;
  as_key_init(&key, m_namespace.c_str(), SET_NAME, key_string.c_str());
  as_bytes bytes;
  as_bytes_init_wrap(
    &bytes, const_cast<uint8_t*>(value.data()), value.size(), false);
  as_record rec;
  as_record_init(&rec, 1);
  as_record_set_bytes(&rec, BIN_NAME, &bytes);
  const auto ec = aerospike_key_put(m_context.get(), &err, &pol, &key, &rec);
  if (ec == AEROSPIKE_OK) {
    return true;
  } else {
    LOG("Unknown error: {}", err.message);
    return tl::unexpected(Failure::error);
  }
}

tl::expected<bool, RemoteStorage::Backend::Failure>
AerospikeStorageBackend::remove(const Hash::Digest& digest)
{
  const auto key_string = get_key_string(digest);
  LOG("Aerospike REMOVE {}", key_string);
  as_error err;
  as_key key;
  as_key_init(&key, m_namespace.c_str(), SET_NAME, key_string.c_str());
  const auto ec = aerospike_key_remove(m_context.get(), &err, NULL, &key);
  if (ec == AEROSPIKE_OK) {
    return true;
  } else if (ec == AEROSPIKE_ERR_RECORD_NOT_FOUND) {
    return false;
  } else {
    LOG("Unknown error: {}", err.message);
    return tl::unexpected(Failure::error);
  }
}

void
AerospikeStorageBackend::connect(const Url& url,
                                 const uint32_t connect_timeout,
                                 const uint32_t /*operation_timeout*/)
{
  const std::string host = url.host().empty() ? "localhost" : url.host();
  const uint32_t port =
    url.port().empty()
      ? DEFAULT_PORT
      : static_cast<uint32_t>(util::value_or_throw<core::Fatal>(
        util::parse_unsigned(url.port(), 1, 65535, "port")));
  ASSERT(url.path().empty() || url.path()[0] == '/');

  as_config config;
  as_config_init(&config);
  if (!as_config_add_hosts(&config, host.c_str(), port)) {
    as_event_close_loops();
    throw Failed("Aerospike config add hosts error");
  }
  const auto [user, password] = split_user_info(url.user_info());
  if (user && password) {
    as_config_set_user(&config, user->c_str(), password->c_str());
  }
  config.conn_timeout_ms = connect_timeout;

  LOG("Aerospike connecting to {}:{} (connect timeout {} ms)",
      host,
      port,
      connect_timeout);

  m_context.reset(aerospike_new(&config));
  as_error err;
  auto ec = aerospike_connect(m_context.get(), &err);
  if (ec != AEROSPIKE_OK) {
    throw Failed(FMT("Aerospike connection error: {}", err.message));
  }

  LOG_RAW("Aerospike connection OK");
}

std::string
AerospikeStorageBackend::get_key_string(const Hash::Digest& digest) const
{
  return util::format_digest(digest);
}

} // namespace

std::unique_ptr<RemoteStorage::Backend>
AerospikeStorage::create_backend(
  const Url& url, const std::vector<Backend::Attribute>& attributes) const
{
  return std::make_unique<AerospikeStorageBackend>(url, attributes);
}

} // namespace storage::remote
