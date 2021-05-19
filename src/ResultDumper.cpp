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

#include "ResultDumper.hpp"

#include "CacheEntryReader.hpp"
#include "Context.hpp"
#include "Logging.hpp"
#include "Util.hpp"
#include "fmtmacros.hpp"

#include <openssl/sha.h>

using nonstd::optional;

ResultDumper::ResultDumper(FILE* stream) : m_stream(stream)
{
}

void
ResultDumper::on_header(CacheEntryReader& cache_entry_reader)
{
  cache_entry_reader.dump_header(m_stream);
}

static SHA256_CTX state;

void
ResultDumper::on_entry_start(uint32_t entry_number,
                             Result::FileType file_type,
                             uint64_t file_len,
                             optional<std::string> raw_file)
{
  PRINT(m_stream,
        "{} file #{}: {} ({} bytes)\n",
        raw_file ? "Raw" : "Embedded",
        entry_number,
        Result::file_type_to_string(file_type),
        file_len);
  if (raw_file) {
    int fd = open(raw_file->c_str(), O_RDONLY);
    if (!fd) {
      throw Error("{}: {}", *raw_file, strerror(errno));
    }
    SHA256_Init(&state);
    ssize_t n;
    char buffer[READ_BUFFER_SIZE];
    while ((n = read(fd, buffer, sizeof(buffer))) != 0) {
      if (n == -1 && errno != EINTR) {
        throw Error("{}: {}", *raw_file, strerror(errno));
      }
      SHA256_Update(&state, buffer, n);
    }
    uint8_t hash[SHA256_DIGEST_LENGTH];
    SHA256_Final(hash, &state);
    close(fd);
    PRINT(m_stream, "{}  {}\n", Util::format_base16(hash, sizeof(hash)), *raw_file);
  } else {
    SHA256_Init(&state);
  }
}

void
ResultDumper::on_entry_data(const uint8_t* data, size_t size)
{
  SHA256_Update(&state, data, size);
}

void
ResultDumper::on_entry_end()
{
  if (state.num) {
    uint8_t hash[SHA256_DIGEST_LENGTH];
    SHA256_Final(hash, &state);
    PRINT(m_stream, "{}  {}\n", Util::format_base16(hash, sizeof(hash)), "-" /*stdin */);
  }
}
