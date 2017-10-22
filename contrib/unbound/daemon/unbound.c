/*
 * daemon/unbound.c - main program for unbound DNS resolver daemon.
 *
 * Copyright (c) 2007, NLnet Labs. All rights reserved.
 *
 * This software is open source.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 
 * Neither the name of the NLNET LABS nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

/**
 * \file
 *
 * Main program to start the DNS resolver daemon.
 */

#include "config.h"
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#include <sys/time.h>
#include "util/log.h"
#include "daemon/daemon.h"
#include "daemon/remote.h"
#include "util/config_file.h"
#include "util/storage/slabhash.h"
#include "services/listen_dnsport.h"
#include "services/cache/rrset.h"
#include "services/cache/infra.h"
#include "util/fptr_wlist.h"
#include "util/data/msgreply.h"
#include "util/module.h"
#include "util/net_help.h"
#include <signal.h>
#include <fcntl.h>
#include <openssl/crypto.h>
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
#ifdef HAVE_GRP_H
#include <grp.h>
#endif

#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif
#ifdef HAVE_LOGIN_CAP_H
#include <login_cap.h>
#endif

#ifdef USE_MINI_EVENT
#  ifdef USE_WINSOCK
#    include "util/winsock_event.h"
#  else
#    include "util/mini_event.h"
#  endif
#else
#  include <event.h>
#endif

#ifdef UB_ON_WINDOWS
#  include "winrc/win_svc.h"
#endif

#ifdef HAVE_NSS
/* nss3 */
#  include "nss.h"
#endif

#ifdef HAVE_SBRK
/** global debug value to keep track of heap memory allocation */
void* unbound_start_brk = 0;
#endif

#if !defined(HAVE_EVENT_BASE_GET_METHOD) && (defined(HAVE_EV_LOOP) || defined(HAVE_EV_DEFAULT_LOOP))
static const char* ev_backend2str(int b)
{
	switch(b) {
	case EVBACKEND_SELECT:	return "select";
	case EVBACKEND_POLL:	return "poll";
	case EVBACKEND_EPOLL:	return "epoll";
	case EVBACKEND_KQUEUE:	return "kqueue";
	case EVBACKEND_DEVPOLL: return "devpoll";
	case EVBACKEND_PORT:	return "evport";
	}
	return "unknown";
}
#endif

/** get the event system in use */
static void get_event_sys(const char** n, const char** s, const char** m)
{
#ifdef USE_WINSOCK
	*n = "event";
	*s = "winsock";
	*m = "WSAWaitForMultipleEvents";
#elif defined(USE_MINI_EVENT)
	*n = "mini-event";
	*s = "internal";
	*m = "select";
#else
	struct event_base* b;
	*s = event_get_version();
#  ifdef HAVE_EVENT_BASE_GET_METHOD
	*n = "libevent";
	b = event_base_new();
	*m = event_base_get_method(b);
#  elif defined(HAVE_EV_LOOP) || defined(HAVE_EV_DEFAULT_LOOP)
	*n = "libev";
	b = (struct event_base*)ev_default_loop(EVFLAG_AUTO);
	*m = ev_backend2str(ev_backend((struct ev_loop*)b));
#  else
	*n = "unknown";
	*m = "not obtainable";
	b = NULL;
#  endif
#  ifdef HAVE_EVENT_BASE_FREE
	event_base_free(b);
#  endif
#endif
}

/** print usage. */
static void usage()
{
	const char** m;
	const char *evnm="event", *evsys="", *evmethod="";
	printf("usage:  unbound [options]\n");
	printf("	start unbound daemon DNS resolver.\n");
	printf("-h	this help\n");
	printf("-c file	config file to read instead of %s\n", CONFIGFILE);
	printf("	file format is described in unbound.conf(5).\n");
	printf("-d	do not fork into the background.\n");
	printf("-v	verbose (more times to increase verbosity)\n");
#ifdef UB_ON_WINDOWS
	printf("-w opt	windows option: \n");
	printf("   	install, remove - manage the services entry\n");
	printf("   	service - used to start from services control panel\n");
#endif
	printf("Version %s\n", PACKAGE_VERSION);
	get_event_sys(&evnm, &evsys, &evmethod);
	printf("linked libs: %s %s (it uses %s), ldns %s, %s\n", 
		evnm, evsys, evmethod, ldns_version(), 
#ifdef HAVE_SSL
		SSLeay_version(SSLEAY_VERSION)
#elif defined(HAVE_NSS)
		NSS_GetVersion()
#endif
		);
	printf("linked modules:");
	for(m = module_list_avail(); *m; m++)
		printf(" %s", *m);
	printf("\n");
	printf("BSD licensed, see LICENSE in source package for details.\n");
	printf("Report bugs to %s\n", PACKAGE_BUGREPORT);
}

#ifndef unbound_testbound
int replay_var_compare(const void* ATTR_UNUSED(a), const void* ATTR_UNUSED(b))
{
        log_assert(0);
        return 0;
}
#endif

/** check file descriptor count */
static void
checkrlimits(struct config_file* cfg)
{
#ifdef HAVE_GETRLIMIT
	/* list has number of ports to listen to, ifs number addresses */
	int list = ((cfg->do_udp?1:0) + (cfg->do_tcp?1 + 
			(int)cfg->incoming_num_tcp:0));
	size_t listen_ifs = (size_t)(cfg->num_ifs==0?
		((cfg->do_ip4 && !cfg->if_automatic?1:0) + 
		 (cfg->do_ip6?1:0)):cfg->num_ifs);
	size_t listen_num = list*listen_ifs;
	size_t outudpnum = (size_t)cfg->outgoing_num_ports;
	size_t outtcpnum = cfg->outgoing_num_tcp;
	size_t misc = 4; /* logfile, pidfile, stdout... */
	size_t perthread_noudp = listen_num + outtcpnum + 
		2/*cmdpipe*/ + 2/*libevent*/ + misc; 
	size_t perthread = perthread_noudp + outudpnum;

#if !defined(HAVE_PTHREAD) && !defined(HAVE_SOLARIS_THREADS)
	int numthread = 1; /* it forks */
#else
	int numthread = (cfg->num_threads?cfg->num_threads:1);
#endif
	size_t total = numthread * perthread + misc;
	size_t avail;
	struct rlimit rlim;

	if(total > 1024 && 
		strncmp(event_get_version(), "mini-event", 10) == 0) {
		log_warn("too many file descriptors requested. The builtin"
			"mini-event cannot handle more than 1024. Config "
			"for less fds or compile with libevent");
		if(numthread*perthread_noudp+15 > 1024)
			fatal_exit("too much tcp. not enough fds.");
		cfg->outgoing_num_ports = (int)((1024 
			- numthread*perthread_noudp 
			- 10 /* safety margin */) /numthread);
		log_warn("continuing with less udp ports: %u",
			cfg->outgoing_num_ports);
		total = 1024;
	}
	if(perthread > 64 && 
		strncmp(event_get_version(), "winsock-event", 13) == 0) {
		log_err("too many file descriptors requested. The winsock"
			" event handler cannot handle more than 64 per "
			" thread. Config for less fds");
		if(perthread_noudp+2 > 64)
			fatal_exit("too much tcp. not enough fds.");
		cfg->outgoing_num_ports = (int)((64 
			- perthread_noudp 
			- 2/* safety margin */));
		log_warn("continuing with less udp ports: %u",
			cfg->outgoing_num_ports);
		total = numthread*(perthread_noudp+
			(size_t)cfg->outgoing_num_ports)+misc;
	}
	if(getrlimit(RLIMIT_NOFILE, &rlim) < 0) {
		log_warn("getrlimit: %s", strerror(errno));
		return;
	}
	if(rlim.rlim_cur == (rlim_t)RLIM_INFINITY)
		return;
	if((size_t)rlim.rlim_cur < total) {
		avail = (size_t)rlim.rlim_cur;
		rlim.rlim_cur = (rlim_t)(total + 10);
		rlim.rlim_max = (rlim_t)(total + 10);
#ifdef HAVE_SETRLIMIT
		if(setrlimit(RLIMIT_NOFILE, &rlim) < 0) {
			log_warn("setrlimit: %s", strerror(errno));
#else
		if(1) {
#endif
			log_warn("cannot increase max open fds from %u to %u",
				(unsigned)avail, (unsigned)total+10);
			/* check that calculation below does not underflow,
			 * with 15 as margin */
			if(numthread*perthread_noudp+15 > avail)
				fatal_exit("too much tcp. not enough fds.");
			cfg->outgoing_num_ports = (int)((avail 
				- numthread*perthread_noudp 
				- 10 /* safety margin */) /numthread);
			log_warn("continuing with less udp ports: %u",
				cfg->outgoing_num_ports);
			log_warn("increase ulimit or decrease threads, "
				"ports in config to remove this warning");
			return;
		}
		log_warn("increased limit(open files) from %u to %u",
			(unsigned)avail, (unsigned)total+10);
	}
#else	
	(void)cfg;
#endif /* HAVE_GETRLIMIT */
}

/** set verbosity, check rlimits, cache settings */
static void
apply_settings(struct daemon* daemon, struct config_file* cfg, 
	int cmdline_verbose)
{
	/* apply if they have changed */
	verbosity = cmdline_verbose + cfg->verbosity;
	daemon_apply_cfg(daemon, cfg);
	checkrlimits(cfg);
}

#ifdef HAVE_KILL
/** Read existing pid from pidfile. 
 * @param file: file name of pid file.
 * @return: the pid from the file or -1 if none.
 */
static pid_t
readpid (const char* file)
{
	int fd;
	pid_t pid;
	char pidbuf[32];
	char* t;
	ssize_t l;

	if ((fd = open(file, O_RDONLY)) == -1) {
		if(errno != ENOENT)
			log_err("Could not read pidfile %s: %s",
				file, strerror(errno));
		return -1;
	}

	if (((l = read(fd, pidbuf, sizeof(pidbuf)))) == -1) {
		if(errno != ENOENT)
			log_err("Could not read pidfile %s: %s",
				file, strerror(errno));
		close(fd);
		return -1;
	}

	close(fd);

	/* Empty pidfile means no pidfile... */
	if (l == 0) {
		return -1;
	}

	pidbuf[sizeof(pidbuf)-1] = 0;
	pid = (pid_t)strtol(pidbuf, &t, 10);
	
	if (*t && *t != '\n') {
		return -1;
	}
	return pid;
}

/** write pid to file. 
 * @param pidfile: file name of pid file.
 * @param pid: pid to write to file.
 */
static void
writepid (const char* pidfile, pid_t pid)
{
	FILE* f;

	if ((f = fopen(pidfile, "w")) ==  NULL ) {
		log_err("cannot open pidfile %s: %s", 
			pidfile, strerror(errno));
		return;
	}
	if(fprintf(f, "%lu\n", (unsigned long)pid) < 0) {
		log_err("cannot write to pidfile %s: %s", 
			pidfile, strerror(errno));
	}
	fclose(f);
}

/**
 * check old pid file.
 * @param pidfile: the file name of the pid file.
 * @param inchroot: if pidfile is inchroot and we can thus expect to
 *	be able to delete it.
 */
static void
checkoldpid(char* pidfile, int inchroot)
{
	pid_t old;
	if((old = readpid(pidfile)) != -1) {
		/* see if it is still alive */
		if(kill(old, 0) == 0 || errno == EPERM)
			log_warn("unbound is already running as pid %u.", 
				(unsigned)old);
		else	if(inchroot)
			log_warn("did not exit gracefully last time (%u)", 
				(unsigned)old);
	}
}
#endif /* HAVE_KILL */

/** detach from command line */
static void
detach(void)
{
#if defined(HAVE_DAEMON) && !defined(DEPRECATED_DAEMON)
	/* use POSIX daemon(3) function */
	if(daemon(1, 0) != 0)
		fatal_exit("daemon failed: %s", strerror(errno));
#else /* no HAVE_DAEMON */
#ifdef HAVE_FORK
	int fd;
	/* Take off... */
	switch (fork()) {
		case 0:
			break;
		case -1:
			fatal_exit("fork failed: %s", strerror(errno));
		default:
			/* exit interactive session */
			exit(0);
	}
	/* detach */
#ifdef HAVE_SETSID
	if(setsid() == -1)
		fatal_exit("setsid() failed: %s", strerror(errno));
#endif
	if ((fd = open("/dev/null", O_RDWR, 0)) != -1) {
		(void)dup2(fd, STDIN_FILENO);
		(void)dup2(fd, STDOUT_FILENO);
		(void)dup2(fd, STDERR_FILENO);
		if (fd > 2)
			(void)close(fd);
	}
#endif /* HAVE_FORK */
#endif /* HAVE_DAEMON */
}

/** daemonize, drop user priviliges and chroot if needed */
static void
perform_setup(struct daemon* daemon, struct config_file* cfg, int debug_mode,
	const char** cfgfile)
{
#ifdef HAVE_GETPWNAM
	struct passwd *pwd = NULL;
	uid_t uid;
	gid_t gid;
	/* initialize, but not to 0 (root) */
	memset(&uid, 112, sizeof(uid));
	memset(&gid, 112, sizeof(gid));
	log_assert(cfg);

	if(cfg->username && cfg->username[0]) {
		if((pwd = getpwnam(cfg->username)) == NULL)
			fatal_exit("user '%s' does not exist.", cfg->username);
		uid = pwd->pw_uid;
		gid = pwd->pw_gid;
		/* endpwent below, in case we need pwd for setusercontext */
	}
#endif

	/* init syslog (as root) if needed, before daemonize, otherwise
	 * a fork error could not be printed since daemonize closed stderr.*/
	if(cfg->use_syslog) {
		log_init(cfg->logfile, cfg->use_syslog, cfg->chrootdir);
	}
	/* if using a logfile, we cannot open it because the logfile would
	 * be created with the wrong permissions, we cannot chown it because
	 * we cannot chown system logfiles, so we do not open at all.
	 * So, using a logfile, the user does not see errors unless -d is
	 * given to unbound on the commandline. */

	/* read ssl keys while superuser and outside chroot */
#ifdef HAVE_SSL
	if(!(daemon->rc = daemon_remote_create(cfg)))
		fatal_exit("could not set up remote-control");
	if(cfg->ssl_service_key && cfg->ssl_service_key[0]) {
		if(!(daemon->listen_sslctx = listen_sslctx_create(
			cfg->ssl_service_key, cfg->ssl_service_pem, NULL)))
			fatal_exit("could not set up listen SSL_CTX");
	}
	if(!(daemon->connect_sslctx = connect_sslctx_create(NULL, NULL, NULL)))
		fatal_exit("could not set up connect SSL_CTX");
#endif

#ifdef HAVE_KILL
	/* check old pid file before forking */
	if(cfg->pidfile && cfg->pidfile[0]) {
		/* calculate position of pidfile */
		if(cfg->pidfile[0] == '/')
			daemon->pidfile = strdup(cfg->pidfile);
		else	daemon->pidfile = fname_after_chroot(cfg->pidfile, 
				cfg, 1);
		if(!daemon->pidfile)
			fatal_exit("pidfile alloc: out of memory");
		checkoldpid(daemon->pidfile,
			/* true if pidfile is inside chrootdir, or nochroot */
			!(cfg->chrootdir && cfg->chrootdir[0]) ||
			(cfg->chrootdir && cfg->chrootdir[0] &&
			strncmp(daemon->pidfile, cfg->chrootdir,
				strlen(cfg->chrootdir))==0));
	}
#endif

	/* daemonize because pid is needed by the writepid func */
	if(!debug_mode && cfg->do_daemonize) {
		detach();
	}

	/* write new pidfile (while still root, so can be outside chroot) */
#ifdef HAVE_KILL
	if(cfg->pidfile && cfg->pidfile[0]) {
		writepid(daemon->pidfile, getpid());
		if(!(cfg->chrootdir && cfg->chrootdir[0]) || 
			(cfg->chrootdir && cfg->chrootdir[0] && 
			strncmp(daemon->pidfile, cfg->chrootdir, 
			strlen(cfg->chrootdir))==0)) {
			/* delete of pidfile could potentially work,
			 * chown to get permissions */
			if(cfg->username && cfg->username[0]) {
			  if(chown(daemon->pidfile, uid, gid) == -1) {
				log_err("cannot chown %u.%u %s: %s",
					(unsigned)uid, (unsigned)gid,
					daemon->pidfile, strerror(errno));
			  }
			}
		}
	}
#else
	(void)daemon;
#endif

	/* Set user context */
#ifdef HAVE_GETPWNAM
	if(cfg->username && cfg->username[0]) {
#ifdef HAVE_SETUSERCONTEXT
		/* setusercontext does initgroups, setuid, setgid, and
		 * also resource limits from login config, but we
		 * still call setresuid, setresgid to be sure to set all uid*/
		if(setusercontext(NULL, pwd, uid,
			LOGIN_SETALL & ~LOGIN_SETUSER & ~LOGIN_SETGROUP) != 0)
			log_warn("unable to setusercontext %s: %s",
				cfg->username, strerror(errno));
#endif /* HAVE_SETUSERCONTEXT */
	}
#endif /* HAVE_GETPWNAM */

	/* box into the chroot */
#ifdef HAVE_CHROOT
	if(cfg->chrootdir && cfg->chrootdir[0]) {
		if(chdir(cfg->chrootdir)) {
			fatal_exit("unable to chdir to chroot %s: %s",
				cfg->chrootdir, strerror(errno));
		}
		verbose(VERB_QUERY, "chdir to %s", cfg->chrootdir);
		if(chroot(cfg->chrootdir))
			fatal_exit("unable to chroot to %s: %s", 
				cfg->chrootdir, strerror(errno));
		if(chdir("/"))
			fatal_exit("unable to chdir to / in chroot %s: %s",
				cfg->chrootdir, strerror(errno));
		verbose(VERB_QUERY, "chroot to %s", cfg->chrootdir);
		if(strncmp(*cfgfile, cfg->chrootdir, 
			strlen(cfg->chrootdir)) == 0) 
			(*cfgfile) += strlen(cfg->chrootdir);

		/* adjust stored pidfile for chroot */
		if(daemon->pidfile && daemon->pidfile[0] && 
			strncmp(daemon->pidfile, cfg->chrootdir,
			strlen(cfg->chrootdir))==0) {
			char* old = daemon->pidfile;
			daemon->pidfile = strdup(old+strlen(cfg->chrootdir));
			free(old);
			if(!daemon->pidfile)
				log_err("out of memory in pidfile adjust");
		}
		daemon->chroot = strdup(cfg->chrootdir);
		if(!daemon->chroot)
			log_err("out of memory in daemon chroot dir storage");
	}
#else
	(void)cfgfile;
#endif
	/* change to working directory inside chroot */
	if(cfg->directory && cfg->directory[0]) {
		char* dir = cfg->directory;
		if(cfg->chrootdir && cfg->chrootdir[0] &&
			strncmp(dir, cfg->chrootdir, 
			strlen(cfg->chrootdir)) == 0)
			dir += strlen(cfg->chrootdir);
		if(dir[0]) {
			if(chdir(dir)) {
				fatal_exit("Could not chdir to %s: %s",
					dir, strerror(errno));
			}
			verbose(VERB_QUERY, "chdir to %s", dir);
		}
	}

	/* drop permissions after chroot, getpwnam, pidfile, syslog done*/
#ifdef HAVE_GETPWNAM
	if(cfg->username && cfg->username[0]) {
#  ifdef HAVE_INITGROUPS
		if(initgroups(cfg->username, gid) != 0)
			log_warn("unable to initgroups %s: %s",
				cfg->username, strerror(errno));
#  endif /* HAVE_INITGROUPS */
		endpwent();

#ifdef HAVE_SETRESGID
		if(setresgid(gid,gid,gid) != 0)
#elif defined(HAVE_SETREGID) && !defined(DARWIN_BROKEN_SETREUID)
		if(setregid(gid,gid) != 0)
#else /* use setgid */
		if(setgid(gid) != 0)
#endif /* HAVE_SETRESGID */
			fatal_exit("unable to set group id of %s: %s", 
				cfg->username, strerror(errno));
#ifdef HAVE_SETRESUID
		if(setresuid(uid,uid,uid) != 0)
#elif defined(HAVE_SETREUID) && !defined(DARWIN_BROKEN_SETREUID)
		if(setreuid(uid,uid) != 0)
#else /* use setuid */
		if(setuid(uid) != 0)
#endif /* HAVE_SETRESUID */
			fatal_exit("unable to set user id of %s: %s", 
				cfg->username, strerror(errno));
		verbose(VERB_QUERY, "drop user privileges, run as %s", 
			cfg->username);
	}
#endif /* HAVE_GETPWNAM */
	/* file logging inited after chroot,chdir,setuid is done so that 
	 * it would succeed on SIGHUP as well */
	if(!cfg->use_syslog)
		log_init(cfg->logfile, cfg->use_syslog, cfg->chrootdir);
}

/**
 * Run the daemon. 
 * @param cfgfile: the config file name.
 * @param cmdline_verbose: verbosity resulting from commandline -v.
 *    These increase verbosity as specified in the config file.
 * @param debug_mode: if set, do not daemonize.
 */
static void 
run_daemon(const char* cfgfile, int cmdline_verbose, int debug_mode)
{
	struct config_file* cfg = NULL;
	struct daemon* daemon = NULL;
	int done_setup = 0;

	if(!(daemon = daemon_init()))
		fatal_exit("alloc failure");
	while(!daemon->need_to_exit) {
		if(done_setup)
			verbose(VERB_OPS, "Restart of %s.", PACKAGE_STRING);
		else	verbose(VERB_OPS, "Start of %s.", PACKAGE_STRING);

		/* config stuff */
		if(!(cfg = config_create()))
			fatal_exit("Could not alloc config defaults");
		if(!config_read(cfg, cfgfile, daemon->chroot)) {
			if(errno != ENOENT)
				fatal_exit("Could not read config file: %s",
					cfgfile);
			log_warn("Continuing with default config settings");
		}
		apply_settings(daemon, cfg, cmdline_verbose);
	
		/* prepare */
		if(!daemon_open_shared_ports(daemon))
			fatal_exit("could not open ports");
		if(!done_setup) { 
			perform_setup(daemon, cfg, debug_mode, &cfgfile); 
			done_setup = 1; 
		} else {
			/* reopen log after HUP to facilitate log rotation */
			if(!cfg->use_syslog)
				log_init(cfg->logfile, 0, cfg->chrootdir);
		}
		/* work */
		daemon_fork(daemon);

		/* clean up for restart */
		verbose(VERB_ALGO, "cleanup.");
		daemon_cleanup(daemon);
		config_delete(cfg);
	}
	verbose(VERB_ALGO, "Exit cleanup.");
	/* this unlink may not work if the pidfile is located outside
	 * of the chroot/workdir or we no longer have permissions */
	if(daemon->pidfile) {
		int fd;
		/* truncate pidfile */
		fd = open(daemon->pidfile, O_WRONLY | O_TRUNC, 0644);
		if(fd != -1)
			close(fd);
		/* delete pidfile */
		unlink(daemon->pidfile);
	}
	daemon_delete(daemon);
}

/** getopt global, in case header files fail to declare it. */
extern int optind;
/** getopt global, in case header files fail to declare it. */
extern char* optarg;

/**
 * main program. Set options given commandline arguments.
 * @param argc: number of commandline arguments.
 * @param argv: array of commandline arguments.
 * @return: exit status of the program.
 */
int 
main(int argc, char* argv[])
{
	int c;
	const char* cfgfile = CONFIGFILE;
	const char* winopt = NULL;
	int cmdline_verbose = 0;
	int debug_mode = 0;
#ifdef UB_ON_WINDOWS
	int cmdline_cfg = 0;
#endif

#ifdef HAVE_SBRK
	/* take debug snapshot of heap */
	unbound_start_brk = sbrk(0);
#endif

	log_init(NULL, 0, NULL);
	/* parse the options */
	while( (c=getopt(argc, argv, "c:dhvw:")) != -1) {
		switch(c) {
		case 'c':
			cfgfile = optarg;
#ifdef UB_ON_WINDOWS
			cmdline_cfg = 1;
#endif
			break;
		case 'v':
			cmdline_verbose ++;
			verbosity++;
			break;
		case 'd':
			debug_mode = 1;
			break;
		case 'w':
			winopt = optarg;
			break;
		case '?':
		case 'h':
		default:
			usage();
			return 1;
		}
	}
	argc -= optind;
	argv += optind;

	if(winopt) {
#ifdef UB_ON_WINDOWS
		wsvc_command_option(winopt, cfgfile, cmdline_verbose, 
			cmdline_cfg);
#else
		fatal_exit("option not supported");
#endif
	}

	if(argc != 0) {
		usage();
		return 1;
	}

	run_daemon(cfgfile, cmdline_verbose, debug_mode);
	log_init(NULL, 0, NULL); /* close logfile */
	return 0;
}
