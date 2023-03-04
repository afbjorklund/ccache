// Copyright (C) 2022 Joel Rosdahl and other contributors
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

#include "zstd.hpp"

#include <zstd.h>

namespace util {

nonstd::expected<void, std::string>
zstd_compress(nonstd::span<const uint8_t> input,
              util::Bytes& output,
              int8_t compression_level,
              bool checksum)
{
  const size_t original_output_size = output.size();
  const size_t compress_bound = zstd_compress_bound(input.size());
  output.resize(original_output_size + compress_bound);

#if 0 // simple API (one parameter)
  const size_t ret = ZSTD_compress(&output[original_output_size],
                                   compress_bound,
                                   input.data(),
                                   input.size(),
                                   compression_level);
#else // advanced API (for checksumFlag)
  ZSTD_CCtx* const cctx = ZSTD_createCCtx();
  if (cctx == NULL) {
    return nonstd::make_unexpected("createCCtx failed");
  }
  ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, compression_level);
  ZSTD_CCtx_setParameter(cctx, ZSTD_c_checksumFlag, checksum ? 1 : 0);
  const size_t ret = ZSTD_compress2(cctx,
                                    &output[original_output_size],
                                    compress_bound,
                                    input.data(),
                                    input.size());
  ZSTD_freeCCtx(cctx);
#endif
  if (ZSTD_isError(ret)) {
    return nonstd::make_unexpected(ZSTD_getErrorName(ret));
  }

  output.resize(original_output_size + ret);
  return {};
}

nonstd::expected<void, std::string>
zstd_decompress(nonstd::span<const uint8_t> input,
                util::Bytes& output,
                size_t original_size)
{
  const size_t original_output_size = output.size();

  output.resize(original_output_size + original_size);
  const size_t ret = ZSTD_decompress(
    &output[original_output_size], original_size, input.data(), input.size());
  if (ZSTD_isError(ret)) {
    return nonstd::make_unexpected(ZSTD_getErrorName(ret));
  }

  output.resize(original_output_size + ret);

  return {};
}

bool
zstd_is_compressed(nonstd::span<const uint8_t> input)
{
  const uint32_t magic = ZSTD_MAGICNUMBER;
  return input.size() > 4 && (input[0] == ((magic >> 0) & 0xff))
         && (input[1] == ((magic >> 8) & 0xff))
         && (input[2] == ((magic >> 16) & 0xff))
         && (input[3] == ((magic >> 24) & 0xff));
}

size_t
zstd_compress_bound(size_t input_size)
{
  return ZSTD_compressBound(input_size);
}

size_t
zstd_decompressed_size(nonstd::span<const uint8_t> input)
{
  auto size = ZSTD_getDecompressedSize(input.data(), input.size());
  if (size == ZSTD_CONTENTSIZE_ERROR || size == ZSTD_CONTENTSIZE_UNKNOWN) {
    return 0;
  }
  return size;
}

std::tuple<int8_t, std::string>
zstd_supported_compression_level(int8_t wanted_level)
{
  // libzstd 1.3.4 and newer support negative levels. However, the query
  // function ZSTD_minCLevel did not appear until 1.3.6, so perform detection
  // based on version instead.
  if (ZSTD_versionNumber() < 10304 && wanted_level < 1) {
    return {1, "minimum level supported by libzstd"};
  }

  const int8_t level = std::min<int>(wanted_level, ZSTD_maxCLevel());
  if (level != wanted_level) {
    return {level, "max libzstd level"};
  }

  return {level, {}};
}

} // namespace util
