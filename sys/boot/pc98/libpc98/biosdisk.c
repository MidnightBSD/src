/*-
 * Copyright (c) 1998 Michael Smith <msmith@freebsd.org>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * BIOS disk device handling.
 * 
 * Ideas and algorithms from:
 *
 * - NetBSD libi386/biosdisk.c
 * - FreeBSD biosboot/disk.c
 *
 */

#include <stand.h>

#include <sys/disklabel.h>
#include <sys/diskpc98.h>
#include <machine/bootinfo.h>

#include <stdarg.h>

#include <bootstrap.h>
#include <btxv86.h>
#include "libi386.h"

#define BIOS_NUMDRIVES		0x475
#define BIOSDISK_SECSIZE	512
#define BUFSIZE			(1 * BIOSDISK_SECSIZE)

#define DT_ATAPI		0x10		/* disk type for ATAPI floppies */
#define WDMAJOR			0		/* major numbers for devices we frontend for */
#define WFDMAJOR		1
#define FDMAJOR			2
#define DAMAJOR			4

#ifdef DISK_DEBUG
# define DEBUG(fmt, args...)	printf("%s: " fmt "\n" , __func__ , ## args)
#else
# define DEBUG(fmt, args...)
#endif

struct open_disk {
    int			od_dkunit;		/* disk unit number */
    int			od_unit;		/* BIOS unit number */
    int			od_cyl;			/* BIOS geometry */
    int			od_hds;
    int			od_sec;
    int			od_boff;		/* block offset from beginning of BIOS disk */
    int			od_flags;
#define BD_MODEINT13		0x0000
#define BD_MODEEDD1		0x0001
#define BD_MODEEDD3		0x0002
#define BD_MODEMASK		0x0003
#define BD_FLOPPY		0x0004
#define BD_LABELOK		0x0008
#define BD_PARTTABOK		0x0010
#define BD_OPTICAL		0x0020
    struct disklabel		od_disklabel;
    int				od_nslices;	/* slice count */
    struct pc98_partition	od_slicetab[NDOSPART];
};

/*
 * List of BIOS devices, translation from disk unit number to
 * BIOS unit number.
 */
static struct bdinfo
{
    int		bd_unit;		/* BIOS unit number */
    int		bd_flags;
    int		bd_type;		/* BIOS 'drive type' (floppy only) */
    int		bd_da_unit;		/* kernel unit number for da */
} bdinfo [MAXBDDEV];
static int nbdinfo = 0;

static int	bd_getgeom(struct open_disk *od);
static int	bd_read(struct open_disk *od, daddr_t dblk, int blks,
		    caddr_t dest);
static int	bd_write(struct open_disk *od, daddr_t dblk, int blks,
		    caddr_t dest);

static int	bd_int13probe(struct bdinfo *bd);

static void	bd_printslice(struct open_disk *od, struct pc98_partition *dp,
		    char *prefix, int verbose);
static void	bd_printbsdslice(struct open_disk *od, daddr_t offset,
		    char *prefix, int verbose);

static int	bd_init(void);
static int	bd_strategy(void *devdata, int flag, daddr_t dblk,
		    size_t size, char *buf, size_t *rsize);
static int	bd_realstrategy(void *devdata, int flag, daddr_t dblk,
		    size_t size, char *buf, size_t *rsize);
static int	bd_open(struct open_file *f, ...);
static int	bd_close(struct open_file *f);
static void	bd_print(int verbose);

struct devsw biosdisk = {
    "disk", 
    DEVT_DISK, 
    bd_init,
    bd_strategy, 
    bd_open, 
    bd_close, 
    noioctl,
    bd_print,
    NULL
};

static int	bd_opendisk(struct open_disk **odp, struct i386_devdesc *dev);
static void	bd_closedisk(struct open_disk *od);
static int	bd_open_pc98(struct open_disk *od, struct i386_devdesc *dev);
static int	bd_bestslice(struct open_disk *od);
static void	bd_checkextended(struct open_disk *od, int slicenum);

/*
 * Translate between BIOS device numbers and our private unit numbers.
 */
int
bd_bios2unit(int biosdev)
{
    int		i;
    
    DEBUG("looking for bios device 0x%x", biosdev);
    for (i = 0; i < nbdinfo; i++) {
	DEBUG("bd unit %d is BIOS device 0x%x", i, bdinfo[i].bd_unit);
	if (bdinfo[i].bd_unit == biosdev)
	    return(i);
    }
    return(-1);
}

int
bd_unit2bios(int unit)
{
    if ((unit >= 0) && (unit < nbdinfo))
	return(bdinfo[unit].bd_unit);
    return(-1);
}

/*    
 * Quiz the BIOS for disk devices, save a little info about them.
 */
static int
bd_init(void) 
{
    int		base, unit;
    int		da_drive=0, n=-0x10;

    /* sequence 0x90, 0x80, 0xa0 */
    for (base = 0x90; base <= 0xa0; base += n, n += 0x30) {
	for (unit = base; (nbdinfo < MAXBDDEV) || ((unit & 0x0f) < 4); unit++) {
	    bdinfo[nbdinfo].bd_unit = unit;
	    bdinfo[nbdinfo].bd_flags = (unit & 0xf0) == 0x90 ? BD_FLOPPY : 0;

	    if (!bd_int13probe(&bdinfo[nbdinfo])){
		if (((unit & 0xf0) == 0x90 && (unit & 0x0f) < 4) ||
		    ((unit & 0xf0) == 0xa0 && (unit & 0x0f) < 6))
		    continue;	/* Target IDs are not contiguous. */
		else
		    break;
	    }

	    if (bdinfo[nbdinfo].bd_flags & BD_FLOPPY){
		/* available 1.44MB access? */
		if (*(u_char *)PTOV(0xA15AE) & (1<<(unit & 0xf))) {
		    /* boot media 1.2MB FD? */
		    if ((*(u_char *)PTOV(0xA1584) & 0xf0) != 0x90)
		        bdinfo[nbdinfo].bd_unit = 0x30 + (unit & 0xf);
		}
	    }
	    else {
		if ((unit & 0xF0) == 0xA0)	/* SCSI HD or MO */
		    bdinfo[nbdinfo].bd_da_unit = da_drive++;
	    }
	    /* XXX we need "disk aliases" to make this simpler */
	    printf("BIOS drive %c: is disk%d\n", 
		   'A' + nbdinfo, nbdinfo);
	    nbdinfo++;
	}
    }
    return(0);
}

/*
 * Try to detect a device supported by the legacy int13 BIOS
 */
static int
bd_int13probe(struct bdinfo *bd)
{
    int addr;

    if (bd->bd_flags & BD_FLOPPY) {
	addr = 0xa155c;
    } else {
	if ((bd->bd_unit & 0xf0) == 0x80)
	    addr = 0xa155d;
	else
	    addr = 0xa1482;
    }
    if ( *(u_char *)PTOV(addr) & (1<<(bd->bd_unit & 0x0f))) {
	bd->bd_flags |= BD_MODEINT13;
	return(1);
    }
    if ((bd->bd_unit & 0xF0) == 0xA0) {
	int media = ((unsigned *)PTOV(0xA1460))[bd->bd_unit & 0x0F] & 0x1F;

	if (media == 7) { /* MO */
	    bd->bd_flags |= BD_MODEINT13 | BD_OPTICAL;
	    return(1);
	}
    }
    return(0);
}

/*
 * Print information about disks
 */
static void
bd_print(int verbose)
{
    int				i, j;
    char			line[80];
    struct i386_devdesc		dev;
    struct open_disk		*od;
    struct pc98_partition	*dptr;
    
    for (i = 0; i < nbdinfo; i++) {
	sprintf(line, "    disk%d:   BIOS drive %c:\n", i, 'A' + i);
	pager_output(line);

	/* try to open the whole disk */
	dev.d_unit = i;
	dev.d_kind.biosdisk.slice = -1;
	dev.d_kind.biosdisk.partition = -1;
	
	if (!bd_opendisk(&od, &dev)) {

	    /* Do we have a partition table? */
	    if (od->od_flags & BD_PARTTABOK) {
		dptr = &od->od_slicetab[0];

		/* Check for a "dedicated" disk */
		for (j = 0; j < od->od_nslices; j++) {
		    sprintf(line, "      disk%ds%d", i, j + 1);
		    bd_printslice(od, &dptr[j], line, verbose);
		}
	    }
	    bd_closedisk(od);
	}
    }
}

/* Given a size in 512 byte sectors, convert it to a human-readable number. */
static char *
display_size(uint64_t size)
{
    static char buf[80];
    char unit;

    size /= 2;
    unit = 'K';
    if (size >= 10485760000LL) {
	size /= 1073741824;
	unit = 'T';
    } else if (size >= 10240000) {
	size /= 1048576;
	unit = 'G';
    } else if (size >= 10000) {
	size /= 1024;
	unit = 'M';
    }
    sprintf(buf, "%.6ld%cB", (long)size, unit);
    return (buf);
}

/*
 * Print information about slices on a disk.  For the size calculations we
 * assume a 512 byte sector.
 */
static void
bd_printslice(struct open_disk *od, struct pc98_partition *dp, char *prefix,
	int verbose)
{
	int cylsecs, start, size;
	char stats[80];
	char line[80];

	cylsecs = od->od_hds * od->od_sec;
	start = dp->dp_scyl * cylsecs + dp->dp_shd * od->od_sec + dp->dp_ssect;
	size = (dp->dp_ecyl - dp->dp_scyl + 1) * cylsecs;

	if (verbose)
		sprintf(stats, " %s (%d - %d)", display_size(size),
		    start, start + size);
	else
		stats[0] = '\0';

	switch(dp->dp_mid & PC98_MID_MASK) {
	case PC98_MID_386BSD:
		bd_printbsdslice(od, start, prefix, verbose);
		return;
	case 0x00:				/* unused partition */
		return;
	case 0x01:
		sprintf(line, "%s: FAT-12%s\n", prefix, stats);
		break;
	case 0x11:
	case 0x20:
	case 0x21:
	case 0x22:
	case 0x23:
	case 0x24:
		sprintf(line, "%s: FAT-16%s\n", prefix, stats);
		break;
	default:
		sprintf(line, "%s: Unknown fs: 0x%x %s\n", prefix, dp->dp_mid,
		    stats);
	}
	pager_output(line);
}

/*
 * Print out each valid partition in the disklabel of a FreeBSD slice.
 * For size calculations, we assume a 512 byte sector size.
 */
static void
bd_printbsdslice(struct open_disk *od, daddr_t offset, char *prefix,
    int verbose)
{
    char		line[80];
    char		buf[BIOSDISK_SECSIZE];
    struct disklabel	*lp;
    int			i;

    /* read disklabel */
    if (bd_read(od, offset + LABELSECTOR, 1, buf))
	return;
    lp =(struct disklabel *)(&buf[0]);
    if (lp->d_magic != DISKMAGIC) {
	sprintf(line, "%s: FFS  bad disklabel\n", prefix);
	pager_output(line);
	return;
    }
    
    /* Print partitions */
    for (i = 0; i < lp->d_npartitions; i++) {
	/*
	 * For each partition, make sure we know what type of fs it is.  If
	 * not, then skip it.  However, since floppies often have bogus
	 * fstypes, print the 'a' partition on a floppy even if it is marked
	 * unused.
	 */
	if ((lp->d_partitions[i].p_fstype == FS_BSDFFS) ||
            (lp->d_partitions[i].p_fstype == FS_SWAP) ||
            (lp->d_partitions[i].p_fstype == FS_VINUM) ||
	    ((lp->d_partitions[i].p_fstype == FS_UNUSED) && 
	     (od->od_flags & BD_FLOPPY) && (i == 0))) {

	    /* Only print out statistics in verbose mode */
	    if (verbose)
	        sprintf(line, "  %s%c: %s %s (%d - %d)\n", prefix, 'a' + i,
		    (lp->d_partitions[i].p_fstype == FS_SWAP) ? "swap " : 
		    (lp->d_partitions[i].p_fstype == FS_VINUM) ? "vinum" :
		    "FFS  ",
		    display_size(lp->d_partitions[i].p_size),
		    lp->d_partitions[i].p_offset,
		    lp->d_partitions[i].p_offset + lp->d_partitions[i].p_size);
	    else
	        sprintf(line, "  %s%c: %s\n", prefix, 'a' + i,
		    (lp->d_partitions[i].p_fstype == FS_SWAP) ? "swap" : 
		    (lp->d_partitions[i].p_fstype == FS_VINUM) ? "vinum" :
		    "FFS");
	    pager_output(line);
	}
    }
}


/*
 * Attempt to open the disk described by (dev) for use by (f).
 *
 * Note that the philosophy here is "give them exactly what
 * they ask for".  This is necessary because being too "smart"
 * about what the user might want leads to complications.
 * (eg. given no slice or partition value, with a disk that is
 *  sliced - are they after the first BSD slice, or the DOS
 *  slice before it?)
 */
static int 
bd_open(struct open_file *f, ...)
{
    va_list			ap;
    struct i386_devdesc		*dev;
    struct open_disk		*od;
    int				error;

    va_start(ap, f);
    dev = va_arg(ap, struct i386_devdesc *);
    va_end(ap);
    if ((error = bd_opendisk(&od, dev)))
	return(error);
    
    /*
     * Save our context
     */
    ((struct i386_devdesc *)(f->f_devdata))->d_kind.biosdisk.data = od;
    DEBUG("open_disk %p, partition at 0x%x", od, od->od_boff);
    return(0);
}

static int
bd_opendisk(struct open_disk **odp, struct i386_devdesc *dev)
{
    struct open_disk		*od;
    int				error;

    if (dev->d_unit >= nbdinfo) {
	DEBUG("attempt to open nonexistent disk");
	return(ENXIO);
    }
    
    od = (struct open_disk *)malloc(sizeof(struct open_disk));
    if (!od) {
	DEBUG("no memory");
	return (ENOMEM);
    }

    /* Look up BIOS unit number, intialise open_disk structure */
    od->od_dkunit = dev->d_unit;
    od->od_unit = bdinfo[od->od_dkunit].bd_unit;
    od->od_flags = bdinfo[od->od_dkunit].bd_flags;
    od->od_boff = 0;
    error = 0;
    DEBUG("open '%s', unit 0x%x slice %d partition %d",
	     i386_fmtdev(dev), dev->d_unit, 
	     dev->d_kind.biosdisk.slice, dev->d_kind.biosdisk.partition);

    /* Get geometry for this open (removable device may have changed) */
    if (bd_getgeom(od)) {
	DEBUG("can't get geometry");
	error = ENXIO;
	goto out;
    }

    /* Determine disk layout. */
    error = bd_open_pc98(od, dev);
    
 out:
    if (error) {
	free(od);
    } else {
	*odp = od;	/* return the open disk */
    }
    return(error);
}

static int
bd_open_pc98(struct open_disk *od, struct i386_devdesc *dev)
{
    struct pc98_partition	*dptr;
    struct disklabel		*lp;
    int				sector, slice, i;
    char			buf[BUFSIZE];

    /*
     * Following calculations attempt to determine the correct value
     * for d->od_boff by looking for the slice and partition specified,
     * or searching for reasonable defaults.
     */

    /*
     * Find the slice in the DOS slice table.
     */
    od->od_nslices = 0;
    if (od->od_flags & BD_FLOPPY) {
	sector = 0;
	goto unsliced;
    }
    if (bd_read(od, 0, 1, buf)) {
	DEBUG("error reading MBR");
	return (EIO);
    }

    /* 
     * Check the slice table magic.
     */
    if (((u_char)buf[0x1fe] != 0x55) || ((u_char)buf[0x1ff] != 0xaa)) {
	/* If a slice number was explicitly supplied, this is an error */
	if (dev->d_kind.biosdisk.slice > 0) {
	    DEBUG("no slice table/MBR (no magic)");
	    return (ENOENT);
	}
	sector = 0;
	goto unsliced;		/* may be a floppy */
    }
    if (bd_read(od, 1, 1, buf)) {
	DEBUG("error reading MBR");
	return (EIO);
    }

    /*
     * copy the partition table, then pick up any extended partitions.
     */
    bcopy(buf + DOSPARTOFF, &od->od_slicetab,
      sizeof(struct pc98_partition) * NDOSPART);
    od->od_nslices = NDOSPART;		/* extended slices start here */
    od->od_flags |= BD_PARTTABOK;
    dptr = &od->od_slicetab[0];

    /* Is this a request for the whole disk? */
    if (dev->d_kind.biosdisk.slice == -1) {
	sector = 0;
	goto unsliced;
    }

    /*
     * if a slice number was supplied but not found, this is an error.
     */
    if (dev->d_kind.biosdisk.slice > 0) {
        slice = dev->d_kind.biosdisk.slice - 1;
        if (slice >= od->od_nslices) {
            DEBUG("slice %d not found", slice);
	    return (ENOENT);
        }
    }

    /* Try to auto-detect the best slice; this should always give a slice number */
    if (dev->d_kind.biosdisk.slice == 0) {
	slice = bd_bestslice(od);
        if (slice == -1) {
	    return (ENOENT);
        }
        dev->d_kind.biosdisk.slice = slice;
    }

    dptr = &od->od_slicetab[0];
    /*
     * Accept the supplied slice number unequivocally (we may be looking
     * at a DOS partition).
     */
    dptr += (dev->d_kind.biosdisk.slice - 1);	/* we number 1-4, offsets are 0-3 */
    sector = dptr->dp_scyl * od->od_hds * od->od_sec +
	dptr->dp_shd * od->od_sec + dptr->dp_ssect;
    {
	int end = dptr->dp_ecyl * od->od_hds * od->od_sec +
	    dptr->dp_ehd * od->od_sec + dptr->dp_esect;
	DEBUG("slice entry %d at %d, %d sectors",
	      dev->d_kind.biosdisk.slice - 1, sector, end-sector);
    }

    /*
     * If we are looking at a BSD slice, and the partition is < 0, assume the 'a' partition
     */
    if ((dptr->dp_mid == DOSMID_386BSD) && (dev->d_kind.biosdisk.partition < 0))
	dev->d_kind.biosdisk.partition = 0;

 unsliced:
    /* 
     * Now we have the slice offset, look for the partition in the disklabel if we have
     * a partition to start with.
     *
     * XXX we might want to check the label checksum.
     */
    if (dev->d_kind.biosdisk.partition < 0) {
	od->od_boff = sector;		/* no partition, must be after the slice */
	DEBUG("opening raw slice");
    } else {
	
	if (bd_read(od, sector + LABELSECTOR, 1, buf)) {
	    DEBUG("error reading disklabel");
	    return (EIO);
	}
	DEBUG("copy %d bytes of label from %p to %p", sizeof(struct disklabel), buf + LABELOFFSET, &od->od_disklabel);
	bcopy(buf + LABELOFFSET, &od->od_disklabel, sizeof(struct disklabel));
	lp = &od->od_disklabel;
	od->od_flags |= BD_LABELOK;

	if (lp->d_magic != DISKMAGIC) {
	    DEBUG("no disklabel");
	    return (ENOENT);
	}
	if (dev->d_kind.biosdisk.partition >= lp->d_npartitions) {
	    DEBUG("partition '%c' exceeds partitions in table (a-'%c')",
		  'a' + dev->d_kind.biosdisk.partition, 'a' + lp->d_npartitions);
	    return (EPART);
	}

#ifdef DISK_DEBUG
	/* Complain if the partition is unused unless this is a floppy. */
	if ((lp->d_partitions[dev->d_kind.biosdisk.partition].p_fstype == FS_UNUSED) &&
	    !(od->od_flags & BD_FLOPPY))
	    DEBUG("warning, partition marked as unused");
#endif
	
	od->od_boff = 
		lp->d_partitions[dev->d_kind.biosdisk.partition].p_offset -
		lp->d_partitions[RAW_PART].p_offset +
		sector;
    }
    return (0);
}

/*
 * Search for a slice with the following preferences:
 *
 * 1: Active FreeBSD slice
 * 2: Non-active FreeBSD slice
 * 3: Active Linux slice
 * 4: non-active Linux slice
 * 5: Active FAT/FAT32 slice
 * 6: non-active FAT/FAT32 slice
 */
#define PREF_RAWDISK	0
#define PREF_FBSD_ACT	1
#define PREF_FBSD	2
#define PREF_LINUX_ACT	3
#define PREF_LINUX	4
#define PREF_DOS_ACT	5
#define PREF_DOS	6
#define PREF_NONE	7

/*
 * slicelimit is in the range 0 .. NDOSPART
 */
static int
bd_bestslice(struct open_disk *od)
{
	struct pc98_partition *dp;
	int pref, preflevel;
	int i, prefslice;
	
	prefslice = 0;
	preflevel = PREF_NONE;

	dp = &od->od_slicetab[0];
	for (i = 0; i < od->od_nslices; i++, dp++) {
		switch(dp->dp_mid & PC98_MID_MASK) {
		case PC98_MID_386BSD:		/* FreeBSD */
			if ((dp->dp_mid & PC98_MID_BOOTABLE) &&
			    (preflevel > PREF_FBSD_ACT)) {
				pref = i;
				preflevel = PREF_FBSD_ACT;
			} else if (preflevel > PREF_FBSD) {
				pref = i;
				preflevel = PREF_FBSD;
			}
			break;

		case 0x11:				/* DOS/Windows */
		case 0x20:
		case 0x21:
		case 0x22:
		case 0x23:
		case 0x63:
			if ((dp->dp_mid & PC98_MID_BOOTABLE) &&
			    (preflevel > PREF_DOS_ACT)) {
				pref = i;
				preflevel = PREF_DOS_ACT;
			} else if (preflevel > PREF_DOS) {
				pref = i;
				preflevel = PREF_DOS;
			}
			break;
		}
	}
	return (prefslice);
}
 
static int 
bd_close(struct open_file *f)
{
    struct open_disk	*od = (struct open_disk *)(((struct i386_devdesc *)(f->f_devdata))->d_kind.biosdisk.data);

    bd_closedisk(od);
    return(0);
}

static void
bd_closedisk(struct open_disk *od)
{
    DEBUG("open_disk %p", od);
#if 0
    /* XXX is this required? (especially if disk already open...) */
    if (od->od_flags & BD_FLOPPY)
	delay(3000000);
#endif
    free(od);
}

static int 
bd_strategy(void *devdata, int rw, daddr_t dblk, size_t size, char *buf, size_t *rsize)
{
    struct bcache_devdata	bcd;
    struct open_disk	*od = (struct open_disk *)(((struct i386_devdesc *)devdata)->d_kind.biosdisk.data);

    bcd.dv_strategy = bd_realstrategy;
    bcd.dv_devdata = devdata;
    return(bcache_strategy(&bcd, od->od_unit, rw, dblk+od->od_boff, size, buf, rsize));
}

static int 
bd_realstrategy(void *devdata, int rw, daddr_t dblk, size_t size, char *buf, size_t *rsize)
{
    struct open_disk	*od = (struct open_disk *)(((struct i386_devdesc *)devdata)->d_kind.biosdisk.data);
    int			blks;
#ifdef BD_SUPPORT_FRAGS
    char		fragbuf[BIOSDISK_SECSIZE];
    size_t		fragsize;

    fragsize = size % BIOSDISK_SECSIZE;
#else
    if (size % BIOSDISK_SECSIZE)
	panic("bd_strategy: %d bytes I/O not multiple of block size", size);
#endif

    DEBUG("open_disk %p", od);
    blks = size / BIOSDISK_SECSIZE;
    if (rsize)
	*rsize = 0;

    switch(rw){
    case F_READ:
	DEBUG("read %d from %d to %p", blks, dblk, buf);

	if (blks && bd_read(od, dblk, blks, buf)) {
	    DEBUG("read error");
	    return (EIO);
	}
#ifdef BD_SUPPORT_FRAGS
	DEBUG("bd_strategy: frag read %d from %d+%d to %p",
	    fragsize, dblk, blks, buf + (blks * BIOSDISK_SECSIZE));
	if (fragsize && bd_read(od, dblk + blks, 1, fragsize)) {
	    DEBUG("frag read error");
	    return(EIO);
	}
	bcopy(fragbuf, buf + (blks * BIOSDISK_SECSIZE), fragsize);
#endif
	break;
    case F_WRITE :
	DEBUG("write %d from %d to %p", blks, dblk, buf);

	if (blks && bd_write(od, dblk, blks, buf)) {
	    DEBUG("write error");
	    return (EIO);
	}
#ifdef BD_SUPPORT_FRAGS
	if(fragsize) {
	    DEBUG("Attempted to write a frag");
	    return (EIO);
	}
#endif
	break;
    default:
	/* DO NOTHING */
	return (EROFS);
    }

    if (rsize)
	*rsize = size;
    return (0);
}

/* Max number of sectors to bounce-buffer if the request crosses a 64k boundary */
#define FLOPPY_BOUNCEBUF	18

static int
bd_chs_io(struct open_disk *od, daddr_t dblk, int blks, caddr_t dest, int write)
{
    u_int	x, bpc, cyl, hd, sec;

    bpc = (od->od_sec * od->od_hds);	/* blocks per cylinder */
    x = dblk;
    cyl = x / bpc;			/* block # / blocks per cylinder */
    x %= bpc;				/* block offset into cylinder */
    hd = x / od->od_sec;		/* offset / blocks per track */
    sec = x % od->od_sec;		/* offset into track */

    v86.ctl = V86_FLAGS;
    v86.addr = 0x1b;
    if (write)
	v86.eax = 0x0500 | od->od_unit;
    else
	v86.eax = 0x0600 | od->od_unit;
    if (od->od_flags & BD_FLOPPY) {
	v86.eax |= 0xd000;
	v86.ecx = 0x0200 | (cyl & 0xff);
	v86.edx = (hd << 8) | (sec + 1);
    } else if (od->od_flags & BD_OPTICAL) {
	v86.eax &= 0xFF7F;
	v86.ecx = dblk & 0xFFFF;
	v86.edx = dblk >> 16;
    } else {
	v86.ecx = cyl;
	v86.edx = (hd << 8) | sec;
    }
    v86.ebx = blks * BIOSDISK_SECSIZE;
    v86.es = VTOPSEG(dest);
    v86.ebp = VTOPOFF(dest);
    v86int();
    return (v86.efl & 0x1);
}

static int
bd_io(struct open_disk *od, daddr_t dblk, int blks, caddr_t dest, int write)
{
    u_int	x, sec, result, resid, retry, maxfer;
    caddr_t	p, xp, bbuf, breg;
    
    /* Just in case some idiot actually tries to read/write -1 blocks... */
    if (blks < 0)
	return (-1);

    resid = blks;
    p = dest;

    /* Decide whether we have to bounce */
    if (VTOP(dest) >> 20 != 0 ||
	((VTOP(dest) >> 16) != (VTOP(dest + blks * BIOSDISK_SECSIZE) >> 16))) {

	/* 
	 * There is a 64k physical boundary somewhere in the
	 * destination buffer, or the destination buffer is above
	 * first 1MB of physical memory so we have to arrange a
	 * suitable bounce buffer.  Allocate a buffer twice as large
	 * as we need to.  Use the bottom half unless there is a break
	 * there, in which case we use the top half.
	 */
	x = min(od->od_sec, (unsigned)blks);
	bbuf = alloca(x * 2 * BIOSDISK_SECSIZE);
	if (((u_int32_t)VTOP(bbuf) & 0xffff0000) ==
	    ((u_int32_t)VTOP(bbuf + x * BIOSDISK_SECSIZE) & 0xffff0000)) {
	    breg = bbuf;
	} else {
	    breg = bbuf + x * BIOSDISK_SECSIZE;
	}
	maxfer = x;		/* limit transfers to bounce region size */
    } else {
	breg = bbuf = NULL;
	maxfer = 0;
    }
    
    while (resid > 0) {
	/*
	 * Play it safe and don't cross track boundaries.
	 * (XXX this is probably unnecessary)
	 */
	sec = dblk % od->od_sec;	/* offset into track */
	x = min(od->od_sec - sec, resid);
	if (maxfer > 0)
	    x = min(x, maxfer);		/* fit bounce buffer */

	/* where do we transfer to? */
	xp = bbuf == NULL ? p : breg;

	/*
	 * Put your Data In, Put your Data out,
	 * Put your Data In, and shake it all about 
	 */
	if (write && bbuf != NULL)
	    bcopy(p, breg, x * BIOSDISK_SECSIZE);

	/*
	 * Loop retrying the operation a couple of times.  The BIOS
	 * may also retry.
	 */
	for (retry = 0; retry < 3; retry++) {
	    /* if retrying, reset the drive */
	    if (retry > 0) {
		v86.ctl = V86_FLAGS;
		v86.addr = 0x1b;
		v86.eax = 0x0300 | od->od_unit;
		v86int();
	    }

	    result = bd_chs_io(od, dblk, x, xp, write);
	    if (result == 0)
		break;
	}

	if (write)
	    DEBUG("Write %d sector(s) from %p (0x%x) to %lld %s", x,
		p, VTOP(p), dblk, result ? "failed" : "ok");
	else
	    DEBUG("Read %d sector(s) from %lld to %p (0x%x) %s", x,
		dblk, p, VTOP(p), result ? "failed" : "ok");
	if (result) {
	    return(-1);
	}
	if (!write && bbuf != NULL)
	    bcopy(breg, p, x * BIOSDISK_SECSIZE);
	p += (x * BIOSDISK_SECSIZE);
	dblk += x;
	resid -= x;
    }

/*    hexdump(dest, (blks * BIOSDISK_SECSIZE)); */
    return(0);
}

static int
bd_read(struct open_disk *od, daddr_t dblk, int blks, caddr_t dest)
{

    return (bd_io(od, dblk, blks, dest, 0));
}

static int
bd_write(struct open_disk *od, daddr_t dblk, int blks, caddr_t dest)
{

    return (bd_io(od, dblk, blks, dest, 1));
}

static int
bd_getgeom(struct open_disk *od)
{

    if (od->od_flags & BD_FLOPPY) {
	od->od_cyl = 79;
	od->od_hds = 2;
	od->od_sec = (od->od_unit & 0xf0) == 0x30 ? 18 : 15;
    } else if (od->od_flags & BD_OPTICAL) {
	od->od_cyl = 0xFFFE;
	od->od_hds = 8;
	od->od_sec = 32;
    } else {
	v86.ctl = V86_FLAGS;
	v86.addr = 0x1b;
	v86.eax = 0x8400 | od->od_unit;
	v86int();
      
	od->od_cyl = v86.ecx;
	od->od_hds = (v86.edx >> 8) & 0xff;
	od->od_sec = v86.edx & 0xff;
	if (v86.efl & 0x1)
	    return(1);
    }

    DEBUG("unit 0x%x geometry %d/%d/%d", od->od_unit, od->od_cyl, od->od_hds, od->od_sec);
    return(0);
}

/*
 * Return the BIOS geometry of a given "fixed drive" in a format
 * suitable for the legacy bootinfo structure.  Since the kernel is
 * expecting raw int 0x13/0x8 values for N_BIOS_GEOM drives, we
 * prefer to get the information directly, rather than rely on being
 * able to put it together from information already maintained for
 * different purposes and for a probably different number of drives.
 *
 * For valid drives, the geometry is expected in the format (31..0)
 * "000000cc cccccccc hhhhhhhh 00ssssss"; and invalid drives are
 * indicated by returning the geometry of a "1.2M" PC-format floppy
 * disk.  And, incidentally, what is returned is not the geometry as
 * such but the highest valid cylinder, head, and sector numbers.
 */
u_int32_t
bd_getbigeom(int bunit)
{
    int hds = 0;
    int unit = 0x80;		/* IDE HDD */
    u_int addr = 0xA155d;

    while (unit < 0xa7) {
	if (*(u_char *)PTOV(addr) & (1 << (unit & 0x0f)))
	    if (hds++ == bunit)
		break;

	if (unit >= 0xA0) {
	    int  media = ((unsigned *)PTOV(0xA1460))[unit & 0x0F] & 0x1F;

	    if (media == 7 && hds++ == bunit)	/* SCSI MO */
		return(0xFFFE0820); /* C:65535 H:8 S:32 */
	}
	if (++unit == 0x84) {
	    unit = 0xA0;	/* SCSI HDD */
	    addr = 0xA1482;
	}
    }
    if (unit == 0xa7)
	return 0x4F020F;	/* 1200KB FD C:80 H:2 S:15 */
    v86.ctl = V86_FLAGS;
    v86.addr = 0x1b;
    v86.eax = 0x8400 | unit;
    v86int();
    if (v86.efl & 0x1)
	return 0x4F020F;	/* 1200KB FD C:80 H:2 S:15 */
    return ((v86.ecx & 0xffff) << 16) | (v86.edx & 0xffff);
}

/*
 * Return a suitable dev_t value for (dev).
 *
 * In the case where it looks like (dev) is a SCSI disk, we allow the number of
 * IDE disks to be specified in $num_ide_disks.  There should be a Better Way.
 */
int
bd_getdev(struct i386_devdesc *dev)
{
    struct open_disk		*od;
    int				biosdev;
    int 			major;
    int				rootdev;
    char			*nip, *cp;
    int				unitofs = 0, i, unit;

    biosdev = bd_unit2bios(dev->d_unit);
    DEBUG("unit %d BIOS device %d", dev->d_unit, biosdev);
    if (biosdev == -1)				/* not a BIOS device */
	return(-1);
    if (bd_opendisk(&od, dev) != 0)		/* oops, not a viable device */
	return(-1);

    if ((biosdev & 0xf0) == 0x90 || (biosdev & 0xf0) == 0x30) {
	/* floppy (or emulated floppy) or ATAPI device */
	if (bdinfo[dev->d_unit].bd_type == DT_ATAPI) {
	    /* is an ATAPI disk */
	    major = WFDMAJOR;
	} else {
	    /* is a floppy disk */
	    major = FDMAJOR;
	}
    } else {
	/* harddisk */
	if ((od->od_flags & BD_LABELOK) && (od->od_disklabel.d_type == DTYPE_SCSI)) {
	    /* label OK, disk labelled as SCSI */
	    major = DAMAJOR;
	    /* check for unit number correction hint, now deprecated */
	    if ((nip = getenv("num_ide_disks")) != NULL) {
		i = strtol(nip, &cp, 0);
		/* check for parse error */
		if ((cp != nip) && (*cp == 0))
		    unitofs = i;
	    }
	} else {
	    /* assume an IDE disk */
	    major = WDMAJOR;
	}
    }
    /* default root disk unit number */
    if ((biosdev & 0xf0) == 0xa0)
	unit = bdinfo[dev->d_unit].bd_da_unit;
    else
	unit = biosdev & 0xf;

    /* XXX a better kludge to set the root disk unit number */
    if ((nip = getenv("root_disk_unit")) != NULL) {
	i = strtol(nip, &cp, 0);
	/* check for parse error */
	if ((cp != nip) && (*cp == 0))
	    unit = i;
    }

    rootdev = MAKEBOOTDEV(major, dev->d_kind.biosdisk.slice + 1, unit,
	dev->d_kind.biosdisk.partition);
    DEBUG("dev is 0x%x\n", rootdev);
    return(rootdev);
}
