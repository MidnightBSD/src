/*	$NetBSD: getrpcent.c,v 1.17 2000/01/22 22:19:17 mycroft Exp $	*/

/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user or with the express written consent of
 * Sun Microsystems, Inc.
 *
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 *
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 *
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 *
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 *
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char *sccsid = "@(#)getrpcent.c 1.14 91/03/11 Copyr 1984 Sun Micro";
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/libc/rpc/getrpcent.c,v 1.14 2003/02/27 13:40:01 nectar Exp $");

/*
 * Copyright (c) 1984 by Sun Microsystems, Inc.
 */

#include "namespace.h"
#include <sys/types.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <assert.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rpc/rpc.h>
#ifdef YP
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>
#endif
#include "un-namespace.h"
#include "libc_private.h"

/*
 * Internet version.
 */
static struct rpcdata {
	FILE	*rpcf;
	int	stayopen;
#define	MAXALIASES	35
	char	*rpc_aliases[MAXALIASES];
	struct	rpcent rpc;
	char	line[BUFSIZ+1];
#ifdef	YP
	char	*domain;
	char	*current;
	int	currentlen;
#endif
} *rpcdata;

static	struct rpcent *interpret(char *val, size_t len);

#ifdef	YP
static int	__yp_nomap = 0;
#endif	/* YP */

#define	RPCDB	"/etc/rpc"

static struct rpcdata *_rpcdata(void);

static struct rpcdata *
_rpcdata()
{
	struct rpcdata *d = rpcdata;

	if (d == 0) {
		d = (struct rpcdata *)calloc(1, sizeof (struct rpcdata));
		rpcdata = d;
	}
	return (d);
}

struct rpcent *
getrpcbynumber(number)
	int number;
{
#ifdef	YP
	int reason;
	char adrstr[16];
#endif
	struct rpcent *p;
	struct rpcdata *d = _rpcdata();

	if (d == 0)
		return (0);
#ifdef	YP
        if (!__yp_nomap && _yp_check(&d->domain)) {
                sprintf(adrstr, "%d", number);
                reason = yp_match(d->domain, "rpc.bynumber", adrstr, strlen(adrstr),
                                  &d->current, &d->currentlen);
                switch(reason) {
                case 0:
                        break;
                case YPERR_MAP:
                        __yp_nomap = 1;
                        goto no_yp;
                        break;
                default:
                        return(0);
                        break;
                }
                d->current[d->currentlen] = '\0';
                p = interpret(d->current, d->currentlen);
                (void) free(d->current);
                return p;
        }
no_yp:
#endif	/* YP */

	setrpcent(0);
	while ((p = getrpcent()) != NULL) {
		if (p->r_number == number)
			break;
	}
	endrpcent();
	return (p);
}

struct rpcent *
getrpcbyname(name)
	char *name;
{
	struct rpcent *rpc = NULL;
	char **rp;

	assert(name != NULL);

	setrpcent(0);
	while ((rpc = getrpcent()) != NULL) {
		if (strcmp(rpc->r_name, name) == 0)
			goto done;
		for (rp = rpc->r_aliases; *rp != NULL; rp++) {
			if (strcmp(*rp, name) == 0)
				goto done;
		}
	}
done:
	endrpcent();
	return (rpc);
}

void
setrpcent(f)
	int f;
{
	struct rpcdata *d = _rpcdata();

	if (d == 0)
		return;
#ifdef	YP
        if (!__yp_nomap && _yp_check(NULL)) {
                if (d->current)
                        free(d->current);
                d->current = NULL;
                d->currentlen = 0;
                return;
        }
        __yp_nomap = 0;
#endif	/* YP */
	if (d->rpcf == NULL)
		d->rpcf = fopen(RPCDB, "r");
	else
		rewind(d->rpcf);
	d->stayopen |= f;
}

void
endrpcent()
{
	struct rpcdata *d = _rpcdata();

	if (d == 0)
		return;
#ifdef	YP
        if (!__yp_nomap && _yp_check(NULL)) {
        	if (d->current && !d->stayopen)
                        free(d->current);
                d->current = NULL;
                d->currentlen = 0;
                return;
        }
        __yp_nomap = 0;
#endif	/* YP */
	if (d->rpcf && !d->stayopen) {
		fclose(d->rpcf);
		d->rpcf = NULL;
	}
}

struct rpcent *
getrpcent()
{
	struct rpcdata *d = _rpcdata();
#ifdef	YP
	struct rpcent *hp;
	int reason;
	char *val = NULL;
	int vallen;
#endif

	if (d == 0)
		return(NULL);
#ifdef	YP
        if (!__yp_nomap && _yp_check(&d->domain)) {
                if (d->current == NULL && d->currentlen == 0) {
                        reason = yp_first(d->domain, "rpc.bynumber",
                                          &d->current, &d->currentlen,
                                          &val, &vallen);
                } else {
                        reason = yp_next(d->domain, "rpc.bynumber",
                                         d->current, d->currentlen,
                                         &d->current, &d->currentlen,
                                         &val, &vallen);
                }
                switch(reason) {
                case 0:
                        break;
                case YPERR_MAP:
                        __yp_nomap = 1;
                        goto no_yp;
                        break;
                default:
                        return(0);
                        break;
                }
                val[vallen] = '\0';
                hp = interpret(val, vallen);
                (void) free(val);
                return hp;
        }
no_yp:
#endif	/* YP */
	if (d->rpcf == NULL && (d->rpcf = fopen(RPCDB, "r")) == NULL)
		return (NULL);
	/* -1 so there is room to append a \n below */
        if (fgets(d->line, BUFSIZ - 1, d->rpcf) == NULL)
		return (NULL);
	return (interpret(d->line, strlen(d->line)));
}

static struct rpcent *
interpret(val, len)
	char *val;
	size_t len;
{
	struct rpcdata *d = _rpcdata();
	char *p;
	char *cp, **q;

	assert(val != NULL);

	if (d == 0)
		return (0);
	(void) strncpy(d->line, val, BUFSIZ);
	d->line[BUFSIZ] = '\0';
	p = d->line;
	p[len] = '\n';
	if (*p == '#')
		return (getrpcent());
	cp = strpbrk(p, "#\n");
	if (cp == NULL)
		return (getrpcent());
	*cp = '\0';
	cp = strpbrk(p, " \t");
	if (cp == NULL)
		return (getrpcent());
	*cp++ = '\0';
	/* THIS STUFF IS INTERNET SPECIFIC */
	d->rpc.r_name = d->line;
	while (*cp == ' ' || *cp == '\t')
		cp++;
	d->rpc.r_number = atoi(cp);
	q = d->rpc.r_aliases = d->rpc_aliases;
	cp = strpbrk(cp, " \t");
	if (cp != NULL)
		*cp++ = '\0';
	while (cp && *cp) {
		if (*cp == ' ' || *cp == '\t') {
			cp++;
			continue;
		}
		if (q < &(d->rpc_aliases[MAXALIASES - 1]))
			*q++ = cp;
		cp = strpbrk(cp, " \t");
		if (cp != NULL)
			*cp++ = '\0';
	}
	*q = NULL;
	return (&d->rpc);
}

