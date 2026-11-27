// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_FIXTURE_STATVFS_H
#define WOVE_FIXTURE_STATVFS_H

#include <stdbool.h>
#include <sys/statvfs.h>

_Static_assert(_Generic(&statvfs, int (*)(const char *, struct statvfs *): 1, default: 0), "statvfs signature");
_Static_assert(_Generic(&fstatvfs, int (*)(int, struct statvfs *): 1, default: 0), "fstatvfs signature");

// Compare the fields that are stable across calls to the same filesystem.
static inline bool fixture_stable_match(const struct statvfs *value, const struct statvfs *expected)
{
	return value->f_bsize == expected->f_bsize && value->f_frsize == expected->f_frsize
		&& value->f_fsid == expected->f_fsid && value->f_flag == expected->f_flag
		&& value->f_namemax == expected->f_namemax;
}

// Query the same path and descriptor independently through C.
static inline bool fixture_stable_fields(int fd, const struct statvfs *path, const struct statvfs *descriptor)
{
	struct statvfs native_path;
	struct statvfs native_fd;
	return statvfs(".", &native_path) == 0 && fstatvfs(fd, &native_fd) == 0
		&& fixture_stable_match(path, &native_path) && fixture_stable_match(descriptor, &native_fd);
}

// Compare the projected counts to the same returned object so concurrent filesystem use is harmless.
static inline bool fixture_counts(const struct statvfs *value, fsblkcnt_t blocks, fsblkcnt_t free_blocks,
	fsblkcnt_t available_blocks, fsfilcnt_t files, fsfilcnt_t free_files, fsfilcnt_t available_files)
{
	return value->f_blocks == blocks && value->f_bfree == free_blocks && value->f_bavail == available_blocks
		&& value->f_files == files && value->f_ffree == free_files && value->f_favail == available_files;
}

// Compare each remaining projection to the native unsigned-long member.
static inline bool fixture_words(const struct statvfs *value, unsigned long block_size,
	unsigned long fragment_size, unsigned long id, unsigned long flags, unsigned long name_max)
{
	return value->f_bsize == block_size && value->f_frsize == fragment_size
		&& value->f_fsid == id && value->f_flag == flags && value->f_namemax == name_max;
}

// Make every field distinct and exercise the high bits at each native count width.
static inline void fixture_seed(struct statvfs *value)
{
	*value = (struct statvfs){.f_bsize = 1, .f_frsize = 2, .f_blocks = (fsblkcnt_t)-3,
		.f_bfree = (fsblkcnt_t)-4, .f_bavail = (fsblkcnt_t)-5, .f_files = (fsfilcnt_t)-6,
		.f_ffree = (fsfilcnt_t)-7, .f_favail = (fsfilcnt_t)-8, .f_fsid = 9, .f_flag = 10, .f_namemax = 11};
}

// A missing path and an invalid descriptor both report the native failure status.
static inline bool fixture_failures(int path_status, int fd_status)
{
	struct statvfs value;
	return path_status == -1 && path_status == statvfs("wove-statvfs-missing", &value)
		&& fd_status == -1 && fd_status == fstatvfs(-1, &value);
}

// Compare the Wove constants to the native flag expressions.
static inline bool fixture_flags(int read_only, int no_suid)
{
	return read_only == ST_RDONLY && no_suid == ST_NOSUID && read_only != no_suid;
}

#ifdef __USE_LARGEFILE64
_Static_assert(_Generic(&statvfs64, int (*)(const char *, struct statvfs64 *): 1, default: 0),
	"statvfs64 signature");
_Static_assert(_Generic(&fstatvfs64, int (*)(int, struct statvfs64 *): 1, default: 0), "fstatvfs64 signature");

// Compare stable fields across calls and counts against the same returned object.
static inline bool fixture_wide_counts(int fd, const struct statvfs64 *path, const struct statvfs64 *descriptor,
	__fsblkcnt64_t blocks, __fsfilcnt64_t files)
{
	struct statvfs64 native_path;
	struct statvfs64 native_fd;
	return statvfs64(".", &native_path) == 0 && fstatvfs64(fd, &native_fd) == 0
		&& path->f_blocks == blocks && descriptor->f_files == files
		&& path->f_bsize == native_path.f_bsize && descriptor->f_flag == native_fd.f_flag;
}
#endif

#ifdef TEST_MEMBER
// Return the specific flag chosen by the optional-member manifest.
static inline int fixture_optional_flag(void)
{
	return TEST_MEMBER;
}
#endif

#endif
