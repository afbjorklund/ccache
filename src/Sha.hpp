// Copyright (C) 2021 Joel Rosdahl and other contributors
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

#include "system.hpp"

#include "assertions.hpp"

#include <openssl/sha.h>

class Sha
{
public:
  Sha(int bits);
  ~Sha();

  void reset();
  void update(const void* data, size_t length);
  void digest(uint8_t hash[]);
  size_t length();

private:
  SHA256_CTX* m_state;
  size_t m_length;
};

inline Sha::Sha(int bits)
{
  ASSERT(bits == SHA256_DIGEST_LENGTH * 8);
  m_state = new SHA256_CTX;
  reset();
}

inline Sha::~Sha()
{
  delete m_state;
  m_state = nullptr;
}

inline void
Sha::reset()
{
  SHA256_Init(m_state);
  m_length = 0;
}

inline void
Sha::update(const void* data, size_t length)
{
  SHA256_Update(m_state, data, length);
  m_length += length;
}

inline void
Sha::digest(uint8_t hash[])
{
  SHA256_Final(hash, m_state);
  m_length = 0;
}

inline size_t
Sha::length()
{
  return m_length;
}
