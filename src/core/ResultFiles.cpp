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
#include "TemporaryFile.hpp"
#include "fmtmacros.hpp"

#include <util/expected.hpp>
#include <util/file.hpp>

namespace core {

ResultFiles::ResultFiles(
  const std::string& tmp_dir,
  std::optional<GetRawFilePathFunction> get_raw_file_path,
  std::optional<GetCasFilePathFunction> get_cas_file_path)
  : m_tmp_dir(tmp_dir),
    m_get_raw_file_path(get_raw_file_path),
    m_get_cas_file_path(get_cas_file_path)
{
}

std::vector<ResultFiles::ResultFile>
ResultFiles::files()
{
  return m_files;
}

void
ResultFiles::on_header(const Result::Deserializer::Header& header)
{
  m_files.reserve(header.n_files);
}

void
ResultFiles::on_embedded_file(uint8_t /*file_number*/,
                              Result::FileType file_type,
                              nonstd::span<const uint8_t> data)
{
  if (!m_tmp_dir.empty()) {
    std::string suffix = Result::file_type_to_string(file_type);
    TemporaryFile tmp_file(m_tmp_dir + "/embedded", suffix);
    util::throw_on_error<Error>(
      util::write_fd(*tmp_file.fd, data.data(), data.size()),
      FMT("Failed to write to {}: ", tmp_file.path));
    m_files.push_back(ResultFiles::ResultFile{file_type, tmp_file.path});
  }
}

void
ResultFiles::on_raw_file(uint8_t file_number,
                         Result::FileType file_type,
                         uint64_t)
{
  if (!m_get_raw_file_path) {
    throw Error("Raw entry for non-local result");
  }
  m_files.push_back(ResultFile{file_type, (*m_get_raw_file_path)(file_number)});
}

void
ResultFiles::on_cas_file(
  uint8_t, Result::FileType file_type, uint64_t, uint16_t, Digest file_hash)
{
  if (!m_get_cas_file_path) {
    throw Error("Cas entry for non-local result");
  }
  m_files.push_back(ResultFile{file_type, (*m_get_cas_file_path)(file_hash)});
}

} // namespace core
