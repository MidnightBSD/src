/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <err.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/sun_disklabel.h>
#include <paths.h>
#include <errno.h>
#include "libdisk.h"

#include "geom_sunlabel_enc.c"

int
Write_Disk(const struct disk *d1)
{
	struct sun_disklabel *sl;
	struct chunk *c, *c1, *c2;
	int i;
	char *p;
	u_long secpercyl;
	char device[64];
	u_char buf[SUN_SIZE];
	int fd;

	strcpy(device, _PATH_DEV);
	strcat(device, d1->name);

	fd = open(device, O_RDWR);
	if (fd < 0) {
		warn("open(%s) failed", device);
		return (1);
	}

	sl = calloc(sizeof *sl, 1);
	c = d1->chunks;
	c2 = c->part;
	secpercyl = d1->bios_sect * d1->bios_hd;
	sl->sl_pcylinders = c->size / secpercyl;
	sl->sl_ncylinders = c2->size / secpercyl;
	sl->sl_acylinders = sl->sl_pcylinders - sl->sl_ncylinders;
	sl->sl_magic = SUN_DKMAGIC;
	sl->sl_nsectors = d1->bios_sect;
	sl->sl_ntracks = d1->bios_hd;
	if (c->size > 4999 * 1024 * 2) {
		sprintf(sl->sl_text, "FreeBSD%luG cyl %u alt %u hd %u sec %u",
			(c->size + 1024 * 1024) / (2 * 1024 * 1024),
			sl->sl_ncylinders, sl->sl_acylinders,
			sl->sl_ntracks, sl->sl_nsectors);
	} else {
		sprintf(sl->sl_text, "FreeBSD%luM cyl %u alt %u hd %u sec %u",
			(c->size + 1024) / (2 * 1024),
			sl->sl_ncylinders, sl->sl_acylinders,
			sl->sl_ntracks, sl->sl_nsectors);
	}
	sl->sl_interleave = 1;
	sl->sl_sparespercyl = 0;
	sl->sl_rpm = 3600;

	for (c1 = c2->part; c1 != NULL; c1 = c1->next) {
		p = c1->name;
		p += strlen(p);
		p--;
		if (*p < 'a')
			continue;
		i = *p - 'a';
		if (i >= SUN_NPART)
			continue;
		sl->sl_part[i].sdkp_cyloffset = c1->offset / secpercyl;
		sl->sl_part[i].sdkp_nsectors = c1->size;
		for (i = 1; i < 16; i++) {
			write_block(fd, c1->offset + i, d1->boot1 + (i * 512),
			    512);
		}
	}

	/*
	 * We need to fill in the "RAW" partition as well.  Emperical data
	 * seems to indicate that this covers the "obviously" visible part
	 * of the disk, ie: sl->sl_ncylinders.
	 */
	sl->sl_part[SUN_RAWPART].sdkp_cyloffset = 0;
	sl->sl_part[SUN_RAWPART].sdkp_nsectors = sl->sl_ncylinders * secpercyl;

	memset(buf, 0, sizeof buf);
	sunlabel_enc(buf, sl);
	write_block(fd, 0, buf, sizeof buf);

	close(fd);
	return 0;
}
