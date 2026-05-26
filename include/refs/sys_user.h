/*-
 * sys_user.h - Lightweight abstractions for system functionality (userspace).
 *
 * Copyright (c) 2022-2025 Erik Larsson
 *
 * This program/include file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program/include file is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (in the main directory of the source
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation,Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _REFS_SYS_USER_H
#define _REFS_SYS_USER_H

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <inttypes.h>

#include <fcntl.h>
#include <unistd.h>
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#include <sys/stat.h>
#ifdef __linux__
#include <linux/fs.h>
#endif
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__)
#include <sys/disk.h>
#endif
#if defined(__OpenBSD__)
#include <sys/disklabel.h>
#include <sys/dkio.h>
#endif
#if defined(__DragonFly__)
#include <sys/diskslice.h>
#endif
#if defined(sun) || defined(__sun)
#include <sys/dkio.h>
#endif
#ifdef _WIN32
#include <windows.h>
#elif defined(HAVE_PTHREAD_H)
#include <pthread.h>
#endif /* defined(_WIN32) ... defined(HAVE_PTHREAD_H) */

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef u16 le16;
typedef u32 le32;
typedef u64 le64;

typedef le16 refschar;

typedef struct {
	u64 tv_sec;
	u32 tv_nsec;
} sys_timespec;

#ifdef _WIN32
typedef HANDLE sys_mutex;
#elif defined(HAVE_PTHREAD_H)
typedef pthread_mutex_t sys_mutex;
#endif /* defined(_WIN32) ... defined(HAVE_PTHREAD_H) */

static inline u16 le16_to_cpup(const le16 *const value)
{
	return (((u16) ((const u8*) value)[1]) << 8) |
		((u16) ((const u8*) value)[0]);
}

static inline u32 le32_to_cpup(const le32 *const value)
{
	return (((u32) ((const u8*) value)[3]) << 24) |
		(((u32) ((const u8*) value)[2]) << 16) |
		(((u32) ((const u8*) value)[1]) << 8) |
		((u32) ((const u8*) value)[0]);
}

static inline u64 le64_to_cpup(const le64 *const value)
{
	return (((u64) ((const u8*) value)[7]) << 56) |
		(((u64) ((const u8*) value)[6]) << 48) |
		(((u64) ((const u8*) value)[5]) << 40) |
		(((u64) ((const u8*) value)[4]) << 32) |
		(((u64) ((const u8*) value)[3]) << 24) |
		(((u64) ((const u8*) value)[2]) << 16) |
		(((u64) ((const u8*) value)[1]) << 8) |
		((u64) ((const u8*) value)[0]);
}

static inline u16 le16_to_cpu(const le16 value)
{
	return le16_to_cpup(&value);
}

static inline u32 le32_to_cpu(const le32 value)
{
	return le32_to_cpup(&value);
}

static inline u64 le64_to_cpu(const le64 value)
{
	return le64_to_cpup(&value);
}

static inline le16 cpu_to_le16(const u16 value)
{
	le16 result = 0;

	((u8*) &result)[0] = value & 0xFF;
	((u8*) &result)[1] = (value >> 8) & 0xFF;

	return result;
}

static inline le32 cpu_to_le32(const u32 value)
{
	le32 result = 0;

	((u8*) &result)[0] = value & 0xFF;
	((u8*) &result)[1] = (value >> 8) & 0xFF;
	((u8*) &result)[2] = (value >> 16) & 0xFF;
	((u8*) &result)[3] = (value >> 24) & 0xFF;

	return result;
}

static inline le64 cpu_to_le64(const u64 value)
{
	le64 result = 0;

	((u8*) &result)[0] = value & 0xFF;
	((u8*) &result)[1] = (value >> 8) & 0xFF;
	((u8*) &result)[2] = (value >> 16) & 0xFF;
	((u8*) &result)[3] = (value >> 24) & 0xFF;
	((u8*) &result)[4] = (value >> 32) & 0xFF;
	((u8*) &result)[5] = (value >> 40) & 0xFF;
	((u8*) &result)[6] = (value >> 48) & 0xFF;
	((u8*) &result)[7] = (value >> 56) & 0xFF;

	return result;
}

static inline u8 sys_fls64(u64 value)
{
#ifdef HAVE_FLSLL
	return (u8) flsll((long long) value);
#elif defined(__GNUC__)
	return (sizeof(value) * 8) - __builtin_clzll((long long) value);
#else
	u8 index = 1;

	if(!value) {
		return 0;
	}

	if((value & 0xFFFFFFFF00000000ULL)) {
		value >>= 32;
		index += 32;
	}

	if((value & 0xFFFF0000UL)) {
		value >>= 16;
		index += 16;
	}

	if((value & 0xFF00U)) {
		value >>= 8;
		index += 8;
	}

	if((value & 0xF0U)) {
		value >>= 4;
		index += 4;
	}

	if((value & 0xCU)) {
		value >>= 2;
		index += 2;
	}

	if((value & 0x2U)) {
		value >>= 1;
		index += 1;
	}

	return index;
#endif /* defined(HAVE_FLSLL) ... */
}

#ifndef SYS_LOG_CRITICAL_ENABLED
#define SYS_LOG_CRITICAL_ENABLED 1
#endif

#ifndef SYS_LOG_ERROR_ENABLED
#define SYS_LOG_ERROR_ENABLED 1
#endif

#ifndef SYS_LOG_WARNING_ENABLED
#define SYS_LOG_WARNING_ENABLED 1
#endif

#ifndef SYS_LOG_INFO_ENABLED
#define SYS_LOG_INFO_ENABLED 1
#endif

#ifndef SYS_LOG_DEBUG_ENABLED
#define SYS_LOG_DEBUG_ENABLED 0
#endif

#ifndef SYS_LOG_TRACE_ENABLED
#define SYS_LOG_TRACE_ENABLED 0
#endif

static inline const char* sys_strerror(int err)
{
	return strerror(err);
}

/**
 * No-op log handler that only exists to be able to statically check the format
 * string and arguments for errors when logging is turned off.
 *
 * @param[in] fmt
 *      @p printf format string for constructing the log message.
 * @param[in] ...
 *      Arguments to the @p printf format string (if any).
 */
static inline void sys_log_noop(const char *fmt, ...)
	__attribute__((format(printf, 1, 2)));

static inline void sys_log_noop(const char *const fmt, ...)
{
	(void) fmt;
}

/**
 * No-op error-suffixed log handler that only exists to be able to statically
 * check the format string and arguments for errors when logging is turned off.
 *
 * @param[in] err
 *      The error thrown by the system.
 * @param[in] fmt
 *      @p printf format string for constructing the log message.
 * @param[in] ...
 *      Arguments to the @p printf format string (if any).
 */
static inline void sys_log_pnoop(int err, const char *fmt, ...)
	__attribute__((format(printf, 2, 3)));

static inline void sys_log_pnoop(int err, const char *const fmt, ...)
{
	(void) err;
	(void) fmt;
}

#if SYS_LOG_CRITICAL_ENABLED
#define sys_log_critical(fmt, ...) \
	fprintf(stderr, "[CRITICAL] " fmt "\n", ##__VA_ARGS__)
#else
#define sys_log_critical sys_log_noop
#endif

#if SYS_LOG_ERROR_ENABLED
#define sys_log_error(fmt, ...) \
	fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__)
#else
#define sys_log_error sys_log_noop
#endif

#if SYS_LOG_ERROR_ENABLED
#define sys_log_perror(err, fmt, ...) \
	fprintf(stderr, "[ERROR] " fmt ": %s (%d)\n", ##__VA_ARGS__, \
		strerror(err), (err))
#else
#define sys_log_perror sys_log_pnoop
#endif

#if SYS_LOG_WARNING_ENABLED
#define sys_log_warning(fmt, ...) \
	fprintf(stderr, "[WARNING] " fmt "\n", ##__VA_ARGS__)
#else
#define sys_log_warning sys_log_noop
#endif

#if SYS_LOG_WARNING_ENABLED
#define sys_log_pwarning(err, fmt, ...) \
	fprintf(stderr, "[WARNING] " fmt ": %s (%d)\n", ##__VA_ARGS__, \
		strerror(err), (err))
#else
#define sys_log_pwarning sys_log_pnoop
#endif

#if SYS_LOG_INFO_ENABLED
#define sys_log_info(fmt, ...) \
	fprintf(stderr, fmt "\n", ##__VA_ARGS__)
#else
#define sys_log_info sys_log_noop
#endif

#if SYS_LOG_INFO_ENABLED
#define sys_log_pinfo(err, fmt, ...) \
	fprintf(stderr, fmt ": %s (%d)\n", ##__VA_ARGS__, strerror(err), (err))
#else
#define sys_log_pinfo sys_log_pnoop
#endif

#if SYS_LOG_DEBUG_ENABLED
#define sys_log_debug(fmt, ...) \
	fprintf(stderr, "[DEBUG] " fmt "\n", ##__VA_ARGS__)
#else
#define sys_log_debug sys_log_noop
#endif

#if SYS_LOG_DEBUG_ENABLED
#define sys_log_pdebug(err, fmt, ...) \
	fprintf(stderr, "[DEBUG] " fmt ": %s (%d)\n", ##__VA_ARGS__, \
		strerror(err), (err))
#else
#define sys_log_pdebug sys_log_pnoop
#endif

#if SYS_LOG_TRACE_ENABLED
#define sys_log_trace(fmt, ...) \
	fprintf(stderr, "[TRACE] " fmt "\n", ##__VA_ARGS__)
#else
#define sys_log_trace sys_log_noop
#endif

#if SYS_LOG_TRACE_ENABLED
#define sys_log_ptrace(err, fmt, ...) \
	fprintf(stderr, "[TRACE] " fmt ": %s (%d)\n", ##__VA_ARGS__, \
		strerror(err), (err))
#else
#define sys_log_ptrace sys_log_pnoop
#endif

#define SYS_TRUE 1
#define SYS_FALSE 0
#define sys_bool u8

#define sys_min(a, b) ((a) < (b) ? (a) : (b))
#define sys_max(a, b) ((a) > (b) ? (a) : (b))

static inline int _sys_malloc(size_t size, void **out_ptr)
{
	int err;
	return (*out_ptr = malloc(size)) ? 0 : ((err = errno) ? err : ENOMEM);
}

#define sys_malloc(size, out_ptr) \
	_sys_malloc((size), (void**) (out_ptr))

static inline int _sys_calloc(size_t size, void **out_ptr)
{
	int err;
	return (*out_ptr = calloc(1, size)) ? 0 : ((err = errno) ? err : ENOMEM);
}

#define sys_calloc(size, out_ptr) \
	_sys_calloc((size), (void**) (out_ptr))

static inline int _sys_realloc(void *cur_ptr, size_t old_size, size_t size,
		void **out_ptr)
{
	int err;

	(void) old_size;

	return (*out_ptr = realloc(cur_ptr, size)) ? 0 :
		((err = errno) ? err : ENOMEM);
}

#define sys_realloc(cur_ptr, old_size, size, out_ptr) \
	_sys_realloc((cur_ptr), (old_size), (size), (void**) (out_ptr))

static inline void _sys_free(size_t size, void **out_ptr)
{
	(void) size;

	free(*out_ptr);
	*out_ptr = NULL;
}

#define sys_free(size, out_ptr) \
	_sys_free((size), (void**) (out_ptr))

#ifdef HAVE_STRNDUP
static inline void sys_strndup(const char *str, size_t len, char **dupstr)
{
	int err = 0;

	if(!(*dupstr = strndup(str, len))) {
		err = (err = errno) ? err : ENOMEM;
	}

	return err;
}
#else
int sys_strndup(const char *str, size_t len, char **dupstr);
#endif /* defined(HAVE_STRNDUP) ... */

static inline int sys_mutex_init(
		sys_mutex *const mutex)
{
	int err = 0;

#ifdef _WIN32
	*mutex = CreateMutex(
		/* LPSECURITY_ATTRIBUTES lpMutexAttributes */
		NULL,
		/* BOOL bInitialOwner */
		FALSE,
		/* LPCTSTR lpName */
		NULL);
	if(!*mutex) {
		err = ENOMEM;
	}
#elif defined(HAVE_PTHREAD_H)
	pthread_mutexattr_t mutexattr;

	err = pthread_mutexattr_init(&mutexattr);
	if(err) {
		goto out;
	}

	err = pthread_mutex_init(mutex, &mutexattr);
#endif /* defined(_WIN32) ... defined(HAVE_PTHREAD_H) */
	if(err) {
		goto out;
	}

	sys_log_debug("Initialized mutex %p.", mutex);
out:
#if !defined(_WIN32) && defined(HAVE_PTHREAD_H)
	pthread_mutexattr_destroy(&mutexattr);
#endif /* !defined(_WIN32) && defined(HAVE_PTHREAD_H) */

	return err;
}

static inline int sys_mutex_deinit(
		sys_mutex *const mutex)
{
	int err = 0;

#ifdef _WIN32
	if(!CloseHandle(
		/* HANDLE hObject */
		*mutex))
	{
		err = EINVAL;
	}
#elif defined(HAVE_PTHREAD_H)
	err = pthread_mutex_destroy(mutex);
#endif /* defined(_WIN32) ... defined(HAVE_PTHREAD_H) */
	if(!err) {
		sys_log_debug("Deinitialized mutex %p.", mutex);
	}

	return err;
}

static inline int sys_mutex_lock(
		sys_mutex *const mutex)
{
	int err = 0;

#ifdef _WIN32
	if(WaitForSingleObject(
		/* HANDLE hHandle */
		*mutex,
		/* DWORD  dwMilliseconds */
		INFINITE) != WAIT_OBJECT_0)
	{
		err = EINVAL;
	}
#elif defined(HAVE_PTHREAD_H)
	err = pthread_mutex_lock(mutex);
#endif /* defined(_WIN32) ... defined(HAVE_PTHREAD_H) */
	if(!err) {
		sys_log_debug("Locked mutex %p.", mutex);
	}

	return err;
}

static inline int sys_mutex_unlock(
		sys_mutex *const mutex)
{
	int err = 0;

	sys_log_debug("Unlocking mutex %p...", mutex);

#ifdef _WIN32
	if(!ReleaseMutex(
		/* HANDLE hMutex */
		*mutex))
	{
		err = EINVAL;
	}
#elif defined(HAVE_PTHREAD_H)
	err = pthread_mutex_unlock(mutex);
#endif /* defined(_WIN32) ... defined(HAVE_PTHREAD_H) */

	return err;
}

#ifndef _WIN32
#define PRIdz "zd"
#define PRIuz "zu"
#define PRIXz "zX"
#elif defined(_WIN64)
#define PRIdz PRId64
#define PRIuz PRIu64
#define PRIXz PRIX64
#else
#define PRIdz PRId32
#define PRIuz PRIu32
#define PRIXz PRIX32
#endif
#define PRIbs ".*s"

#define PRAoz(arg) ((size_t) (arg))
#define PRAdz(arg) ((ssize_t) (arg))
#define PRAuz(arg) ((size_t) (arg))
#define PRAxz(arg) ((size_t) (arg))
#define PRAXz(arg) ((size_t) (arg))
#define PRAbs(precision, arg) ((int) (precision)), ((const char*) (arg))
#define PRAo8(arg) ((uint8_t) (arg))
#define PRAd8(arg) ((int8_t) (arg))
#define PRAu8(arg) ((uint8_t) (arg))
#define PRAx8(arg) ((uint8_t) (arg))
#define PRAX8(arg) ((uint8_t) (arg))
#define PRAo16(arg) ((uint16_t) (arg))
#define PRAd16(arg) ((int16_t) (arg))
#define PRAu16(arg) ((uint16_t) (arg))
#define PRAx16(arg) ((uint16_t) (arg))
#define PRAX16(arg) ((uint16_t) (arg))
#define PRAo32(arg) ((uint32_t) (arg))
#define PRAd32(arg) ((int32_t) (arg))
#define PRAu32(arg) ((uint32_t) (arg))
#define PRAx32(arg) ((uint32_t) (arg))
#define PRAX32(arg) ((uint32_t) (arg))
#define PRAo64(arg) ((uint64_t) (arg))
#define PRAd64(arg) ((int64_t) (arg))
#define PRAu64(arg) ((uint64_t) (arg))
#define PRAx64(arg) ((uint64_t) (arg))
#define PRAX64(arg) ((uint64_t) (arg))

#define PRI0PAD(precision) "0" #precision
#define PRIPAD(precision) #precision

int sys_unistr_decode(const refschar *ins, size_t ins_len,
		char **outs, size_t *outs_len);

int sys_unistr_encode(const char *ins, size_t ins_len,
		refschar **outs, size_t *outs_len);

/*
 * Opaque device handle. Previously this typedef encoded a POSIX file
 * descriptor cast to `void *` (the inline `sys_device_*` helpers cast it
 * back to an `int` to call `pread` / `ioctl`). The handle is now an
 * implementation-defined pointer allocated by `sys_device_open` /
 * `sys_device_open_callbacks`; callers MUST treat it as opaque and only
 * touch it via the `sys_device_*` API.
 */
typedef void sys_device;

/**
 * Callback dispatch table for `sys_device_open_callbacks`. Lets external
 * code drive the library's I/O without going through a real file
 * descriptor — typical use is a Rust / Python wrapper that already owns
 * the image bytes through a mmap, an EWF reader, a BitLocker decryptor,
 * etc., and wants `librefs` to call back into its `read` routine instead
 * of opening a file path locally.
 *
 * `user_data` is opaque to refsprogs and forwarded to every callback.
 * `pread` is mandatory and must return 0 on success or a non-0 `errno`
 * value on failure. `get_size` is optional (NULL → `sys_device_get_size`
 * returns ENOTSUP). `close` is optional and is invoked exactly once from
 * `sys_device_close` so the consumer can release any state attached to
 * `user_data`.
 */
typedef struct {
	void *user_data;
	int (*pread)(void *user_data, u64 offset, size_t size, void *buf);
	int (*get_size)(void *user_data, u64 *out_size);
	void (*close)(void *user_data);
} sys_device_callbacks;

int sys_device_open(sys_device **dev, const char *path);
int sys_device_open_callbacks(sys_device **dev,
		const sys_device_callbacks *cb);
int sys_device_close(sys_device **dev);
int sys_device_pread(sys_device *dev, u64 offset, size_t nbytes, void *buf);

int sys_device_get_sector_size(sys_device *dev, u32 *out_sector_size);
int sys_device_get_size(sys_device *dev, u64 *out_size);

#endif /* !defined(_REFS_SYS_USER_H) */
