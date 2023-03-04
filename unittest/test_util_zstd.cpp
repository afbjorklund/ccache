// Copyright (C) 2019-2022 Joel Rosdahl and other contributors
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

#include <TestUtil.hpp>
#include <util/Bytes.hpp>
#include <util/zstd.hpp>

#include <third_party/doctest.h>
#include <third_party/xxhash.h>

#include <string>

using TestUtil::TestContext;

const util::Bytes compressed_ab{
  0x28, 0xb5, 0x2f, 0xfd, 0x20, 0x02, 0x11, 0x00, 0x00, 0x61, 0x62};

const util::Bytes checksum_ab{0x61, 0x4a, 0xd0, 0x92};

TEST_CASE("util::zstd_compress")
{
  TestContext test_context;

  util::Bytes output{'x'};
  auto result = util::zstd_compress(util::Bytes{'a', 'b'}, output, 1);
  CHECK(result);
  CHECK(output.size() == 12);
  util::Bytes expected{'x'};
  expected.insert(expected.end(), compressed_ab.begin(), compressed_ab.end());
  CHECK(output == expected);
}

TEST_CASE("util::zstd_compress (checksum)")
{
  TestContext test_context;

  util::Bytes output;
  auto result = util::zstd_compress(util::Bytes{'a', 'b'}, output, 0, true);
  CHECK(result);
  CHECK(output.size() == 11 + 4);
  util::Bytes expected;
  expected.insert(expected.end(), compressed_ab.begin(), compressed_ab.end());
  expected[4] |= 1 << 2; // checksum
  expected.insert(expected.end(), checksum_ab.begin(), checksum_ab.end());
  CHECK(output == expected);
}

TEST_CASE("XXH64")
{
  XXH64_hash_t hash = XXH64("ab", 2, 0); // truncated to 32
  util::Bytes output{static_cast<uint8_t>((hash >> 0) & 0xff),
                     static_cast<uint8_t>((hash >> 8) & 0xff),
                     static_cast<uint8_t>((hash >> 16) & 0xff),
                     static_cast<uint8_t>((hash >> 24) & 0xff)};
  util::Bytes expected;
  expected.insert(expected.end(), checksum_ab.begin(), checksum_ab.end());
  CHECK(output == expected);
}

TEST_CASE("util::zstd_decompress")
{
  TestContext test_context;

  util::Bytes input = compressed_ab;
  util::Bytes output{'x'};
  auto result = util::zstd_decompress(input, output, 2);
  CHECK(result);
  CHECK(output == util::Bytes{'x', 'a', 'b'});
}

TEST_CASE("util::zstd_decompress (checksum: good)")
{
  TestContext test_context;

  util::Bytes input = compressed_ab;
  input[4] |= 1 << 2; // checksum
  input.insert(input.end(), checksum_ab.begin(), checksum_ab.end());
  util::Bytes output;
  auto result = util::zstd_decompress(input, output, 2);
  CHECK(result);
  CHECK(output == util::Bytes{'a', 'b'});
}

TEST_CASE("util::zstd_decompress (checksum: bad)")
{
  TestContext test_context;

  util::Bytes input = compressed_ab;
  input[4] |= 1 << 2; // checksum
  input.insert(input.end(), "1234", 4);
  util::Bytes output;
  auto result = util::zstd_decompress(input, output, 2);
  CHECK(!result);
}

TEST_CASE("util::zstd_is_compressed")
{
  util::Bytes input = compressed_ab;
  auto result = util::zstd_is_compressed(input);
  CHECK(result == true);

  input = util::Bytes("ab", 2);
  result = util::zstd_is_compressed(input);
  CHECK(result == false);
}

TEST_CASE("util::zstd_decompressed_size")
{
  util::Bytes input = compressed_ab;
  auto result = util::zstd_decompressed_size(input);
  CHECK(result == 2);
}

TEST_CASE("ZSTD roundtrip")
{
  TestContext test_context;

  const util::Bytes data{0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  const size_t copies = 10000;
  util::Bytes original_input;
  for (size_t i = 0; i < copies; i++) {
    original_input.insert(original_input.end(), data.begin(), data.end());
  }

  util::Bytes output;
  auto result = util::zstd_compress(original_input, output, 1);
  CHECK(result);
  CHECK(output.size() < 100);

  util::Bytes decompressed_input;
  result =
    util::zstd_decompress(output, decompressed_input, copies * data.size());
  CHECK(result);
  CHECK(decompressed_input == original_input);
}
