/*
 * Copyright (c) 1996, 1998-2004 Todd C. Miller <Todd.Miller@courtesan.com>
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
 */

/*
 * Lock the sudoers file for safe editing (ala vipw) and check for parse errors.
 */

#define _SUDO_MAIN

#ifdef __TANDEM
# include <floss.h>
#endif

#include "config.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#ifndef __TANDEM
# include <sys/file.h>
#endif
#include <sys/wait.h>
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
# include <string.h>
#else
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif /* HAVE_STRING_H */
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */
#ifdef HAVE_ERR_H
# include <err.h>
#else
# include "emul/err.h"
#endif /* HAVE_ERR_H */
#include <ctype.h>
#include <pwd.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>

#include "sudo.h"
#include "version.h"

#ifndef lint
static const char rcsid[] = "$Sudo: visudo.c,v 1.170 2004/09/08 15:48:23 millert Exp $";
#endif /* lint */

/*
 * Function prototypes
 */
static void usage		__P((void));
static char whatnow		__P((void));
static RETSIGTYPE Exit		__P((int));
static void setup_signals	__P((void));
static int run_command		__P((char *, char **));
static int check_syntax		__P((int));
int command_matches		__P((char *, char *));
int addr_matches		__P((char *));
int hostname_matches		__P((char *, char *, char *));
int netgr_matches		__P((char *, char *, char *, char *));
int usergr_matches		__P((char *, char *, struct passwd *));
int userpw_matches		__P((char *, char *, struct passwd *));
void init_parser		__P((void));
void yyerror			__P((char *));
void yyrestart			__P((FILE *));

/*
 * External globals exported by the parser
 */
extern FILE *yyin, *yyout;
extern int errorlineno;
extern int pedantic;
extern int quiet;

/* For getopt(3) */
extern char *optarg;
extern int optind;

/*
 * Globals
 */
char **Argv;
char *sudoers = _PATH_SUDOERS;
char *stmp = _PATH_SUDOERS_TMP;
struct sudo_user sudo_user;
int Argc, parse_error = FALSE;

int
main(argc, argv)
    int argc;
    char **argv;
{
    char buf[PATH_MAX*2];		/* buffer used for copying files */
    char *Editor;			/* editor to use */
    char *UserEditor;			/* editor user wants to use */
    char *EditorPath;			/* colon-separated list of editors */
    char *av[4];			/* argument vector for run_command */
    int checkonly;			/* only check existing file? */
    int sudoers_fd;			/* sudoers file descriptor */
    int stmp_fd;			/* stmp file descriptor */
    int n;				/* length parameter */
    int ch;				/* getopt char */
    struct timespec ts1, ts2;		/* time before and after edit */
    struct timespec sudoers_mtim;	/* starting mtime of sudoers file */
    off_t sudoers_size;			/* starting size of sudoers file */
    struct stat sb;			/* stat buffer */

    /* Warn about aliases that are used before being defined. */
    pedantic = 1;

    Argv = argv;
    if ((Argc = argc) < 1)
	usage();

    /*
     * Arg handling.
     */
    checkonly = 0;
    while ((ch = getopt(argc, argv, "Vcf:sq")) != -1) {
	switch (ch) {
	    case 'V':
		(void) printf("%s version %s\n", getprogname(), version);
		exit(0);
	    case 'c':
		checkonly++;		/* check mode */
		break;
	    case 'f':
		sudoers = optarg;	/* sudoers file path */
		easprintf(&stmp, "%s.tmp", optarg);
		break;
	    case 's':
		pedantic++;		/* strict mode */
		break;
	    case 'q':
		quiet++;		/* quiet mode */
		break;
	    default:
		usage();
	}
    }
    argc -= optind;
    argv += optind;
    if (argc)
	usage();

    /* Mock up a fake sudo_user struct. */
    user_host = user_shost = user_cmnd = "";
    if ((sudo_user.pw = getpwuid(getuid())) == NULL)
	errx(1, "you don't exist in the passwd database");

    /* Setup defaults data structures. */
    init_defaults();

    if (checkonly)
	exit(check_syntax(quiet));

    /*
     * Open sudoers, lock it and stat it.
     * sudoers_fd must remain open throughout in order to hold the lock.
     */
    sudoers_fd = open(sudoers, O_RDWR | O_CREAT, SUDOERS_MODE);
    if (sudoers_fd == -1)
	err(1, "%s", sudoers);
    if (!lock_file(sudoers_fd, SUDO_TLOCK))
	errx(1, "sudoers file busy, try again later");
#ifdef HAVE_FSTAT
    if (fstat(sudoers_fd, &sb) == -1)
#else
    if (stat(sudoers, &sb) == -1)
#endif
	err(1, "can't stat %s", sudoers);
    sudoers_size = sb.st_size;
    sudoers_mtim.tv_sec = mtim_getsec(sb);
    sudoers_mtim.tv_nsec = mtim_getnsec(sb);

    /*
     * Open sudoers temp file.
     */
    stmp_fd = open(stmp, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (stmp_fd < 0)
	err(1, "%s", stmp);

    /* Install signal handlers to clean up stmp if we are killed. */
    setup_signals();

    /* Copy sudoers -> stmp and reset the mtime */
    if (sudoers_size) {
	while ((n = read(sudoers_fd, buf, sizeof(buf))) > 0)
	    if (write(stmp_fd, buf, n) != n)
		err(1, "write error");

	/* Add missing newline at EOF if needed. */
	if (n > 0 && buf[n - 1] != '\n') {
	    buf[0] = '\n';
	    write(stmp_fd, buf, 1);
	}

	(void) touch(stmp_fd, stmp, &sudoers_mtim);
	(void) close(stmp_fd);

	/* Parse sudoers to pull in editor and env_editor conf values. */
	if ((yyin = fopen(stmp, "r"))) {
	    yyout = stdout;
	    n = quiet;
	    quiet = 1;
	    init_parser();
	    yyparse();
	    parse_error = FALSE;
	    quiet = n;
	    fclose(yyin);
	}
    } else
	(void) close(stmp_fd);

    /*
     * Check VISUAL and EDITOR environment variables to see which editor
     * the user wants to use (we may not end up using it though).
     * If the path is not fully-qualified, make it so and check that
     * the specified executable actually exists.
     */
    if ((UserEditor = getenv("VISUAL")) == NULL || *UserEditor == '\0')
	UserEditor = getenv("EDITOR");
    if (UserEditor && *UserEditor == '\0')
	UserEditor = NULL;
    else if (UserEditor) {
	if (find_path(UserEditor, &Editor, NULL, getenv("PATH")) == FOUND) {
	    UserEditor = Editor;
	} else {
	    if (def_env_editor) {
		/* If we are honoring $EDITOR this is a fatal error. */
		warnx("specified editor (%s) doesn't exist!", UserEditor);
		Exit(-1);
	    } else {
		/* Otherwise, just ignore $EDITOR. */
		UserEditor = NULL;
	    }
	}
    }

    /*
     * See if we can use the user's choice of editors either because
     * we allow any $EDITOR or because $EDITOR is in the allowable list.
     */
    Editor = EditorPath = NULL;
    if (def_env_editor && UserEditor)
	Editor = UserEditor;
    else if (UserEditor) {
	struct stat editor_sb;
	struct stat user_editor_sb;
	char *base, *userbase;

	if (stat(UserEditor, &user_editor_sb) != 0) {
	    /* Should never happen since we already checked above. */
	    warn("unable to stat editor (%s)", UserEditor);
	    Exit(-1);
	}
	EditorPath = estrdup(def_editor);
	Editor = strtok(EditorPath, ":");
	do {
	    /*
	     * Both Editor and UserEditor should be fully qualified but
	     * check anyway...
	     */
	    if ((base = strrchr(Editor, '/')) == NULL)
		continue;
	    if ((userbase = strrchr(UserEditor, '/')) == NULL) {
		Editor = NULL;
		break;
	    }
	    base++, userbase++;

	    /*
	     * We compare the basenames first and then use stat to match
	     * for sure.
	     */
	    if (strcmp(base, userbase) == 0) {
		if (stat(Editor, &editor_sb) == 0 && S_ISREG(editor_sb.st_mode)
		    && (editor_sb.st_mode & 0000111) &&
		    editor_sb.st_dev == user_editor_sb.st_dev &&
		    editor_sb.st_ino == user_editor_sb.st_ino)
		    break;
	    }
	} while ((Editor = strtok(NULL, ":")));
    }

    /*
     * Can't use $EDITOR, try each element of def_editor until we
     * find one that exists, is regular, and is executable.
     */
    if (Editor == NULL || *Editor == '\0') {
	if (EditorPath != NULL)
	    free(EditorPath);
	EditorPath = estrdup(def_editor);
	Editor = strtok(EditorPath, ":");
	do {
	    if (sudo_goodpath(Editor, NULL))
		break;
	} while ((Editor = strtok(NULL, ":")));

	/* Bleah, none of the editors existed! */
	if (Editor == NULL || *Editor == '\0') {
	    warnx("no editor found (editor path = %s)", def_editor);
	    Exit(-1);
	}
    }

    /*
     * Edit the temp file and parse it (for sanity checking)
     */
    do {
	char linestr[64];

	/* Build up argument vector for the command */
	if ((av[0] = strrchr(Editor, '/')) != NULL)
	    av[0]++;
	else
	    av[0] = Editor;
	n = 1;
	if (parse_error == TRUE) {
	    (void) snprintf(linestr, sizeof(linestr), "+%d", errorlineno);
	    av[n++] = linestr;
	}
	av[n++] = stmp;
	av[n++] = NULL;

	/*
	 * Do the edit:
	 *  We cannot check the editor's exit value against 0 since
	 *  XPG4 specifies that vi's exit value is a function of the
	 *  number of errors during editing (?!?!).
	 */
	gettime(&ts1);
	if (run_command(Editor, av) != -1) {
	    gettime(&ts2);
	    /*
	     * Sanity checks.
	     */
	    if (stat(stmp, &sb) < 0) {
		warnx("cannot stat temporary file (%s), %s unchanged",
		    stmp, sudoers);
		Exit(-1);
	    }
	    if (sb.st_size == 0) {
		warnx("zero length temporary file (%s), %s unchanged",
		    stmp, sudoers);
		Exit(-1);
	    }

	    /*
	     * Passed sanity checks so reopen stmp file and check
	     * for parse errors.
	     */
	    yyout = stdout;
	    yyin = fopen(stmp, "r+");
	    if (yyin == NULL) {
		warnx("can't re-open temporary file (%s), %s unchanged.",
		    stmp, sudoers);
		Exit(-1);
	    }

	    /* Add missing newline at EOF if needed. */
	    if (fseek(yyin, -1, SEEK_END) == 0 && (ch = fgetc(yyin)) != '\n')
		fputc('\n', yyin);
	    rewind(yyin);

	    /* Clean slate for each parse */
	    user_runas = NULL;
	    init_defaults();
	    init_parser();

	    /* Parse the sudoers temp file */
	    yyrestart(yyin);
	    if (yyparse() && parse_error != TRUE) {
		warnx("unabled to parse temporary file (%s), unknown error",
		    stmp);
		parse_error = TRUE;
	    }
	    fclose(yyin);
	} else {
	    warnx("editor (%s) failed, %s unchanged", Editor, sudoers);
	    Exit(-1);
	}

	/*
	 * Got an error, prompt the user for what to do now
	 */
	if (parse_error == TRUE) {
	    switch (whatnow()) {
		case 'Q' :	parse_error = FALSE;	/* ignore parse error */
				break;
		case 'x' :	if (sudoers_size == 0)
				    unlink(sudoers);
				Exit(0);
				break;
	    }
	}
    } while (parse_error == TRUE);

    /*
     * If the user didn't change the temp file, just unlink it.
     */
    if (sudoers_size == sb.st_size &&
	sudoers_mtim.tv_sec == mtim_getsec(sb) &&
	sudoers_mtim.tv_nsec == mtim_getnsec(sb)) {
	/*
	 * If mtime and size match but the user spent no measurable
	 * time in the editor we can't tell if the file was changed.
	 */
#ifdef HAVE_TIMESPECSUB2
	timespecsub(&ts1, &ts2);
#else
	timespecsub(&ts1, &ts2, &ts2);
#endif
	if (timespecisset(&ts2)) {
	    warnx("sudoers file unchanged");
	    Exit(0);
	}
    }

    /*
     * Change mode and ownership of temp file so when
     * we move it to sudoers things are kosher.
     */
    if (chown(stmp, SUDOERS_UID, SUDOERS_GID)) {
	warn("unable to set (uid, gid) of %s to (%d, %d)",
	    stmp, SUDOERS_UID, SUDOERS_GID);
	Exit(-1);
    }
    if (chmod(stmp, SUDOERS_MODE)) {
	warn("unable to change mode of %s to 0%o", stmp, SUDOERS_MODE);
	Exit(-1);
    }

    /*
     * Now that we have a sane stmp file (parses ok) it needs to be
     * rename(2)'d to sudoers.  If the rename(2) fails we try using
     * mv(1) in case stmp and sudoers are on different file systems.
     */
    if (rename(stmp, sudoers)) {
	if (errno == EXDEV) {
	    warnx("%s and %s not on the same file system, using mv to rename",
	      stmp, sudoers);

	    /* Build up argument vector for the command */
	    if ((av[0] = strrchr(_PATH_MV, '/')) != NULL)
		av[0]++;
	    else
		av[0] = _PATH_MV;
	    av[1] = stmp;
	    av[2] = sudoers;
	    av[3] = NULL;

	    /* And run it... */
	    if (run_command(_PATH_MV, av)) {
		warnx("command failed: '%s %s %s', %s unchanged",
		    _PATH_MV, stmp, sudoers, sudoers);
		Exit(-1);
	    }
	} else {
	    warn("error renaming %s, %s unchanged", stmp, sudoers);
	    Exit(-1);
	}
    }

    exit(0);
}

/*
 * Dummy *_matches routines.
 * These exist to allow us to use the same parser as sudo(8).
 */
int
command_matches(path, sudoers_args)
    char *path;
    char *sudoers_args;
{
    return(TRUE);
}

int
addr_matches(n)
    char *n;
{
    return(TRUE);
}

int
hostname_matches(s, l, p)
    char *s, *l, *p;
{
    return(TRUE);
}

int
usergr_matches(g, u, pw)
    char *g, *u;
    struct passwd *pw;
{
    return(TRUE);
}

int
userpw_matches(s, u, pw)
    char *s, *u;
    struct passwd *pw;
{
    return(TRUE);
}

int
netgr_matches(n, h, sh, u)
    char *n, *h, *sh, *u;
{
    return(TRUE);
}

void
set_fqdn()
{
    return;
}

int
set_runaspw(user)
    char *user;
{
    extern int sudolineno, used_runas;

    if (used_runas) {
	(void) fprintf(stderr,
	    "%s: runas_default set after old value is in use near line %d\n",
	    pedantic > 1 ? "Error" : "Warning", sudolineno);
	if (pedantic > 1)
	    yyerror(NULL);
    }
    return(TRUE);
}

int
user_is_exempt()
{
    return(TRUE);
}

void
init_envtables()
{
    return;
}

/*
 * Assuming a parse error occurred, prompt the user for what they want
 * to do now.  Returns the first letter of their choice.
 */
static char
whatnow()
{
    int choice, c;

    for (;;) {
	(void) fputs("What now? ", stdout);
	choice = getchar();
	for (c = choice; c != '\n' && c != EOF;)
	    c = getchar();

	switch (choice) {
	    case EOF:
		choice = 'x';
		/* FALLTHROUGH */
	    case 'e':
	    case 'x':
	    case 'Q':
		return(choice);
	    default:
		(void) puts("Options are:");
		(void) puts("  (e)dit sudoers file again");
		(void) puts("  e(x)it without saving changes to sudoers file");
		(void) puts("  (Q)uit and save changes to sudoers file (DANGER!)\n");
	}
    }
}

/*
 * Install signal handlers for visudo.
 */
static void
setup_signals()
{
	sigaction_t sa;

	/*
	 * Setup signal handlers to cleanup nicely.
	 */
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = Exit;
	(void) sigaction(SIGTERM, &sa, NULL);
	(void) sigaction(SIGHUP, &sa, NULL);
	(void) sigaction(SIGINT, &sa, NULL);
	(void) sigaction(SIGQUIT, &sa, NULL);
}

static int
run_command(path, argv)
    char *path;
    char **argv;
{
    int status;
    pid_t pid;
    sigset_t set, oset;

    (void) sigemptyset(&set);
    (void) sigaddset(&set, SIGCHLD);
    (void) sigprocmask(SIG_BLOCK, &set, &oset);

    switch (pid = fork()) {
	case -1:
	    warn("unable to run %s", path);
	    Exit(-1);
	    break;	/* NOTREACHED */
	case 0:
	    (void) sigprocmask(SIG_SETMASK, &oset, NULL);
	    execv(path, argv);
	    warn("unable to run %s", path);
	    _exit(127);
	    break;	/* NOTREACHED */
    }

#ifdef sudo_waitpid
    pid = sudo_waitpid(pid, &status, 0);
#else
    pid = wait(&status);
#endif

    (void) sigprocmask(SIG_SETMASK, &oset, NULL);

    if (pid == -1 || !WIFEXITED(status))
	return(-1);
    return(WEXITSTATUS(status));
}

static int
check_syntax(quiet)
    int quiet;
{

    if ((yyin = fopen(sudoers, "r")) == NULL) {
	if (!quiet)
	    warn("unable to open %s", sudoers);
	exit(1);
    }
    yyout = stdout;
    init_parser();
    if (yyparse() && parse_error != TRUE) {
	if (!quiet)
	    warnx("failed to parse %s file, unknown error", sudoers);
	parse_error = TRUE;
    }
    if (!quiet){
	if (parse_error)
	    (void) printf("parse error in %s near line %d\n", sudoers,
		errorlineno);
	else
	    (void) printf("%s file parsed OK\n", sudoers);
    }

    return(parse_error == TRUE);
}

/*
 * Unlink the sudoers temp file (if it exists) and exit.
 * Used in place of a normal exit() and as a signal handler.
 * A positive parameter indicates we were called as a signal handler.
 */
static RETSIGTYPE
Exit(sig)
    int sig;
{
#define	emsg	 " exiting due to signal.\n"

    (void) unlink(stmp);

    if (sig > 0) {
	write(STDERR_FILENO, getprogname(), strlen(getprogname()));
	write(STDERR_FILENO, emsg, sizeof(emsg) - 1);
	_exit(sig);
    }
    exit(-sig);
}

static void
usage()
{
    (void) fprintf(stderr, "usage: %s [-c] [-f sudoers] [-q] [-s] [-V]\n",
	getprogname());
    exit(1);
}
