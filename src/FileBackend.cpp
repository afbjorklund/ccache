// Copyright (C) 2020 Joel Rosdahl and other contributors
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

#include "FileBackend.hpp"

#include "AtomicFile.hpp"
#include "File.hpp"
#include "Logging.hpp"
#include "Util.hpp"
#include "exceptions.hpp"

FileBackend::FileBackend(const std::string& url, bool store_in_backend_only)
  : m_url(fixupUrl(url)), m_store_in_backend_only(store_in_backend_only)
{
}

FileBackend::~FileBackend()
{
}

std::string
FileBackend::fixupUrl(std::string url)
{
  if (url.empty()) {
    throw Error("file cache URL is empty.");
  }

  if (url.back() == '/') {
    url.resize(url.size() - 1u);
  }

  return url;
}

bool
FileBackend::storeInBackendOnly() const
{
  return m_store_in_backend_only;
}

bool
FileBackend::getResult(const Digest& digest, const std::string& path)
{
  auto url = getUrl(digest, CacheFile::Type::result);
  return get(url, path);
}

bool
FileBackend::getManifest(const Digest& digest, const std::string& path)
{
  auto url = getUrl(digest, CacheFile::Type::manifest);
  return get(url, path);
}

bool
FileBackend::putResult(const Digest& digest, const std::string& path)
{
  auto url = getUrl(digest, CacheFile::Type::result);
  return put(url, path);
}

bool
FileBackend::putManifest(const Digest& digest, const std::string& path)
{
  auto url = getUrl(digest, CacheFile::Type::manifest);
  return put(url, path);
}

std::string
FileBackend::getUrl(const Digest& digest, CacheFile::Type type) const
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

  return m_url + '/' + file_part;
}

bool
FileBackend::put(const std::string& url, const std::string& path)
{
  try {
    Util::copy_file(path, url, true);
  } catch (const std::exception& e) {
    LOG("Failed to put {} to http cache: exception: {}", path, e.what());
    return false;
  }
  LOG("Succeeded to put {} to file cache", path);
  return true;
}

bool
FileBackend::get(const std::string& url, const std::string& path)
{
  try {
    Util::copy_file(url, path, true);
  } catch (const std::exception& e) {
    LOG(
      "Failed to get {} from file cache: exception: {}", path, e.what());
    return false;
  }
  LOG("Succeeded to get {} from file cache", path);
  return true;
}
