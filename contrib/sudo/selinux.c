/*
 * Copyright (c) 2008 Dan Walsh <dwalsh@redhat.com>
 *
 * Borrowed heavily from newrole source code
 * Authors:
 *	Anthony Colatrella
 *	Tim Fraser
 *	Steve Grubb <sgrubb@redhat.com>
 *	Darrel Goeddel <DGoeddel@trustedcs.com>
 *	Michael Thompson <mcthomps@us.ibm.com>
 *	Dan Walsh <dwalsh@redhat.com>
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
 */

#include <config.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#ifdef WITH_AUDIT
#include <libaudit.h>
#endif

#include <selinux/flask.h>             /* for SECCLASS_CHR_FILE */
#include <selinux/selinux.h>           /* for is_selinux_enabled() */
#include <selinux/context.h>           /* for context-mangling functions */
#include <selinux/get_default_type.h>
#include <selinux/get_context_list.h>

#include "sudo.h"
#include "pathnames.h"

#ifndef lint
__unused static const char rcsid[] = "$Sudo: selinux.c,v 1.5 2008/02/22 20:33:00 millert Exp $";
#endif /* lint */

/*
 * This function attempts to revert the relabeling done to the tty.
 * fd		   - referencing the opened ttyn
 * ttyn		   - name of tty to restore
 * tty_context	   - original context of the tty
 * new_tty_context - context tty was relabeled to
 *
 * Returns zero on success, non-zero otherwise
 */
static int
restore_tty_label(int fd, const char *ttyn, security_context_t tty_context,
    security_context_t new_tty_context)
{
    int rc = 0;
    security_context_t chk_tty_context = NULL;

    if (!ttyn)
	    goto skip_relabel;

    if (!new_tty_context)
	    goto skip_relabel;

    /* Verify that the tty still has the context set by sudo. */
    if ((rc = fgetfilecon(fd, &chk_tty_context)) < 0) {
	    warning("unable to fgetfilecon %s", ttyn);
	    goto skip_relabel;
    }

    if ((rc = strcmp(chk_tty_context, new_tty_context))) {
	    warningx("%s changed labels.", ttyn);
	    goto skip_relabel;
    }

    if ((rc = fsetfilecon(fd, tty_context)) < 0)
	warning("unable to restore context for %s", ttyn);

skip_relabel:
    freecon(chk_tty_context);
    return(rc);
}

/*
 * This function attempts to relabel the tty. If this function fails, then
 * the fd is closed, the contexts are free'd and -1 is returned. On success,
 * a valid fd is returned and tty_context and new_tty_context are set.
 *
 * This function will not fail if it can not relabel the tty when selinux is
 * in permissive mode.
 */
static int
relabel_tty(const char *ttyn, security_context_t new_context,
    security_context_t * tty_context, security_context_t * new_tty_context,
    int enforcing)
{
    int fd;
    security_context_t tty_con = NULL;
    security_context_t new_tty_con = NULL;

    if (!ttyn)
	return(0);

    /* Re-open TTY descriptor */
    fd = open(ttyn, O_RDWR | O_NONBLOCK);
    if (fd == -1) {
	warning("unable to open %s", ttyn);
	return(-1);
    }
    (void)fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) & ~O_NONBLOCK);

    if (fgetfilecon(fd, &tty_con) < 0) {
	warning("unable to get current context for %s, not relabeling tty",
	    ttyn);
	if (enforcing)
	    goto error;
    }

    if (tty_con && (security_compute_relabel(new_context, tty_con,
	SECCLASS_CHR_FILE, &new_tty_con) < 0)) {
	warning("unable to get new context for %s, not relabeling tty", ttyn);
	if (enforcing)
	    goto error;
    }

    if (new_tty_con != NULL) {
	if (fsetfilecon(fd, new_tty_con) < 0) {
	    warning("unable to set new context for %s", ttyn);
	    if (enforcing)
		goto error;
	}
    }

    *tty_context = tty_con;
    *new_tty_context = new_tty_con;
    return(fd);

error:
    freecon(tty_con);
    close(fd);
    return(-1);
}

/*
 * Returns a new security context based on the old context and the
 * specified role and type.
 */
security_context_t
get_exec_context(security_context_t old_context, char *role, char *type)
{
    security_context_t new_context = NULL;
    context_t context = NULL;
    char *typebuf = NULL;
    
    /* We must have a role, the type is optional (we can use the default). */
    if (!role) {
	warningx("you must specify a role.");
	return(NULL);
    }
    if (!type) {
	if (get_default_type(role, &typebuf)) {
	    warningx("unable to get default type");
	    return(NULL);
	}
	type = typebuf;
    }
    
    /* 
     * Expand old_context into a context_t so that we extract and modify 
     * its components easily. 
     */
    context = context_new(old_context);
    
    /*
     * Replace the role and type in "context" with the role and
     * type we will be running the command as.
     */
    if (context_role_set(context, role)) {
	warningx("failed to set new role %s", role);
	goto error;
    }
    if (context_type_set(context, type)) {
	warningx("failed to set new type %s", type);
	goto error;
    }
      
    /*
     * Convert "context" back into a string and verify it.
     */
    new_context = estrdup(context_str(context));
    if (security_check_context(new_context) < 0) {
	warningx("%s is not a valid context", new_context);
	goto error;
    }

#ifdef DEBUG
    warningx("Your new context is %s", new_context);
#endif

    context_free(context);
    return(new_context);

error:
    free(typebuf);
    context_free(context);
    freecon(new_context);
    return(NULL);
}

/* 
 * If the program is being run with a different security context we
 * need to go through an intermediary process for the transition to
 * be allowed by the policy.  We use the "sesh" shell for this, which
 * will simply execute the command pass to it on the command line.
 */
void
selinux_exec(char *role, char *type, char **argv, int login_shell)
{
    security_context_t old_context = NULL;
    security_context_t new_context = NULL;
    security_context_t tty_context = NULL;
    security_context_t new_tty_context = NULL;
    pid_t childPid;
    int enforcing, ttyfd;

    /* Must have a tty. */
    if (user_ttypath == NULL || *user_ttypath == '\0')
	error(EXIT_FAILURE, "unable to determine tty");

    /* Store the caller's SID in old_context. */
    if (getprevcon(&old_context))
	error(EXIT_FAILURE, "failed to get old_context");

    enforcing = security_getenforce();
    if (enforcing < 0)
	error(EXIT_FAILURE, "unable to determine enforcing mode.");

    
#ifdef DEBUG
    warningx("your old context was %s", old_context);
#endif
    new_context = get_exec_context(old_context, role, type);
    if (!new_context)
	exit(EXIT_FAILURE);
    
    ttyfd = relabel_tty(user_ttypath, new_context, &tty_context,
	&new_tty_context, enforcing);
    if (ttyfd < 0)
	error(EXIT_FAILURE, "unable to setup tty context for %s", new_context);

#ifdef DEBUG
    warningx("your old tty context is %s", tty_context);
    warningx("your new tty context is %s", new_tty_context);
#endif

    childPid = fork();
    if (childPid < 0) {
	/* fork failed, no child to worry about */
	warning("unable to fork");
	if (restore_tty_label(ttyfd, user_ttypath, tty_context, new_tty_context))
	    warningx("unable to restore tty label");
	exit(EXIT_FAILURE);
    } else if (childPid) {
	pid_t pid;
	int status;
	
	/* Parent, wait for child to finish. */
	do {
		pid = waitpid(childPid, &status, 0);
	} while (pid == -1 && errno == EINTR);

	if (pid == -1)
	    error(EXIT_FAILURE, "waitpid");
	
	if (restore_tty_label(ttyfd, user_ttypath, tty_context, new_tty_context))
	    errorx(EXIT_FAILURE, "unable to restore tty label");

	/* Preserve child exit status. */
	if (WIFEXITED(status))
	    exit(WEXITSTATUS(status));
	exit(EXIT_FAILURE);
    }
    /* Child */
    /* Close the tty and reopen descriptors 0 through 2 */
    if (close(ttyfd) || close(STDIN_FILENO) || close(STDOUT_FILENO) ||
	close(STDERR_FILENO)) {
	warning("could not close descriptors");
	goto error;
    }
    ttyfd = open(user_ttypath, O_RDONLY | O_NONBLOCK);
    if (ttyfd != STDIN_FILENO)
	goto error;
    fcntl(ttyfd, F_SETFL, fcntl(ttyfd, F_GETFL, 0) & ~O_NONBLOCK);
    ttyfd = open(user_ttypath, O_RDWR | O_NONBLOCK);
    if (ttyfd != STDOUT_FILENO)
	goto error;
    fcntl(ttyfd, F_SETFL, fcntl(ttyfd, F_GETFL, 0) & ~O_NONBLOCK);
    ttyfd = dup(STDOUT_FILENO);
    if (ttyfd != STDERR_FILENO)
	goto error;

    if (setexeccon(new_context)) {
	warning("unable to set exec context to %s", new_context);
	if (enforcing)
	    goto error;
    }

    if (setkeycreatecon(new_context)) {
	warning("unable to set key creation context to %s", new_context);
	if (enforcing)
	    goto error;
    }

#ifdef WITH_AUDIT
    if (send_audit_message(1, old_context, new_context, user_ttypath)) 
	goto error;
#endif

    /* We use the "spare" slot in argv to store sesh. */
    --argv;
    argv[0] = login_shell ? "-sesh" : "sesh";
    argv[1] = safe_cmnd;

    execv(_PATH_SUDO_SESH, argv);
    warning("%s", safe_cmnd);

error:
    _exit(EXIT_FAILURE);
}
