/*
 * Copyright (c) 1995 - 2001 Kungliga Tekniska H�gskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
RCSID("$Id: sl.c,v 1.29 2001/02/20 01:44:55 assar Exp $");
#endif

#include "sl_locl.h"
#include <setjmp.h>

static size_t
print_sl (FILE *stream, int mdoc, int longp, SL_cmd *c)
    __attribute__ ((unused));

static size_t
print_sl (FILE *stream, int mdoc, int longp, SL_cmd *c)
{
    if(mdoc){
	if(longp)
	    fprintf(stream, "= Ns");
	fprintf(stream, " Ar ");
    }else
	if (longp)
	    putc ('=', stream);
	else
	    putc (' ', stream);

    return 1;
}

static void
mandoc_template(SL_cmd *cmds,
		const char *extra_string)
{
    SL_cmd *c, *prev;
    char timestr[64], cmd[64];
    const char *p;
    time_t t;

    printf(".\\\" Things to fix:\n");
    printf(".\\\"   * correct section, and operating system\n");
    printf(".\\\"   * remove Op from mandatory flags\n");
    printf(".\\\"   * use better macros for arguments (like .Pa for files)\n");
    printf(".\\\"\n");
    t = time(NULL);
    strftime(timestr, sizeof(timestr), "%b %d, %Y", localtime(&t));
    printf(".Dd %s\n", timestr);
    p = strrchr(getprogname(), '/');
    if(p) p++; else p = getprogname();
    strncpy(cmd, p, sizeof(cmd));
    cmd[sizeof(cmd)-1] = '\0';
    strupr(cmd);
       
    printf(".Dt %s SECTION\n", cmd);
    printf(".Os OPERATING_SYSTEM\n");
    printf(".Sh NAME\n");
    printf(".Nm %s\n", p);
    printf(".Nd\n");
    printf("in search of a description\n");
    printf(".Sh SYNOPSIS\n");
    printf(".Nm\n");
    for(c = cmds; c->name; ++c) {
/*	if (c->func == NULL)
	    continue; */
	printf(".Op Fl %s", c->name);
/*	print_sl(stdout, 1, 0, c);*/
	printf("\n");
	
    }
    if (extra_string && *extra_string)
	printf (".Ar %s\n", extra_string);
    printf(".Sh DESCRIPTION\n");
    printf("Supported options:\n");
    printf(".Bl -tag -width Ds\n");
    prev = NULL;
    for(c = cmds; c->name; ++c) {
	if (c->func) {
	    if (prev)
		printf ("\n%s\n", prev->usage);

	    printf (".It Fl %s", c->name);
	    prev = c;
	} else
	    printf (", %s\n", c->name);
    }
    if (prev)
	printf ("\n%s\n", prev->usage);

    printf(".El\n");
    printf(".\\\".Sh ENVIRONMENT\n");
    printf(".\\\".Sh FILES\n");
    printf(".\\\".Sh EXAMPLES\n");
    printf(".\\\".Sh DIAGNOSTICS\n");
    printf(".\\\".Sh SEE ALSO\n");
    printf(".\\\".Sh STANDARDS\n");
    printf(".\\\".Sh HISTORY\n");
    printf(".\\\".Sh AUTHORS\n");
    printf(".\\\".Sh BUGS\n");
}

static SL_cmd *
sl_match (SL_cmd *cmds, char *cmd, int exactp)
{
    SL_cmd *c, *current = NULL, *partial_cmd = NULL;
    int partial_match = 0;

    for (c = cmds; c->name; ++c) {
	if (c->func)
	    current = c;
	if (strcmp (cmd, c->name) == 0)
	    return current;
	else if (strncmp (cmd, c->name, strlen(cmd)) == 0 &&
		 partial_cmd != current) {
	    ++partial_match;
	    partial_cmd = current;
	}
    }
    if (partial_match == 1 && !exactp)
	return partial_cmd;
    else
	return NULL;
}

void
sl_help (SL_cmd *cmds, int argc, char **argv)
{
    SL_cmd *c, *prev_c;

    if (getenv("SLMANDOC")) {
	mandoc_template(cmds, NULL);
	return;
    }

    if (argc == 1) {
	prev_c = NULL;
	for (c = cmds; c->name; ++c) {
	    if (c->func) {
		if(prev_c)
		    printf ("\n\t%s%s", prev_c->usage ? prev_c->usage : "",
			    prev_c->usage ? "\n" : "");
		prev_c = c;
		printf ("%s", c->name);
	    } else
		printf (", %s", c->name);
	}
	if(prev_c)
	    printf ("\n\t%s%s", prev_c->usage ? prev_c->usage : "",
		    prev_c->usage ? "\n" : "");
    } else { 
	c = sl_match (cmds, argv[1], 0);
	if (c == NULL)
	    printf ("No such command: %s. "
		    "Try \"help\" for a list of all commands\n",
		    argv[1]);
	else {
	    printf ("%s\t%s\n", c->name, c->usage);
	    if(c->help && *c->help)
		printf ("%s\n", c->help);
	    if((++c)->name && c->func == NULL) {
		printf ("Synonyms:");
		while (c->name && c->func == NULL)
		    printf ("\t%s", (c++)->name);
		printf ("\n");
	    }
	}
    }
}

#ifdef HAVE_READLINE

char *readline(char *prompt);
void add_history(char *p);

#else

static char *
readline(char *prompt)
{
    char buf[BUFSIZ];
    printf ("%s", prompt);
    fflush (stdout);
    if(fgets(buf, sizeof(buf), stdin) == NULL)
	return NULL;
    if (buf[strlen(buf) - 1] == '\n')
	buf[strlen(buf) - 1] = '\0';
    return strdup(buf);
}

static void
add_history(char *p)
{
}

#endif

int
sl_command(SL_cmd *cmds, int argc, char **argv)
{
    SL_cmd *c;
    c = sl_match (cmds, argv[0], 0);
    if (c == NULL)
	return -1;
    return (*c->func)(argc, argv);
}

struct sl_data {
    int max_count;
    char **ptr;
};

int
sl_make_argv(char *line, int *ret_argc, char ***ret_argv)
{
    char *foo = NULL;
    char *p;
    int argc, nargv;
    char **argv;
    
    nargv = 10;
    argv = malloc(nargv * sizeof(*argv));
    if(argv == NULL)
	return ENOMEM;
    argc = 0;

    for(p = strtok_r (line, " \t", &foo);
	p;
	p = strtok_r (NULL, " \t", &foo)) {
	if(argc == nargv - 1) {
	    char **tmp;
	    nargv *= 2;
	    tmp = realloc (argv, nargv * sizeof(*argv));
	    if (tmp == NULL) {
		free(argv);
		return ENOMEM;
	    }
	    argv = tmp;
	}
	argv[argc++] = p;
    }
    argv[argc] = NULL;
    *ret_argc = argc;
    *ret_argv = argv;
    return 0;
}

static jmp_buf sl_jmp;

static void sl_sigint(int sig)
{
    longjmp(sl_jmp, 1);
}

static char *sl_readline(const char *prompt)
{
    char *s;
    void (*old)(int);
    old = signal(SIGINT, sl_sigint);
    if(setjmp(sl_jmp))
	printf("\n");
    s = readline((char*)prompt);
    signal(SIGINT, old);
    return s;
}

/* return values: 0 on success, -1 on fatal error, or return value of command */
int
sl_command_loop(SL_cmd *cmds, const char *prompt, void **data)
{
    int ret = 0;
    char *buf;
    int argc;
    char **argv;
	
    ret = 0;
    buf = sl_readline(prompt);
    if(buf == NULL)
	return 1;

    if(*buf)
	add_history(buf);
    ret = sl_make_argv(buf, &argc, &argv);
    if(ret) {
	fprintf(stderr, "sl_loop: out of memory\n");
	free(buf);
	return -1;
    }
    if (argc >= 1) {
	ret = sl_command(cmds, argc, argv);
	if(ret == -1) {
	    printf ("Unrecognized command: %s\n", argv[0]);
	    ret = 0;
	}
    }
    free(buf);
    free(argv);
    return ret;
}

int 
sl_loop(SL_cmd *cmds, const char *prompt)
{
    void *data = NULL;
    int ret;
    while((ret = sl_command_loop(cmds, prompt, &data)) == 0)
	;
    return ret;
}

void
sl_apropos (SL_cmd *cmd, const char *topic)
{
    for (; cmd->name != NULL; ++cmd)
        if (cmd->usage != NULL && strstr(cmd->usage, topic) != NULL)
	    printf ("%-20s%s\n", cmd->name, cmd->usage);
}
