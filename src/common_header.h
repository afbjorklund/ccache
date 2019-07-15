#ifndef COMMON_HEADER_H
#define COMMON_HEADER_H

#include "compression.h"
#include "xxhash.h"

#define COMMON_HEADER_SIZE 15

struct common_header {
	char magic[4];
	uint8_t version;
	uint8_t compression_type;
	int8_t compression_level;
	uint64_t content_size;
};

// Initialize a common_header and write the header data to a file.
//
// header:            Header to initialize.
// output:            Open file to write to.
// magic:             File format magic bytes.
// version:           File format version.
// compression_type:  Compression type to use.
// compression_level: Compression level to use.
// content_size:      Content size.
// compressor:        Compressor created from compression_type.
// compressor_state:  State for the compressor.
// checksum:          Checksum state that will be updated with the written
//                    bytes.
bool common_header_initialize_for_writing(
	struct common_header *header,
	FILE *output,
	const char magic[4],
	uint8_t version,
	uint8_t compression_type,
	int8_t compression_level,
	uint64_t content_size,
	XXH64_state_t *checksum,
	struct compressor **compressor,
	struct compr_state **compr_state);

// Initialize a common_header by reading header data from a file.
//
// header:             Header to initialize.
// input:              Open file to read from.
// expected_magic:     Expected file format magic bytes.
// expected_version:   Expected file format version.
// decompressor:       Decompressor created from the compression type field in
//                     the header. Pass NULL to not create a decompressor.
// decompressor_state: State for the decompressor. Should be NULL if
//                     decompressor is NULL.
// checksum:           Checksum state that will be updated with the read bytes.
//                     May be NULL for no checksumming.
bool common_header_initialize_for_reading(
	struct common_header *header,
	FILE *input,
	const char expected_magic[4],
	uint8_t expected_version,
	struct decompressor **decompressor,
	struct decompr_state **decompr_state,
	XXH64_state_t *checksum,
	char **errmsg);

size_t common_header_size(const struct common_header *header, bool *is_compressed);

void common_header_dump(const struct common_header *header, FILE *f);

#endif
