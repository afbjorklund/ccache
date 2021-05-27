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
#include "Sha.hpp"
#include "Util.hpp"
#include "fmtmacros.hpp"

using nonstd::optional;

ResultDumper::ResultDumper(FILE* stream) : m_stream(stream)
{
}

void
ResultDumper::on_header(CacheEntryReader& cache_entry_reader)
{
  cache_entry_reader.dump_header(m_stream);
}

static Sha sha(256);

void
ResultDumper::on_entry_start(uint32_t entry_number,
                             Result::FileType file_type,
                             uint64_t file_len,
                             optional<std::string> raw_file,
                             optional<std::string> sha_hex)
{
  PRINT(m_stream,
        "{} {} #{}: {} ({} bytes)\n",
        sha_hex ? "SHA-256" : (raw_file ? "Raw" : "Embedded"),
        sha_hex ? "checksum" : "file",
        entry_number,
        Result::file_type_to_string(file_type),
        file_len);
  if (raw_file) {
    int fd = open(raw_file->c_str(), O_RDONLY);
    if (!fd) {
      throw Error("{}: {}", *raw_file, strerror(errno));
    }
    sha.reset();
    ssize_t n;
    char buffer[READ_BUFFER_SIZE];
    while ((n = read(fd, buffer, sizeof(buffer))) != 0) {
      if (n == -1 && errno != EINTR) {
        throw Error("{}: {}", *raw_file, strerror(errno));
      }
      sha.update(buffer, n);
    }
    uint8_t hash[SHA256_DIGEST_LENGTH];
    sha.digest(hash);
    close(fd);
    std::string sha_256 = Util::format_base16(hash, sizeof(hash));
    PRINT(m_stream, "{}  {}\n", sha_256, *raw_file);
  } else if (sha_hex) {
    PRINT(m_stream, "{}\n", *sha_hex);
  } else {
    sha.reset();
  }
}

void
ResultDumper::on_entry_data(const uint8_t* data, size_t size)
{
  sha.update(data, size);
}

void
ResultDumper::on_entry_end()
{
  if (sha.length()) {
    uint8_t hash[SHA256_DIGEST_LENGTH];
    sha.digest(hash);
    std::string sha_256 = Util::format_base16(hash, sizeof(hash));
    PRINT(m_stream, "{}  {}\n", sha_256, "-" /*stdin */);
  }
}
