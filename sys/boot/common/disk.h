/*-
 * Copyright (c) 2011 Google, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * Device descriptor for partitioned disks. We assume that all disk addresses
 * are 512 byte block offsets from the start of the disk. To use, set the
 * d_slice and d_partition variables as follows:
 *
 * Whole disk access:
 *
 * 	d_slice = -1
 * 	d_partition = -1
 *
 * Whole MBR slice:
 *
 * 	d_slice = MBR slice number (typically 1..4)
 * 	d_partition = -1
 *
 * BSD disklabel partition within an MBR slice:
 *
 * 	d_slice = MBR slice number (typically 1..4)
 * 	d_partition = disklabel partition (typically 0..7)
 *
 * GPT partition:
 *
 * 	d_slice = GPT partition number (typically 1..N)
 * 	d_partition = 255
 *
 * For both MBR and GPT, to automatically find the 'best' slice or partition,
 * set d_slice to zero. This uses the partition type to decide which partition
 * to use according to the following list of preferences:
 *
 * 	FreeBSD (active)
 * 	FreeBSD (inactive)
 * 	Linux (active)
 * 	Linux (inactive)
 * 	DOS/Windows (active)
 * 	DOS/Windows (inactive)
 *
 * Active MBR slices (marked as bootable) are preferred over inactive. GPT
 * doesn't have the concept of active/inactive partitions. In both MBR and GPT,
 * if there are multiple slices/partitions of a given type, the first one
 * is chosen.
 *
 * The low-level disk device will typically call slice_open() from its open
 * method to interpret the disk partition tables according to the rules above.
 * This will initialize d_offset to the block offset of the start of the
 * selected partition - this offset should be added to the offset passed to
 * the device's strategy method.
 */

#define DISK_SECSIZE	512

struct disk_devdesc
{
	struct devsw	*d_dev;
	int		d_type;
	int		d_unit;
	void		*d_opendata;
	int		d_slice;
	int		d_partition;
	int		d_offset;
};

/*
 * Parse disk metadata and initialise dev->d_offset.
 */
extern int disk_open(struct disk_devdesc * dev);

/*
 * Print information about slices on a disk.  For the size calculations we
 * assume a 512 byte sector.
 */
extern void disk_print(struct disk_devdesc *dev, char *prefix, int verbose);
