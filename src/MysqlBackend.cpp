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

#include "MysqlBackend.hpp"

#include "AtomicFile.hpp"
#include "File.hpp"
#include "Logging.hpp"
#include "Util.hpp"
#include "exceptions.hpp"

MysqlBackend::MysqlBackend(const std::string& url, bool store_in_backend_only)
  : m_store_in_backend_only(store_in_backend_only)
{
  m_mysql = mysql_init(NULL);
  if (!m_mysql) {
    throw Error("failed to initialize mysql");
  }

  // TODO: parse url
  std::string hostname = url;
  std::string username = "ccache";
  std::string password = "ccache";
  std::string dbname = "ccache";
  std::string port = "";
  if (!mysql_real_connect(m_mysql, hostname.c_str(),
                          username.c_str(), password.c_str(),
                          dbname.c_str(),
                          port.empty() ? 0 : std::stoi(port), NULL, 0)) {
    throw Error("failed to connect to server: {}", mysql_error(m_mysql));
  }

  if (mysql_query(m_mysql, "CREATE TABLE IF NOT EXISTS `ccache` ("
                           "`key` varchar(42) NOT NULL, "
                           "`val` longblob, "
                           "`ts` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP, "
                           "PRIMARY KEY (`key`))")) {
    throw Error("failed to create table: {}", mysql_error(m_mysql));
  }

  if ((m_get_stmt = mysql_stmt_init(m_mysql))) {
    std::string stmt = "SELECT `val` FROM `ccache` WHERE `key` = ?";
    if (mysql_stmt_prepare(m_get_stmt, stmt.c_str(), stmt.length()) != 0) {
      throw Error("failed to create stmt: {}", mysql_error(m_mysql));
    }
  }

  if ((m_put_stmt = mysql_stmt_init(m_mysql))) {
    std::string stmt = "REPLACE INTO `ccache` (`key`, `val`) VALUES (?, ?)";
    if (mysql_stmt_prepare(m_put_stmt, stmt.c_str(), stmt.length()) != 0) {
      throw Error("failed to create stmt: {}", mysql_error(m_mysql));
    }
  }
}

MysqlBackend::~MysqlBackend()
{
  mysql_stmt_close(m_put_stmt);
  m_put_stmt = nullptr;
  mysql_stmt_close(m_get_stmt);
  m_get_stmt = nullptr;
  mysql_close(m_mysql);
  m_mysql = nullptr;
}

bool
MysqlBackend::storeInBackendOnly() const
{
  return m_store_in_backend_only;
}

bool
MysqlBackend::getResult(const Digest& digest, const std::string& path)
{
  auto url = getKey(digest, CacheFile::Type::result);
  return get(url, path);
}

bool
MysqlBackend::getManifest(const Digest& digest, const std::string& path)
{
  auto url = getKey(digest, CacheFile::Type::manifest);
  return get(url, path);
}

bool
MysqlBackend::putResult(const Digest& digest, const std::string& path)
{
  auto url = getKey(digest, CacheFile::Type::result);
  return put(url, path);
}

bool
MysqlBackend::putManifest(const Digest& digest, const std::string& path)
{
  auto url = getKey(digest, CacheFile::Type::manifest);
  return put(url, path);
}

std::string
MysqlBackend::getKey(const Digest& digest, CacheFile::Type type) const
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
MysqlBackend::put(const std::string& key, const std::string& path)
{
  std::string contents(Util::read_file(path));

  bool result = true;

  MYSQL_BIND par[2];
  memset(par, 0, sizeof(par));

  void* buffer = const_cast<char*>(key.c_str());
  unsigned long length = key.length();
  par[0].buffer_type = MYSQL_TYPE_STRING;
  par[0].buffer = buffer;
  par[0].length = &length;
  void* data = const_cast<char*>(contents.data());
  unsigned long size = contents.size();
  par[1].buffer_type = MYSQL_TYPE_BLOB;
  par[1].buffer = data;
  par[1].length = &size;
  if (mysql_stmt_bind_param(m_put_stmt, par)) {
    LOG("bind param: {}", mysql_stmt_error(m_put_stmt));
    return false;
  }

  if (mysql_stmt_execute(m_put_stmt) != 0) {
      LOG("execute: {}", mysql_stmt_error(m_put_stmt));
      return false;
  }

  LOG("Succeeded to put {} to sql cache {}", path, key);
  return result;
}

bool
MysqlBackend::get(const std::string& key, const std::string& path)
{
  AtomicFile file(path, AtomicFile::Mode::binary);

  bool result = true;

  MYSQL_BIND par[1];
  memset(par, 0, sizeof(par));

  void* buffer = const_cast<char*>(key.c_str());
  unsigned long length = key.length();
  par[0].buffer_type = MYSQL_TYPE_STRING;
  par[0].buffer = buffer;
  par[0].length = &length;
  if (mysql_stmt_bind_param(m_get_stmt, par)) {
    LOG("bind param: {}", mysql_stmt_error(m_get_stmt));
    return false;
  }

  if (mysql_stmt_execute(m_get_stmt) != 0) {
      LOG("execute: {}", mysql_stmt_error(m_get_stmt));
      return false;
  }

  MYSQL_BIND res[1];
  memset(res, 0, sizeof(res));

  char* data_buffer = NULL;
  unsigned long real_length = 0;
  res[0].buffer_type = MYSQL_TYPE_BLOB;
  res[0].buffer = NULL;
  res[0].length = &real_length;
  if (mysql_stmt_bind_result(m_get_stmt, res)) {
    LOG("bind result: {}", mysql_stmt_error(m_get_stmt));
    return false;
  }

  if (mysql_stmt_fetch(m_get_stmt) == 1) {
      LOG("fetch: {}", mysql_stmt_error(m_get_stmt));
      return false;
  }

  data_buffer = (char*) malloc(real_length);
  res[0].buffer = data_buffer;
  res[0].buffer_length = real_length;
  if (mysql_stmt_fetch_column(m_get_stmt, res, 0, 0) != 0) {
      LOG("fetch column: {}", mysql_stmt_error(m_get_stmt));
      return false;
  }

  std::string contents(data_buffer, real_length);
  file.write(contents);
  file.commit();
  free(data_buffer);
  mysql_stmt_free_result(m_get_stmt);
  LOG("Succeeded to get {} from sql cache {}", path, key);
  return result;
}
