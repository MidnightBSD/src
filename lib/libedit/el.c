/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Christos Zoulas of Cornell University.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 *
 *	$NetBSD: el.c,v 1.55 2009/07/25 21:19:23 christos Exp $
 */

#if !defined(lint) && !defined(SCCSID)
static char sccsid[] = "@(#)el.c	8.2 (Berkeley) 1/3/94";
#endif /* not lint && not SCCSID */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * el.c: EditLine interface functions
 */
#include "sys.h"

#include <sys/types.h>
#include <sys/param.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include "el.h"

#define	HAVE_ISSETUGID

/* el_init():
 *	Initialize editline and set default parameters.
 */
public EditLine *
el_init(const char *prog, FILE *fin, FILE *fout, FILE *ferr)
{

	EditLine *el = (EditLine *) el_malloc(sizeof(EditLine));

	if (el == NULL)
		return (NULL);

	memset(el, 0, sizeof(EditLine));

	el->el_infile = fin;
	el->el_outfile = fout;
	el->el_errfile = ferr;

	el->el_infd = fileno(fin);

	if ((el->el_prog = el_strdup(prog)) == NULL) {
		el_free(el);
		return NULL;
	}

	/*
         * Initialize all the modules. Order is important!!!
         */
	el->el_flags = 0;

	if (term_init(el) == -1) {
		el_free(el->el_prog);
		el_free(el);
		return NULL;
	}
	(void) key_init(el);
	(void) map_init(el);
	if (tty_init(el) == -1)
		el->el_flags |= NO_TTY;
	(void) ch_init(el);
	(void) search_init(el);
	(void) hist_init(el);
	(void) prompt_init(el);
	(void) sig_init(el);
	(void) read_init(el);

	return (el);
}


/* el_end():
 *	Clean up.
 */
public void
el_end(EditLine *el)
{

	if (el == NULL)
		return;

	el_reset(el);

	term_end(el);
	key_end(el);
	map_end(el);
	tty_end(el);
	ch_end(el);
	search_end(el);
	hist_end(el);
	prompt_end(el);
	sig_end(el);

	el_free((ptr_t) el->el_prog);
	el_free((ptr_t) el);
}


/* el_reset():
 *	Reset the tty and the parser
 */
public void
el_reset(EditLine *el)
{

	tty_cookedmode(el);
	ch_reset(el, 0);		/* XXX: Do we want that? */
}


/* el_set():
 *	set the editline parameters
 */
public int
el_set(EditLine *el, int op, ...)
{
	va_list ap;
	int rv = 0;

	if (el == NULL)
		return (-1);
	va_start(ap, op);

	switch (op) {
	case EL_PROMPT:
	case EL_RPROMPT: {
		el_pfunc_t p = va_arg(ap, el_pfunc_t);

		rv = prompt_set(el, p, 0, op);
		break;
	}

	case EL_PROMPT_ESC:
	case EL_RPROMPT_ESC: {
		el_pfunc_t p = va_arg(ap, el_pfunc_t);
		char c = va_arg(ap, int);

		rv = prompt_set(el, p, c, op);
		break;
	}

	case EL_TERMINAL:
		rv = term_set(el, va_arg(ap, char *));
		break;

	case EL_EDITOR:
		rv = map_set_editor(el, va_arg(ap, char *));
		break;

	case EL_SIGNAL:
		if (va_arg(ap, int))
			el->el_flags |= HANDLE_SIGNALS;
		else
			el->el_flags &= ~HANDLE_SIGNALS;
		break;

	case EL_BIND:
	case EL_TELLTC:
	case EL_SETTC:
	case EL_GETTC:
	case EL_ECHOTC:
	case EL_SETTY:
	{
		const char *argv[20];
		int i;

		for (i = 1; i < 20; i++)
			if ((argv[i] = va_arg(ap, char *)) == NULL)
				break;

		switch (op) {
		case EL_BIND:
			argv[0] = "bind";
			rv = map_bind(el, i, argv);
			break;

		case EL_TELLTC:
			argv[0] = "telltc";
			rv = term_telltc(el, i, argv);
			break;

		case EL_SETTC:
			argv[0] = "settc";
			rv = term_settc(el, i, argv);
			break;

		case EL_ECHOTC:
			argv[0] = "echotc";
			rv = term_echotc(el, i, argv);
			break;

		case EL_SETTY:
			argv[0] = "setty";
			rv = tty_stty(el, i, argv);
			break;

		default:
			rv = -1;
			EL_ABORT((el->el_errfile, "Bad op %d\n", op));
			break;
		}
		break;
	}

	case EL_ADDFN:
	{
		char *name = va_arg(ap, char *);
		char *help = va_arg(ap, char *);
		el_func_t func = va_arg(ap, el_func_t);

		rv = map_addfunc(el, name, help, func);
		break;
	}

	case EL_HIST:
	{
		hist_fun_t func = va_arg(ap, hist_fun_t);
		ptr_t ptr = va_arg(ap, char *);

		rv = hist_set(el, func, ptr);
		break;
	}

	case EL_EDITMODE:
		if (va_arg(ap, int))
			el->el_flags &= ~EDIT_DISABLED;
		else
			el->el_flags |= EDIT_DISABLED;
		rv = 0;
		break;

	case EL_GETCFN:
	{
		el_rfunc_t rc = va_arg(ap, el_rfunc_t);
		rv = el_read_setfn(el, rc);
		break;
	}

	case EL_CLIENTDATA:
		el->el_data = va_arg(ap, void *);
		break;

	case EL_UNBUFFERED:
		rv = va_arg(ap, int);
		if (rv && !(el->el_flags & UNBUFFERED)) {
			el->el_flags |= UNBUFFERED;
			read_prepare(el);
		} else if (!rv && (el->el_flags & UNBUFFERED)) {
			el->el_flags &= ~UNBUFFERED;
			read_finish(el);
		}
		rv = 0;
		break;

	case EL_PREP_TERM:
		rv = va_arg(ap, int);
		if (rv)
			(void) tty_rawmode(el);
		else
			(void) tty_cookedmode(el);
		rv = 0;
		break;

	case EL_SETFP:
	{
		FILE *fp;
		int what;

		what = va_arg(ap, int);
		fp = va_arg(ap, FILE *);

		rv = 0;
		switch (what) {
		case 0:
			el->el_infile = fp;
			el->el_infd = fileno(fp);
			break;
		case 1:
			el->el_outfile = fp;
			break;
		case 2:
			el->el_errfile = fp;
			break;
		default:
			rv = -1;
			break;
		}
		break;
	}

	case EL_REFRESH:
		re_clear_display(el);
		re_refresh(el);
		term__flush(el);
		break;

	default:
		rv = -1;
		break;
	}

	va_end(ap);
	return (rv);
}


/* el_get():
 *	retrieve the editline parameters
 */
public int
el_get(EditLine *el, int op, ...)
{
	va_list ap;
	int rv;

	if (el == NULL)
		return -1;

	va_start(ap, op);

	switch (op) {
	case EL_PROMPT:
	case EL_RPROMPT: {
		el_pfunc_t *p = va_arg(ap, el_pfunc_t *);
		char *c = va_arg(ap, char *);

		rv = prompt_get(el, p, c, op);
		break;
	}

	case EL_EDITOR:
		rv = map_get_editor(el, va_arg(ap, const char **));
		break;

	case EL_SIGNAL:
		*va_arg(ap, int *) = (el->el_flags & HANDLE_SIGNALS);
		rv = 0;
		break;

	case EL_EDITMODE:
		*va_arg(ap, int *) = !(el->el_flags & EDIT_DISABLED);
		rv = 0;
		break;

	case EL_TERMINAL:
		term_get(el, va_arg(ap, const char **));
		rv = 0;
		break;

	case EL_GETTC:
	{
		static char name[] = "gettc";
		char *argv[20];
		int i;

 		for (i = 1; i < (int)(sizeof(argv) / sizeof(argv[0])); i++)
			if ((argv[i] = va_arg(ap, char *)) == NULL)
				break;

		switch (op) {
		case EL_GETTC:
			argv[0] = name;
			rv = term_gettc(el, i, argv);
			break;

		default:
			rv = -1;
			EL_ABORT((el->el_errfile, "Bad op %d\n", op));
			break;
		}
		break;
	}

#if 0 /* XXX */
	case EL_ADDFN:
	{
		char *name = va_arg(ap, char *);
		char *help = va_arg(ap, char *);
		el_func_t func = va_arg(ap, el_func_t);

		rv = map_addfunc(el, name, help, func);
		break;
	}

	case EL_HIST:
		{
			hist_fun_t func = va_arg(ap, hist_fun_t);
			ptr_t ptr = va_arg(ap, char *);
			rv = hist_set(el, func, ptr);
		}
		break;
#endif /* XXX */

	case EL_GETCFN:
		*va_arg(ap, el_rfunc_t *) = el_read_getfn(el);
		rv = 0;
		break;

	case EL_CLIENTDATA:
		*va_arg(ap, void **) = el->el_data;
		rv = 0;
		break;

	case EL_UNBUFFERED:
		*va_arg(ap, int *) = (!(el->el_flags & UNBUFFERED));
		rv = 0;
		break;

	case EL_GETFP:
	{
		int what;
		FILE **fpp;

		what = va_arg(ap, int);
		fpp = va_arg(ap, FILE **);
		rv = 0;
		switch (what) {
		case 0:
			*fpp = el->el_infile;
			break;
		case 1:
			*fpp = el->el_outfile;
			break;
		case 2:
			*fpp = el->el_errfile;
			break;
		default:
			rv = -1;
			break;
		}
		break;
	}
	default:
		rv = -1;
		break;
	}
	va_end(ap);

	return (rv);
}

/* el_data_get():
 *	Set user private data.
 */
public void
el_data_set (el, data)
    EditLine *el;
    void *data;
{
    el->el_data = data;

    return;
}

/* el_data_get():
 *	Return user private data.
 */
public void *
el_data_get (el)
    EditLine *el;
{
    if (el->el_data)
	return (el->el_data);
    return (NULL);
}

/* el_line():
 *	Return editing info
 */
public const LineInfo *
el_line(EditLine *el)
{

	return (const LineInfo *) (void *) &el->el_line;
}


/* el_source():
 *	Source a file
 */
public int
el_source(EditLine *el, const char *fname)
{
	FILE *fp;
	size_t len;
	char *ptr;
#ifdef HAVE_ISSETUGID
	char path[MAXPATHLEN];
#endif

	fp = NULL;
	if (fname == NULL) {
#ifdef HAVE_ISSETUGID
		static const char elpath[] = "/.editrc";

		if (issetugid())
			return (-1);
		if ((ptr = getenv("HOME")) == NULL)
			return (-1);
		if (strlcpy(path, ptr, sizeof(path)) >= sizeof(path))
			return (-1);
		if (strlcat(path, elpath, sizeof(path)) >= sizeof(path))
			return (-1);
		fname = path;
#else
		/*
		 * If issetugid() is missing, always return an error, in order
		 * to keep from inadvertently opening up the user to a security
		 * hole.
		 */
		return (-1);
#endif
	}
	if (fp == NULL)
		fp = fopen(fname, "r");
	if (fp == NULL)
		return (-1);

	while ((ptr = fgetln(fp, &len)) != NULL) {
		if (len > 0 && ptr[len - 1] == '\n')
			--len;
		ptr[len] = '\0';

		/* loop until first non-space char or EOL */
		while (*ptr != '\0' && isspace((unsigned char)*ptr))
			ptr++;
		if (*ptr == '#')
			continue;   /* ignore, this is a comment line */

		if (parse_line(el, ptr) == -1) {
			(void) fclose(fp);
			return (-1);
		}
	}

	(void) fclose(fp);
	return (0);
}


/* el_resize():
 *	Called from program when terminal is resized
 */
public void
el_resize(EditLine *el)
{
	int lins, cols;
	sigset_t oset, nset;

	(void) sigemptyset(&nset);
	(void) sigaddset(&nset, SIGWINCH);
	(void) sigprocmask(SIG_BLOCK, &nset, &oset);

	/* get the correct window size */
	if (term_get_size(el, &lins, &cols))
		term_change_size(el, lins, cols);

	(void) sigprocmask(SIG_SETMASK, &oset, NULL);
}


/* el_beep():
 *	Called from the program to beep
 */
public void
el_beep(EditLine *el)
{

	term_beep(el);
}


/* el_editmode()
 *	Set the state of EDIT_DISABLED from the `edit' command.
 */
protected int
/*ARGSUSED*/
el_editmode(EditLine *el, int argc, const char **argv)
{
	const char *how;

	if (argv == NULL || argc != 2 || argv[1] == NULL)
		return (-1);

	how = argv[1];
	if (strcmp(how, "on") == 0) {
		el->el_flags &= ~EDIT_DISABLED;
		tty_rawmode(el);
	} else if (strcmp(how, "off") == 0) {
		tty_cookedmode(el);
		el->el_flags |= EDIT_DISABLED;
	}
	else {
		(void) fprintf(el->el_errfile, "edit: Bad value `%s'.\n", how);
		return (-1);
	}
	return (0);
}
