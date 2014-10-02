/*
 * Copyright (c) 1993-1996, 1998-2010 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F39502-99-1-0512.
 *
 * For a brief history of sudo, please see the HISTORY file included
 * with this distribution.
 */

#define _SUDO_MAIN

#ifdef __TANDEM
# include <floss.h>
#endif

#include <config.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/param.h>
#include <sys/socket.h>
#ifdef HAVE_SETRLIMIT
# include <sys/time.h>
# include <sys/resource.h>
#endif
#include <stdio.h>
#ifdef STDC_HEADERS
# include <stdlib.h>
# include <stddef.h>
#else
# ifdef HAVE_STDLIB_H
#  include <stdlib.h>
# endif
#endif /* STDC_HEADERS */
#ifdef HAVE_STRING_H
# if defined(HAVE_MEMORY_H) && !defined(STDC_HEADERS)
#  include <memory.h>
# endif
# include <string.h>
#endif /* HAVE_STRING_H */
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif /* HAVE_STRINGS_H */
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif /* HAVE_UNISTD_H */
#include <pwd.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <grp.h>
#if TIME_WITH_SYS_TIME
# include <time.h>
#endif
#ifdef HAVE_SETLOCALE
# include <locale.h>
#endif
#include <netinet/in.h>
#include <netdb.h>
#if defined(HAVE_GETPRPWNAM) && defined(HAVE_SET_AUTH_PARAMETERS)
# ifdef __hpux
#  undef MAXINT
#  include <hpsecurity.h>
# else
#  include <sys/security.h>
# endif /* __hpux */
# include <prot.h>
#endif /* HAVE_GETPRPWNAM && HAVE_SET_AUTH_PARAMETERS */
#ifdef HAVE_LOGIN_CAP_H
# include <login_cap.h>
# ifndef LOGIN_DEFROOTCLASS
#  define LOGIN_DEFROOTCLASS	"daemon"
# endif
#endif
#ifdef HAVE_MBR_CHECK_MEMBERSHIP
# include <membership.h>
#endif

#include "sudo.h"
#include "lbuf.h"
#include "interfaces.h"
#include <sudo_usage.h>

#ifdef USING_NONUNIX_GROUPS
# include "nonunix.h"
#endif

#if defined(HAVE_PAM) && !defined(NO_PAM_SESSION)
# define CMND_WAIT	TRUE
#else
# define CMND_WAIT	FALSE
#endif

/*
 * Prototypes
 */
static void init_vars			__P((char **));
static int set_cmnd			__P((int));
static void initial_setup		__P((void));
static void set_loginclass		__P((struct passwd *));
static void set_runaspw			__P((const char *));
static void set_runasgr			__P((const char *));
static int cb_runas_default		__P((const char *));
static void show_version		__P((void));
static void create_admin_success_flag	__P((void));
extern int sudo_edit			__P((int, char **, char **));
int run_command __P((const char *path, char *argv[], char *envp[], uid_t uid, int dowait)); /* XXX should be in sudo.h */

/*
 * Globals
 */
int Argc, NewArgc;
char **Argv, **NewArgv;
char *prev_user;
int user_closefrom = -1;
struct sudo_user sudo_user;
struct passwd *list_pw;
struct interface *interfaces;
int num_interfaces;
int tgetpass_flags;
int long_list;
uid_t timestamp_uid;
extern int errorlineno;
extern int parse_error;
extern char *errorfile;
#if defined(RLIMIT_CORE) && !defined(SUDO_DEVEL)
static struct rlimit corelimit;
#endif /* RLIMIT_CORE && !SUDO_DEVEL */
#if defined(__linux__)
static struct rlimit nproclimit;
#endif
#ifdef HAVE_LOGIN_CAP_H
login_cap_t *lc;
#endif /* HAVE_LOGIN_CAP_H */
sigaction_t saved_sa_int, saved_sa_quit, saved_sa_tstp;
char *runas_user;
char *runas_group;
static struct sudo_nss_list *snl;
int sudo_mode;

/* For getopt(3) */
extern char *optarg;
extern int optind;

int
main(argc, argv, envp)
    int argc;
    char *argv[];
    char *envp[];
{
    int sources = 0, validated;
    int fd, cmnd_status, pwflag, rc = 0;
    sigaction_t sa;
    struct sudo_nss *nss;
#if defined(SUDO_DEVEL) && defined(__OpenBSD__)
    extern char *malloc_options;
    malloc_options = "AFGJPR";
#endif

#ifdef HAVE_SETLOCALE
    setlocale(LC_ALL, "");
#endif

    Argv = argv;
    if ((Argc = argc) < 1)
	usage(1);

    /* Must be done as the first thing... */
#if defined(HAVE_GETPRPWNAM) && defined(HAVE_SET_AUTH_PARAMETERS)
    (void) set_auth_parameters(Argc, Argv);
# ifdef HAVE_INITPRIVS
    initprivs();
# endif
#endif /* HAVE_GETPRPWNAM && HAVE_SET_AUTH_PARAMETERS */

    if (geteuid() != 0)
	errorx(1, "must be setuid root");

    /*
     * Signal setup:
     *	Ignore keyboard-generated signals so the user cannot interrupt
     *  us at some point and avoid the logging.
     *  Install handler to wait for children when they exit.
     */
    zero_bytes(&sa, sizeof(sa));
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sa.sa_handler = SIG_IGN;
    (void) sigaction(SIGINT, &sa, &saved_sa_int);
    (void) sigaction(SIGQUIT, &sa, &saved_sa_quit);
    (void) sigaction(SIGTSTP, &sa, &saved_sa_tstp);

    /* Initialize environment functions (including replacements). */
    env_init(FALSE);

    /*
     * Turn off core dumps and make sure fds 0-2 are open.
     */
    initial_setup();
    sudo_setpwent();
    sudo_setgrent();

    /* Parse our arguments. */
    sudo_mode = parse_args(Argc, Argv);

    /* Setup defaults data structures. */
    init_defaults();

    /* Load the list of local ip addresses and netmasks.  */
    load_interfaces();

    pwflag = 0;
    if (ISSET(sudo_mode, MODE_SHELL))
	user_cmnd = "shell";
    else if (ISSET(sudo_mode, MODE_EDIT))
	user_cmnd = "sudoedit";
    else {
	switch (sudo_mode & MODE_MASK) {
	    case MODE_VERSION:
		show_version();
		break;
	    case MODE_HELP:
		help();
		break;
	    case MODE_VALIDATE:
	    case MODE_VALIDATE|MODE_INVALIDATE:
		user_cmnd = "validate";
		pwflag = I_VERIFYPW;
		break;
	    case MODE_KILL:
	    case MODE_INVALIDATE:
		user_cmnd = "kill";
		pwflag = -1;
		break;
	    case MODE_LISTDEFS:
		list_options();
		exit(0);
		break;
	    case MODE_LIST:
	    case MODE_LIST|MODE_INVALIDATE:
		user_cmnd = "list";
		pwflag = I_LISTPW;
		break;
	    case MODE_CHECK:
	    case MODE_CHECK|MODE_INVALIDATE:
		pwflag = I_LISTPW;
		break;
	}
    }

    /* Must have a command to run... */
    if (user_cmnd == NULL && NewArgc == 0)
	usage(1);

    init_vars(envp);			/* XXX - move this later? */

#ifdef USING_NONUNIX_GROUPS
    sudo_nonunix_groupcheck_init();	/* initialise nonunix groups impl */
#endif /* USING_NONUNIX_GROUPS */

    /* Parse nsswitch.conf for sudoers order. */
    snl = sudo_read_nss();

    /* Open and parse sudoers, set global defaults */
    tq_foreach_fwd(snl, nss) {
	if (nss->open(nss) == 0 && nss->parse(nss) == 0) {
	    sources++;
	    if (nss->setdefs(nss) != 0)
		log_error(NO_STDERR|NO_EXIT, "problem with defaults entries");
	}
    }
    if (sources == 0)
	log_error(0, "no valid sudoers sources found, quitting");

    /* XXX - collect post-sudoers parse settings into a function */

    /*
     * Set runas passwd/group entries based on command line or sudoers.
     * Note that if runas_group was specified without runas_user we
     * defer setting runas_pw so the match routines know to ignore it.
     */
    if (runas_group != NULL) {
	set_runasgr(runas_group);
	if (runas_user != NULL)
	    set_runaspw(runas_user);
    } else
	set_runaspw(runas_user ? runas_user : def_runas_default);

    if (!update_defaults(SETDEF_RUNAS))
	log_error(NO_STDERR|NO_EXIT, "problem with defaults entries");

    if (def_fqdn)
	set_fqdn();	/* deferred until after sudoers is parsed */

    /* Set login class if applicable. */
    set_loginclass(sudo_user.pw);

    /* Update initial shell now that runas is set. */
    if (ISSET(sudo_mode, MODE_LOGIN_SHELL))
	NewArgv[0] = estrdup(runas_pw->pw_shell);

    /* This goes after sudoers is parsed since it may have timestamp options. */
    if (sudo_mode == MODE_KILL || sudo_mode == MODE_INVALIDATE) {
	remove_timestamp((sudo_mode == MODE_KILL));
	cleanup(0);
	exit(0);
    }

    /* Is root even allowed to run sudo? */
    if (user_uid == 0 && !def_root_sudo) {
	(void) fprintf(stderr,
	    "Sorry, %s has been configured to not allow root to run it.\n",
	    getprogname());
	exit(1);
    }

    /* Check for -C overriding def_closefrom. */
    if (user_closefrom >= 0 && user_closefrom != def_closefrom) {
	if (!def_closefrom_override)
	    errorx(1, "you are not permitted to use the -C option");
	else
	    def_closefrom = user_closefrom;
    }

    /* If given the -P option, set the "preserve_groups" flag. */
    if (ISSET(sudo_mode, MODE_PRESERVE_GROUPS))
	def_preserve_groups = TRUE;

    cmnd_status = set_cmnd(sudo_mode);

#ifdef HAVE_SETLOCALE
    if (!setlocale(LC_ALL, def_sudoers_locale)) {
	warningx("unable to set locale to \"%s\", using \"C\"",
	    def_sudoers_locale);
	setlocale(LC_ALL, "C");
    }
#endif

    validated = FLAG_NO_USER | FLAG_NO_HOST;
    tq_foreach_fwd(snl, nss) {
	validated = nss->lookup(nss, validated, pwflag);

	if (ISSET(validated, VALIDATE_OK)) {
	    /* Handle "= auth" in netsvc.conf */
	    if (nss->ret_if_found)
		break;
	} else {
	    /* Handle [NOTFOUND=return] */
	    if (nss->ret_if_notfound)
		break;
	}
    }

    if (safe_cmnd == NULL)
	safe_cmnd = estrdup(user_cmnd);

#ifdef HAVE_SETLOCALE
    setlocale(LC_ALL, "");
#endif

    /* If only a group was specified, set runas_pw based on invoking user. */
    if (runas_pw == NULL)
	set_runaspw(user_name);

    /*
     * Look up the timestamp dir owner if one is specified.
     */
    if (def_timestampowner) {
	struct passwd *pw;

	if (*def_timestampowner == '#')
	    pw = sudo_getpwuid(atoi(def_timestampowner + 1));
	else
	    pw = sudo_getpwnam(def_timestampowner);
	if (!pw)
	    log_error(0, "timestamp owner (%s): No such user",
		def_timestampowner);
	timestamp_uid = pw->pw_uid;
	pw_delref(pw);
    }

    /* If no command line args and "set_home" is not set, error out. */
    if (ISSET(sudo_mode, MODE_IMPLIED_SHELL) && !def_shell_noargs)
	usage(1);

    /* Bail if a tty is required and we don't have one.  */
    if (def_requiretty) {
	if ((fd = open(_PATH_TTY, O_RDWR|O_NOCTTY)) == -1) {
	    audit_failure(NewArgv, "no tty");
	    log_error(NO_MAIL, "sorry, you must have a tty to run sudo");
	} else
	    (void) close(fd);
    }

    /* Use askpass value from sudoers unless user specified their own. */
    if (def_askpass && !user_askpass)
	user_askpass = def_askpass;

    /*
     * We don't reset the environment for sudoedit or if the user
     * specified the -E command line flag and they have setenv privs.
     */
    if (ISSET(sudo_mode, MODE_EDIT) ||
        (ISSET(sudo_mode, MODE_PRESERVE_ENV) && def_setenv))
        def_env_reset = FALSE;

    /* Build a new environment that avoids any nasty bits. */
    rebuild_env(def_noexec);

    /* Require a password if sudoers says so.  */
    if (def_authenticate)
	check_user(validated, sudo_mode);

    /* If run as root with SUDO_USER set, set sudo_user.pw to that user. */
    /* XXX - causes confusion when root is not listed in sudoers */
    if (sudo_mode & (MODE_RUN | MODE_EDIT) && prev_user != NULL) {
	if (user_uid == 0 && strcmp(prev_user, "root") != 0) {
	    struct passwd *pw;

	    if ((pw = sudo_getpwnam(prev_user)) != NULL) {
		    if (sudo_user.pw != NULL)
			pw_delref(sudo_user.pw);
		    sudo_user.pw = pw;
#ifdef HAVE_MBR_CHECK_MEMBERSHIP
		    mbr_uid_to_uuid(user_uid, user_uuid);
#endif
	    }
	}
    }

    if (ISSET(validated, VALIDATE_OK)) {
	/* Create Ubuntu-style dot file to indicate sudo was successful. */
	create_admin_success_flag();

	/* Finally tell the user if the command did not exist. */
	if (cmnd_status == NOT_FOUND_DOT) {
	    audit_failure(NewArgv, "command in current directory");
	    errorx(1, "ignoring `%s' found in '.'\nUse `sudo ./%s' if this is the `%s' you wish to run.", user_cmnd, user_cmnd, user_cmnd);
	} else if (cmnd_status == NOT_FOUND) {
	    audit_failure(NewArgv, "%s: command not found", user_cmnd);
	    errorx(1, "%s: command not found", user_cmnd);
	}

	/* If user specified env vars make sure sudoers allows it. */
	if (ISSET(sudo_mode, MODE_RUN) && !def_setenv) {
	    if (ISSET(sudo_mode, MODE_PRESERVE_ENV))
		log_error(NO_MAIL,
		    "sorry, you are not allowed to preserve the environment");
	    else
		validate_env_vars(sudo_user.env_vars);
	}

#ifdef _PATH_SUDO_IO_LOGDIR
	/* Get next session ID so we can log it. */
	if (ISSET(sudo_mode, (MODE_RUN | MODE_EDIT)) && (def_log_input || def_log_output))
	    io_nextid();
#endif
	log_allowed(validated);
	if (ISSET(sudo_mode, MODE_CHECK))
	    rc = display_cmnd(snl, list_pw ? list_pw : sudo_user.pw);
	else if (ISSET(sudo_mode, MODE_LIST))
	    display_privs(snl, list_pw ? list_pw : sudo_user.pw);

	/* Cleanup sudoers sources */
	tq_foreach_fwd(snl, nss)
	    nss->close(nss);

#ifdef USING_NONUNIX_GROUPS
	/* Finished with the groupcheck code */
	sudo_nonunix_groupcheck_cleanup();
#endif

	/* Deferred exit due to sudo_ldap_close() */
	if (ISSET(sudo_mode, (MODE_VALIDATE|MODE_CHECK|MODE_LIST))) {
	    if (list_pw != NULL)
		pw_delref(list_pw);
	    exit(rc);
	}

	/* Must audit before uid change. */
	audit_success(NewArgv);

	if (ISSET(sudo_mode, MODE_LOGIN_SHELL)) {
	    char *p;

	    /* Convert /bin/sh -> -sh so shell knows it is a login shell */
	    if ((p = strrchr(NewArgv[0], '/')) == NULL)
		p = NewArgv[0];
	    *p = '-';
	    NewArgv[0] = p;

	    /*
	     * Newer versions of bash require the --login option to be used
	     * in conjunction with the -c option even if the shell name starts
	     * with a '-'.  Unfortunately, bash 1.x uses -login, not --login
	     * so this will cause an error for that.
	     */
	    if (NewArgc > 1 && strcmp(NewArgv[0], "-bash") == 0) {
		/* Use an extra slot before NewArgv so we can store --login. */
		NewArgv--;
		NewArgc++;
		NewArgv[0] = NewArgv[1];
		NewArgv[1] = "--login";
	    }

#if defined(__linux__) || defined(_AIX)
	    /* Insert system-wide environment variables. */
	    read_env_file(_PATH_ENVIRONMENT, TRUE);
#endif
	}

	if (ISSET(sudo_mode, MODE_RUN)) {
	    /* Insert system-wide environment variables. */
	    if (def_env_file)
		read_env_file(def_env_file, FALSE);

	    /* Insert user-specified environment variables. */
	    insert_env_vars(sudo_user.env_vars);
	}

	/* Restore signal handlers before we exec. */
	(void) sigaction(SIGINT, &saved_sa_int, NULL);
	(void) sigaction(SIGQUIT, &saved_sa_quit, NULL);
	(void) sigaction(SIGTSTP, &saved_sa_tstp, NULL);

	if (ISSET(sudo_mode, MODE_EDIT)) {
	    exit(sudo_edit(NewArgc, NewArgv, envp));
	} else {
	    exit(run_command(safe_cmnd, NewArgv, env_get(), runas_pw->pw_uid,
		CMND_WAIT));
	}
    } else if (ISSET(validated, FLAG_NO_USER | FLAG_NO_HOST)) {
	audit_failure(NewArgv, "No user or host");
	log_denial(validated, 1);
	exit(1);
    } else {
	if (def_path_info) {
	    /*
	     * We'd like to not leak path info at all here, but that can
	     * *really* confuse the users.  To really close the leak we'd
	     * have to say "not allowed to run foo" even when the problem
	     * is just "no foo in path" since the user can trivially set
	     * their path to just contain a single dir.
	     */
	    log_denial(validated,
		!(cmnd_status == NOT_FOUND_DOT || cmnd_status == NOT_FOUND));
	    if (cmnd_status == NOT_FOUND)
		warningx("%s: command not found", user_cmnd);
	    else if (cmnd_status == NOT_FOUND_DOT)
		warningx("ignoring `%s' found in '.'\nUse `sudo ./%s' if this is the `%s' you wish to run.", user_cmnd, user_cmnd, user_cmnd);
	} else {
	    /* Just tell the user they are not allowed to run foo. */
	    log_denial(validated, 1);
	}
	audit_failure(NewArgv, "validation failure");
	exit(1);
    }
    exit(0);	/* not reached */
}

/*
 * Initialize timezone, set umask, fill in ``sudo_user'' struct and
 * load the ``interfaces'' array.
 */
static void
init_vars(envp)
    char **envp;
{
    char *p, **ep, thost[MAXHOSTNAMELEN + 1];
    int nohostname;

    /* Sanity check command from user. */
    if (user_cmnd == NULL && strlen(NewArgv[0]) >= PATH_MAX)
	errorx(1, "%s: File name too long", NewArgv[0]);

#ifdef HAVE_TZSET
    (void) tzset();		/* set the timezone if applicable */
#endif /* HAVE_TZSET */

    /* Default value for cmnd and cwd, overridden later. */
    if (user_cmnd == NULL)
	user_cmnd = NewArgv[0];
    (void) strlcpy(user_cwd, "unknown", sizeof(user_cwd));

    /*
     * We avoid gethostbyname() if possible since we don't want
     * sudo to block if DNS or NIS is hosed.
     * "host" is the (possibly fully-qualified) hostname and
     * "shost" is the unqualified form of the hostname.
     */
    nohostname = gethostname(thost, sizeof(thost));
    if (nohostname) {
	user_host = user_shost = "localhost";
    } else {
	thost[sizeof(thost) - 1] = '\0';
	user_host = estrdup(thost);
	if ((p = strchr(user_host, '.'))) {
	    *p = '\0';
	    user_shost = estrdup(user_host);
	    *p = '.';
	} else {
	    user_shost = user_host;
	}
    }

    if ((p = ttyname(STDIN_FILENO)) || (p = ttyname(STDOUT_FILENO)) ||
	(p = ttyname(STDERR_FILENO))) {
	user_tty = user_ttypath = estrdup(p);
	if (strncmp(user_tty, _PATH_DEV, sizeof(_PATH_DEV) - 1) == 0)
	    user_tty += sizeof(_PATH_DEV) - 1;
    } else
	user_tty = "unknown";

    for (ep = envp; *ep; ep++) {
	/* XXX - don't fill in if empty string */
	switch (**ep) {
	    case 'D':
		if (strncmp("DISPLAY=", *ep, 8) == 0)
		    user_display = *ep + 8;
		break;
	    case 'K':
		if (strncmp("KRB5CCNAME=", *ep, 11) == 0)
		    user_ccname = *ep + 11;
		break;
	    case 'P':
		if (strncmp("PATH=", *ep, 5) == 0)
		    user_path = *ep + 5;
		break;
	    case 'S':
		if (strncmp("SHELL=", *ep, 6) == 0)
		    user_shell = *ep + 6;
		else if (!user_prompt && strncmp("SUDO_PROMPT=", *ep, 12) == 0)
		    user_prompt = *ep + 12;
		else if (strncmp("SUDO_USER=", *ep, 10) == 0)
		    prev_user = *ep + 10;
		else if (strncmp("SUDO_ASKPASS=", *ep, 13) == 0)
		    user_askpass = *ep + 13;
		break;
	    }
    }

    /*
     * Stash a local copy of the user's struct passwd.
     */
    if ((sudo_user.pw = sudo_getpwuid(getuid())) == NULL) {
	uid_t uid = getuid();
	gid_t gid = getgid();

	/*
	 * If we are in -k/-K mode, just spew to stderr.  It is not unusual for
	 * users to place "sudo -k" in a .logout file which can cause sudo to
	 * be run during reboot after the YP/NIS/NIS+/LDAP/etc daemon has died.
	 */
	if (sudo_mode == MODE_KILL || sudo_mode == MODE_INVALIDATE)
	    errorx(1, "unknown uid: %u", (unsigned int) uid);

	/* Need to make a fake struct passwd for the call to log_error(). */
	sudo_user.pw = sudo_fakepwuid(uid, gid);
	log_error(0, "unknown uid: %u", (unsigned int) uid);
    }
#ifdef HAVE_MBR_CHECK_MEMBERSHIP
    mbr_uid_to_uuid(user_uid, user_uuid);
#endif
    if (user_shell == NULL || *user_shell == '\0')
	user_shell = estrdup(sudo_user.pw->pw_shell);

    /* It is now safe to use log_error() and set_perms() */

#ifdef HAVE_GETGROUPS
    if ((user_ngroups = getgroups(0, NULL)) > 0) {
	user_groups = emalloc2(user_ngroups, sizeof(GETGROUPS_T));
	if (getgroups(user_ngroups, user_groups) < 0)
	    log_error(USE_ERRNO|MSG_ONLY, "can't get group vector");
    }
#endif

    if (nohostname)
	log_error(USE_ERRNO|MSG_ONLY, "can't get hostname");

    /*
     * Get current working directory.  Try as user, fall back to root.
     */
    set_perms(PERM_USER);
    if (!getcwd(user_cwd, sizeof(user_cwd))) {
	set_perms(PERM_ROOT);
	if (!getcwd(user_cwd, sizeof(user_cwd))) {
	    warningx("cannot get working directory");
	    (void) strlcpy(user_cwd, "unknown", sizeof(user_cwd));
	}
    } else
	set_perms(PERM_ROOT);

    /*
     * If in shell or edit mode, or if running a pseudo-command
     * such as "list", we need to redo NewArgv and NewArgc.
     */
    if (ISSET(sudo_mode, MODE_SHELL)) {
	char **av;

	/* Allocate 2 extra slots for --login and execve() failure (ENOEXEC). */
	av = (char **) emalloc2(6, sizeof(char *));
	av += 2;

	av[0] = user_shell;	/* may be updated later */
	if (NewArgc > 0) {
	    /* shell -c "command" */
	    char *cmnd, *src, *dst;
	    size_t cmnd_size = (size_t) (NewArgv[NewArgc - 1] - NewArgv[0]) +
		    strlen(NewArgv[NewArgc - 1]) + 1;

	    cmnd = dst = emalloc2(cmnd_size, 2);
	    for (av = NewArgv; *av != NULL; av++) {
		for (src = *av; *src != '\0'; src++) {
		    /* quote potential meta characters */
		    if (!isalnum((unsigned char)*src) && *src != '_' && *src != '-')
			*dst++ = '\\';
		    *dst++ = *src;
		}
		*dst++ = ' ';
	    }
	    if (cmnd != dst)
		dst--;	/* replace last space with a NUL */
	    *dst = '\0';

	    av[1] = "-c";
	    av[2] = cmnd;
	    NewArgc = 2;
	}
	av[++NewArgc] = NULL;
	NewArgv = av;
    } else if (ISSET(sudo_mode, MODE_EDIT) || NewArgc == 0) {
	NewArgv--;
	NewArgc++;
	NewArgv[0] = user_cmnd;
    }

    /* Set runas callback. */
    sudo_defs_table[I_RUNAS_DEFAULT].callback = cb_runas_default;
}

/*
 * Fill in user_cmnd, user_args, user_base and user_stat variables
 * and apply any command-specific defaults entries.
 */
static int
set_cmnd(sudo_mode)
    int sudo_mode;
{
    int rval;
    char *path = user_path;

    /* Resolve the path and return. */
    rval = FOUND;
    user_stat = emalloc(sizeof(struct stat));
    if (sudo_mode & (MODE_RUN | MODE_EDIT | MODE_CHECK)) {
	if (ISSET(sudo_mode, MODE_RUN | MODE_CHECK)) {
	    if (def_secure_path && !user_is_exempt())
		path = def_secure_path;
	    set_perms(PERM_RUNAS);
	    rval = find_path(NewArgv[0], &user_cmnd, user_stat, path,
		def_ignore_dot);
	    set_perms(PERM_ROOT);
	    if (rval != FOUND) {
		/* Failed as root, try as invoking user. */
		set_perms(PERM_USER);
		rval = find_path(NewArgv[0], &user_cmnd, user_stat, path,
		    def_ignore_dot);
		set_perms(PERM_ROOT);
	    }
	}

	/* set user_args */
	if (NewArgc > 1) {
	    char *to, *from, **av;
	    size_t size, n;

	    if (ISSET(sudo_mode, MODE_SHELL)) {
		for (size = 0, av = NewArgv + 1; *av; av++)
		    size += strlen(*av) + 1;
		user_args = emalloc(size);

		/*
		 * When running a command via a shell, sudo escapes potential
		 * meta chars in NewArgv.  We unescape non-spaces for sudoers
		 * matching and logging purposes.
		 */
		for (to = user_args, av = NewArgv + 1; (from = *av); av++) {
		    while (*from) {
			if (from[0] == '\\' && !isspace((unsigned char)from[1]))
			    from++;
			*to++ = *from++;
		    }
		    *to++ = ' ';
		}
		*--to = '\0';
	    } else {
		/* NewArgv is contiguous so just count. */
		size = (size_t) (NewArgv[NewArgc - 1] - NewArgv[1]) +
			strlen(NewArgv[NewArgc - 1]) + 1;
		user_args = emalloc(size);

		for (to = user_args, av = NewArgv + 1; *av; av++) {
		    n = strlcpy(to, *av, size - (to - user_args));
		    if (n >= size - (to - user_args))
			errorx(1, "internal error, init_vars() overflow");
		    to += n;
		    *to++ = ' ';
		}
		*--to = '\0';
	    }
	}
    }
    if ((user_base = strrchr(user_cmnd, '/')) != NULL)
	user_base++;
    else
	user_base = user_cmnd;

    if (!update_defaults(SETDEF_CMND))
	log_error(NO_STDERR|NO_EXIT, "problem with defaults entries");

    return rval;
}

/*
 * Setup the execution environment immediately prior to the call to execve()
 * Returns TRUE on success and FALSE on failure.
 */
int
exec_setup(rbac_enabled, ttyname, ttyfd)
    int rbac_enabled;
    const char *ttyname;
    int ttyfd;
{
    int rval = FALSE;

#ifdef HAVE_SELINUX
    if (rbac_enabled) {
       if (selinux_setup(user_role, user_type, ttyname, ttyfd) == -1)
	   goto done;
    }
#endif

    /*
     * For sudoedit, the command runas a the user with no additional setup.
     */
    if (ISSET(sudo_mode, MODE_EDIT)) {
	set_perms(PERM_FULL_USER);
	rval = TRUE;
	goto done;
    }

    /*
     * Set umask based on sudoers.
     * If user's umask is more restrictive, OR in those bits too
     * unless umask_override is set.
     */
    if (def_umask != 0777) {
	if (def_umask_override) {
	    umask(def_umask);
	} else {
	    mode_t mask = umask(def_umask);
	    mask |= def_umask;
	    if (mask != def_umask)
		umask(mask);
	}
    }

    /* Restore coredumpsize resource limit. */
#if defined(RLIMIT_CORE) && !defined(SUDO_DEVEL)
    (void) setrlimit(RLIMIT_CORE, &corelimit);
#endif /* RLIMIT_CORE && !SUDO_DEVEL */

    if (ISSET(sudo_mode, MODE_RUN))
	set_perms(PERM_FULL_RUNAS);

    if (ISSET(sudo_mode, MODE_LOGIN_SHELL)) {
	/* Change to target user's homedir. */
	if (chdir(runas_pw->pw_dir) == -1) {
	    warning("unable to change directory to %s", runas_pw->pw_dir);
	    goto done;
	}
    }

    /*
     * Restore nproc resource limit if pam_limits didn't do it for us.
     * We must do this *after* the uid change to avoid potential EAGAIN
     * from setuid().
     */
#if defined(__linux__)
    {
	struct rlimit rl;
	if (getrlimit(RLIMIT_NPROC, &rl) == 0) {
	    if (rl.rlim_cur == RLIM_INFINITY && rl.rlim_max == RLIM_INFINITY)
		(void) setrlimit(RLIMIT_NPROC, &nproclimit);
	}
    }
#endif

    /* Close the password and group files and free up memory. */
    sudo_endpwent();
    sudo_endgrent();
    pw_delref(sudo_user.pw);
    pw_delref(runas_pw);
    if (runas_gr != NULL)
	gr_delref(runas_gr);

    rval = TRUE;

done:
    return rval;
}

/*
 * Run the command and wait for it to complete.
 */
int
run_command(path, argv, envp, uid, dowait)
    const char *path;
    char *argv[];
    char *envp[];
    uid_t uid;
    int dowait;
{
    struct command_status cstat;
    int exitcode = 1;

#ifdef PROFILING
    exit(0);
#endif

    cstat.type = CMD_INVALID;
    cstat.val = 0;

    sudo_execve(path, argv, envp, uid, &cstat, dowait,
	ISSET(sudo_mode, MODE_BACKGROUND));

    switch (cstat.type) {
    case CMD_ERRNO:
	/* exec_setup() or execve() returned an error. */
	warningx("unable to execute %s: %s", path, strerror(cstat.val));
	exitcode = 127;
	break;
    case CMD_WSTATUS:
	/* Command ran, exited or was killed. */
	if (WIFEXITED(cstat.val))
	    exitcode = WEXITSTATUS(cstat.val);
	else if (WIFSIGNALED(cstat.val))
	    exitcode = WTERMSIG(cstat.val) | 128;
	break;
    default:
	warningx("unexpected child termination condition: %d", cstat.type);
	break;
    }
#ifdef HAVE_PAM
    pam_end_session(runas_pw);
#endif /* HAVE_PAM */
#ifdef _PATH_SUDO_IO_LOGDIR
    io_log_close();
#endif
    sudo_endpwent();
    sudo_endgrent();
    pw_delref(sudo_user.pw);
    pw_delref(runas_pw);
    if (runas_gr != NULL)
	gr_delref(runas_gr);
    return exitcode;
}

/*
 * Open sudoers and sanity check mode/owner/type.
 * Returns a handle to the sudoers file or NULL on error.
 */
FILE *
open_sudoers(sudoers, doedit, keepopen)
    const char *sudoers;
    int doedit;
    int *keepopen;
{
    struct stat statbuf;
    FILE *fp = NULL;
    int rootstat;

    /*
     * Fix the mode and group on sudoers file from old default.
     * Only works if file system is readable/writable by root.
     */
    if ((rootstat = stat_sudoers(sudoers, &statbuf)) == 0 &&
	SUDOERS_UID == statbuf.st_uid && SUDOERS_MODE != 0400 &&
	(statbuf.st_mode & 0007777) == 0400) {

	if (chmod(sudoers, SUDOERS_MODE) == 0) {
	    warningx("fixed mode on %s", sudoers);
	    SET(statbuf.st_mode, SUDOERS_MODE);
	    if (statbuf.st_gid != SUDOERS_GID) {
		if (chown(sudoers, (uid_t) -1, SUDOERS_GID) == 0) {
		    warningx("set group on %s", sudoers);
		    statbuf.st_gid = SUDOERS_GID;
		} else
		    warning("unable to set group on %s", sudoers);
	    }
	} else
	    warning("unable to fix mode on %s", sudoers);
    }

    /*
     * Sanity checks on sudoers file.  Must be done as sudoers
     * file owner.  We already did a stat as root, so use that
     * data if we can't stat as sudoers file owner.
     */
    set_perms(PERM_SUDOERS);

    if (rootstat != 0 && stat_sudoers(sudoers, &statbuf) != 0)
	log_error(USE_ERRNO|NO_EXIT, "can't stat %s", sudoers);
    else if (!S_ISREG(statbuf.st_mode))
	log_error(NO_EXIT, "%s is not a regular file", sudoers);
    else if ((statbuf.st_mode & 07577) != SUDOERS_MODE)
	log_error(NO_EXIT, "%s is mode 0%o, should be 0%o", sudoers,
	    (unsigned int) (statbuf.st_mode & 07777),
	    (unsigned int) SUDOERS_MODE);
    else if (statbuf.st_uid != SUDOERS_UID)
	log_error(NO_EXIT, "%s is owned by uid %u, should be %u", sudoers,
	    (unsigned int) statbuf.st_uid, (unsigned int) SUDOERS_UID);
    else if (statbuf.st_gid != SUDOERS_GID && ISSET(statbuf.st_mode, S_IRGRP|S_IWGRP))
	log_error(NO_EXIT, "%s is owned by gid %u, should be %u", sudoers,
	    (unsigned int) statbuf.st_gid, (unsigned int) SUDOERS_GID);
    else if ((fp = fopen(sudoers, "r")) == NULL)
	log_error(USE_ERRNO|NO_EXIT, "can't open %s", sudoers);
    else {
	/*
	 * Make sure we can actually read sudoers so we can present the
	 * user with a reasonable error message (unlike the lexer).
	 */
	if (statbuf.st_size != 0 && fgetc(fp) == EOF) {
	    log_error(USE_ERRNO|NO_EXIT, "can't read %s", sudoers);
	    fclose(fp);
	    fp = NULL;
	}
    }

    if (fp != NULL) {
	rewind(fp);
	(void) fcntl(fileno(fp), F_SETFD, 1);
    }

    set_perms(PERM_ROOT);		/* change back to root */
    return fp;
}

/*
 * Close all open files (except std*) and turn off core dumps.
 * Also sets the set_perms() pointer to the correct function.
 */
static void
initial_setup()
{
    int miss[3], devnull = -1;
    sigset_t mask;
#if defined(__linux__) || (defined(RLIMIT_CORE) && !defined(SUDO_DEVEL))
    struct rlimit rl;
#endif

    /* Reset signal mask. */
    (void) sigemptyset(&mask);
    (void) sigprocmask(SIG_SETMASK, &mask, NULL);

#if defined(__linux__)
    /*
     * Unlimit the number of processes since Linux's setuid() will
     * apply resource limits when changing uid and return EAGAIN if
     * nproc would be violated by the uid switch.
     */
    (void) getrlimit(RLIMIT_NPROC, &nproclimit);
    rl.rlim_cur = rl.rlim_max = RLIM_INFINITY;
    if (setrlimit(RLIMIT_NPROC, &rl)) {
	memcpy(&rl, &nproclimit, sizeof(struct rlimit));
	rl.rlim_cur = rl.rlim_max;
	(void)setrlimit(RLIMIT_NPROC, &rl);
    }
#endif /* __linux__ */
#if defined(RLIMIT_CORE) && !defined(SUDO_DEVEL)
    /*
     * Turn off core dumps.
     */
    (void) getrlimit(RLIMIT_CORE, &corelimit);
    memcpy(&rl, &corelimit, sizeof(struct rlimit));
    rl.rlim_cur = 0;
    (void) setrlimit(RLIMIT_CORE, &rl);
#endif /* RLIMIT_CORE && !SUDO_DEVEL */

    /*
     * stdin, stdout and stderr must be open; set them to /dev/null
     * if they are closed and close all other fds.
     */
    miss[STDIN_FILENO] = fcntl(STDIN_FILENO, F_GETFL, 0) == -1;
    miss[STDOUT_FILENO] = fcntl(STDOUT_FILENO, F_GETFL, 0) == -1;
    miss[STDERR_FILENO] = fcntl(STDERR_FILENO, F_GETFL, 0) == -1;
    if (miss[STDIN_FILENO] || miss[STDOUT_FILENO] || miss[STDERR_FILENO]) {
	if ((devnull = open(_PATH_DEVNULL, O_RDWR, 0644)) == -1)
	    error(1, "unable to open %s", _PATH_DEVNULL);
	if (miss[STDIN_FILENO] && dup2(devnull, STDIN_FILENO) == -1)
	    error(1, "dup2");
	if (miss[STDOUT_FILENO] && dup2(devnull, STDOUT_FILENO) == -1)
	    error(1, "dup2");
	if (miss[STDERR_FILENO] && dup2(devnull, STDERR_FILENO) == -1)
	    error(1, "dup2");
	if (devnull > STDERR_FILENO)
	    close(devnull);
    }
}

#ifdef HAVE_LOGIN_CAP_H
static void
set_loginclass(pw)
    struct passwd *pw;
{
    int errflags;

    /*
     * Don't make it a fatal error if the user didn't specify the login
     * class themselves.  We do this because if login.conf gets
     * corrupted we want the admin to be able to use sudo to fix it.
     */
    if (login_class)
	errflags = NO_MAIL|MSG_ONLY;
    else
	errflags = NO_MAIL|MSG_ONLY|NO_EXIT;

    if (login_class && strcmp(login_class, "-") != 0) {
	if (user_uid != 0 &&
	    strcmp(runas_user ? runas_user : def_runas_default, "root") != 0)
	    errorx(1, "only root can use -c %s", login_class);
    } else {
	login_class = pw->pw_class;
	if (!login_class || !*login_class)
	    login_class =
		(pw->pw_uid == 0) ? LOGIN_DEFROOTCLASS : LOGIN_DEFCLASS;
    }

    lc = login_getclass(login_class);
    if (!lc || !lc->lc_class || strcmp(lc->lc_class, login_class) != 0) {
	log_error(errflags, "unknown login class: %s", login_class);
	if (!lc)
	    lc = login_getclass(NULL);	/* needed for login_getstyle() later */
    }
}
#else
static void
set_loginclass(pw)
    struct passwd *pw;
{
}
#endif /* HAVE_LOGIN_CAP_H */

/*
 * Look up the fully qualified domain name and set user_host and user_shost.
 */
void
set_fqdn()
{
#ifdef HAVE_GETADDRINFO
    struct addrinfo *res0, hint;
#else
    struct hostent *hp;
#endif
    char *p;

#ifdef HAVE_GETADDRINFO
    zero_bytes(&hint, sizeof(hint));
    hint.ai_family = PF_UNSPEC;
    hint.ai_flags = AI_CANONNAME;
    if (getaddrinfo(user_host, NULL, &hint, &res0) != 0) {
#else
    if (!(hp = gethostbyname(user_host))) {
#endif
	log_error(MSG_ONLY|NO_EXIT,
	    "unable to resolve host %s", user_host);
    } else {
	if (user_shost != user_host)
	    efree(user_shost);
	efree(user_host);
#ifdef HAVE_GETADDRINFO
	user_host = estrdup(res0->ai_canonname);
	freeaddrinfo(res0);
#else
	user_host = estrdup(hp->h_name);
#endif
    }
    if ((p = strchr(user_host, '.'))) {
	*p = '\0';
	user_shost = estrdup(user_host);
	*p = '.';
    } else {
	user_shost = user_host;
    }
}

/*
 * Get passwd entry for the user we are going to run commands as
 * and store it in runas_pw.  By default, commands run as "root".
 */
void
set_runaspw(user)
    const char *user;
{
    if (runas_pw != NULL)
	pw_delref(runas_pw);
    if (*user == '#') {
	if ((runas_pw = sudo_getpwuid(atoi(user + 1))) == NULL)
	    runas_pw = sudo_fakepwnam(user, runas_gr ? runas_gr->gr_gid : 0);
    } else {
	if ((runas_pw = sudo_getpwnam(user)) == NULL) {
	    audit_failure(NewArgv, "unknown user: %s", user);
	    log_error(NO_MAIL|MSG_ONLY, "unknown user: %s", user);
	}
    }
}

/*
 * Get group entry for the group we are going to run commands as
 * and store it in runas_gr.
 */
static void
set_runasgr(group)
    const char *group;
{
    if (runas_gr != NULL)
	gr_delref(runas_gr);
    if (*group == '#') {
	if ((runas_gr = sudo_getgrgid(atoi(group + 1))) == NULL)
	    runas_gr = sudo_fakegrnam(group);
    } else {
	if ((runas_gr = sudo_getgrnam(group)) == NULL)
	    log_error(NO_MAIL|MSG_ONLY, "unknown group: %s", group);
    }
}

/*
 * Callback for runas_default sudoers setting.
 */
static int
cb_runas_default(user)
    const char *user;
{
    /* Only reset runaspw if user didn't specify one. */
    if (!runas_user && !runas_group)
	set_runaspw(user);
    return TRUE;
}

/*
 * Cleanup hook for error()/errorx()
 */
void
cleanup(gotsignal)
    int gotsignal;
{
    struct sudo_nss *nss;

    if (!gotsignal) {
	if (snl != NULL) {
	    tq_foreach_fwd(snl, nss)
		nss->close(nss);
	}
#ifdef USING_NONUNIX_GROUPS
	sudo_nonunix_groupcheck_cleanup();
#endif
	sudo_endpwent();
	sudo_endgrent();
#ifdef _PATH_SUDO_IO_LOGDIR
	io_log_close();
#endif
    }
    term_restore(STDIN_FILENO, 0);
#ifdef HAVE_SELINUX
    selinux_restore_tty();
#endif
}

static void
show_version()
{
    (void) printf("Sudo version %s\n", PACKAGE_VERSION);
    if (getuid() == 0) {
	putchar('\n');
	(void) printf("Configure args: %s\n", CONFIGURE_ARGS);
	(void) printf("Sudoers path: %s\n", _PATH_SUDOERS);
#ifdef HAVE_LDAP
# ifdef _PATH_NSSWITCH_CONF
	(void) printf("nsswitch path: %s\n", _PATH_NSSWITCH_CONF);
# endif
	(void) printf("ldap.conf path: %s\n", _PATH_LDAP_CONF);
	(void) printf("ldap.secret path: %s\n", _PATH_LDAP_SECRET);
#endif
	dump_auth_methods();
	dump_defaults();
	dump_interfaces();
    }
    exit(0);
}

#ifdef USE_ADMIN_FLAG
static void
create_admin_success_flag()
{
    struct stat statbuf;
    char flagfile[PATH_MAX];
    int fd, n;

    /* Check whether the user is in the admin group. */
    if (!user_in_group(sudo_user.pw, "admin"))
	return;

    /* Build path to flag file. */
    n = snprintf(flagfile, sizeof(flagfile), "%s/.sudo_as_admin_successful",
	user_dir);
    if (n <= 0 || n >= sizeof(flagfile))
	return;

    /* Create admin flag file if it doesn't already exist. */
    set_perms(PERM_USER);
    if (stat(flagfile, &statbuf) == 0) {
	set_perms(PERM_ROOT);
	return;
    }

    fd = open(flagfile, O_CREAT|O_WRONLY|O_EXCL, 0644);
    close(fd);
    set_perms(PERM_ROOT);
}
#else /* !USE_ADMIN_FLAG */
static void
create_admin_success_flag()
{
    /* STUB */
}
#endif /* USE_ADMIN_FLAG */
