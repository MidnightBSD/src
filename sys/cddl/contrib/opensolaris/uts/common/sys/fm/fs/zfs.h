/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef	_SYS_FM_FS_ZFS_H
#define	_SYS_FM_FS_ZFS_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	ZFS_ERROR_CLASS				"fs.zfs"

#define	FM_EREPORT_ZFS_CHECKSUM			"checksum"
#define	FM_EREPORT_ZFS_IO			"io"
#define	FM_EREPORT_ZFS_DATA			"data"
#define	FM_EREPORT_ZFS_POOL			"zpool"
#define	FM_EREPORT_ZFS_DEVICE_UNKNOWN		"vdev.unknown"
#define	FM_EREPORT_ZFS_DEVICE_OPEN_FAILED	"vdev.open_failed"
#define	FM_EREPORT_ZFS_DEVICE_CORRUPT_DATA	"vdev.corrupt_data"
#define	FM_EREPORT_ZFS_DEVICE_NO_REPLICAS	"vdev.no_replicas"
#define	FM_EREPORT_ZFS_DEVICE_BAD_GUID_SUM	"vdev.bad_guid_sum"
#define	FM_EREPORT_ZFS_DEVICE_TOO_SMALL		"vdev.too_small"
#define	FM_EREPORT_ZFS_DEVICE_BAD_LABEL		"vdev.bad_label"

#define	FM_EREPORT_PAYLOAD_ZFS_POOL		"pool"
#define	FM_EREPORT_PAYLOAD_ZFS_POOL_GUID	"pool_guid"
#define	FM_EREPORT_PAYLOAD_ZFS_POOL_CONTEXT	"pool_context"
#define	FM_EREPORT_PAYLOAD_ZFS_VDEV_GUID	"vdev_guid"
#define	FM_EREPORT_PAYLOAD_ZFS_VDEV_TYPE	"vdev_type"
#define	FM_EREPORT_PAYLOAD_ZFS_VDEV_PATH	"vdev_path"
#define	FM_EREPORT_PAYLOAD_ZFS_VDEV_DEVID	"vdev_devid"
#define	FM_EREPORT_PAYLOAD_ZFS_PARENT_GUID	"parent_guid"
#define	FM_EREPORT_PAYLOAD_ZFS_PARENT_TYPE	"parent_type"
#define	FM_EREPORT_PAYLOAD_ZFS_PARENT_PATH	"parent_path"
#define	FM_EREPORT_PAYLOAD_ZFS_PARENT_DEVID	"parent_devid"
#define	FM_EREPORT_PAYLOAD_ZFS_ZIO_OBJSET	"zio_objset"
#define	FM_EREPORT_PAYLOAD_ZFS_ZIO_OBJECT	"zio_object"
#define	FM_EREPORT_PAYLOAD_ZFS_ZIO_LEVEL	"zio_level"
#define	FM_EREPORT_PAYLOAD_ZFS_ZIO_BLKID	"zio_blkid"
#define	FM_EREPORT_PAYLOAD_ZFS_ZIO_ERR		"zio_err"
#define	FM_EREPORT_PAYLOAD_ZFS_ZIO_OFFSET	"zio_offset"
#define	FM_EREPORT_PAYLOAD_ZFS_ZIO_SIZE		"zio_size"
#define	FM_EREPORT_PAYLOAD_ZFS_PREV_STATE	"prev_state"

#define	FM_RESOURCE_OK				"ok"

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FM_FS_ZFS_H */
