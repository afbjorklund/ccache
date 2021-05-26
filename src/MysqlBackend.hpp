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

#pragma once

#include "CacheFile.hpp"
#include "Digest.hpp"
#include "StorageBackend.hpp"

#include <mysql/mysql.h>
#include <string>

class MysqlBackend : public StorageBackend
{
public:
  MysqlBackend(const std::string& url, bool store_in_backend_only);

  ~MysqlBackend() override;

  bool storeInBackendOnly() const override;

  bool getResult(const Digest& digest, const std::string& path) override;
  bool getManifest(const Digest& digest, const std::string& path) override;

  bool putResult(const Digest& digest, const std::string& path) override;
  bool putManifest(const Digest& digest, const std::string& path) override;

private:
  std::string getKey(const Digest& digest, CacheFile::Type type) const;
  bool get(const std::string& key, const std::string& path);
  bool put(const std::string& key, const std::string& path);

  const bool m_store_in_backend_only;
  MYSQL* m_mysql;
  MYSQL_STMT *m_get_stmt;
  MYSQL_STMT *m_put_stmt;
};
