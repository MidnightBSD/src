/*-
 *  COPYRIGHT (C) 1986 Gary S. Brown.  You may use this program, or
 *  code or tables extracted from it, as desired without restriction.
 *
 * $FreeBSD: release/10.0.0/sys/boot/common/crc32.h 213136 2010-09-24 19:49:12Z pjd $
 */

#ifndef _CRC32_H_
#define	_CRC32_H_

uint32_t crc32(const void *buf, size_t size);

#endif	/* !_CRC32_H_ */
