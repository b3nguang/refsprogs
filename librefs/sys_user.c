/*-
 * sys_user.c - System abstractions implementation (userspace).
 *
 * Real (non-inline) implementations of the `sys_device_*` API. These used
 * to live as `static inline` definitions in `include/refs/sys_user.h`;
 * pulling them into a single translation unit lets `librefs.so` /
 * `librefs-13.dll` export them as proper symbols so out-of-tree consumers
 * (Rust bindings via `libloading`, language wrappers, ...) can drive the
 * library without re-implementing the platform glue.
 *
 * The `sys_device` ABI used to be "POSIX file descriptor cast to `void *`".
 * It is now an opaque pointer to an internal struct allocated by
 * `sys_device_open` / `sys_device_open_callbacks`. Every internal caller
 * inside librefs reached `sys_device` only through `sys_device_pread` and
 * `sys_device_get_*`, so no other source file needs to change. External
 * code that linked against the previous (inlined) ABI just rebuilds against
 * the new declarations and starts dynamically linking the exported symbols.
 *
 * Copyright (c) 2022-2026 Erik Larsson
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "sys.h"

#ifdef _WIN32
#include <io.h>
#endif

typedef enum {
	SYS_DEVICE_KIND_FD = 1,
	SYS_DEVICE_KIND_CALLBACKS = 2
} sys_device_kind;

struct sys_device_impl {
	sys_device_kind kind;
	union {
		int fd;
		sys_device_callbacks cb;
	} u;
};

#ifdef _WIN32
/* Win32 has no `pread`; emulate via seek + read. Single-threaded per
 * volume (librefs holds no internal locks), so the seek/read pair is
 * safe for the library's call sites. */
static ssize_t pread_emul(int fd, void *buf, size_t nbyte, off_t offset)
{
	errno = 0;

	if(lseek(fd, offset, SEEK_SET) != offset ||
		read(fd, buf, nbyte) != (ssize_t) nbyte)
	{
		return -1;
	}

	return (ssize_t) nbyte;
}
#endif /* defined(_WIN32) */

int sys_device_open(sys_device **const dev, const char *const path)
{
	struct sys_device_impl *impl;
	int fd;

	if(!dev || !path) {
		return EINVAL;
	}

	impl = (struct sys_device_impl*) calloc(1, sizeof(*impl));
	if(!impl) {
		return ENOMEM;
	}

	fd = open(
		path,
#ifdef O_BINARY
		O_BINARY |
#endif
		O_RDONLY);
	if(fd == -1) {
		int err = errno ? errno : EIO;
		free(impl);
		return err;
	}

	impl->kind = SYS_DEVICE_KIND_FD;
	impl->u.fd = fd;
	*dev = (sys_device*) impl;
	return 0;
}

int sys_device_open_callbacks(sys_device **const dev,
		const sys_device_callbacks *const cb)
{
	struct sys_device_impl *impl;

	if(!dev || !cb || !cb->pread) {
		return EINVAL;
	}

	impl = (struct sys_device_impl*) calloc(1, sizeof(*impl));
	if(!impl) {
		return ENOMEM;
	}

	impl->kind = SYS_DEVICE_KIND_CALLBACKS;
	impl->u.cb = *cb;
	*dev = (sys_device*) impl;
	return 0;
}

int sys_device_close(sys_device **const dev)
{
	struct sys_device_impl *impl;
	int err = 0;

	if(!dev || !*dev) {
		return 0;
	}

	impl = (struct sys_device_impl*) *dev;
	switch(impl->kind) {
	case SYS_DEVICE_KIND_FD:
		if(impl->u.fd >= 0 && close(impl->u.fd)) {
			err = errno ? errno : EIO;
		}
		break;
	case SYS_DEVICE_KIND_CALLBACKS:
		if(impl->u.cb.close) {
			impl->u.cb.close(impl->u.cb.user_data);
		}
		break;
	}

	free(impl);
	*dev = NULL;
	return err;
}

int sys_device_pread(sys_device *const dev, const u64 offset,
		const size_t nbytes, void *const buf)
{
	struct sys_device_impl *impl;
	ssize_t res;
	int err = 0;

	sys_log_debug("pread: offset=%" PRIu64 " nbytes=%" PRIuz,
		PRAu64(offset), PRAuz(nbytes));

	if(!dev) {
		return EINVAL;
	}
	if(nbytes && !buf) {
		return EINVAL;
	}
	if(offset > INT64_MAX || nbytes > SSIZE_MAX) {
		return EINVAL;
	}

	impl = (struct sys_device_impl*) dev;
	switch(impl->kind) {
	case SYS_DEVICE_KIND_FD:
#ifdef _WIN32
		res = pread_emul(impl->u.fd, buf, nbytes, (off_t) offset);
#else
		res = pread(impl->u.fd, buf, nbytes, (off_t) offset);
#endif
		if(res < 0) {
			err = errno ? errno : EIO;
		}
		else if((size_t) res != nbytes) {
			err = EIO;
		}
		return err;
	case SYS_DEVICE_KIND_CALLBACKS:
		return impl->u.cb.pread(
			impl->u.cb.user_data, offset, nbytes, buf);
	}

	return EINVAL;
}

int sys_device_get_sector_size(sys_device *const dev,
		u32 *const out_sector_size)
{
	struct sys_device_impl *impl;
	int err = ENOTSUP;

	if(!dev || !out_sector_size) {
		return EINVAL;
	}

	impl = (struct sys_device_impl*) dev;
	if(impl->kind != SYS_DEVICE_KIND_FD) {
		/* Callback devices don't advertise a real sector size — the
		 * library tolerates ENOTSUP here and falls back to the size
		 * declared in the boot sector. */
		return ENOTSUP;
	}

#ifdef __linux__
	{
		int sector_size = 0;

		if(ioctl(impl->u.fd, BLKSSZGET, &sector_size)) {
			err = (err = errno) ? err : EIO;
		}
		else {
			err = 0;
			*out_sector_size = sector_size;
		}
	}
#endif

#ifdef __APPLE__
	{
		uint32_t block_size = 0;

		if(ioctl(impl->u.fd, DKIOCGETBLOCKSIZE, &block_size)) {
			err = (err = errno) ? err : EIO;
		}
		else {
			err = 0;
			*out_sector_size = block_size;
		}
	}
#endif

#if defined(__FreeBSD__) || defined(__DragonFly__) || defined(__NetBSD__)
	{
		size_t sector_size = 0;

		if(ioctl(impl->u.fd, DIOCGSECTORSIZE, &sector_size)) {
			err = (err = errno) ? err : EIO;
		}
		else {
			*out_sector_size = (u32) sector_size;
			err = 0;
		}
	}
#endif

#ifdef __OpenBSD__
	{
		struct disklabel dl;

		memset(&dl, 0, sizeof(dl));

		if(ioctl(impl->u.fd, DIOCGDINFO, &dl)) {
			err = (err = errno) ? err : EIO;
		}
		else {
			*out_sector_size = (u32) dl.d_secsize;
			err = 0;
		}
	}
#endif

#if defined(sun) || defined(__sun)
	{
		struct dk_minfo_ext minfo_ext;

		if(ioctl(impl->u.fd, DKIOCGMEDIAINFOEXT, &minfo_ext) == -1) {
			struct dk_minfo minfo;

			if(ioctl(impl->u.fd, DKIOCGMEDIAINFO, &minfo) == -1) {
				err = (err = errno) ? err : EIO;
			}
			else {
				*out_sector_size = (u32) minfo.dki_lbsize;
				err = 0;
			}
		}
		else {
			*out_sector_size = (u32) minfo_ext.dki_lbsize;
			err = 0;
		}
	}
#endif

#ifdef _WIN32
	{
		BYTE buf[sizeof(DISK_GEOMETRY) + sizeof(DISK_PARTITION_INFO) +
			sizeof(DISK_DETECTION_INFO) + 512];
		DWORD bytes_returned = 0;

		if(DeviceIoControl(
			/* _In_        HANDLE       hDevice */
			(HANDLE) _get_osfhandle(impl->u.fd),
			/* _In_        DWORD        dwIoControlCode */
			IOCTL_DISK_GET_DRIVE_GEOMETRY_EX,
			/* _In_opt_    LPVOID       lpInBuffer */
			NULL,
			/* _In_        DWORD        nInBufferSize */
			0,
			/* _Out_opt_   LPVOID       lpOutBuffer */
			buf,
			/* _In_        DWORD        nOutBufferSize */
			sizeof(buf),
			/* _Out_opt_   LPDWORD      lpBytesReturned */
			&bytes_returned,
			/* _Inout_opt_ LPOVERLAPPED lpOverlapped */
			NULL))
		{
			const DISK_GEOMETRY_EX *const geom =
				(const DISK_GEOMETRY_EX*) buf;

			if(offsetof(DISK_GEOMETRY_EX, Geometry.BytesPerSector) +
				sizeof(DWORD) > bytes_returned)
			{
				err = EIO;
			}
			else {
				*out_sector_size = geom->Geometry.BytesPerSector;
				err = 0;
			}
		}
		else {
			err = EIO;
		}

		if(err);
		else if(DeviceIoControl(
			/* _In_        HANDLE       hDevice */
			(HANDLE) _get_osfhandle(impl->u.fd),
			/* _In_        DWORD        dwIoControlCode */
			IOCTL_DISK_GET_DRIVE_GEOMETRY,
			/* _In_opt_    LPVOID       lpInBuffer */
			NULL,
			/* _In_        DWORD        nInBufferSize */
			0,
			/* _Out_opt_   LPVOID       lpOutBuffer */
			buf,
			/* _In_        DWORD        nOutBufferSize */
			sizeof(buf),
			/* _Out_opt_   LPDWORD      lpBytesReturned */
			&bytes_returned,
			/* _Inout_opt_ LPOVERLAPPED lpOverlapped */
			NULL))
		{
			const DISK_GEOMETRY *const geom =
				(const DISK_GEOMETRY*) buf;

			if(offsetof(DISK_GEOMETRY, BytesPerSector) +
				sizeof(DWORD) > bytes_returned)
			{
				err = EIO;
			}
			else {
				*out_sector_size = geom->BytesPerSector;
				err = 0;
			}
		}
		else {
			err = EIO;
		}
	}
#endif

	return err;
}

int sys_device_get_size(sys_device *const dev, u64 *const out_size)
{
	struct sys_device_impl *impl;
	int err = ENOTSUP;
	struct stat st;

	if(!dev || !out_size) {
		return EINVAL;
	}

	impl = (struct sys_device_impl*) dev;
	if(impl->kind == SYS_DEVICE_KIND_CALLBACKS) {
		if(impl->u.cb.get_size) {
			return impl->u.cb.get_size(
				impl->u.cb.user_data, out_size);
		}
		return ENOTSUP;
	}

	if(!fstat(impl->u.fd, &st) &&
		(st.st_mode & S_IFMT) == S_IFREG)
	{
		/* Regular file, size can be obtained through stat. */
		*out_size = st.st_size;
		return 0;
	}

#ifdef __linux__
	{
		uint64_t device_size = 0;

		if(ioctl(impl->u.fd, BLKGETSIZE64, &device_size)) {
			err = (err = errno) ? err : EIO;
		}
		else {
			err = 0;
			*out_size = device_size;
		}
	}
#endif

#ifdef __APPLE__
	{
		uint32_t block_size = 0;

		if(ioctl(impl->u.fd, DKIOCGETBLOCKSIZE, &block_size)) {
			err = (err = errno) ? err : EIO;
		}
		else {
			uint64_t block_count = 0;

			if(ioctl(impl->u.fd, DKIOCGETBLOCKCOUNT, &block_count)) {
				err = (err = errno) ? err : EIO;
			}
			else {
				err = 0;
				*out_size = block_size * block_count;
			}
		}
	}
#endif

#if defined(__FreeBSD__) || defined(__DragonFly__) || defined(__NetBSD__)
	{
		size_t media_size = 0;

		if(ioctl(impl->u.fd, DIOCGMEDIASIZE, &media_size)) {
			err = (err = errno) ? err : EIO;
		}
		else {
			*out_size = media_size;
			err = 0;
		}
	}
#endif

#ifdef __OpenBSD__
	{
		struct stat stbuf;
		struct disklabel dl;

		memset(&dl, 0, sizeof(dl));

		if(fstat(impl->u.fd, &stbuf)) {
			err = (err = errno) ? err : EIO;
		}
		else if(!S_ISBLK(stbuf.st_mode) && !S_ISCHR(stbuf.st_mode)) {
			err = EINVAL;
		}
		else if(ioctl(impl->u.fd, DIOCGDINFO, &dl)) {
			err = (err = errno) ? err : EIO;
		}
		else {
			const struct partition *const part =
				&dl.d_partitions[DISKPART(stbuf.st_rdev)];

			*out_size =
				(u64) (DL_GETPSIZE(part)) * (u32) dl.d_secsize;
			err = 0;
		}
	}
#endif

#if defined(sun) || defined(__sun)
	{
		struct dk_minfo_ext minfo_ext;

		if(ioctl(impl->u.fd, DKIOCGMEDIAINFOEXT, &minfo_ext) == -1) {
			struct dk_minfo minfo;

			if(ioctl(impl->u.fd, DKIOCGMEDIAINFO, &minfo) == -1) {
				err = (err = errno) ? err : EIO;
			}
			else {
				*out_size = ((u64) minfo.dki_capacity) *
					(u32) minfo.dki_lbsize;
				err = 0;
			}
		}
		else {
			*out_size = ((u64) minfo_ext.dki_capacity) *
				(u32) minfo_ext.dki_lbsize;
			err = 0;
		}
	}
#endif

#ifdef _WIN32
	{
		GET_LENGTH_INFORMATION info;
		DWORD bytes_returned = 0;

		memset(&info, 0, sizeof(info));

		if(!DeviceIoControl(
			(HANDLE) _get_osfhandle(impl->u.fd),
			IOCTL_DISK_GET_LENGTH_INFO,
			NULL,
			0,
			&info,
			sizeof(info),
			&bytes_returned,
			NULL))
		{
			err = EIO;
		}
		else if(bytes_returned != sizeof(info)) {
			err = EIO;
		}
		else {
			err = 0;
			*out_size = (u64) info.Length.QuadPart;
		}
	}
#endif

	return err;
}
