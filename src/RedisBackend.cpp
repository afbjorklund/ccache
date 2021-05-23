// Copyright (C) 2021 Joel Rosdahl and other contributors
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

#include "RedisBackend.hpp"

#include "AtomicFile.hpp"
#include "File.hpp"
#include "Logging.hpp"
#include "Util.hpp"
#include "exceptions.hpp"

RedisBackend::RedisBackend(const std::string& url)
{
  std::string host = url;
  int port = 6379; // TODO
  m_context = redisConnect(host.c_str(), port);
  if (!m_context || m_context->err) {
    throw Error("failed to initialize redis");
  }
}

RedisBackend::~RedisBackend()
{
  redisFree(m_context);
  m_context = nullptr;
}

bool
RedisBackend::storeInBackendOnly() const
{
  return false;
}

bool
RedisBackend::getResult(const Digest& digest, const std::string& path)
{
  auto url = getKey(digest, CacheFile::Type::result);
  return get(url, path);
}

bool
RedisBackend::getManifest(const Digest& digest, const std::string& path)
{
  auto url = getKey(digest, CacheFile::Type::manifest);
  return get(url, path);
}

bool
RedisBackend::putResult(const Digest& digest, const std::string& path)
{
  auto url = getKey(digest, CacheFile::Type::result);
  return put(url, path);
}

bool
RedisBackend::putManifest(const Digest& digest, const std::string& path)
{
  auto url = getKey(digest, CacheFile::Type::manifest);
  return put(url, path);
}

std::string
RedisBackend::getKey(const Digest& digest, CacheFile::Type type) const
{
  auto file_part = digest.to_string();

  switch (type) {
  case CacheFile::Type::result:
    file_part.append(".result");
    break;

  case CacheFile::Type::manifest:
    file_part.append(".manifest");
    break;

  case CacheFile::Type::unknown:
    file_part.append(".unknown");
    break;
  }

  return file_part;
}

bool
RedisBackend::put(const std::string& key, const std::string& path)
{
  std::string contents(Util::read_file(path));

  redisReply *reply = static_cast<redisReply*>(redisCommand(m_context,
                                                            "SET %s %b", key.c_str(), contents.data(), contents.size()));
  bool result;
  if (!reply) {
    LOG("Failed to put {} to redis cache", path);
    result = false;
  } else if (reply->type == REDIS_REPLY_ERROR) {
    LOG("Failed to put {} to redis cache: reply string {}", path, reply->str);
    result = false;
  } else {
    LOG("Succeeded to put {} to redis cache {}: reply string: {}", path, key, reply->str);
    result = true;
  }
  freeReplyObject(reply);
  return result;
}

bool
RedisBackend::get(const std::string& key, const std::string& path)
{
  AtomicFile file(path, AtomicFile::Mode::binary);

  redisReply *reply = static_cast<redisReply*>(redisCommand(m_context,
                                                           "GET %s", key.c_str()));
  bool result;
  if (!reply) {
    LOG("Failed to get {} from redis cache", path);
    result = false;
  } else if (reply->type == REDIS_REPLY_ERROR) {
    LOG("Failed to get {} from redis cache: reply string {}", path, reply->str);
    result = false;
  } else if (reply->type == REDIS_REPLY_STRING) {
    std::string contents(reply->str, reply->len);
    file.write(contents);
    file.commit();
    LOG("Succeeded to get {} from redis cache {}", path, key);
    result = true;
  }
  freeReplyObject(reply);
  return result;
}
