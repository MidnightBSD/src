/*	$OpenBSD: edit.c,v 1.16 2007/04/25 05:02:17 ray Exp $ */

/*
 * Written by Raymond Lai <ray@cyth.net>.
 * Public domain.
 */

#include <sys/types.h>
#include <sys/wait.h>

#include <ctype.h>
__MBSDID("$MidnightBSD$");

#include <err.h>
#include <errno.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
#include "extern.h"

int editit(const char *);

/*
 * Takes the name of a file and opens it with an editor.
 */
int
editit(const char *pathname)
{
	char *argp[] = {"sh", "-c", NULL, NULL}, *ed, *p;
	sig_t sighup, sigint, sigquit;
	pid_t pid;
	int st;

	ed = getenv("VISUAL");
	if (ed == NULL || ed[0] == '\0')
		ed = getenv("EDITOR");
	if (ed == NULL || ed[0] == '\0')
		ed = _PATH_VI;
	if (asprintf(&p, "%s %s", ed, pathname) == -1)
		return (-1);
	argp[2] = p;

 top:
	sighup = signal(SIGHUP, SIG_IGN);
	sigint = signal(SIGINT, SIG_IGN);
	sigquit = signal(SIGQUIT, SIG_IGN);
	if ((pid = fork()) == -1) {
		int saved_errno = errno;

		(void)signal(SIGHUP, sighup);
		(void)signal(SIGINT, sigint);
		(void)signal(SIGQUIT, sigquit);
		if (saved_errno == EAGAIN) {
			sleep(1);
			goto top;
		}
		free(p);
		errno = saved_errno;
		return (-1);
	}
	if (pid == 0) {
		execv(_PATH_BSHELL, argp);
		_exit(127);
	}
	free(p);
	for (;;) {
		if (waitpid(pid, &st, 0) == -1) {
			if (errno != EINTR)
				return (-1);
		} else
			break;
	}
	(void)signal(SIGHUP, sighup);
	(void)signal(SIGINT, sigint);
	(void)signal(SIGQUIT, sigquit);
	if (!WIFEXITED(st) || WEXITSTATUS(st) != 0) {
		errno = ECHILD;
		return (-1);
	}
	return (0);
}

/*
 * Parse edit command.  Returns 0 on success, -1 on error.
 */
int
eparse(const char *cmd, const char *left, const char *right)
{
	FILE *file;
	size_t nread;
	int fd;
	char *filename;
	char buf[BUFSIZ], *text;

	/* Skip whitespace. */
	while (isspace(*cmd))
		++cmd;

	text = NULL;
	switch (*cmd) {
	case '\0':
		/* Edit empty file. */
		break;

	case 'b':
		/* Both strings. */
		if (left == NULL)
			goto RIGHT;
		if (right == NULL)
			goto LEFT;

		/* Neither column is blank, so print both. */
		if (asprintf(&text, "%s\n%s\n", left, right) == -1)
			err(2, "could not allocate memory");
		break;

	case 'l':
LEFT:
		/* Skip if there is no left column. */
		if (left == NULL)
			break;

		if (asprintf(&text, "%s\n", left) == -1)
			err(2, "could not allocate memory");

		break;

	case 'r':
RIGHT:
		/* Skip if there is no right column. */
		if (right == NULL)
			break;

		if (asprintf(&text, "%s\n", right) == -1)
			err(2, "could not allocate memory");

		break;

	default:
		return (-1);
	}

	/* Create temp file. */
	if (asprintf(&filename, "%s/sdiff.XXXXXXXXXX", tmpdir) == -1)
		err(2, "asprintf");
	if ((fd = mkstemp(filename)) == -1)
		err(2, "mkstemp");
	if (text != NULL) {
		ssize_t len;
		ssize_t nwritten;

		len = strlen(text);
		if ((nwritten = write(fd, text, len)) == -1 ||
		    nwritten != len) {
			warn("error writing to temp file");
			cleanup(filename);
		}
	}
	close(fd);

	/* text is no longer used. */
	free(text);

	/* Edit temp file. */
	if (editit(filename) == -1) {
		if (errno == ECHILD)
			warnx("editor terminated abnormally");
		else
			warn("error editing %s", filename);
		cleanup(filename);
	}

	/* Open temporary file. */
	if (!(file = fopen(filename, "r"))) {
		warn("could not open edited file: %s", filename);
		cleanup(filename);
	}

	/* Copy temporary file contents to output file. */
	for (nread = sizeof(buf); nread == sizeof(buf);) {
		size_t nwritten;

		nread = fread(buf, sizeof(*buf), sizeof(buf), file);
		/* Test for error or end of file. */
		if (nread != sizeof(buf) &&
		    (ferror(file) || !feof(file))) {
			warnx("error reading edited file: %s", filename);
			cleanup(filename);
		}

		/*
		 * If we have nothing to read, break out of loop
		 * instead of writing nothing.
		 */
		if (!nread)
			break;

		/* Write data we just read. */
		nwritten = fwrite(buf, sizeof(*buf), nread, outfile);
		if (nwritten != nread) {
			warnx("error writing to output file");
			cleanup(filename);
		}
	}

	/* We've reached the end of the temporary file, so remove it. */
	if (unlink(filename))
		warn("could not delete: %s", filename);
	fclose(file);

	free(filename);

	return (0);
}
