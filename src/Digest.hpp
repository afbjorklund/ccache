// Copyright (C) 2020-2021 Joel Rosdahl and other contributors
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

#include "Util.hpp"

#include "third_party/fmt/core.h"

#include <cstdint>
#include <string>

// Digest represents the binary form of the final digest (AKA hash or checksum)
// produced by the hash algorithm.
class Digest
{
public:
  Digest() = default;

  // Access the raw byte buffer, which is `size()` bytes long.
  const uint8_t* bytes() const;
  uint8_t* bytes();

  // Size of the digest in bytes.
  constexpr static size_t size();

  // Format the digest as hex string.
  std::string to_string() const;

  bool operator==(const Digest& other) const;
  bool operator!=(const Digest& other) const;

private:
  constexpr static size_t k_digest_size = 20;
  uint8_t m_bytes[k_digest_size];
};

// FullDigest represents the non-truncated binary form of the final digest
// produced by the hash algorithm.
class FullDigest
{
public:
  FullDigest(size_t size) : m_digest_size(size), m_bytes(new uint8_t[size])
  {
  }

  // Access the raw byte buffer, which is `size()` bytes long.
  const uint8_t* bytes() const;
  uint8_t* bytes();

  // Size of the digest in bytes.
  size_t size() const;

  // Format the digest as hex string.
  std::string to_string() const;

  // Format the digest as mtb string.
  std::string to_mtb() const;

  // Format the digest as cid string.
  std::string to_cid() const;

private:
  size_t m_digest_size;
  uint8_t* m_bytes;
};

inline const uint8_t*
Digest::bytes() const
{
  return m_bytes;
}

inline uint8_t*
Digest::bytes()
{
  return m_bytes;
}

inline constexpr size_t
Digest::size()
{
  return k_digest_size;
}

inline std::string
Digest::to_string() const
{
  // The first two bytes are encoded as four lowercase base16 digits to maintain
  // compatibility with the cleanup algorithm in older ccache versions and to
  // allow for up to four uniform cache levels. The rest are encoded as
  // lowercase base32hex digits without padding characters.
  const size_t base16_bytes = 2;
  return Util::format_base16(m_bytes, base16_bytes)
         + Util::format_base32hex(m_bytes + base16_bytes,
                                  size() - base16_bytes);
}

inline bool
Digest::operator==(const Digest& other) const
{
  return memcmp(bytes(), other.bytes(), size()) == 0;
}

inline bool
Digest::operator!=(const Digest& other) const
{
  return !(*this == other);
}

inline const uint8_t*
FullDigest::bytes() const
{
  return m_bytes;
}

inline uint8_t*
FullDigest::bytes()
{
  return m_bytes;
}

inline size_t
FullDigest::size() const
{
  return m_digest_size;
}

inline std::string
FullDigest::to_string() const
{
  // hexadecimal
  return Util::format_base16(m_bytes, size());
}

static inline std::string
_varint(int value)
{
  uint8_t output[4];
  size_t outputSize = 0;
  // While more than 7 bits of data are left, occupy the last output byte
  // and set the next byte flag
  while (value > 127) {
    // |128: Set the next byte flag
    output[outputSize] = ((uint8_t)(value & 127)) | 128;
    // Remove the seven bits we just wrote
    value >>= 7;
    outputSize++;
  }
  output[outputSize++] = ((uint8_t)value) & 127;
  return std::string(reinterpret_cast<const char*>(output), outputSize);
}

inline std::string
FullDigest::to_mtb() const
{
  std::string hash(reinterpret_cast<char*>(m_bytes), size());
  std::string mtb = _varint(0x1e) /* blake3 */ + _varint(32) /* bytes */ + hash;
  auto data = reinterpret_cast<const uint8_t*>(mtb.data());
#if 0
  // base32hex: v (rfc4648 case-insensitive - no padding - highest char)
  return "v" + Util::format_base32hex(data, mtb.size());
#else
  // base32upper: B (rfc4648 case-insensitive - no padding)
  return "B" + Util::format_base32(data, mtb.size());
#endif
}

static inline std::string
_tolower(std::string s)
{
  std::transform(s.begin(), s.end(), s.begin(), ::tolower);
  return s;
}

inline std::string
FullDigest::to_cid() const
{
  std::string hash(reinterpret_cast<char*>(m_bytes), size());
  std::string mtb = _varint(0x1e) /* blake3 */ + _varint(32) /* bytes */ + hash;
  std::string cid = _varint(1) /* v */ + _varint(0x55) /* raw */ + mtb;
  auto data = reinterpret_cast<const uint8_t*>(cid.data());
  return "b" + _tolower(Util::format_base32(data, cid.size()));
}
