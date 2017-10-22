/*
 * Copyright (c) 1994 SigmaSoft, Th. Lockert <tholo@sigmasoft.com>
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/7.0.0/usr.bin/ipcs/ipcs.c 158587 2006-05-15 08:20:38Z maxim $");

#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <grp.h>
#include <kvm.h>
#include <nlist.h>
#include <limits.h>
#include <paths.h>
#include <pwd.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#define _KERNEL
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/msg.h>

/* SysCtlGatherStruct structure. */
struct scgs_vector {
	const char *sysctl;
	off_t offset;
	size_t size;
};

int	use_sysctl = 1;
struct semid_kernel	*sema;
struct seminfo	seminfo;
struct msginfo	msginfo;
struct msqid_kernel	*msqids;
struct shminfo	shminfo;
struct shmid_kernel	*shmsegs;

char   *fmt_perm(u_short);
void	cvt_time(time_t, char *);
void	sysctlgatherstruct(void *addr, size_t size, struct scgs_vector *vec);
void	kget(int idx, void *addr, size_t size);
void	usage(void);
uid_t	user2uid(char *username);

static struct nlist symbols[] = {
	{"sema"},
#define X_SEMA		0
	{"seminfo"},
#define X_SEMINFO	1
	{"msginfo"},
#define X_MSGINFO	2
	{"msqids"},
#define X_MSQIDS	3
	{"shminfo"},
#define X_SHMINFO	4
	{"shmsegs"},
#define X_SHMSEGS	5
	{NULL}
};

#define	SHMINFO_XVEC				\
X(shmmax, sizeof(u_long))				\
X(shmmin, sizeof(u_long))				\
X(shmmni, sizeof(u_long))				\
X(shmseg, sizeof(u_long))				\
X(shmall, sizeof(u_long))

#define	SEMINFO_XVEC				\
X(semmap, sizeof(int))				\
X(semmni, sizeof(int))				\
X(semmns, sizeof(int))				\
X(semmnu, sizeof(int))				\
X(semmsl, sizeof(int))				\
X(semopm, sizeof(int))				\
X(semume, sizeof(int))				\
X(semusz, sizeof(int))				\
X(semvmx, sizeof(int))				\
X(semaem, sizeof(int))

#define	MSGINFO_XVEC				\
X(msgmax, sizeof(int))				\
X(msgmni, sizeof(int))				\
X(msgmnb, sizeof(int))				\
X(msgtql, sizeof(int))				\
X(msgssz, sizeof(int))				\
X(msgseg, sizeof(int))

#define	X(a, b)	{ "kern.ipc." #a, offsetof(TYPEC, a), (b) },
#define	TYPEC	struct shminfo
struct scgs_vector shminfo_scgsv[] = { SHMINFO_XVEC { NULL } };
#undef	TYPEC
#define	TYPEC	struct seminfo
struct scgs_vector seminfo_scgsv[] = { SEMINFO_XVEC { NULL } };
#undef	TYPEC
#define	TYPEC	struct msginfo
struct scgs_vector msginfo_scgsv[] = { MSGINFO_XVEC { NULL } };
#undef	TYPEC
#undef	X

static kvm_t *kd;

char   *
fmt_perm(u_short mode)
{
	static char buffer[100];

	buffer[0] = '-';
	buffer[1] = '-';
	buffer[2] = ((mode & 0400) ? 'r' : '-');
	buffer[3] = ((mode & 0200) ? 'w' : '-');
	buffer[4] = ((mode & 0100) ? 'a' : '-');
	buffer[5] = ((mode & 0040) ? 'r' : '-');
	buffer[6] = ((mode & 0020) ? 'w' : '-');
	buffer[7] = ((mode & 0010) ? 'a' : '-');
	buffer[8] = ((mode & 0004) ? 'r' : '-');
	buffer[9] = ((mode & 0002) ? 'w' : '-');
	buffer[10] = ((mode & 0001) ? 'a' : '-');
	buffer[11] = '\0';
	return (&buffer[0]);
}

void
cvt_time(time_t t, char *buf)
{
	struct tm *tm;

	if (t == 0) {
		strcpy(buf, "no-entry");
	} else {
		tm = localtime(&t);
		sprintf(buf, "%2d:%02d:%02d",
			tm->tm_hour, tm->tm_min, tm->tm_sec);
	}
}
#define	SHMINFO		1
#define	SHMTOTAL	2
#define	MSGINFO		4
#define	MSGTOTAL	8
#define	SEMINFO		16
#define	SEMTOTAL	32

#define BIGGEST		1
#define CREATOR		2
#define OUTSTANDING	4
#define PID		8
#define TIME		16

int
main(int argc, char *argv[])
{
	int     display = SHMINFO | MSGINFO | SEMINFO;
	int     option = 0;
	char   *core = NULL, *user = NULL, *namelist = NULL;
	char	kvmoferr[_POSIX2_LINE_MAX];  /* Error buf for kvm_openfiles. */
	int     i;
	uid_t   uid;

	while ((i = getopt(argc, argv, "MmQqSsabC:cN:optTu:y")) != -1)
		switch (i) {
		case 'M':
			display = SHMTOTAL;
			break;
		case 'm':
			display = SHMINFO;
			break;
		case 'Q':
			display = MSGTOTAL;
			break;
		case 'q':
			display = MSGINFO;
			break;
		case 'S':
			display = SEMTOTAL;
			break;
		case 's':
			display = SEMINFO;
			break;
		case 'T':
			display = SHMTOTAL | MSGTOTAL | SEMTOTAL;
			break;
		case 'a':
			option |= BIGGEST | CREATOR | OUTSTANDING | PID | TIME;
			break;
		case 'b':
			option |= BIGGEST;
			break;
		case 'C':
			core = optarg;
			break;
		case 'c':
			option |= CREATOR;
			break;
		case 'N':
			namelist = optarg;
			break;
		case 'o':
			option |= OUTSTANDING;
			break;
		case 'p':
			option |= PID;
			break;
		case 't':
			option |= TIME;
			break;
		case 'y':
			use_sysctl = 0;
			break;
		case 'u':
			user = optarg;
			uid = user2uid(user);
			break;
		default:
			usage();
		}

	/*
	 * If paths to the exec file or core file were specified, we
	 * aren't operating on the running kernel, so we can't use
	 * sysctl.
	 */
	if (namelist != NULL || core != NULL)
		use_sysctl = 0;

	if (!use_sysctl) {
		kd = kvm_openfiles(namelist, core, NULL, O_RDONLY, kvmoferr);
		if (kd == NULL)
			errx(1, "kvm_openfiles: %s", kvmoferr);
		switch (kvm_nlist(kd, symbols)) {
		case 0:
			break;
		case -1:
			errx(1, "unable to read kernel symbol table");
		default:
#ifdef notdef		/* they'll be told more civilly later */
			warnx("nlist failed");
			for (i = 0; symbols[i].n_name != NULL; i++)
				if (symbols[i].n_value == 0)
					warnx("symbol %s not found",
					    symbols[i].n_name);
#endif
			break;
		}
	}

	kget(X_MSGINFO, &msginfo, sizeof(msginfo));
	if ((display & (MSGINFO | MSGTOTAL))) {
		if (display & MSGTOTAL) {
			printf("msginfo:\n");
			printf("\tmsgmax: %12d\t(max characters in a message)\n",
			    msginfo.msgmax);
			printf("\tmsgmni: %12d\t(# of message queues)\n",
			    msginfo.msgmni);
			printf("\tmsgmnb: %12d\t(max characters in a message queue)\n",
			    msginfo.msgmnb);
			printf("\tmsgtql: %12d\t(max # of messages in system)\n",
			    msginfo.msgtql);
			printf("\tmsgssz: %12d\t(size of a message segment)\n",
			    msginfo.msgssz);
			printf("\tmsgseg: %12d\t(# of message segments in system)\n\n",
			    msginfo.msgseg);
		}
		if (display & MSGINFO) {
			struct msqid_kernel *kxmsqids;
			size_t kxmsqids_len;


			kxmsqids_len = sizeof(struct msqid_kernel) * msginfo.msgmni;
			kxmsqids = malloc(kxmsqids_len);
			kget(X_MSQIDS, kxmsqids, kxmsqids_len);

			printf("Message Queues:\n");
			printf("T %12s %12s %-11s %-8s %-8s", "ID", "KEY", "MODE",
			    "OWNER", "GROUP");
			if (option & CREATOR)
				printf(" %-8s %-8s", "CREATOR", "CGROUP");
			if (option & OUTSTANDING)
				printf(" %20s %20s", "CBYTES", "QNUM");
			if (option & BIGGEST)
				printf(" %20s", "QBYTES");
			if (option & PID)
				printf(" %12s %12s", "LSPID", "LRPID");
			if (option & TIME)
				printf(" %-8s %-8s %-8s", "STIME", "RTIME", "CTIME");
			printf("\n");
			for (i = 0; i < msginfo.msgmni; i += 1) {
				if (kxmsqids[i].u.msg_qbytes != 0) {
					char    stime_buf[100], rtime_buf[100],
					        ctime_buf[100];
					struct msqid_kernel *kmsqptr = &kxmsqids[i];

					if (user)
						if (uid != kmsqptr->u.msg_perm.uid)
							continue;
					cvt_time(kmsqptr->u.msg_stime, stime_buf);
					cvt_time(kmsqptr->u.msg_rtime, rtime_buf);
					cvt_time(kmsqptr->u.msg_ctime, ctime_buf);

					printf("q %12d %12d %s %8s %8s",
					    IXSEQ_TO_IPCID(i, kmsqptr->u.msg_perm),
					    (int)kmsqptr->u.msg_perm.key,
					    fmt_perm(kmsqptr->u.msg_perm.mode),
					    user_from_uid(kmsqptr->u.msg_perm.uid, 0),
					    group_from_gid(kmsqptr->u.msg_perm.gid, 0));

					if (option & CREATOR)
						printf(" %8s %8s",
						    user_from_uid(kmsqptr->u.msg_perm.cuid, 0),
						    group_from_gid(kmsqptr->u.msg_perm.cgid, 0));

					if (option & OUTSTANDING)
						printf(" %12lu %12lu",
						    kmsqptr->u.msg_cbytes,
						    kmsqptr->u.msg_qnum);

					if (option & BIGGEST)
						printf(" %20lu",
						    kmsqptr->u.msg_qbytes);

					if (option & PID)
						printf(" %12d %12d",
						    kmsqptr->u.msg_lspid,
						    kmsqptr->u.msg_lrpid);

					if (option & TIME)
						printf(" %s %s %s",
						    stime_buf,
						    rtime_buf,
						    ctime_buf);

					printf("\n");
				}
			}
			printf("\n");
		}
	} else
		if (display & (MSGINFO | MSGTOTAL)) {
			fprintf(stderr,
			    "SVID messages facility not configured in the system\n");
		}

	kget(X_SHMINFO, &shminfo, sizeof(shminfo));
	if ((display & (SHMINFO | SHMTOTAL))) {
		if (display & SHMTOTAL) {
			printf("shminfo:\n");
			printf("\tshmmax: %12d\t(max shared memory segment size)\n",
			    shminfo.shmmax);
			printf("\tshmmin: %12d\t(min shared memory segment size)\n",
			    shminfo.shmmin);
			printf("\tshmmni: %12d\t(max number of shared memory identifiers)\n",
			    shminfo.shmmni);
			printf("\tshmseg: %12d\t(max shared memory segments per process)\n",
			    shminfo.shmseg);
			printf("\tshmall: %12d\t(max amount of shared memory in pages)\n\n",
			    shminfo.shmall);
		}
		if (display & SHMINFO) {
			struct shmid_kernel *kxshmids;
			size_t kxshmids_len;

			kxshmids_len = sizeof(struct shmid_kernel) * shminfo.shmmni;
			kxshmids = malloc(kxshmids_len);
			kget(X_SHMSEGS, kxshmids, kxshmids_len);

			printf("Shared Memory:\n");
			printf("T %12s %12s %-11s %-8s %-8s", "ID", "KEY", "MODE",
			    "OWNER", "GROUP");
			if (option & CREATOR)
				printf(" %-8s %-8s", "CREATOR", "CGROUP");
			if (option & OUTSTANDING)
				printf(" %12s", "NATTCH");
			if (option & BIGGEST)
				printf(" %12s", "SEGSZ");
			if (option & PID)
				printf(" %12s %12s", "CPID", "LPID");
			if (option & TIME)
				printf(" %-8s %-8s %-8s", "ATIME", "DTIME", "CTIME");
			printf("\n");
			for (i = 0; i < shminfo.shmmni; i += 1) {
				if (kxshmids[i].u.shm_perm.mode & 0x0800) {
					char    atime_buf[100], dtime_buf[100],
					        ctime_buf[100];
					struct shmid_kernel *kshmptr = &kxshmids[i];

					if (user)
						if (uid != kshmptr->u.shm_perm.uid)
							continue;
					cvt_time(kshmptr->u.shm_atime, atime_buf);
					cvt_time(kshmptr->u.shm_dtime, dtime_buf);
					cvt_time(kshmptr->u.shm_ctime, ctime_buf);

					printf("m %12d %12d %s %8s %8s",
					    IXSEQ_TO_IPCID(i, kshmptr->u.shm_perm),
					    (int)kshmptr->u.shm_perm.key,
					    fmt_perm(kshmptr->u.shm_perm.mode),
					    user_from_uid(kshmptr->u.shm_perm.uid, 0),
					    group_from_gid(kshmptr->u.shm_perm.gid, 0));

					if (option & CREATOR)
						printf(" %8s %8s",
						    user_from_uid(kshmptr->u.shm_perm.cuid, 0),
						    group_from_gid(kshmptr->u.shm_perm.cgid, 0));

					if (option & OUTSTANDING)
						printf(" %12d",
						    kshmptr->u.shm_nattch);

					if (option & BIGGEST)
						printf(" %12d",
						    kshmptr->u.shm_segsz);

					if (option & PID)
						printf(" %12d %12d",
						    kshmptr->u.shm_cpid,
						    kshmptr->u.shm_lpid);

					if (option & TIME)
						printf(" %s %s %s",
						    atime_buf,
						    dtime_buf,
						    ctime_buf);

					printf("\n");
				}
			}
			printf("\n");
		}
	} else
		if (display & (SHMINFO | SHMTOTAL)) {
			fprintf(stderr,
			    "SVID shared memory facility not configured in the system\n");
		}

	kget(X_SEMINFO, &seminfo, sizeof(seminfo));
	if ((display & (SEMINFO | SEMTOTAL))) {
		struct semid_kernel *kxsema;
		size_t kxsema_len;

		if (display & SEMTOTAL) {
			printf("seminfo:\n");
			printf("\tsemmap: %12d\t(# of entries in semaphore map)\n",
			    seminfo.semmap);
			printf("\tsemmni: %12d\t(# of semaphore identifiers)\n",
			    seminfo.semmni);
			printf("\tsemmns: %12d\t(# of semaphores in system)\n",
			    seminfo.semmns);
			printf("\tsemmnu: %12d\t(# of undo structures in system)\n",
			    seminfo.semmnu);
			printf("\tsemmsl: %12d\t(max # of semaphores per id)\n",
			    seminfo.semmsl);
			printf("\tsemopm: %12d\t(max # of operations per semop call)\n",
			    seminfo.semopm);
			printf("\tsemume: %12d\t(max # of undo entries per process)\n",
			    seminfo.semume);
			printf("\tsemusz: %12d\t(size in bytes of undo structure)\n",
			    seminfo.semusz);
			printf("\tsemvmx: %12d\t(semaphore maximum value)\n",
			    seminfo.semvmx);
			printf("\tsemaem: %12d\t(adjust on exit max value)\n\n",
			    seminfo.semaem);
		}
		if (display & SEMINFO) {
			kxsema_len = sizeof(struct semid_kernel) * seminfo.semmni;
			kxsema = malloc(kxsema_len);
			kget(X_SEMA, kxsema, kxsema_len);

			printf("Semaphores:\n");
			printf("T %12s %12s %-11s %-8s %-8s", "ID", "KEY", "MODE",
			    "OWNER", "GROUP");
			if (option & CREATOR)
				printf(" %-8s %-8s", "CREATOR", "CGROUP");
			if (option & BIGGEST)
				printf(" %12s", "NSEMS");
			if (option & TIME)
				printf(" %-8s %-8s", "OTIME", "CTIME");
			printf("\n");
			for (i = 0; i < seminfo.semmni; i += 1) {
				if ((kxsema[i].u.sem_perm.mode & SEM_ALLOC) != 0) {
					char    ctime_buf[100], otime_buf[100];
					struct semid_kernel *ksemaptr = &kxsema[i];

					if (user)
						if (uid != ksemaptr->u.sem_perm.uid)
							continue;
					cvt_time(ksemaptr->u.sem_otime, otime_buf);
					cvt_time(ksemaptr->u.sem_ctime, ctime_buf);

					printf("s %12d %12d %s %8s %8s",
					    IXSEQ_TO_IPCID(i, ksemaptr->u.sem_perm),
					    (int)ksemaptr->u.sem_perm.key,
					    fmt_perm(ksemaptr->u.sem_perm.mode),
					    user_from_uid(ksemaptr->u.sem_perm.uid, 0),
					    group_from_gid(ksemaptr->u.sem_perm.gid, 0));

					if (option & CREATOR)
						printf(" %8s %8s",
						    user_from_uid(ksemaptr->u.sem_perm.cuid, 0),
						    group_from_gid(ksemaptr->u.sem_perm.cgid, 0));

					if (option & BIGGEST)
						printf(" %12d",
						    ksemaptr->u.sem_nsems);

					if (option & TIME)
						printf(" %s %s",
						    otime_buf,
						    ctime_buf);

					printf("\n");
				}
			}

			printf("\n");
		}
	} else
		if (display & (SEMINFO | SEMTOTAL)) {
			fprintf(stderr, "SVID semaphores facility not configured in the system\n");
		}
	if (!use_sysctl)
		kvm_close(kd);

	exit(0);
}

void
sysctlgatherstruct(void *addr, size_t size, struct scgs_vector *vecarr)
{
	struct scgs_vector *xp;
	size_t tsiz;
	int rv;

	for (xp = vecarr; xp->sysctl != NULL; xp++) {
		assert(xp->offset <= size);
		tsiz = xp->size;
		rv = sysctlbyname(xp->sysctl, (char *)addr + xp->offset,
		    &tsiz, NULL, 0);
		if (rv == -1)
			err(1, "sysctlbyname: %s", xp->sysctl);
		if (tsiz != xp->size)
			errx(1, "%s size mismatch (expected %d, got %d)",
			    xp->sysctl, xp->size, tsiz);
	}
}

void
kget(int idx, void *addr, size_t size)
{
	char *symn;			/* symbol name */
	size_t tsiz;
	int rv;
	unsigned long kaddr;
	const char *sym2sysctl[] = {	/* symbol to sysctl name table */
		"kern.ipc.sema",
		"kern.ipc.seminfo",
		"kern.ipc.msginfo",
		"kern.ipc.msqids",
		"kern.ipc.shminfo",
		"kern.ipc.shmsegs" };

	assert((unsigned)idx <= sizeof(sym2sysctl) / sizeof(*sym2sysctl));
	if (!use_sysctl) {
		symn = symbols[idx].n_name;
		if (*symn == '_')
			symn++;
		if (symbols[idx].n_type == 0 || symbols[idx].n_value == 0)
			errx(1, "symbol %s undefined", symn);
		/*
		 * For some symbols, the value we retrieve is
		 * actually a pointer; since we want the actual value,
		 * we have to manually dereference it.
		 */
		switch (idx) {
		case X_MSQIDS:
			tsiz = sizeof(msqids);
			rv = kvm_read(kd, symbols[idx].n_value,
			    &msqids, tsiz);
			kaddr = (u_long)msqids;
			break;
		case X_SHMSEGS:
			tsiz = sizeof(shmsegs);
			rv = kvm_read(kd, symbols[idx].n_value,
			    &shmsegs, tsiz);
			kaddr = (u_long)shmsegs;
			break;
		case X_SEMA:
			tsiz = sizeof(sema);
			rv = kvm_read(kd, symbols[idx].n_value,
			    &sema, tsiz);
			kaddr = (u_long)sema;
			break;
		default:
			rv = tsiz = 0;
			kaddr = symbols[idx].n_value;
			break;
		}
		if ((unsigned)rv != tsiz)
			errx(1, "%s: %s", symn, kvm_geterr(kd));
		if ((unsigned)kvm_read(kd, kaddr, addr, size) != size)
			errx(1, "%s: %s", symn, kvm_geterr(kd));
	} else {
		switch (idx) {
		case X_SHMINFO:
			sysctlgatherstruct(addr, size, shminfo_scgsv);
			break;
		case X_SEMINFO:
			sysctlgatherstruct(addr, size, seminfo_scgsv);
			break;
		case X_MSGINFO:
			sysctlgatherstruct(addr, size, msginfo_scgsv);
			break;
		default:
			tsiz = size;
			rv = sysctlbyname(sym2sysctl[idx], addr, &tsiz,
			    NULL, 0);
			if (rv == -1)
				err(1, "sysctlbyname: %s", sym2sysctl[idx]);
			if (tsiz != size)
				errx(1, "%s size mismatch "
				    "(expected %d, got %d)",
				    sym2sysctl[idx], size, tsiz);
			break;
		}
	}
}

uid_t 
user2uid(char *username)
{
	struct passwd *pwd;
	uid_t uid;
	char *r;

	uid = strtoul(username, &r, 0);
	if (!*r && r != username)
		return (uid);
	if ((pwd = getpwnam(username)) == NULL)
		errx(1, "getpwnam failed: No such user");
	endpwent();
	return (pwd->pw_uid);
}

void
usage(void)
{

	fprintf(stderr,
	    "usage: ipcs [-abcmopqstyMQST] [-C corefile] [-N namelist] [-u user]\n");
	exit(1);
}
