/*
 * Copyright (c) 1980, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Robert Elz at The University of Melbourne.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if 0
#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1980, 1990, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)quotacheck.c	8.3 (Berkeley) 1/29/94";
#endif /* not lint */
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sbin/quotacheck/quotacheck.c,v 1.25 2005/02/10 09:19:33 ru Exp $");

/*
 * Fix up / report on disk quotas & usage
 */
#include <sys/param.h>
#include <sys/disklabel.h>
#include <sys/stat.h>

#include <ufs/ufs/dinode.h>
#include <ufs/ufs/quota.h>
#include <ufs/ffs/fs.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fstab.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char *qfname = QUOTAFILENAME;
char *qfextension[] = INITQFNAMES;
char *quotagroup = QUOTAGROUP;

union {
	struct	fs	sblk;
	char	dummy[MAXBSIZE];
} sb_un;
#define	sblock	sb_un.sblk
union {
	struct	cg	cgblk;
	char	dummy[MAXBSIZE];
} cg_un;
#define	cgblk	cg_un.cgblk
long dev_bsize = 1;
ino_t maxino;

union dinode {
	struct ufs1_dinode dp1;
	struct ufs2_dinode dp2;
};
#define	DIP(dp, field) \
	((sblock.fs_magic == FS_UFS1_MAGIC) ? \
	(dp)->dp1.field : (dp)->dp2.field)

struct quotaname {
	long	flags;
	char	grpqfname[PATH_MAX];
	char	usrqfname[PATH_MAX];
};
#define	HASUSR	1
#define	HASGRP	2

struct fileusage {
	struct	fileusage *fu_next;
	u_long	fu_curinodes;
	u_long	fu_curblocks;
	u_long	fu_id;
	char	fu_name[1];
	/* actually bigger */
};
#define FUHASH 1024	/* must be power of two */
struct fileusage *fuhead[MAXQUOTAS][FUHASH];

int	aflag;			/* all file systems */
int	gflag;			/* check group quotas */
int	uflag;			/* check user quotas */
int	vflag;			/* verbose */
int	fi;			/* open disk file descriptor */
u_long	highid[MAXQUOTAS];	/* highest addid()'ed identifier per type */

struct fileusage *
	 addid(u_long, int, char *);
char	*blockcheck(char *);
void	 bread(ufs2_daddr_t, char *, long);
extern int checkfstab(int, int, void * (*)(struct fstab *),
				int (*)(char *, char *, struct quotaname *));
int	 chkquota(char *, char *, struct quotaname *);
void	 freeinodebuf(void);
union dinode *
	 getnextinode(ino_t);
int	 getquotagid(void);
int	 hasquota(struct fstab *, int, char **);
struct fileusage *
	 lookup(u_long, int);
void	*needchk(struct fstab *);
int	 oneof(char *, char*[], int);
void	 setinodebuf(ino_t);
int	 update(char *, char *, int);
void	 usage(void);

int
main(argc, argv)
	int argc;
	char *argv[];
{
	struct fstab *fs;
	struct passwd *pw;
	struct group *gr;
	struct quotaname *auxdata;
	int i, argnum, maxrun, errs, ch;
	long done = 0;
	char *name;

	errs = maxrun = 0;
	while ((ch = getopt(argc, argv, "aguvl:")) != -1) {
		switch(ch) {
		case 'a':
			aflag++;
			break;
		case 'g':
			gflag++;
			break;
		case 'u':
			uflag++;
			break;
		case 'v':
			vflag++;
			break;
		case 'l':
			maxrun = atoi(optarg);
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if ((argc == 0 && !aflag) || (argc > 0 && aflag))
		usage();
	if (!gflag && !uflag) {
		gflag++;
		uflag++;
	}
	if (gflag) {
		setgrent();
		while ((gr = getgrent()) != NULL)
			(void) addid((u_long)gr->gr_gid, GRPQUOTA, gr->gr_name);
		endgrent();
	}
	if (uflag) {
		setpwent();
		while ((pw = getpwent()) != NULL)
			(void) addid((u_long)pw->pw_uid, USRQUOTA, pw->pw_name);
		endpwent();
	}
	if (aflag)
		exit(checkfstab(1, maxrun, needchk, chkquota));
	if (setfsent() == 0)
		errx(1, "%s: can't open", FSTAB);
	while ((fs = getfsent()) != NULL) {
		if (((argnum = oneof(fs->fs_file, argv, argc)) >= 0 ||
		    (argnum = oneof(fs->fs_spec, argv, argc)) >= 0) &&
		    (auxdata = needchk(fs)) &&
		    (name = blockcheck(fs->fs_spec))) {
			done |= 1 << argnum;
			errs += chkquota(name, fs->fs_file, auxdata);
		}
	}
	endfsent();
	for (i = 0; i < argc; i++)
		if ((done & (1 << i)) == 0)
			fprintf(stderr, "%s not found in %s\n",
				argv[i], FSTAB);
	exit(errs);
}

void
usage()
{
	(void)fprintf(stderr, "%s\n%s\n", 
		"usage: quotacheck [-guv] -a",
		"       quotacheck [-guv] filesystem ...");
	exit(1);
}

void *
needchk(fs)
	struct fstab *fs;
{
	struct quotaname *qnp;
	char *qfnp;

	if (strcmp(fs->fs_vfstype, "ufs") ||
	    strcmp(fs->fs_type, FSTAB_RW))
		return (NULL);
	if ((qnp = malloc(sizeof(*qnp))) == NULL)
		errx(1, "malloc failed");
	qnp->flags = 0;
	if (gflag && hasquota(fs, GRPQUOTA, &qfnp)) {
		strcpy(qnp->grpqfname, qfnp);
		qnp->flags |= HASGRP;
	}
	if (uflag && hasquota(fs, USRQUOTA, &qfnp)) {
		strcpy(qnp->usrqfname, qfnp);
		qnp->flags |= HASUSR;
	}
	if (qnp->flags)
		return (qnp);
	free(qnp);
	return (NULL);
}

/*
 * Possible superblock locations ordered from most to least likely.
 */
static int sblock_try[] = SBLOCKSEARCH;

/*
 * Scan the specified file system to check quota(s) present on it.
 */
int
chkquota(fsname, mntpt, qnp)
	char *fsname, *mntpt;
	struct quotaname *qnp;
{
	struct fileusage *fup;
	union dinode *dp;
	int cg, i, mode, errs = 0;
	ino_t ino, inosused;
	char *cp;

	if ((fi = open(fsname, O_RDONLY, 0)) < 0) {
		warn("%s", fsname);
		return (1);
	}
	if (vflag) {
		(void)printf("*** Checking ");
		if (qnp->flags & HASUSR)
			(void)printf("%s%s", qfextension[USRQUOTA],
			    (qnp->flags & HASGRP) ? " and " : "");
		if (qnp->flags & HASGRP)
			(void)printf("%s", qfextension[GRPQUOTA]);
		(void)printf(" quotas for %s (%s)\n", fsname, mntpt);
	}
	sync();
	dev_bsize = 1;
	for (i = 0; sblock_try[i] != -1; i++) {
		bread(sblock_try[i], (char *)&sblock, (long)SBLOCKSIZE);
		if ((sblock.fs_magic == FS_UFS1_MAGIC ||
		     (sblock.fs_magic == FS_UFS2_MAGIC &&
		      sblock.fs_sblockloc == sblock_try[i])) &&
		    sblock.fs_bsize <= MAXBSIZE &&
		    sblock.fs_bsize >= sizeof(struct fs))
			break;
	}
	if (sblock_try[i] == -1) {
		warn("Cannot find file system superblock");
		return (1);
	}
	dev_bsize = sblock.fs_fsize / fsbtodb(&sblock, 1);
	maxino = sblock.fs_ncg * sblock.fs_ipg;
	for (cg = 0; cg < sblock.fs_ncg; cg++) {
		ino = cg * sblock.fs_ipg;
		setinodebuf(ino);
		bread(fsbtodb(&sblock, cgtod(&sblock, cg)), (char *)(&cgblk),
		    sblock.fs_cgsize);
		if (sblock.fs_magic == FS_UFS2_MAGIC)
			inosused = cgblk.cg_initediblk;
		else
			inosused = sblock.fs_ipg;
		/*
		 * If we are using soft updates, then we can trust the
		 * cylinder group inode allocation maps to tell us which
		 * inodes are allocated. We will scan the used inode map
		 * to find the inodes that are really in use, and then
		 * read only those inodes in from disk.
		 */
		if (sblock.fs_flags & FS_DOSOFTDEP) {
			if (!cg_chkmagic(&cgblk))
				errx(1, "CG %d: BAD MAGIC NUMBER\n", cg);
			cp = &cg_inosused(&cgblk)[(inosused - 1) / CHAR_BIT];
			for ( ; inosused > 0; inosused -= CHAR_BIT, cp--) {
				if (*cp == 0)
					continue;
				for (i = 1 << (CHAR_BIT - 1); i > 0; i >>= 1) {
					if (*cp & i)
						break;
					inosused--;
				}
				break;
			}
			if (inosused <= 0)
				continue;
		}
		for (i = 0; i < inosused; i++, ino++) {
			if ((dp = getnextinode(ino)) == NULL || ino < ROOTINO ||
			    (mode = DIP(dp, di_mode) & IFMT) == 0)
				continue;
			if (qnp->flags & HASGRP) {
				fup = addid((u_long)DIP(dp, di_gid), GRPQUOTA,
				    (char *)0);
				fup->fu_curinodes++;
				if (mode == IFREG || mode == IFDIR ||
				    mode == IFLNK)
					fup->fu_curblocks += DIP(dp, di_blocks);
			}
			if (qnp->flags & HASUSR) {
				fup = addid((u_long)DIP(dp, di_uid), USRQUOTA,
				    (char *)0);
				fup->fu_curinodes++;
				if (mode == IFREG || mode == IFDIR ||
				    mode == IFLNK)
					fup->fu_curblocks += DIP(dp, di_blocks);
			}
		}
	}
	freeinodebuf();
	if (qnp->flags & HASUSR)
		errs += update(mntpt, qnp->usrqfname, USRQUOTA);
	if (qnp->flags & HASGRP)
		errs += update(mntpt, qnp->grpqfname, GRPQUOTA);
	close(fi);
	return (errs);
}

/*
 * Update a specified quota file.
 */
int
update(fsname, quotafile, type)
	char *fsname, *quotafile;
	int type;
{
	struct fileusage *fup;
	FILE *qfi, *qfo;
	u_long id, lastid;
	off_t offset;
	struct dqblk dqbuf;
	static int warned = 0;
	static struct dqblk zerodqbuf;
	static struct fileusage zerofileusage;

	if ((qfo = fopen(quotafile, "r+")) == NULL) {
		if (errno == ENOENT)
			qfo = fopen(quotafile, "w+");
		if (qfo) {
			warnx("creating quota file %s", quotafile);
#define	MODE	(S_IRUSR|S_IWUSR|S_IRGRP)
			(void) fchown(fileno(qfo), getuid(), getquotagid());
			(void) fchmod(fileno(qfo), MODE);
		} else {
			warn("%s", quotafile);
			return (1);
		}
	}
	if ((qfi = fopen(quotafile, "r")) == NULL) {
		warn("%s", quotafile);
		(void) fclose(qfo);
		return (1);
	}
	if (quotactl(fsname, QCMD(Q_SYNC, type), (u_long)0, (caddr_t)0) < 0 &&
	    errno == EOPNOTSUPP && !warned && vflag) {
		warned++;
		(void)printf("*** Warning: %s\n",
		    "Quotas are not compiled into this kernel");
	}
	for (lastid = highid[type], id = 0, offset = 0; id <= lastid; 
	    id++, offset += sizeof(struct dqblk)) {
		if (fread((char *)&dqbuf, sizeof(struct dqblk), 1, qfi) == 0)
			dqbuf = zerodqbuf;
		if ((fup = lookup(id, type)) == 0)
			fup = &zerofileusage;
		if (dqbuf.dqb_curinodes == fup->fu_curinodes &&
		    dqbuf.dqb_curblocks == fup->fu_curblocks) {
			fup->fu_curinodes = 0;
			fup->fu_curblocks = 0;
			continue;
		}
		if (vflag) {
			if (aflag)
				printf("%s: ", fsname);
			printf("%-8s fixed:", fup->fu_name);
			if (dqbuf.dqb_curinodes != fup->fu_curinodes)
				(void)printf("\tinodes %lu -> %lu",
				    (u_long)dqbuf.dqb_curinodes,
				    (u_long)fup->fu_curinodes);
			if (dqbuf.dqb_curblocks != fup->fu_curblocks)
				(void)printf("\tblocks %lu -> %lu",
				    (u_long)dqbuf.dqb_curblocks,
				    (u_long)fup->fu_curblocks);
			(void)printf("\n");
		}
		/*
		 * Reset time limit if have a soft limit and were
		 * previously under it, but are now over it.
		 */
		if (dqbuf.dqb_bsoftlimit &&
		    dqbuf.dqb_curblocks < dqbuf.dqb_bsoftlimit &&
		    fup->fu_curblocks >= dqbuf.dqb_bsoftlimit)
			dqbuf.dqb_btime = 0;
		if (dqbuf.dqb_isoftlimit &&
		    dqbuf.dqb_curblocks < dqbuf.dqb_isoftlimit &&
		    fup->fu_curblocks >= dqbuf.dqb_isoftlimit)
			dqbuf.dqb_itime = 0;
		dqbuf.dqb_curinodes = fup->fu_curinodes;
		dqbuf.dqb_curblocks = fup->fu_curblocks;
		if (fseek(qfo, offset, SEEK_SET) < 0) {
			warn("%s: seek failed", quotafile);
			return(1);
		}
		fwrite((char *)&dqbuf, sizeof(struct dqblk), 1, qfo);
		(void) quotactl(fsname, QCMD(Q_SETUSE, type), id,
		    (caddr_t)&dqbuf);
		fup->fu_curinodes = 0;
		fup->fu_curblocks = 0;
	}
	fclose(qfi);
	fflush(qfo);
	ftruncate(fileno(qfo),
	    (((off_t)highid[type] + 1) * sizeof(struct dqblk)));
	fclose(qfo);
	return (0);
}

/*
 * Check to see if target appears in list of size cnt.
 */
int
oneof(target, list, cnt)
	char *target, *list[];
	int cnt;
{
	int i;

	for (i = 0; i < cnt; i++)
		if (strcmp(target, list[i]) == 0)
			return (i);
	return (-1);
}

/*
 * Determine the group identifier for quota files.
 */
int
getquotagid()
{
	struct group *gr;

	if ((gr = getgrnam(quotagroup)) != NULL)
		return (gr->gr_gid);
	return (-1);
}

/*
 * Check to see if a particular quota is to be enabled.
 */
int
hasquota(fs, type, qfnamep)
	struct fstab *fs;
	int type;
	char **qfnamep;
{
	char *opt;
	char *cp;
	static char initname, usrname[100], grpname[100];
	static char buf[BUFSIZ];

	if (!initname) {
		(void)snprintf(usrname, sizeof(usrname),
		    "%s%s", qfextension[USRQUOTA], qfname);
		(void)snprintf(grpname, sizeof(grpname),
		    "%s%s", qfextension[GRPQUOTA], qfname);
		initname = 1;
	}
	strcpy(buf, fs->fs_mntops);
	for (opt = strtok(buf, ","); opt; opt = strtok(NULL, ",")) {
		if ((cp = index(opt, '=')) != NULL)
			*cp++ = '\0';
		if (type == USRQUOTA && strcmp(opt, usrname) == 0)
			break;
		if (type == GRPQUOTA && strcmp(opt, grpname) == 0)
			break;
	}
	if (!opt)
		return (0);
	if (cp)
		*qfnamep = cp;
	else {
		(void)snprintf(buf, sizeof(buf),
		    "%s/%s.%s", fs->fs_file, qfname, qfextension[type]);
		*qfnamep = buf;
	}
	return (1);
}

/*
 * Routines to manage the file usage table.
 *
 * Lookup an id of a specific type.
 */
struct fileusage *
lookup(id, type)
	u_long id;
	int type;
{
	struct fileusage *fup;

	for (fup = fuhead[type][id & (FUHASH-1)]; fup != 0; fup = fup->fu_next)
		if (fup->fu_id == id)
			return (fup);
	return (NULL);
}

/*
 * Add a new file usage id if it does not already exist.
 */
struct fileusage *
addid(id, type, name)
	u_long id;
	int type;
	char *name;
{
	struct fileusage *fup, **fhp;
	int len;

	if ((fup = lookup(id, type)) != NULL)
		return (fup);
	if (name)
		len = strlen(name);
	else
		len = 10;
	if ((fup = calloc(1, sizeof(*fup) + len)) == NULL)
		errx(1, "calloc failed");
	fhp = &fuhead[type][id & (FUHASH - 1)];
	fup->fu_next = *fhp;
	*fhp = fup;
	fup->fu_id = id;
	if (id > highid[type])
		highid[type] = id;
	if (name)
		bcopy(name, fup->fu_name, len + 1);
	else {
		(void)sprintf(fup->fu_name, "%lu", id);
		if (vflag) 
			printf("unknown %cid: %lu\n", 
			    type == USRQUOTA ? 'u' : 'g', id);
	}
	return (fup);
}

/*
 * Special purpose version of ginode used to optimize pass
 * over all the inodes in numerical order.
 */
static ino_t nextino, lastinum, lastvalidinum;
static long readcnt, readpercg, fullcnt, inobufsize, partialcnt, partialsize;
static caddr_t inodebuf;
#define INOBUFSIZE	56*1024		/* size of buffer to read inodes */

union dinode *
getnextinode(ino_t inumber)
{
	long size;
	ufs2_daddr_t dblk;
	union dinode *dp;
	static caddr_t nextinop;

	if (inumber != nextino++ || inumber > lastvalidinum)
		errx(1, "bad inode number %d to nextinode", inumber);
	if (inumber >= lastinum) {
		readcnt++;
		dblk = fsbtodb(&sblock, ino_to_fsba(&sblock, lastinum));
		if (readcnt % readpercg == 0) {
			size = partialsize;
			lastinum += partialcnt;
		} else {
			size = inobufsize;
			lastinum += fullcnt;
		}
		/*
		 * If bread returns an error, it will already have zeroed
		 * out the buffer, so we do not need to do so here.
		 */
		bread(dblk, inodebuf, size);
		nextinop = inodebuf;
	}
	dp = (union dinode *)nextinop;
	if (sblock.fs_magic == FS_UFS1_MAGIC)
		nextinop += sizeof(struct ufs1_dinode);
	else
		nextinop += sizeof(struct ufs2_dinode);
	return (dp);
}

/*
 * Prepare to scan a set of inodes.
 */
void
setinodebuf(ino_t inum)
{

	if (inum % sblock.fs_ipg != 0)
		errx(1, "bad inode number %d to setinodebuf", inum);
	lastvalidinum = inum + sblock.fs_ipg - 1;
	nextino = inum;
	lastinum = inum;
	readcnt = 0;
	if (inodebuf != NULL)
		return;
	inobufsize = blkroundup(&sblock, INOBUFSIZE);
	fullcnt = inobufsize / ((sblock.fs_magic == FS_UFS1_MAGIC) ?
	    sizeof(struct ufs1_dinode) : sizeof(struct ufs2_dinode));
	readpercg = sblock.fs_ipg / fullcnt;
	partialcnt = sblock.fs_ipg % fullcnt;
	partialsize = partialcnt * ((sblock.fs_magic == FS_UFS1_MAGIC) ?
	    sizeof(struct ufs1_dinode) : sizeof(struct ufs2_dinode));
	if (partialcnt != 0) {
		readpercg++;
	} else {
		partialcnt = fullcnt;
		partialsize = inobufsize;
	}
	if ((inodebuf = malloc((unsigned)inobufsize)) == NULL)
		errx(1, "cannot allocate space for inode buffer");
}

/*
 * Free up data structures used to scan inodes.
 */
void
freeinodebuf()
{

	if (inodebuf != NULL)
		free(inodebuf);
	inodebuf = NULL;
}

/*
 * Read specified disk blocks.
 */
void
bread(bno, buf, cnt)
	ufs2_daddr_t bno;
	char *buf;
	long cnt;
{

	if (lseek(fi, (off_t)bno * dev_bsize, SEEK_SET) < 0 ||
	    read(fi, buf, cnt) != cnt)
		errx(1, "bread failed on block %ld", (long)bno);
}
