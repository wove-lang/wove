// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <stdbool.h>
#include <stdint.h>
#include <sys/stat.h>

// Compare the borrowed GNU callback status with the same live file's native fields.
static inline bool ftw_oracle_stat64_matches(uint64_t device, uint64_t inode, uint64_t mode,
	uint64_t links, uint64_t uid, uint64_t gid, uint64_t special, uint64_t size,
	uint64_t block_size, uint64_t blocks) {
	struct stat64 native;
	// The fixture visits this single file below its root directory.
	if (stat64("walk-root/leaf", &native) != 0) {
		return false;
	}
	return device == (uint64_t)native.st_dev && inode == (uint64_t)native.st_ino
		&& mode == (uint64_t)native.st_mode && links == (uint64_t)native.st_nlink
		&& uid == (uint64_t)native.st_uid && gid == (uint64_t)native.st_gid
		&& special == (uint64_t)native.st_rdev && size == (uint64_t)native.st_size
		&& block_size == (uint64_t)native.st_blksize && blocks == (uint64_t)native.st_blocks;
}
