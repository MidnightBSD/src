/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Robert Paul Corbett.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
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
 */

#if 0
#ifndef lint
static char sccsid[] = "@(#)error.c	5.3 (Berkeley) 6/1/90";
#endif
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/* routines for printing error messages  */

#include "defs.h"

static void print_pos(char *, char *);

void
fatal(const char *msg)
{
    warnx("f - %s", msg);
    done(2);
}


void
no_space(void)
{
    warnx("f - out of space");
    done(2);
}


void
open_error(const char *filename)
{
    warnx("f - cannot open \"%s\"", filename);
    done(2);
}


void
unexpected_EOF(void)
{
    warnx("e - line %d of \"%s\", unexpected end-of-file",
	    lineno, input_file_name);
    done(1);
}


static void
print_pos(char *st_line, char *st_cptr)
{
    char *s;

    if (st_line == 0) return;
    for (s = st_line; *s != '\n'; ++s)
    {
	if (isprint(*s) || *s == '\t')
	    putc(*s, stderr);
	else
	    putc('?', stderr);
    }
    putc('\n', stderr);
    for (s = st_line; s < st_cptr; ++s)
    {
	if (*s == '\t')
	    putc('\t', stderr);
	else
	    putc(' ', stderr);
    }
    putc('^', stderr);
    putc('\n', stderr);
}


void
syntax_error(int st_lineno, char *st_line, char *st_cptr)
{
    warnx("e - line %d of \"%s\", syntax error",
	    st_lineno, input_file_name);
    print_pos(st_line, st_cptr);
    done(1);
}


void
unterminated_comment(int c_lineno, char *c_line, char *c_cptr)
{
    warnx("e - line %d of \"%s\", unmatched /*",
	    c_lineno, input_file_name);
    print_pos(c_line, c_cptr);
    done(1);
}


void
unterminated_string(int s_lineno, char *s_line, char *s_cptr)
{
    warnx("e - line %d of \"%s\", unterminated string",
	    s_lineno, input_file_name);
    print_pos(s_line, s_cptr);
    done(1);
}


void
unterminated_text(int t_lineno, char *t_line, char *t_cptr)
{
    warnx("e - line %d of \"%s\", unmatched %%{",
	    t_lineno, input_file_name);
    print_pos(t_line, t_cptr);
    done(1);
}


void
unterminated_union(int u_lineno, char *u_line, char *u_cptr)
{
    warnx("e - line %d of \"%s\", unterminated %%union declaration",
		u_lineno, input_file_name);
    print_pos(u_line, u_cptr);
    done(1);
}


void
over_unionized(char *u_cptr)
{
    warnx("e - line %d of \"%s\", too many %%union declarations",
		lineno, input_file_name);
    print_pos(line, u_cptr);
    done(1);
}


void
illegal_tag(int t_lineno, char *t_line, char *t_cptr)
{
    warnx("e - line %d of \"%s\", illegal tag", t_lineno, input_file_name);
    print_pos(t_line, t_cptr);
    done(1);
}


void
illegal_character(char *c_cptr)
{
    warnx("e - line %d of \"%s\", illegal character", lineno, input_file_name);
    print_pos(line, c_cptr);
    done(1);
}


void
used_reserved(char *s)
{
    warnx("e - line %d of \"%s\", illegal use of reserved symbol %s",
		lineno, input_file_name, s);
    done(1);
}


void
tokenized_start(char *s)
{
     warnx("e - line %d of \"%s\", the start symbol %s cannot be \
declared to be a token", lineno, input_file_name, s);
     done(1);
}


void
retyped_warning(char *s)
{
    warnx("w - line %d of \"%s\", the type of %s has been redeclared",
		lineno, input_file_name, s);
}


void
reprec_warning(char *s)
{
    warnx("w - line %d of \"%s\", the precedence of %s has been redeclared",
		lineno, input_file_name, s);
}


void
revalued_warning(char *s)
{
    warnx("w - line %d of \"%s\", the value of %s has been redeclared",
		lineno, input_file_name, s);
}


void
terminal_start(char *s)
{
    warnx("e - line %d of \"%s\", the start symbol %s is a token",
		lineno, input_file_name, s);
    done(1);
}


void
restarted_warning(void)
{
    warnx("w - line %d of \"%s\", the start symbol has been redeclared",
		lineno, input_file_name);
}


void
no_grammar(void)
{
    warnx("e - line %d of \"%s\", no grammar has been specified",
		lineno, input_file_name);
    done(1);
}


void
terminal_lhs(int s_lineno)
{
    warnx("e - line %d of \"%s\", a token appears on the lhs of a production",
		s_lineno, input_file_name);
    done(1);
}


void
prec_redeclared(void)
{
    warnx("w - line %d of  \"%s\", conflicting %%prec specifiers",
		lineno, input_file_name);
}


void
unterminated_action(int a_lineno, char *a_line, char *a_cptr)
{
    warnx("e - line %d of \"%s\", unterminated action",
	    a_lineno, input_file_name);
    print_pos(a_line, a_cptr);
    done(1);
}


void
dollar_warning(int a_lineno, int i)
{
    warnx("w - line %d of \"%s\", $%d references beyond the \
end of the current rule", a_lineno, input_file_name, i);
}


void
dollar_error(int a_lineno, char *a_line, char *a_cptr)
{
    warnx("e - line %d of \"%s\", illegal $-name", a_lineno, input_file_name);
    print_pos(a_line, a_cptr);
    done(1);
}


void
untyped_lhs(void)
{
    warnx("e - line %d of \"%s\", $$ is untyped", lineno, input_file_name);
    done(1);
}


void
untyped_rhs(int i, char *s)
{
    warnx("e - line %d of \"%s\", $%d (%s) is untyped",
	    lineno, input_file_name, i, s);
    done(1);
}


void
unknown_rhs(int i)
{
    warnx("e - line %d of \"%s\", $%d is untyped", lineno, input_file_name, i);
    done(1);
}


void
default_action_warning(void)
{
    warnx("w - line %d of \"%s\", the default action assigns an \
undefined value to $$", lineno, input_file_name);
}


void
undefined_goal(char *s)
{
    warnx("e - the start symbol %s is undefined", s);
    done(1);
}


void
undefined_symbol_warning(char *s)
{
    warnx("w - the symbol %s is undefined", s);
}
