// Copyright (C) 2020-2022 Joel Rosdahl and other contributors
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

#include "ResultFiles.hpp"

#include "Context.hpp"
#include "Logging.hpp"
#include "fmtmacros.hpp"

namespace core {

ResultFiles::ResultFiles(
  std::optional<GetRawFilePathFunction> get_raw_file_path,
  std::optional<GetCasFilePathFunction> get_cas_file_path)
  : m_get_raw_file_path(get_raw_file_path),
    m_get_cas_file_path(get_cas_file_path)
{
}

std::vector<std::string>
ResultFiles::files()
{
  return m_files;
}

void
ResultFiles::on_header(const Result::Deserializer::Header& header)
{
  m_files.reserve(header.n_files);
}

void ResultFiles::on_embedded_file(uint8_t file_number,
                                   Result::FileType,
                                   nonstd::span<const uint8_t>)
{
  if (!m_get_raw_file_path) {
    throw Error("Raw entry for non-local result");
  }
  m_files.push_back((*m_get_raw_file_path)(file_number));
}

void ResultFiles::on_raw_file(uint8_t, Result::FileType, uint64_t)
{
}

void
ResultFiles::on_cas_file(uint8_t, Result::FileType, uint64_t, Digest file_hash)
{
  if (!m_get_cas_file_path) {
    throw Error("Cas entry for non-local result");
  }
  m_files.push_back((*m_get_cas_file_path)(file_hash));
}

} // namespace core
