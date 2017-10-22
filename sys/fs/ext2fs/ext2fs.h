/*-
 *  modified for EXT2FS support in Lites 1.1
 *
 *  Aug 1995, Godmar Back (gback@cs.utah.edu)
 *  University of Utah, Department of Computer Science
 *
 * $FreeBSD$
 */
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
 * 
 */

#ifndef _FS_EXT2FS_EXT2FS_H_
#define _FS_EXT2FS_EXT2FS_H_

#include <sys/types.h>

/*
 * Maximal count of links to a file
 */
#define EXT2_LINK_MAX		32000

/*
 * A summary of contiguous blocks of various sizes is maintained
 * in each cylinder group. Normally this is set by the initial
 * value of fs_maxcontig.
 *
 * XXX:FS_MAXCONTIG is set to 16 to conserve space. Here we set
 * EXT2_MAXCONTIG to 32 for better performance.
 */
#define EXT2_MAXCONTIG		32

/*
 * Constants relative to the data blocks
 */
#define	EXT2_NDIR_BLOCKS		12
#define	EXT2_IND_BLOCK			EXT2_NDIR_BLOCKS
#define	EXT2_DIND_BLOCK			(EXT2_IND_BLOCK + 1)
#define	EXT2_TIND_BLOCK			(EXT2_DIND_BLOCK + 1)
#define	EXT2_N_BLOCKS			(EXT2_TIND_BLOCK + 1)
#define EXT2_MAXSYMLINKLEN		(EXT2_N_BLOCKS * sizeof(uint32_t))

/*
 * The path name on which the file system is mounted is maintained
 * in fs_fsmnt. MAXMNTLEN defines the amount of space allocated in
 * the super block for this name.
 */
#define MAXMNTLEN 512

/*
 * Super block for an ext2fs file system.
 */
struct ext2fs {
	uint32_t  e2fs_icount;		/* Inode count */
	uint32_t  e2fs_bcount;		/* blocks count */
	uint32_t  e2fs_rbcount;		/* reserved blocks count */
	uint32_t  e2fs_fbcount;		/* free blocks count */
	uint32_t  e2fs_ficount;		/* free inodes count */
	uint32_t  e2fs_first_dblock;	/* first data block */
	uint32_t  e2fs_log_bsize;	/* block size = 1024*(2^e2fs_log_bsize) */
	uint32_t  e2fs_log_fsize;	/* fragment size */
	uint32_t  e2fs_bpg;		/* blocks per group */
	uint32_t  e2fs_fpg;		/* frags per group */
	uint32_t  e2fs_ipg;		/* inodes per group */
	uint32_t  e2fs_mtime;		/* mount time */
	uint32_t  e2fs_wtime;		/* write time */
	uint16_t  e2fs_mnt_count;	/* mount count */
	uint16_t  e2fs_max_mnt_count;	/* max mount count */
	uint16_t  e2fs_magic;		/* magic number */
	uint16_t  e2fs_state;		/* file system state */
	uint16_t  e2fs_beh;		/* behavior on errors */
	uint16_t  e2fs_minrev;		/* minor revision level */
	uint32_t  e2fs_lastfsck;	/* time of last fsck */
	uint32_t  e2fs_fsckintv;	/* max time between fscks */
	uint32_t  e2fs_creator;		/* creator OS */
	uint32_t  e2fs_rev;		/* revision level */
	uint16_t  e2fs_ruid;		/* default uid for reserved blocks */
	uint16_t  e2fs_rgid;		/* default gid for reserved blocks */
	/* EXT2_DYNAMIC_REV superblocks */
	uint32_t  e2fs_first_ino;	/* first non-reserved inode */
	uint16_t  e2fs_inode_size;	/* size of inode structure */
	uint16_t  e2fs_block_group_nr;	/* block grp number of this sblk*/
	uint32_t  e2fs_features_compat; /*  compatible feature set */
	uint32_t  e2fs_features_incompat; /* incompatible feature set */
	uint32_t  e2fs_features_rocompat; /* RO-compatible feature set */
	uint8_t	  e2fs_uuid[16];	/* 128-bit uuid for volume */
	char      e2fs_vname[16];	/* volume name */
	char      e2fs_fsmnt[64]; 	/* name mounted on */
	uint32_t  e2fs_algo;		/* For compression */
	uint8_t   e2fs_prealloc;	/* # of blocks for old prealloc */
	uint8_t   e2fs_dir_prealloc;	/* # of blocks for old prealloc dirs */
	uint16_t  e2fs_reserved_ngdb;	/* # of reserved gd blocks for resize */
	char      e3fs_journal_uuid[16]; /* uuid of journal superblock */
	uint32_t  e3fs_journal_inum;	/* inode number of journal file */
	uint32_t  e3fs_journal_dev;	/* device number of journal file */
	uint32_t  e3fs_last_orphan;	/* start of list of inodes to delete */
	uint32_t  e3fs_hash_seed[4];	/* HTREE hash seed */
	char      e3fs_def_hash_version; /* Default hash version to use */
	char      e3fs_reserved_char_pad;
	uint32_t  e3fs_default_mount_opts;
	uint32_t  e3fs_first_meta_bg;	/* First metablock block group */
	uint32_t  e3fs_mkfs_time;      /* when the fs was created */
	uint32_t  e3fs_jnl_blks[17];   /* backup of the journal inode */
	uint32_t  e4fs_bcount_hi;      /* block count */
	uint32_t  e4fs_rbcount_hi;     /* reserved blocks count */
	uint32_t  e4fs_fbcount_hi;     /* free blocks count */
	uint16_t  e4fs_min_extra_isize;/* all inodes have at least some bytes */
	uint16_t  e4fs_want_extra_isize; /* inodes must reserve some bytes */
	uint32_t  e4fs_flags;	  /* miscellaneous flags */
	uint16_t  e4fs_raid_stride;    /* RAID stride */
	uint16_t  e4fs_mmpintv;	/* number of seconds to wait in MMP checking */
	uint64_t  e4fs_mmpblk;	 /* block for multi-mount protection */
	uint32_t  e4fs_raid_stripe_wid;/* blocks on all data disks (N * stride) */
	uint8_t   e4fs_log_gpf;	/* FLEX_BG group size */ 
	uint8_t   e4fs_char_pad2;
	uint16_t  e4fs_pad;
	uint32_t  reserved2[162];	/* Padding to the end of the block */	
};


/*
 * In-Memory Superblock
 */

struct m_ext2fs {
	struct ext2fs * e2fs;
	char     e2fs_fsmnt[MAXMNTLEN];/* name mounted on */
	char     e2fs_ronly;      /* mounted read-only flag */
	char     e2fs_fmod;       /* super block modified flag */
	uint32_t e2fs_bsize;      /* Block size */
	uint32_t e2fs_bshift;     /* calc of logical block no */
	int32_t  e2fs_bmask;      /* calc of block offset */
	int32_t  e2fs_bpg;	  /* Number of blocks per group */
	int64_t  e2fs_qbmask;     /* = s_blocksize -1 */
	uint32_t e2fs_fsbtodb;    /* Shift to get disk block */
	uint32_t e2fs_ipg;	  /* Number of inodes per group */
	uint32_t e2fs_ipb;	  /* Number of inodes per block */
	uint32_t e2fs_itpg;       /* Number of inode table per group */
	uint32_t e2fs_fsize;      /* Size of fragments per block */
	uint32_t e2fs_fpb;	  /* Number of fragments per block */
	uint32_t e2fs_fpg;	  /* Number of fragments per group */
	uint32_t e2fs_dbpg;       /* Number of descriptor blocks per group */
	uint32_t e2fs_descpb;     /* Number of group descriptors per block */
	uint32_t e2fs_gdbcount;   /* Number of group descriptors */
	uint32_t e2fs_gcount;     /* Number of groups */
	uint32_t e2fs_first_inode;/* First inode on fs */
	int32_t  e2fs_isize;      /* Size of inode */
	uint32_t e2fs_mount_opt;
	uint32_t e2fs_blocksize_bits;
	uint32_t e2fs_total_dir;  /* Total number of directories */
	uint8_t	*e2fs_contigdirs; /* (u) # of contig. allocated dirs */
	char     e2fs_wasvalid;   /* valid at mount time */
	off_t    e2fs_maxfilesize;
	struct   ext2_gd *e2fs_gd; /* Group Descriptors */
	int32_t  e2fs_maxcontig;	/* max number of contiguous blks */
	int32_t  e2fs_contigsumsize;    /* size of cluster summary array */
	int32_t *e2fs_maxcluster;       /* max cluster in each cyl group */
	struct   csum *e2fs_clustersum; /* cluster summary in each cyl group */
};

/*
 * The second extended file system version
 */
#define E2FS_DATE		"95/08/09"
#define E2FS_VERSION		"0.5b"

/*
 * The second extended file system magic number
 */
#define E2FS_MAGIC		0xEF53

/*
 * Revision levels
 */
#define E2FS_REV0		0	/* The good old (original) format */
#define E2FS_REV1		1 	/* V2 format w/ dynamic inode sizes */

#define E2FS_CURRENT_REV	E2FS_REV0
#define E2FS_MAX_SUPP_REV	E2FS_REV1

#define E2FS_REV0_INODE_SIZE 128

/*
 * compatible/incompatible features
 */
#define EXT2F_COMPAT_PREALLOC		0x0001
#define EXT2F_COMPAT_HASJOURNAL		0x0004
#define EXT2F_COMPAT_RESIZE		0x0010

#define EXT2F_ROCOMPAT_SPARSESUPER	0x0001
#define EXT2F_ROCOMPAT_LARGEFILE	0x0002
#define EXT2F_ROCOMPAT_BTREE_DIR	0x0004
#define EXT4F_ROCOMPAT_EXTRA_ISIZE	0x0040

#define EXT2F_INCOMPAT_COMP		0x0001
#define EXT2F_INCOMPAT_FTYPE		0x0002

/*
 * Features supported in this implementation
 *
 * We support the following REV1 features:
 * - EXT2F_ROCOMPAT_SPARSESUPER
 * - EXT2F_ROCOMPAT_LARGEFILE
 * - EXT2F_INCOMPAT_FTYPE
 */
#define EXT2F_COMPAT_SUPP		0x0000
#define EXT2F_ROCOMPAT_SUPP		(EXT2F_ROCOMPAT_SPARSESUPER | \
					 EXT2F_ROCOMPAT_LARGEFILE | \
					 EXT4F_ROCOMPAT_EXTRA_ISIZE)
#define EXT2F_INCOMPAT_SUPP		EXT2F_INCOMPAT_FTYPE

/* Assume that user mode programs are passing in an ext2fs superblock, not
 * a kernel struct super_block.  This will allow us to call the feature-test
 * macros from user land. */
#define EXT2_SB(sb)	(sb)

/*
 * Feature set definitions
 */
#define EXT2_HAS_COMPAT_FEATURE(sb,mask)			\
	( EXT2_SB(sb)->e2fs->e2fs_features_compat & htole32(mask) )
#define EXT2_HAS_RO_COMPAT_FEATURE(sb,mask)			\
	( EXT2_SB(sb)->e2fs->e2fs_features_rocompat & htole32(mask) )
#define EXT2_HAS_INCOMPAT_FEATURE(sb,mask)			\
	( EXT2_SB(sb)->e2fs->e2fs_features_incompat & htole32(mask) )

/*
 * Definitions of behavior on errors
 */
#define E2FS_BEH_CONTINUE		1	/* continue operation */
#define E2FS_BEH_READONLY		2	/* remount fs read only */
#define E2FS_BEH_PANIC			3	/* cause panic */
#define E2FS_BEH_DEFAULT		E2FS_BEH_CONTINUE

/*
 * OS identification
 */
#define E2FS_OS_LINUX		0
#define E2FS_OS_HURD		1
#define E2FS_OS_MASIX		2
#define E2FS_OS_FREEBSD		3
#define E2FS_OS_LITES		4

/*
 * File clean flags
 */
#define	E2FS_ISCLEAN			0x0001	/* Unmounted cleanly */
#define	E2FS_ERRORS			0x0002	/* Errors detected */

/* ext2 file system block group descriptor */

struct ext2_gd {
	uint32_t ext2bgd_b_bitmap;	/* blocks bitmap block */
	uint32_t ext2bgd_i_bitmap;	/* inodes bitmap block */
	uint32_t ext2bgd_i_tables;	/* inodes table block  */
	uint16_t ext2bgd_nbfree;	/* number of free blocks */
	uint16_t ext2bgd_nifree;	/* number of free inodes */
	uint16_t ext2bgd_ndirs;	/* number of directories */
	uint16_t reserved;
	uint32_t reserved2[3];
};

/* cluster summary information */

struct csum {
	int8_t   cs_init; /* cluster summary has been initialized */
	int32_t *cs_sum;  /* cluster summary array */
};

/* EXT2FS metadatas are stored in little-endian byte order. These macros
 * helps reading these metadatas
 */

#define e2fs_cgload(old, new, size) memcpy((new), (old), (size));
#define e2fs_cgsave(old, new, size) memcpy((new), (old), (size));

/*
 * Macro-instructions used to manage several block sizes
 */
#define EXT2_MIN_BLOCK_SIZE		1024
#define	EXT2_MAX_BLOCK_SIZE		4096
#define EXT2_MIN_BLOCK_LOG_SIZE		  10
#if defined(_KERNEL)
# define EXT2_BLOCK_SIZE(s)		((s)->e2fs_bsize)
#else
# define EXT2_BLOCK_SIZE(s)		(EXT2_MIN_BLOCK_SIZE << (s)->e2fs_log_bsize)
#endif
#define	EXT2_ADDR_PER_BLOCK(s)		(EXT2_BLOCK_SIZE(s) / sizeof(uint32_t))
#if defined(_KERNEL)
# define EXT2_BLOCK_SIZE_BITS(s)	((s)->e2fs_blocksize_bits)
#else
# define EXT2_BLOCK_SIZE_BITS(s)	((s)->e2fs_log_bsize + 10)
#endif
#if defined(_KERNEL)
#define	EXT2_ADDR_PER_BLOCK_BITS(s)	(EXT2_SB(s)->s_addr_per_block_bits)
#define EXT2_INODE_SIZE(s)		(EXT2_SB(s)->e2fs_isize)
#define EXT2_FIRST_INO(s)		(EXT2_SB(s)->e2fs_first_inode)
#else
#define EXT2_INODE_SIZE(s)	(((s)->s_rev_level == E2FS_REV0) ? \
				 E2FS_REV0 : (s)->s_inode_size)
#define EXT2_FIRST_INO(s)	(((s)->s_rev_level == E2FS_REV0) ? \
				 E2FS_REV0 : (s)->e2fs_first_ino)
#endif

/*
 * Macro-instructions used to manage fragments
 */
#define EXT2_MIN_FRAG_SIZE		1024
#define	EXT2_MAX_FRAG_SIZE		4096
#define EXT2_MIN_FRAG_LOG_SIZE		  10
#if defined(_KERNEL)
# define EXT2_FRAG_SIZE(s)		(EXT2_SB(s)->e2fs_fsize)
# define EXT2_FRAGS_PER_BLOCK(s)	(EXT2_SB(s)->e2fs_fpb)
#else
# define EXT2_FRAG_SIZE(s)		(EXT2_MIN_FRAG_SIZE << (s)->e2fs_log_fsize)
# define EXT2_FRAGS_PER_BLOCK(s)	(EXT2_BLOCK_SIZE(s) / EXT2_FRAG_SIZE(s))
#endif

/*
 * Macro-instructions used to manage group descriptors
 */
#if defined(_KERNEL)
# define EXT2_BLOCKS_PER_GROUP(s)	(EXT2_SB(s)->e2fs_bpg)
# define EXT2_DESC_PER_BLOCK(s)		(EXT2_SB(s)->e2fs_descpb)
# define EXT2_DESC_PER_BLOCK_BITS(s)	(EXT2_SB(s)->s_desc_per_block_bits)
#else
# define EXT2_BLOCKS_PER_GROUP(s)	((s)->e2fs_bpg)
# define EXT2_DESC_PER_BLOCK(s)		(EXT2_BLOCK_SIZE(s) / sizeof(struct ext2_gd))

#endif

#endif	/* !_FS_EXT2FS_EXT2FS_H_ */
