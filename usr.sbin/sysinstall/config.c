/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $FreeBSD$
 *
 * Copyright (c) 1995
 *	Jordan Hubbard.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    verbatim and that no modifications are made prior to this
 *    point in the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY JORDAN HUBBARD ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL JORDAN HUBBARD OR HIS PETS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, LIFE OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include "sysinstall.h"
#include <sys/disklabel.h>
#include <sys/wait.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mount.h>
#include <libdisk.h>
#include <time.h>
#include <kenv.h>

static Chunk *chunk_list[MAX_CHUNKS];
static int nchunks;
static int rootdev_is_od;

/* arg to sort */
static int
chunk_compare(Chunk *c1, Chunk *c2)
{
    if (!c1 && !c2)
	return 0;
    else if (!c1 && c2)
	return 1;
    else if (c1 && !c2)
	return -1;
    else if (!c1->private_data && !c2->private_data)
	return 0;
    else if (c1->private_data && !c2->private_data)
	return 1;
    else if (!c1->private_data && c2->private_data)
	return -1;
    else
	return strcmp(((PartInfo *)(c1->private_data))->mountpoint, ((PartInfo *)(c2->private_data))->mountpoint);
}

static void
chunk_sort(void)
{
    int i, j;

    for (i = 0; i < nchunks; i++) {
	for (j = 0; j < nchunks; j++) {
	    if (chunk_compare(chunk_list[j], chunk_list[j + 1]) > 0) {
		Chunk *tmp = chunk_list[j];

		chunk_list[j] = chunk_list[j + 1];
		chunk_list[j + 1] = tmp;
	    }
	}
    }
}

static void
check_rootdev(Chunk **list, int n)
{
	int i;
	Chunk *c;

	rootdev_is_od = 0;
	for (i = 0; i < n; i++) {
		c = *list++;
		if (c->type == part && (c->flags & CHUNK_IS_ROOT)
		    && strncmp(c->disk->name, "od", 2) == 0)
			rootdev_is_od = 1;
	}
}

static char *
name_of(Chunk *c1)
{
    return c1->name;
}

static char *
mount_point(Chunk *c1)
{
    if (c1->type == part && c1->subtype == FS_SWAP)
	return "none";
    else if (c1->type == part || c1->type == fat || c1->type == efi)
	return ((PartInfo *)c1->private_data)->mountpoint;
    return "/bogus";
}

static char *
fstype(Chunk *c1)
{
    if (c1->type == fat || c1->type == efi)
	return "msdosfs";
    else if (c1->type == part) {
	if (c1->subtype != FS_SWAP)
	    return "ufs";
	else
	    return "swap";
    }
    return "bogus";
}

static char *
fstype_short(Chunk *c1)
{
    if (c1->type == part) {
	if (c1->subtype != FS_SWAP) {
	    if (rootdev_is_od == 0 && strncmp(c1->name, "od", 2) == 0)
		return "rw,noauto";
	    else
		return "rw";
	}
	else
	    return "sw";
    }
    else if (c1->type == fat) {
	if (strncmp(c1->name, "od", 2) == 0)
	    return "ro,noauto";
	else
	    return "ro";
    }
    else if (c1->type == efi)
	return "rw";

    return "bog";
}

static int
seq_num(Chunk *c1)
{
    if (c1->type == part && c1->subtype != FS_SWAP) {
	if (rootdev_is_od == 0 && strncmp(c1->name, "od", 2) == 0)
	    return 0;
	else if (c1->flags & CHUNK_IS_ROOT)
	    return 1;
	else
	    return 2;
    }
    return 0;
}

int
configFstab(dialogMenuItem *self)
{
    Device **devs;
    Disk *disk;
    FILE *fstab;
    int i, cnt;
    Chunk *c1, *c2;

    if (!RunningAsInit) {
	if (file_readable("/etc/fstab"))
	    return DITEM_SUCCESS;
	else {
	    msgConfirm("Attempting to rebuild your /etc/fstab file.  Warning: If you had\n"
		       "any CD devices in use before running %s then they may NOT\n"
		       "be found by this run!", ProgName);
	}
    }

    devs = deviceFind(NULL, DEVICE_TYPE_DISK);
    if (!devs) {
	msgConfirm("No disks found!");
	return DITEM_FAILURE;
    }

    /* Record all the chunks */
    nchunks = 0;
    for (i = 0; devs[i]; i++) {
	if (!devs[i]->enabled)
	    continue;
	disk = (Disk *)devs[i]->private;
	if (!disk->chunks)
	    msgFatal("No chunk list found for %s!", disk->name);
	for (c1 = disk->chunks->part; c1; c1 = c1->next) {
#ifdef __powerpc__
	    if (c1->type == apple) {
#else
	    if (c1->type == freebsd) {
#endif
		for (c2 = c1->part; c2; c2 = c2->next) {
		    if (c2->type == part && (c2->subtype == FS_SWAP || c2->private_data))
			chunk_list[nchunks++] = c2;
		}
	    }
	    else if (((c1->type == fat || c1->type == efi || c1->type == part) &&
		    c1->private_data) || (c1->type == part && c1->subtype == FS_SWAP))
		chunk_list[nchunks++] = c1;
	}
    }
    chunk_list[nchunks] = 0;
    chunk_sort();

    fstab = fopen("/etc/fstab", "w");
    if (!fstab) {
	msgConfirm("Unable to create a new /etc/fstab file!  Manual intervention\n"
		   "will be required.");
	return DITEM_FAILURE;
    }

    check_rootdev(chunk_list, nchunks);

    /* Go for the burn */
    msgDebug("Generating /etc/fstab file\n");
    fprintf(fstab, "# Device\t\tMountpoint\tFStype\tOptions\t\tDump\tPass#\n");
    for (i = 0; i < nchunks; i++)
	fprintf(fstab, "/dev/%s\t\t%s\t\t%s\t%s\t\t%d\t%d\n", name_of(chunk_list[i]), mount_point(chunk_list[i]),
		fstype(chunk_list[i]), fstype_short(chunk_list[i]), seq_num(chunk_list[i]), seq_num(chunk_list[i]));

    /* Now look for the CDROMs */
    devs = deviceFind(NULL, DEVICE_TYPE_CDROM);
    cnt = deviceCount(devs);

    /* Write out the CDROM entries */
    for (i = 0; i < cnt; i++) {
	char cdname[10];

	sprintf(cdname, "/cdrom%s", i ? itoa(i) : "");
	if (Mkdir(cdname))
	    msgConfirm("Unable to make mount point for: %s", cdname);
	else
	    fprintf(fstab, "/dev/%s\t\t%s\t\tcd9660\tro,noauto\t0\t0\n", devs[i]->name, cdname);
    }

    fclose(fstab);
    if (isDebug())
	msgDebug("Wrote out /etc/fstab file\n");
    return DITEM_SUCCESS;
}

/* Do the work of sucking in a config file.
 * config is the filename to read in.
 * lines is a fixed (max) sized array of char*
 * returns number of lines read.  line contents
 * are malloc'd and must be freed by the caller.
 */
static int
readConfig(char *config, char **lines, int max)
{
    FILE *fp;
    char line[256];
    int i, nlines;

    fp = fopen(config, "r");
    if (!fp)
	return -1;

    nlines = 0;
    /* Read in the entire file */
    for (i = 0; i < max; i++) {
	if (!fgets(line, sizeof line, fp))
	    break;
	lines[nlines++] = strdup(line);
    }
    fclose(fp);
    if (isDebug())
	msgDebug("readConfig: Read %d lines from %s.\n", nlines, config);
    return nlines;
}

#define MAX_LINES  2000 /* Some big number we're not likely to ever reach - I'm being really lazy here, I know */

static void
readConfigFile(char *config, int marked)
{
    char *lines[MAX_LINES], *cp, *cp2;
    int i, nlines;

    nlines = readConfig(config, lines, MAX_LINES);
    if (nlines == -1)
	return;

    for (i = 0; i < nlines; i++) {
	/* Skip the comments & non-variable settings */
	if (lines[i][0] == '#' || !(cp = index(lines[i], '='))) {
	    free(lines[i]);
	    continue;
	}
	*cp++ = '\0';
	/* Find quotes */
	if ((cp2 = index(cp, '"')) || (cp2 = index(cp, '\047'))) {
	    cp = cp2 + 1;
	    cp2 = index(cp, *cp2);
	}
	/* If valid quotes, use it */
	if (cp2) {
	    *cp2 = '\0';
 	    /* If we have a legit value, set it */
	    if (strlen(cp))
		variable_set2(lines[i], cp, marked);
	}
	free(lines[i]);
    }
}

/* Load the environment from rc.conf file(s) */
void
configEnvironmentRC_conf(void)
{
    static struct {
	char *fname;
	int marked;
    } configs[] = {
	{ "/etc/defaults/rc.conf", 0 },
	{ "/etc/rc.conf", 0 },
	{ "/etc/rc.conf.local", 0 },
	{ NULL, 0 },
    };
    int i;

    for (i = 0; configs[i].fname; i++) {
	if (file_readable(configs[i].fname))
	    readConfigFile(configs[i].fname, configs[i].marked);
    }
}

/* Load the environment from a resolv.conf file */
void
configEnvironmentResolv(char *config)
{
    char *lines[MAX_LINES];
    int i, nlines;

    nlines = readConfig(config, lines, MAX_LINES);
    if (nlines == -1)
	return;
    for (i = 0; i < nlines; i++) {
	Boolean name_set = variable_get(VAR_NAMESERVER) ? 1 : 0;

	if (!strncmp(lines[i], "domain", 6) && !variable_get(VAR_DOMAINNAME))
	    variable_set2(VAR_DOMAINNAME, string_skipwhite(string_prune(lines[i] + 6)), 0);
	else if (!name_set && !strncmp(lines[i], "nameserver", 10)) {
	    /* Only take the first nameserver setting - we're lame */
	    variable_set2(VAR_NAMESERVER, string_skipwhite(string_prune(lines[i] + 10)), 0);
	}
	free(lines[i]);
    }
}

/* Version of below for dispatch routines */
int
configRC(dialogMenuItem *unused)
{
    configRC_conf();
    return DITEM_SUCCESS;
}

/*
 * Write out rc.conf
 *
 * rc.conf is sorted if running as init and the needed utilities are
 * present
 *
 * If rc.conf is sorted, all variables in rc.conf which conflict with
 * the variables in the environment are removed from the original
 * rc.conf
 */
void
configRC_conf(void)
{
    char line[256];
    FILE *rcSite, *rcOld;
    Variable *v;
    int write_header;
    time_t t_loc;
    char *cp;
    static int did_marker = 0;
    int do_sort;
    int do_merge;
    time_t tp;

    configTtys();
    write_header = !file_readable("/etc/rc.conf");
    do_sort = RunningAsInit && file_readable("/usr/bin/sort") &&
	file_readable("/usr/bin/uniq");
    do_merge = do_sort && file_readable("/etc/rc.conf");

    if(do_merge) {
	rcSite = fopen("/etc/rc.conf.new", "w");
    } else
	rcSite = fopen("/etc/rc.conf", "a");
    if (rcSite == NULL) {
	msgError("Error opening new rc.conf for writing: %s (%u)", strerror(errno), errno);
	return;
    }

    if (do_merge) {
	/* "Copy" the old rc.conf */
	rcOld = fopen("/etc/rc.conf", "r");
	if(!rcOld) {
	    msgError("Error opening rc.conf for reading: %s (%u)", strerror(errno), errno);
	    return;
	}
	while(fgets(line, sizeof(line), rcOld)) {
	    if(line[0] == '#' || variable_check2(line) != 0)
		fprintf(rcSite, "%s", line);
	    else {
		if (variable_get(VAR_KEEPRCCONF) != NULL)
		    fprintf(rcSite, "%s", line);
		else
		    fprintf(rcSite, "#REMOVED: %s", line);
	    }
	}
	fclose(rcOld);
    } else if (write_header) {
	fprintf(rcSite, "# This file now contains just the overrides from /etc/defaults/rc.conf.\n");
	fprintf(rcSite, "# Please make all changes to this file, not to /etc/defaults/rc.conf.\n\n");
	fprintf(rcSite, "# Enable network daemons for user convenience.\n");
	if ((t_loc = time(NULL)) != -1 && (cp = ctime(&t_loc)))
	    fprintf(rcSite, "# Created: %s", cp);
    }

    /* Now do variable substitutions */
    for (v = VarHead; v; v = v->next) {
	if (v->dirty) {
	    if (!did_marker) {
		time(&tp);
		fprintf(rcSite, "# -- sysinstall generated deltas -- # "
		    "%s", ctime(&tp));
		did_marker = 1;
	    }
	    fprintf(rcSite, "%s=\"%s\"\n", v->name, v->value);
	    v->dirty = 0;
	}
    }
    fclose(rcSite);

    if(do_merge) {
	if(rename("/etc/rc.conf.new", "/etc/rc.conf") != 0) {
	    msgError("Error renaming temporary rc.conf: %s (%u)", strerror(errno), errno);
	    return;
	}
    }

    /* Tidy up the resulting file if it's late enough in the installation
	for sort and uniq to be available */
    if (do_sort) {
	(void)vsystem("sort /etc/rc.conf | uniq > /etc/rc.conf.new && mv /etc/rc.conf.new /etc/rc.conf");
    }
}

int
configSaver(dialogMenuItem *self)
{
    variable_set((char *)self->data, 1);
    if (!variable_get(VAR_BLANKTIME))
	variable_set2(VAR_BLANKTIME, "300", 1);
    return DITEM_SUCCESS;
}

int
configSaverTimeout(dialogMenuItem *self)
{
    return (variable_get_value(VAR_BLANKTIME,
	    "Enter time-out period in seconds for screen saver", 1) ?
	DITEM_SUCCESS : DITEM_FAILURE);
}

int
configNTP(dialogMenuItem *self)
{
    int status;

    status = variable_get_value(VAR_NTPDATE_HOSTS,
				"Enter the name of an NTP server", 1)
	     ? DITEM_SUCCESS : DITEM_FAILURE;
    if (status == DITEM_SUCCESS) {
	static char tmp[255];

	snprintf(tmp, sizeof(tmp), "ntpdate_enable=YES,ntpdate_hosts=%s",
		 variable_get(VAR_NTPDATE_HOSTS));
	self->data = tmp;
	dmenuSetVariables(self);
    }
    return status;
}

int
configCountry(dialogMenuItem *self)
{
    int choice, scroll, curr, max;

    WINDOW *w = savescr();

    dialog_clear_norefresh();
    dmenuSetDefaultItem(&MenuCountry, NULL, NULL,
	VAR_COUNTRY "=" DEFAULT_COUNTRY, &choice, &scroll, &curr, &max);
    dmenuOpen(&MenuCountry, &choice, &scroll, &curr, &max, FALSE);
    restorescr(w);
    return DITEM_SUCCESS;
}

int
configUsers(dialogMenuItem *self)
{
    WINDOW *w = savescr();

    dialog_clear_norefresh();
    dmenuOpenSimple(&MenuUsermgmt, FALSE);
    restorescr(w);
    return DITEM_SUCCESS;
}

int
configSecurelevel(dialogMenuItem *self)
{
    WINDOW *w = savescr();

    dialog_clear_norefresh();
    dmenuOpenSimple(&MenuSecurelevel, FALSE);
    restorescr(w);
    return DITEM_SUCCESS;
}

int
configSecurelevelDisabled(dialogMenuItem *self)
{

    variable_set2("kern_securelevel_enable", "NO", 1);
    return DITEM_SUCCESS;
}

int
configSecurelevelSecure(dialogMenuItem *self)
{

    variable_set2("kern_securelevel_enable", "YES", 1);
    variable_set2("kern_securelevel", "1", 1);
    return DITEM_SUCCESS;
}

int
configSecurelevelHighlySecure(dialogMenuItem *self)
{

    variable_set2("kern_securelevel_enable", "YES", 1);
    variable_set2("kern_securelevel", "2", 1);
    return DITEM_SUCCESS;
}

int
configSecurelevelNetworkSecure(dialogMenuItem *self)
{

    variable_set2("kern_securelevel_enable", "YES", 1);
    variable_set2("kern_securelevel", "3", 1);
    return DITEM_SUCCESS;
}

int
configResolv(dialogMenuItem *ditem)
{
    FILE *fp;
    char *cp, *c6p, *dp, *hp;

    cp = variable_get(VAR_NAMESERVER);
    if (!cp || !*cp)
	goto skip;
    Mkdir("/etc");
    fp = fopen("/etc/resolv.conf", "w");
    if (!fp)
	return DITEM_FAILURE;
    if (variable_get(VAR_DOMAINNAME))
	fprintf(fp, "domain\t%s\n", variable_get(VAR_DOMAINNAME));
    fprintf(fp, "nameserver\t%s\n", cp);
    fclose(fp);
    if (isDebug())
	msgDebug("Wrote out /etc/resolv.conf\n");

skip:
    dp = variable_get(VAR_DOMAINNAME);
    cp = variable_get(VAR_IPADDR);
    c6p = variable_get(VAR_IPV6ADDR);
    hp = variable_get(VAR_HOSTNAME);
    /* Tack ourselves into /etc/hosts */
    fp = fopen("/etc/hosts", "w");
    if (!fp)
	return DITEM_FAILURE;
    /* Add an entry for localhost */
    if (dp) {
	fprintf(fp, "::1\t\t\tlocalhost localhost.%s\n", dp);
	fprintf(fp, "127.0.0.1\t\tlocalhost localhost.%s\n", dp);
    } else {
	fprintf(fp, "::1\t\t\tlocalhost\n");
	fprintf(fp, "127.0.0.1\t\tlocalhost\n");
    }
    /* Now the host entries, if applicable */
    if (((cp && cp[0] != '0') || (c6p && c6p[0] != '0')) && hp) {
	char cp2[255];

	if (!index(hp, '.'))
	    cp2[0] = '\0';
	else {
	    SAFE_STRCPY(cp2, hp);
	    *(index(cp2, '.')) = '\0';
	}
	if (c6p && c6p[0] != '0') {
	    fprintf(fp, "%s\t%s %s\n", c6p, hp, cp2);
	    fprintf(fp, "%s\t%s.\n", c6p, hp);
	}
	if (cp && cp[0] != '0') {
	    fprintf(fp, "%s\t\t%s %s\n", cp, hp, cp2);
	    fprintf(fp, "%s\t\t%s.\n", cp, hp);
	}
    }
    fclose(fp);
    if (isDebug())
	msgDebug("Wrote out /etc/hosts\n");
    return DITEM_SUCCESS;
}

int
configRouter(dialogMenuItem *self)
{
    int ret;

    ret = variable_get_value(VAR_ROUTER,
			     "Please specify the router you wish to use.  Routed is\n"
			     "provided with the stock system and gated is provided\n"
			     "as an optional package which this installation system\n"
			     "will attempt to load if you select gated.  Any other\n"
			     "choice of routing daemon will be assumed to be something\n"
			     "the user intends to install themselves before rebooting\n"
			     "the system.  If you don't want any routing daemon, choose NO", 1)
      ? DITEM_SUCCESS : DITEM_FAILURE;

    if (ret == DITEM_SUCCESS) {
	char *cp = variable_get(VAR_ROUTER);

	if (cp && strcmp(cp, "NO")) {
	    variable_set2(VAR_ROUTER_ENABLE, "YES", 1);
	    if (!strcmp(cp, "gated")) {
		if (package_add("gated") != DITEM_SUCCESS) {
		    msgConfirm("Unable to load gated package.  Falling back to no router.");
		    variable_unset(VAR_ROUTER);
		    variable_unset(VAR_ROUTERFLAGS);
		    variable_set2(VAR_ROUTER_ENABLE, "NO", 1);
		    cp = NULL;
		}
	    }
	    if (cp) {
		/* Now get the flags, if they chose a router */
		ret = variable_get_value(VAR_ROUTERFLAGS,
					 "Please Specify the routing daemon flags; if you're running routed\n"
					 "then -q is the right choice for nodes and -s for gateway hosts.\n", 1)
		  ? DITEM_SUCCESS : DITEM_FAILURE;
		if (ret != DITEM_SUCCESS)
		    variable_unset(VAR_ROUTERFLAGS);
	    }
	}
	else {
	    /* No router case */
	    variable_set2(VAR_ROUTER_ENABLE, "NO", 1);
	    variable_unset(VAR_ROUTERFLAGS);
	    variable_unset(VAR_ROUTER);
	}
    }
    else {
	variable_set2(VAR_ROUTER_ENABLE, "NO", 1);
	variable_unset(VAR_ROUTERFLAGS);
	variable_unset(VAR_ROUTER);
    }
    return ret;
}

/* Shared between us and index_initialize() */
extern PkgNode Top, Plist;

int
configPackages(dialogMenuItem *self)
{
    int i, restoreflag = 0;
    PkgNodePtr tmp;

    /* Did we get an INDEX? */
    i = index_initialize("packages/INDEX");
    if (DITEM_STATUS(i) == DITEM_FAILURE)
	return i;

    while (1) {
	int ret, pos, scroll;
	int current, low, high;

	/* Bring up the packages menu */
	pos = scroll = 0;
	index_menu(&Top, &Top, &Plist, &pos, &scroll);

	if (Plist.kids && Plist.kids->name) {
	    /* Now show the packing list menu */
	    pos = scroll = 0;
	    ret = index_menu(&Plist, &Plist, NULL, &pos, &scroll);
	    if (ret & DITEM_LEAVE_MENU)
		break;
	    else if (DITEM_STATUS(ret) != DITEM_FAILURE) {
		dialog_clear();
		restoreflag = 1;
		if (have_volumes) {
		    low = low_volume;
		    high = high_volume;
		} else
		    low = high = 0;
		for (current = low; current <= high; current++)
		    for (tmp = Plist.kids; tmp && tmp->name; tmp = tmp->next)
		        (void)index_extract(mediaDevice, &Top, tmp, FALSE, current);
		break;
	    }
	}
	else {
	    msgConfirm("No packages were selected for extraction.");
	    break;
	}
    }
    tmp = Plist.kids;
    while (tmp) {
        PkgNodePtr tmp2 = tmp->next;

        safe_free(tmp);
        tmp = tmp2;
    }
    index_init(NULL, &Plist);
    return DITEM_SUCCESS | (restoreflag ? DITEM_RESTORE : 0);
}

/* Load pcnfsd package */
int
configPCNFSD(dialogMenuItem *self)
{
    int ret;

    ret = package_add("pcnfsd");
    if (DITEM_STATUS(ret) == DITEM_SUCCESS) {
	variable_set2(VAR_PCNFSD, "YES", 0);
	variable_set2("mountd_flags", "-n", 1);
    }
    return ret;
}

int
configInetd(dialogMenuItem *self)
{
    char cmd[256];

    WINDOW *w = savescr();

    if (msgYesNo("The Internet Super Server (inetd) allows a number of simple Internet\n"
                 "services to be enabled, including finger, ftp, and telnetd.  Enabling\n"
                 "these services may increase risk of security problems by increasing\n"
                 "the exposure of your system.\n\n"
                 "With this in mind, do you wish to enable inetd?\n")) {
        variable_set2("inetd_enable", "NO", 1);
    } else {
        /* If inetd is enabled, we'll need an inetd.conf */
        variable_set2("inetd_enable", "YES", 1);
	if (!msgYesNo("inetd(8) relies on its configuration file, /etc/inetd.conf, to determine\n"
                   "which of its Internet services will be available.  The default FreeBSD\n"
                   "inetd.conf(5) leaves all services disabled by default, so they must be\n"
                   "specifically enabled in the configuration file before they will\n"
                   "function, even once inetd(8) is enabled.  Note that services for\n"
		   "IPv6 must be separately enabled from IPv4 services.\n\n"
                   "Select [Yes] now to invoke an editor on /etc/inetd.conf, or [No] to\n"
                   "use the current settings.\n")) {
            sprintf(cmd, "%s /etc/inetd.conf", variable_get(VAR_EDITOR));
            dialog_clear();
            systemExecute(cmd);
	}
    }
    restorescr(w);
    return DITEM_SUCCESS;
}

int
configNFSServer(dialogMenuItem *self)
{
    char cmd[256];
    int retval = 0;

    /* If we're an NFS server, we need an exports file */
    if (!file_readable("/etc/exports")) {
	WINDOW *w = savescr();

	if (file_readable("/etc/exports.disabled"))
	    vsystem("mv /etc/exports.disabled /etc/exports");
	else {
	    dialog_clear_norefresh();
	    msgConfirm("Operating as an NFS server means that you must first configure\n"
		       "an /etc/exports file to indicate which hosts are allowed certain\n"
		       "kinds of access to your local file systems.\n"
		       "Press [ENTER] now to invoke an editor on /etc/exports\n");
	    vsystem("echo '#The following examples export /usr to 3 machines named after ducks,' > /etc/exports");
	    vsystem("echo '#/usr/src and /usr/obj read-only to machines named after trouble makers,' >> /etc/exports");
	    vsystem("echo '#/home and all directories under it to machines named after dead rock stars' >> /etc/exports");
	    vsystem("echo '#and, /a to a network of privileged machines allowed to write on it as root.' >> /etc/exports");
	    vsystem("echo '#/usr                   huey louie dewie' >> /etc/exports");
	    vsystem("echo '#/usr/src /usr/obj -ro  calvin hobbes' >> /etc/exports");
	    vsystem("echo '#/home   -alldirs       janis jimi frank' >> /etc/exports");
	    vsystem("echo '#/a      -maproot=0  -network 10.0.1.0 -mask 255.255.248.0' >> /etc/exports");
	    vsystem("echo '#' >> /etc/exports");
	    vsystem("echo '# You should replace these lines with your actual exported filesystems.' >> /etc/exports");
	    vsystem("echo \"# Note that BSD's export syntax is 'host-centric' vs. Sun's 'FS-centric' one.\" >> /etc/exports");
	    vsystem("echo >> /etc/exports");
	    sprintf(cmd, "%s /etc/exports", variable_get(VAR_EDITOR));
	    dialog_clear();
	    systemExecute(cmd);
	}
	variable_set2(VAR_NFS_SERVER, "YES", 1);
	retval = configRpcBind(NULL);
	restorescr(w);
    }
    else if (variable_get(VAR_NFS_SERVER)) { /* We want to turn it off again? */
	vsystem("mv -f /etc/exports /etc/exports.disabled");
	variable_unset(VAR_NFS_SERVER);
    }
    return DITEM_SUCCESS | retval;
}

/*
 * Extend the standard dmenuToggleVariable() method to also check and set
 * the rpcbind variable if needed.
 */
int
configRpcBind(dialogMenuItem *self)
{
    char *tmp, *tmp2;
    int retval = 0;
    int doupdate = 1;

    if (self != NULL) {
    	retval = dmenuToggleVariable(self);
	tmp = strdup(self->data);
	if ((tmp2 = index(tmp, '=')) != NULL)
	    *tmp2 = '\0';
	if (strcmp(variable_get(tmp), "YES") != 0)
	    doupdate = 0;
	free(tmp);
    }

    if (doupdate && strcmp(variable_get(VAR_RPCBIND_ENABLE), "YES") != 0) {
	variable_set2(VAR_RPCBIND_ENABLE, "YES", 1);
	retval |= DITEM_REDRAW;
    }

   return retval;
}

int
configEtcTtys(dialogMenuItem *self)
{
    char cmd[256];

    WINDOW *w = savescr();

    /* Simply prompt for confirmation, then edit away. */
    if (msgYesNo("Configuration of system TTYs requires editing the /etc/ttys file.\n"
		 "Typical configuration activities might include enabling getty(8)\n"
		 "on the first serial port to allow login via serial console after\n"
		 "reboot, or to enable xdm.  The default ttys file enables normal\n"
		 "virtual consoles, and most sites will not need to perform manual\n"
		 "configuration.\n\n"
		 "To load /etc/ttys in the editor, select [Yes], otherwise, [No].")) {
    } else {
	configTtys();
	sprintf(cmd, "%s /etc/ttys", variable_get(VAR_EDITOR));
	dialog_clear();
	systemExecute(cmd);
    }

    restorescr(w);
    return DITEM_SUCCESS;
}

#ifdef __i386__
int
checkLoaderACPI(void)
{
    char val[4];

    if (kenv(KENV_GET, "loader.acpi_disabled_by_user", &val[0], 4) <= 0) {
	return (0);
    }

    if (strtol(&val[0], NULL, 10) <= 0) {
	return (0);
    }

    return (1);
}

int
configLoaderACPI(int disable)
{
    FILE *ldconf;

    ldconf = fopen("/boot/loader.conf", "a");
    if (ldconf == NULL) {
	msgConfirm("Unable to open /boot/loader.conf.  Please consult the\n"
		  "FreeBSD Handbook for instructions on disabling ACPI");
	return DITEM_FAILURE;
    }

    fprintf(ldconf, "# --- Generated by sysinstall ---\n");
    fprintf(ldconf, "hint.acpi.0.disabled=%d\n", disable);
    fclose(ldconf);

    return DITEM_SUCCESS;
}
#endif

int
configMTAPostfix(dialogMenuItem *self)
{
    int ret;
    FILE *perconf;

    if(setenv("POSTFIX_DEFAULT_MTA", "YES", 1) != 0)
	msgError("Error setting the enviroment variable POSTFIX_DEFAULT_MTA: %s (%u)",
		 strerror(errno), errno);

    ret = package_add("postfix-2.4");
    unsetenv("POSTFIX_DEFAULT_MTA");

    if(DITEM_STATUS(ret) == DITEM_FAILURE) {
	msgConfirm("An error occurred while adding the postfix package\n"
		   "Please change installation media and try again.");
	return ret;
    }

    variable_set2(VAR_SENDMAIL_ENABLE, "YES", 1);
    variable_set2("sendmail_flags", "-bd", 1);
    variable_set2("sendmail_outbound_enable", "NO", 1);
    variable_set2("sendmail_submit_enable", "NO", 1);
    variable_set2("sendmail_msp_queue_enable", "NO", 1);

    perconf = fopen("/etc/periodic.conf", "a");
    if (perconf == NULL) {
	msgConfirm("Unable to open /etc/periodic.conf.\n"
		   "The daily cleanup scripts might generate errors when\n"
		   "trying to run some sendmail only cleanup scripts.\n"
		   "Please consult the documentation for the postfix port on how to\n"
		   "fix this.");

	/* Not really a serious problem, so we return success */
	return DITEM_SUCCESS;
    }

    fprintf(perconf, "# --- Generated by sysinstall ---\n");
    fprintf(perconf, "daily_clean_hoststat_enable=\"NO\"\n");
    fprintf(perconf, "daily_status_mail_rejects_enable=\"NO\"\n");
    fprintf(perconf, "daily_status_include_submit_mailq=\"NO\"\n");
    fprintf(perconf, "daily_submit_queuerun=\"NO\"\n");
    fclose(perconf);

    msgConfirm("Postfix is now installed and enabled as the default MTA.\n"
	       "Please check that the configuration works as expected.\n"
	       "See the Postfix documentation for more information.\n"
	       "The documentation can be found in /usr/local/share/doc/postfix/\n"
	       "or on the Postfix website at http://www.postfix.org/.");

    return DITEM_SUCCESS;
}

int
configMTAExim(dialogMenuItem *self)
{
    int ret;
    FILE *perconf, *mailerconf, *newsyslogconf;

    ret = package_add("exim");

    if(DITEM_STATUS(ret) == DITEM_FAILURE) {
	msgConfirm("An error occurred while adding the exim package\n"
		   "Please change installation media and try again.");
	return ret;
    }

    variable_set2(VAR_SENDMAIL_ENABLE, "NONE", 1);
    variable_set2("exim_enable", "YES", 1);

    /* Update periodic.conf */
    perconf = fopen("/etc/periodic.conf", "a");
    if (perconf == NULL) {
	/* Not really a serious problem, so we do not abort */
	msgConfirm("Unable to open /etc/periodic.conf.\n"
		   "The daily cleanup scripts might generate errors when\n"
		   "trying to run some sendmail only cleanup scripts.\n"
		   "Please consult the documentation for the exim port on how to\n"
		   "fix this.");
    } else {
	fprintf(perconf, "# --- Generated by sysinstall ---\n");
	fprintf(perconf, "daily_clean_hoststat_enable=\"NO\"\n");
	fprintf(perconf, "daily_status_include_submit_mailq=\"NO\"\n");
	fprintf(perconf, "daily_status_mail_rejects_enable=\"NO\"\n");
	fprintf(perconf, "daily_submit_queuerun=\"NO\"\n");
	fclose(perconf);
    }

    /* Update mailer.conf */
    vsystem("mv -f /etc/mail/mailer.conf /etc/mail/mailer.conf.old");
    mailerconf = fopen("/etc/mail/mailer.conf", "w");
    if (mailerconf == NULL) {
	/* Not really a serious problem, so we do not abort */
	msgConfirm("Unable to open /etc/mailer.conf.\n"
		   "Some programs which use the sendmail wrappers may not work.\n"
		   "Please consult the documentation for the exim port on how\n"
		   "to correct this.");
    } else {
	fprintf(mailerconf, "# --- Generated by sysinstall ---\n");
	fprintf(mailerconf, "# Execute exim instead of sendmail\n");
	fprintf(mailerconf, "#\n");
	fprintf(mailerconf, "sendmail	/usr/local/sbin/exim\n");
	fprintf(mailerconf, "send-mail	/usr/local/sbin/exim\n");
	fprintf(mailerconf, "mailq		/usr/local/sbin/exim\n");
	fprintf(mailerconf, "newaliases	/usr/local/sbin/exim\n");
	fprintf(mailerconf, "hoststat	/usr/bin/true\n");
	fprintf(mailerconf, "purgestat	/usr/bin/true\n");
	fclose(mailerconf);
    }

    /* Make newsyslog rotate exim logfiles */
    newsyslogconf = fopen("/etc/newsyslog.conf", "a");
    if (newsyslogconf == NULL) {
	/* Not really a serious problem, so we do not abort */
	msgConfirm("Unable to open /etc/newsyslog.conf.\n"
		   "The exim logfiles will not be rotated.\n"
		   "Please consult the documentation for the exim port on how to\n"
		   "rotate the logfiles.");
    } else {
	fprintf(newsyslogconf, "# --- Generated by sysinstall ---\n");
	fprintf(newsyslogconf, "/var/log/exim/mainlog	mailnull:mail	640  7	   *	@T00  ZN\n");
	fprintf(newsyslogconf, "/var/log/exim/rejectlog	mailnull:mail	640  7	   *	@T00  ZN\n");
	fclose(newsyslogconf);
    }

    msgConfirm("Exim is now installed and enabled as the default MTA.\n"
	       "Please check that the configuration works as expected.\n"
	       "See the Exim documentation for more information.\n"
	       "The documentation can be found in /usr/local/share/doc/exim/\n"
	       "or on the Exim website at http://www.exim.org/.");

    return DITEM_SUCCESS;
}
