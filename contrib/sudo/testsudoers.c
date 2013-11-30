/*
 * Copyright (c) 1996, 1998-2005 Todd C. Miller <Todd.Miller@courtesan.com>
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
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F39502-99-1-0512.
 */

#define _SUDO_MAIN

#include <config.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
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
# include <unistd.h>
#endif /* HAVE_UNISTD_H */
#ifdef HAVE_FNMATCH
# include <fnmatch.h>
#endif /* HAVE_FNMATCH */
#ifdef HAVE_NETGROUP_H
# include <netgroup.h>
#endif /* HAVE_NETGROUP_H */
#ifdef HAVE_ERR_H
# include <err.h>
#else
# include "emul/err.h"
#endif /* HAVE_ERR_H */
#include <ctype.h>
#include <pwd.h>
#include <grp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <dirent.h>

#include "sudo.h"
#include "parse.h"
#include "interfaces.h"

#ifndef HAVE_FNMATCH
# include "emul/fnmatch.h"
#endif /* HAVE_FNMATCH */

#ifndef lint
__unused static const char rcsid[] = "$Sudo: testsudoers.c,v 1.88.2.8 2008/10/29 17:31:58 millert Exp $";
#endif /* lint */


/*
 * Prototypes
 */
void init_parser	__P((void));
void dumpaliases	__P((void));

/*
 * Globals
 */
int  Argc, NewArgc;
char **Argv, **NewArgv;
int parse_error = FALSE;
int num_interfaces;
struct interface *interfaces;
struct sudo_user sudo_user;
extern int clearaliases;
extern int pedantic;

/*
 * Returns TRUE if "s" has shell meta characters in it,
 * else returns FALSE.
 */
int
has_meta(s)
    char *s;
{
    char *t;

    for (t = s; *t; t++) {
	if (*t == '\\' || *t == '?' || *t == '*' || *t == '[' || *t == ']')
	    return(TRUE);
    }
    return(FALSE);
}

/*
 * Returns TRUE if user_cmnd matches, in the sudo sense,
 * the pathname in path; otherwise, return FALSE
 */
int
command_matches(path, sudoers_args)
    char *path;
    char *sudoers_args;
{
    int clen, plen;
    char *args;

    if (user_cmnd == NULL)
	return(FALSE);

    if ((args = strchr(path, ' ')))
	*args++ = '\0';

    if (has_meta(path)) {
	if (fnmatch(path, user_cmnd, FNM_PATHNAME))
	    return(FALSE);
	if (!sudoers_args)
	    return(TRUE);
	else if (!user_args && sudoers_args && !strcmp("\"\"", sudoers_args))
	    return(TRUE);
	else if (sudoers_args)
	    return((fnmatch(sudoers_args, user_args ? user_args : "", 0) == 0));
	else
	    return(FALSE);
    } else {
	plen = strlen(path);
	if (path[plen - 1] != '/') {
	    if (strcmp(user_cmnd, path))
		return(FALSE);
	    if (!sudoers_args)
		return(TRUE);
	    else if (!user_args && sudoers_args && !strcmp("\"\"", sudoers_args))
		return(TRUE);
	    else if (sudoers_args)
		return((fnmatch(sudoers_args, user_args ? user_args : "", 0) == 0));
	    else
		return(FALSE);
	}

	clen = strlen(user_cmnd);
	if (clen < plen + 1)
	    /* path cannot be the parent dir of user_cmnd */
	    return(FALSE);

	if (strchr(user_cmnd + plen + 1, '/') != NULL)
	    /* path could only be an anscestor of user_cmnd -- */
	    /* ignoring, of course, things like // & /./  */
	    return(FALSE);

	/* see whether path is the prefix of user_cmnd */
	return((strncmp(user_cmnd, path, plen) == 0));
    }
}

static int
addr_matches_if(n)
    char *n;
{
    int i;
    struct in_addr addr;
    struct interface *ifp;
#ifdef HAVE_IN6_ADDR
    struct in6_addr addr6;
    int j;
#endif
    int family;

#ifdef HAVE_IN6_ADDR
    if (inet_pton(AF_INET6, n, &addr6) > 0) {
	family = AF_INET6;
    } else
#endif
    {
	family = AF_INET;
	addr.s_addr = inet_addr(n);
    }

    for (i = 0; i < num_interfaces; i++) {
	ifp = &interfaces[i];
	if (ifp->family != family)
	    continue;
	switch(family) {
	    case AF_INET:
		if (ifp->addr.ip4.s_addr == addr.s_addr ||
		    (ifp->addr.ip4.s_addr & ifp->netmask.ip4.s_addr)
		    == addr.s_addr)
		    return(TRUE);
		break;
#ifdef HAVE_IN6_ADDR
	    case AF_INET6:
		if (memcmp(ifp->addr.ip6.s6_addr, addr6.s6_addr,
		    sizeof(addr6.s6_addr)) == 0)
		    return(TRUE);
		for (j = 0; j < sizeof(addr6.s6_addr); j++) {
		    if ((ifp->addr.ip6.s6_addr[j] & ifp->netmask.ip6.s6_addr[j]) != addr6.s6_addr[j])
			break;
		}
		if (j == sizeof(addr6.s6_addr))
		    return(TRUE);
#endif /* HAVE_IN6_ADDR */
	}
    }

    return(FALSE);
}

static int
addr_matches_if_netmask(n, m)
    char *n;
    char *m;
{
    int i;
    struct in_addr addr, mask;
    struct interface *ifp;
#ifdef HAVE_IN6_ADDR
    struct in6_addr addr6, mask6;
    int j;
#endif
    int family;

#ifdef HAVE_IN6_ADDR
    if (inet_pton(AF_INET6, n, &addr6) > 0)
	family = AF_INET6;
    else
#endif
    {
	family = AF_INET;
	addr.s_addr = inet_addr(n);
    }

    if (family == AF_INET) {
	if (strchr(m, '.'))
	    mask.s_addr = inet_addr(m);
	else {
	    i = 32 - atoi(m);
	    mask.s_addr = 0xffffffff;
	    mask.s_addr >>= i;
	    mask.s_addr <<= i;
	    mask.s_addr = htonl(mask.s_addr);
	}
    }
#ifdef HAVE_IN6_ADDR
    else {
	if (inet_pton(AF_INET6, m, &mask6) <= 0) {
	    j = atoi(m);
	    for (i = 0; i < 16; i++) {
		if (j < i * 8)
		    mask6.s6_addr[i] = 0;
		else if (i * 8 + 8 <= j)
		    mask6.s6_addr[i] = 0xff;
		else
		    mask6.s6_addr[i] = 0xff00 >> (j - i * 8);
	    }
	}
    }
#endif /* HAVE_IN6_ADDR */

    for (i = 0; i < num_interfaces; i++) {
	ifp = &interfaces[i];
	if (ifp->family != family)
	    continue;
	switch(family) {
	    case AF_INET:
		if ((ifp->addr.ip4.s_addr & mask.s_addr) == addr.s_addr)
		    return(TRUE);
#ifdef HAVE_IN6_ADDR
	    case AF_INET6:
		for (j = 0; j < sizeof(addr6.s6_addr); j++) {
		    if ((ifp->addr.ip6.s6_addr[j] & mask6.s6_addr[j]) != addr6.s6_addr[j])
			break;
		}
		if (j == sizeof(addr6.s6_addr))
		    return(TRUE);
#endif /* HAVE_IN6_ADDR */
	}
    }

    return(FALSE);
}

/*
 * Returns TRUE if "n" is one of our ip addresses or if
 * "n" is a network that we are on, else returns FALSE.
 */
int
addr_matches(n)
    char *n;
{
    char *m;
    int retval;

    /* If there's an explicit netmask, use it. */
    if ((m = strchr(n, '/'))) {
	*m++ = '\0';
	retval = addr_matches_if_netmask(n, m);
	*(m - 1) = '/';
    } else
	retval = addr_matches_if(n);

    return(retval);
}

int
hostname_matches(shost, lhost, pattern)
    char *shost;
    char *lhost;
    char *pattern;
{
    if (has_meta(pattern)) {
        if (strchr(pattern, '.'))
            return(fnmatch(pattern, lhost, FNM_CASEFOLD));
        else
            return(fnmatch(pattern, shost, FNM_CASEFOLD));
    } else {
        if (strchr(pattern, '.'))
            return(strcasecmp(lhost, pattern));
        else
            return(strcasecmp(shost, pattern));
    }
}

int
userpw_matches(sudoers_user, user, pw)
    char *sudoers_user;
    char *user;
    struct passwd *pw;
{
    if (pw != NULL && *sudoers_user == '#') {
	uid_t uid = atoi(sudoers_user + 1);
	if (uid == pw->pw_uid)
	    return(1);
    }
    return(strcmp(sudoers_user, user) == 0);
}

int
usergr_matches(group, user, pw)
    char *group;
    char *user;
    struct passwd *pw;
{
    struct group *grp;
    char **cur;

    /* Make sure we have a valid usergroup, sudo style. */
    if (*group++ != '%')
	return(FALSE);

    if ((grp = getgrnam(group)) == NULL)
	return(FALSE);

    /*
     * Check against user's real gid as well as group's user list
     */
    if (getgid() == grp->gr_gid)
	return(TRUE);

    for (cur=grp->gr_mem; *cur; cur++) {
	if (strcmp(*cur, user) == 0)
	    return(TRUE);
    }

    return(FALSE);
}

int
netgr_matches(netgr, host, shost, user)
    char *netgr;
    char *host;
    char *shost;
    char *user;
{
#ifdef HAVE_GETDOMAINNAME
    static char *domain = (char *) -1;
#else
    static char *domain = NULL;
#endif /* HAVE_GETDOMAINNAME */

    /* Make sure we have a valid netgroup, sudo style. */
    if (*netgr++ != '+')
	return(FALSE);

#ifdef HAVE_GETDOMAINNAME
    /* Get the domain name (if any). */
    if (domain == (char *) -1) {
	domain = (char *) emalloc(MAXHOSTNAMELEN + 1);

	if (getdomainname(domain, MAXHOSTNAMELEN + 1) != 0 || *domain == '\0') {
	    efree(domain);
	    domain = NULL;
	}
    }
#endif /* HAVE_GETDOMAINNAME */

#ifdef HAVE_INNETGR
    if (innetgr(netgr, host, user, domain))
	return(TRUE);
    else if (host != shost && innetgr(netgr, shost, user, domain))
	return(TRUE);
#endif /* HAVE_INNETGR */

    return(FALSE);
}

void
set_perms(i)
    int i;
{
    return;
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
    return(TRUE);
}

void
init_envtables()
{
    return;
}

int
main(argc, argv)
    int argc;
    char **argv;
{
    struct passwd pw;
    char *p;
#ifdef	YYDEBUG
    extern int yydebug;
    yydebug = 1;
#endif

    Argv = argv;
    Argc = argc;

    if (Argc >= 6 && strcmp(Argv[1], "-u") == 0) {
	user_runas = &Argv[2];
	pw.pw_name = Argv[3];
	user_host = Argv[4];
	user_cmnd = Argv[5];

	NewArgv = &Argv[5];
	NewArgc = Argc - 5;
    } else if (Argc >= 4) {
	pw.pw_name = Argv[1];
	user_host = Argv[2];
	user_cmnd = Argv[3];

	NewArgv = &Argv[3];
	NewArgc = Argc - 3;
    } else {
	(void) fprintf(stderr,
	    "usage: sudo [-u user] <user> <host> <command> [args]\n");
	exit(1);
    }

    sudo_user.pw = &pw;		/* user_name needs to be defined */

    if ((p = strchr(user_host, '.'))) {
	*p = '\0';
	user_shost = estrdup(user_host);
	*p = '.';
    } else {
	user_shost = user_host;
    }

    /* Fill in user_args from NewArgv. */
    if (NewArgc > 1) {
	char *to, **from;
	size_t size, n;

	size = (size_t) (NewArgv[NewArgc-1] - NewArgv[1]) +
		strlen(NewArgv[NewArgc-1]) + 1;
	user_args = (char *) emalloc(size);
	for (to = user_args, from = NewArgv + 1; *from; from++) {
	    n = strlcpy(to, *from, size - (to - user_args));
	    if (n >= size - (to - user_args))
		    errx(1, "internal error, init_vars() overflow");
	    to += n;
	    *to++ = ' ';
	}
	*--to = '\0';
    }

    /* Initialize default values. */
    init_defaults();

    /* Warn about aliases that are used before being defined. */
    pedantic = TRUE;

    /* Need to keep aliases around for dumpaliases(). */
    clearaliases = FALSE;

    /* Load ip addr/mask for each interface. */
    load_interfaces();

    /* Allocate space for data structures in the parser. */
    init_parser();

    if (yyparse() || parse_error) {
	(void) printf("doesn't parse.\n");
    } else {
	(void) printf("parses OK.\n\n");
	if (top == 0)
	    (void) printf("User %s not found\n", pw.pw_name);
	else while (top) {
	    (void) printf("[%d]\n", top-1);
	    (void) printf("user_match : %d\n", user_matches);
	    (void) printf("host_match : %d\n", host_matches);
	    (void) printf("cmnd_match : %d\n", cmnd_matches);
	    (void) printf("no_passwd  : %d\n", no_passwd);
	    (void) printf("runas_match: %d\n", runas_matches);
	    (void) printf("runas      : %s\n", *user_runas);
	    if (match[top-1].role)
		(void) printf("role       : %s\n", match[top-1].role);
	    if (match[top-1].type)
		(void) printf("type       : %s\n", match[top-1].type);
	    top--;
	}
    }

    /* Dump aliases. */
    (void) printf("Matching Aliases --\n");
    dumpaliases();

    exit(0);
}
