/*
 * Copyright (c) 2005, 2008, 2010 Todd C. Miller <Todd.Miller@courtesan.com>
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

/*
 * Trivial replacements for the libc get{gr,pw}{uid,nam}() routines
 * for use by testsudoers in the sudo test harness.
 * We need our own since many platforms don't provide set{pw,gr}file().
 */

#include <config.h>

#include <sys/types.h>
#include <sys/param.h>
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
#include <fcntl.h>
#include <limits.h>
#include <pwd.h>
#include <grp.h>

#include "sudo.h"

#ifndef LINE_MAX
# define LINE_MAX 2048
#endif

#undef GRMEM_MAX
#define GRMEM_MAX 200

static FILE *pwf;
static const char *pwfile = "/etc/passwd";
static int pw_stayopen;

static FILE *grf;
static const char *grfile = "/etc/group";
static int gr_stayopen;

void setgrfile __P((const char *));
void setgrent __P((void));
void endgrent __P((void));
struct group *getgrent __P((void));
struct group *getgrnam __P((const char *));
struct group *getgrgid __P((gid_t));

void setpwfile __P((const char *));
void setpwent __P((void));
void endpwent __P((void));
struct passwd *getpwent __P((void));
struct passwd *getpwnam __P((const char *));
struct passwd *getpwuid __P((uid_t));

void
setpwfile(file)
    const char *file;
{
    pwfile = file;
    if (pwf != NULL)
	endpwent();
}

void
setpwent()
{
    if (pwf == NULL) {
	pwf = fopen(pwfile, "r");
	if (pwf != NULL)
	    fcntl(fileno(pwf), F_SETFD, FD_CLOEXEC);
    } else {
	rewind(pwf);
    }
    pw_stayopen = 1;
}

void
endpwent()
{
    if (pwf != NULL) {
	fclose(pwf);
	pwf = NULL;
    }
    pw_stayopen = 0;
}

struct passwd *
getpwent()
{
    static struct passwd pw;
    static char pwbuf[LINE_MAX];
    size_t len;
    char *cp, *colon;

    if ((colon = fgets(pwbuf, sizeof(pwbuf), pwf)) == NULL)
	return(NULL);

    zero_bytes(&pw, sizeof(pw));
    if ((colon = strchr(cp = colon, ':')) == NULL)
	return(NULL);
    *colon++ = '\0';
    pw.pw_name = cp;
    if ((colon = strchr(cp = colon, ':')) == NULL)
	return(NULL);
    *colon++ = '\0';
    pw.pw_passwd = cp;
    if ((colon = strchr(cp = colon, ':')) == NULL)
	return(NULL);
    *colon++ = '\0';
    pw.pw_uid = atoi(cp);
    if ((colon = strchr(cp = colon, ':')) == NULL)
	return(NULL);
    *colon++ = '\0';
    pw.pw_gid = atoi(cp);
    if ((colon = strchr(cp = colon, ':')) == NULL)
	return(NULL);
    *colon++ = '\0';
    pw.pw_gecos = cp;
    if ((colon = strchr(cp = colon, ':')) == NULL)
	return(NULL);
    *colon++ = '\0';
    pw.pw_dir = cp;
    pw.pw_shell = colon;
    len = strlen(colon);
    if (len > 0 && colon[len - 1] == '\n')
	colon[len - 1] = '\0';
    return(&pw);
}

struct passwd *
getpwnam(name)
    const char *name;
{
    struct passwd *pw;

    if (pwf == NULL) {
	if ((pwf = fopen(pwfile, "r")) == NULL)
	    return(NULL);
	fcntl(fileno(pwf), F_SETFD, FD_CLOEXEC);
    } else {
	rewind(pwf);
    }
    while ((pw = getpwent()) != NULL) {
	if (strcmp(pw->pw_name, name) == 0)
	    break;
    }
    if (!pw_stayopen) {
	fclose(pwf);
	pwf = NULL;
    }
    return(pw);
}

struct passwd *
getpwuid(uid)
    uid_t uid;
{
    struct passwd *pw;

    if (pwf == NULL) {
	if ((pwf = fopen(pwfile, "r")) == NULL)
	    return(NULL);
	fcntl(fileno(pwf), F_SETFD, FD_CLOEXEC);
    } else {
	rewind(pwf);
    }
    while ((pw = getpwent()) != NULL) {
	if (pw->pw_uid == uid)
	    break;
    }
    if (!pw_stayopen) {
	fclose(pwf);
	pwf = NULL;
    }
    return(pw);
}

void
setgrfile(file)
    const char *file;
{
    grfile = file;
    if (grf != NULL)
	endgrent();
}

void
setgrent()
{
    if (grf == NULL) {
	grf = fopen(grfile, "r");
	if (grf != NULL)
	    fcntl(fileno(grf), F_SETFD, FD_CLOEXEC);
    } else {
	rewind(grf);
    }
    gr_stayopen = 1;
}

void
endgrent()
{
    if (grf != NULL) {
	fclose(grf);
	grf = NULL;
    }
    gr_stayopen = 0;
}

struct group *
getgrent()
{
    static struct group gr;
    static char grbuf[LINE_MAX], *gr_mem[GRMEM_MAX+1];
    size_t len;
    char *cp, *colon;
    int n;

    if ((colon = fgets(grbuf, sizeof(grbuf), grf)) == NULL)
	return(NULL);

    zero_bytes(&gr, sizeof(gr));
    if ((colon = strchr(cp = colon, ':')) == NULL)
	return(NULL);
    *colon++ = '\0';
    gr.gr_name = cp;
    if ((colon = strchr(cp = colon, ':')) == NULL)
	return(NULL);
    *colon++ = '\0';
    gr.gr_passwd = cp;
    if ((colon = strchr(cp = colon, ':')) == NULL)
	return(NULL);
    *colon++ = '\0';
    gr.gr_gid = atoi(cp);
    len = strlen(colon);
    if (len > 0 && colon[len - 1] == '\n')
	colon[len - 1] = '\0';
    if (*colon != '\0') {
	gr.gr_mem = gr_mem;
	cp = strtok(colon, ",");
	for (n = 0; cp != NULL && n < GRMEM_MAX; n++) {
	    gr.gr_mem[n] = cp;
	    cp = strtok(NULL, ",");
	}
	gr.gr_mem[n++] = NULL;
    } else
	gr.gr_mem = NULL;
    return(&gr);
}

struct group *
getgrnam(name)
    const char *name;
{
    struct group *gr;

    if (grf == NULL) {
	if ((grf = fopen(grfile, "r")) == NULL)
	    return(NULL);
	fcntl(fileno(grf), F_SETFD, FD_CLOEXEC);
    } else {
	rewind(grf);
    }
    while ((gr = getgrent()) != NULL) {
	if (strcmp(gr->gr_name, name) == 0)
	    break;
    }
    if (!gr_stayopen) {
	fclose(grf);
	grf = NULL;
    }
    return(gr);
}

struct group *
getgrgid(gid)
    gid_t gid;
{
    struct group *gr;

    if (grf == NULL) {
	if ((grf = fopen(grfile, "r")) == NULL)
	    return(NULL);
	fcntl(fileno(grf), F_SETFD, FD_CLOEXEC);
    } else {
	rewind(grf);
    }
    while ((gr = getgrent()) != NULL) {
	if (gr->gr_gid == gid)
	    break;
    }
    if (!gr_stayopen) {
	fclose(grf);
	grf = NULL;
    }
    return(gr);
}
