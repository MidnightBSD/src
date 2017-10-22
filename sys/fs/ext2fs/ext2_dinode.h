/*-
 * Copyright (c) 2009 Aditya Sarawgi
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

#ifndef _FS_EXT2FS_EXT2_DINODE_H_
#define _FS_EXT2FS_EXT2_DINODE_H_

#define e2di_size_high	e2di_dacl

/*
 * Special inode numbers
 * The root inode is the root of the file system.  Inode 0 can't be used for
 * normal purposes and bad blocks are normally linked to inode 1, thus
 * the root inode is 2.
 * Inode 3 to 10 are reserved in ext2fs.
 */
#define	EXT2_BADBLKINO		((ino_t)1)
#define	EXT2_ROOTINO		((ino_t)2)
#define	EXT2_ACLIDXINO		((ino_t)3)
#define	EXT2_ACLDATAINO		((ino_t)4)
#define	EXT2_BOOTLOADERINO	((ino_t)5)
#define	EXT2_UNDELDIRINO	((ino_t)6)
#define	EXT2_RESIZEINO		((ino_t)7)
#define	EXT2_JOURNALINO		((ino_t)8)
#define	EXT2_FIRSTINO		((ino_t)11)

/*
 * Inode flags
 * The current implementation uses only EXT2_IMMUTABLE and EXT2_APPEND flags
 */
#define EXT2_SECRM		0x00000001	/* Secure deletion */
#define EXT2_UNRM		0x00000002	/* Undelete */
#define EXT2_COMPR		0x00000004	/* Compress file */
#define EXT2_SYNC		0x00000008	/* Synchronous updates */
#define EXT2_IMMUTABLE		0x00000010	/* Immutable file */
#define EXT2_APPEND		0x00000020	/* writes to file may only append */
#define EXT2_NODUMP		0x00000040	/* do not dump file */
#define EXT2_NOATIME		0x00000080	/* do not update atime */

/*
 * Definitions for nanosecond timestamps.
 * Ext3 inode versioning, 2006-12-13.
 */
#define EXT3_EPOCH_BITS	2
#define EXT3_EPOCH_MASK	((1 << EXT3_EPOCH_BITS) - 1)
#define EXT3_NSEC_MASK	(~0UL << EXT3_EPOCH_BITS)

#define E2DI_HAS_XTIME(ip)	(EXT2_INODE_SIZE((ip)->i_e2fs) > \
				    E2FS_REV0_INODE_SIZE)

/*
 * Structure of an inode on the disk
 */
struct ext2fs_dinode {
	uint16_t	e2di_mode;	/*   0: IFMT, permissions; see below. */
	uint16_t	e2di_uid;	/*   2: Owner UID */
	uint32_t	e2di_size;	/*	 4: Size (in bytes) */
	uint32_t	e2di_atime;	/*	 8: Access time */
	uint32_t	e2di_ctime;	/*	12: Change time */
	uint32_t	e2di_mtime;	/*	16: Modification time */
	uint32_t	e2di_dtime;	/*	20: Deletion time */
	uint16_t	e2di_gid;	/*  24: Owner GID */
	uint16_t	e2di_nlink;	/*  26: File link count */
	uint32_t	e2di_nblock;	/*  28: Blocks count */
	uint32_t	e2di_flags;	/*  32: Status flags (chflags) */
	uint32_t	e2di_version;	/*  36: Low 32 bits inode version */
	uint32_t	e2di_blocks[EXT2_N_BLOCKS]; /* 40: disk blocks */
	uint32_t	e2di_gen;	/* 100: generation number */
	uint32_t	e2di_facl;	/* 104: file ACL (not implemented) */
	uint32_t	e2di_dacl;	/* 108: dir ACL (not implemented) */
	uint32_t	e2di_faddr;	/* 112: fragment address */
	uint8_t		e2di_nfrag;	/* 116: fragment number */
	uint8_t		e2di_fsize;	/* 117: fragment size */
	uint16_t	e2di_linux_reserved2; /* 118 */
	uint16_t	e2di_uid_high;	/* 120: Owner UID top 16 bits */
	uint16_t	e2di_gid_high;	/* 122: Owner GID top 16 bits */
	uint32_t	e2di_linux_reserved3; /* 124 */
	uint16_t	e2di_extra_isize;
	uint16_t	e2di_pad1;
	uint32_t        e2di_ctime_extra; /* Extra change time */
	uint32_t        e2di_mtime_extra; /* Extra modification time */
	uint32_t        e2di_atime_extra; /* Extra access time */
	uint32_t        e2di_crtime;	  /* Creation (birth)time */
	uint32_t        e2di_crtime_extra; /* Extra creation (birth)time */
	uint32_t        e2di_version_hi;  /* High 30 bits of inode version */
};

#endif /* !_FS_EXT2FS_EXT2_DINODE_H_ */

