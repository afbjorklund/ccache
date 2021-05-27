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

#include "ResultExtractor.hpp"

#include "Util.hpp"
#include "fmtmacros.hpp"

ResultExtractor::ResultExtractor(const std::string& directory,
                                 const std::string& cas_path)
  : m_directory(directory),
    m_cas_path(cas_path)
{
}

void
ResultExtractor::on_header(CacheEntryReader& /*cache_entry_reader*/)
{
}

void
ResultExtractor::on_entry_start(uint32_t /*entry_number*/,
                                Result::FileType file_type,
                                uint64_t file_len,
                                nonstd::optional<std::string> raw_file,
                                nonstd::optional<std::string> sha_hex)
{
  std::string suffix = Result::file_type_to_string(file_type);
  if (suffix == Result::k_unknown_file_type) {
    suffix = FMT(".type_{}", file_type);
  } else if (suffix[0] == '<') {
    suffix[0] = '.';
    suffix.resize(suffix.length() - 1);
  }

  m_dest_path = FMT("{}/ccache-result{}", m_directory, suffix);

  if (!raw_file && !sha_hex) {
    m_dest_fd = Fd(
      open(m_dest_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0666));
    if (!m_dest_fd) {
      throw Error(
        "Failed to open {} for writing: {}", m_dest_path, strerror(errno));
    }
  } else if (raw_file) {
    try {
      Util::copy_file(*raw_file, m_dest_path, false);
    } catch (Error& e) {
      throw Error(
        "Failed to copy {} to {}: {}", *raw_file, m_dest_path, e.what());
    }
  } else if (sha_hex) {
    std::string cas_file = m_cas_path + "/" + *sha_hex;
    auto st = Stat::stat(cas_file, Stat::OnError::throw_error);
    if (st.size() != file_len) {
      throw Error("Bad file size of {} (actual {} bytes, expected {} bytes)",
                  cas_file,
                  st.size(),
                  file_len);
    }
    try {
      Util::copy_file(cas_file, m_dest_path, false);
    } catch (Error& e) {
      throw Error(
        "Failed to copy {} to {}: {}", cas_file, m_dest_path, e.what());
    }
  }
}

void
ResultExtractor::on_entry_data(const uint8_t* data, size_t size)
{
  ASSERT(m_dest_fd);

  try {
    Util::write_fd(*m_dest_fd, data, size);
  } catch (Error& e) {
    throw Error("Failed to write to {}: {}", m_dest_path, e.what());
  }
}

void
ResultExtractor::on_entry_end()
{
  if (m_dest_fd) {
    m_dest_fd.close();
  }
}
