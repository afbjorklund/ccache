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

#pragma once

#include <Digest.hpp>
#include <core/Result.hpp>
#include <core/exceptions.hpp>

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace core {

// This class gathers information about non-contained files.
class ResultFiles : public Result::Deserializer::Visitor
{
public:
  using GetRawFilePathFunction = std::function<std::string(uint8_t)>;
  using GetCasFilePathFunction = std::function<std::string(Digest&)>;

  ResultFiles(
    const std::string& tmp_dir,
    std::optional<GetRawFilePathFunction> get_raw_file_path = std::nullopt,
    std::optional<GetCasFilePathFunction> get_cas_file_path = std::nullopt);

  struct ResultFile
  {
    Result::FileType file_type;
    std::string path;
  };

  std::vector<ResultFile> files();

  void on_header(const Result::Deserializer::Header& header) override;

  void on_embedded_file(uint8_t file_number,
                        Result::FileType file_type,
                        nonstd::span<const uint8_t> data) override;
  void on_raw_file(uint8_t file_number,
                   Result::FileType file_type,
                   uint64_t file_size) override;
  void on_cas_file(uint8_t file_number,
                   Result::FileType file_type,
                   uint64_t file_size,
                   Digest file_hash) override;

private:
  std::string m_tmp_dir;
  std::vector<ResultFile> m_files;
  std::optional<GetRawFilePathFunction> m_get_raw_file_path;
  std::optional<GetCasFilePathFunction> m_get_cas_file_path;
};

} // namespace core
