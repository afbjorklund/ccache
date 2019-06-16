// Copyright (C) 2010-2019 Joel Rosdahl
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

// This file contains tests for functions in hash.c.

#include "../src/ccache.h"
#include "../src/hash.h"
#include "framework.h"

TEST_SUITE(mdfour)

TEST(test_vectors_from_rfc_1320_should_be_correct)
{
	char d[DIGEST_STRING_BUFFER_SIZE];

	{
		struct hash *h = hash_init();
		hash_string(h, "");
		hash_result_as_string(h, d);
#ifdef USE_BLAKE2
		CHECK_STR_EQ("3345524abf6bbe1809449224b5972c41790b6cf2", d);
#else
		CHECK_STR_EQ("31d6cfe0d16ae931b73c59d7e0c089c000000000", d);
#endif
		hash_free(h);
	}

	{
		struct hash *h = hash_init();
		hash_string(h, "a");
		hash_result_as_string(h, d);
#ifdef USE_BLAKE2
		CHECK_STR_EQ("948caa2db61bc4cdb4faf7740cd491f195043914", d);
#else
		CHECK_STR_EQ("bde52cb31de33e46245e05fbdbd6fb2400000001", d);
#endif
		hash_free(h);
	}

	{
		struct hash *h = hash_init();
		hash_string(h, "message digest");
		hash_result_as_string(h, d);
#ifdef USE_BLAKE2
		CHECK_STR_EQ("6bfec6f65e52962be863d6ea1005fc5e4cc8478c", d);
#else
		CHECK_STR_EQ("d9130a8164549fe818874806e1c7014b0000000e", d);
#endif
		hash_free(h);
	}

	{
		struct hash *h = hash_init();
		hash_string(
			h,
			"12345678901234567890123456789012345678901234567890123456789012345678901"
			"234567890");
		hash_result_as_string(h, d);
#ifdef USE_BLAKE2
		CHECK_STR_EQ("c2be0e534a67d25947f0c7e78527b2f82abd260f", d);
#else
		CHECK_STR_EQ("e33b4ddc9c38f2199c3e7b164fcc053600000050", d);
#endif
		hash_free(h);
	}
}

TEST(hash_result_should_not_alter_state)
{
	char d[DIGEST_STRING_BUFFER_SIZE];
	struct hash *h = hash_init();
	hash_string(h, "message");
	hash_result_as_string(h, d);
	hash_string(h, " digest");
	hash_result_as_string(h, d);
#ifdef USE_BLAKE2
	CHECK_STR_EQ("6bfec6f65e52962be863d6ea1005fc5e4cc8478c", d);
#else
	CHECK_STR_EQ("d9130a8164549fe818874806e1c7014b0000000e", d);
#endif
	hash_free(h);
}

TEST(hash_result_should_be_idempotent)
{
	char d[DIGEST_STRING_BUFFER_SIZE];
	struct hash *h = hash_init();
	hash_string(h, "");
#ifdef USE_BLAKE2
	hash_result_as_string(h, d);
	CHECK_STR_EQ("3345524abf6bbe1809449224b5972c41790b6cf2", d);
	hash_result_as_string(h, d);
	CHECK_STR_EQ("3345524abf6bbe1809449224b5972c41790b6cf2", d);
#else
	hash_result_as_string(h, d);
	CHECK_STR_EQ("31d6cfe0d16ae931b73c59d7e0c089c000000000", d);
	hash_result_as_string(h, d);
	CHECK_STR_EQ("31d6cfe0d16ae931b73c59d7e0c089c000000000", d);
#endif

	hash_free(h);
}

TEST(hash_result_as_bytes)
{
	struct hash *h = hash_init();
	hash_string(h, "message digest");
	struct digest d;
	hash_result_as_bytes(h, &d);
	uint8_t expected[sizeof(d.bytes)] = {
#ifdef USE_BLAKE2
		0x6b, 0xfe, 0xc6, 0xf6, 0x5e, 0x52, 0x96, 0x2b, 0xe8, 0x63, 0xd6, 0xea,
		0x10, 0x05, 0xfc, 0x5e, 0x4c, 0xc8, 0x47, 0x8c
#else
		0xd9, 0x13, 0x0a, 0x81, 0x64, 0x54, 0x9f, 0xe8, 0x18, 0x87, 0x48, 0x06,
		0xe1, 0xc7, 0x01, 0x4b, 0x00, 0x00, 0x00, 0x0e
#endif
	};
	CHECK_DATA_EQ(d.bytes, expected, sizeof(d.bytes));
	hash_free(h);
}

TEST_SUITE_END
