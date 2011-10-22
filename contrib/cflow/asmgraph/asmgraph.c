/*-
 * Copyright (c) 2007-2009, Marcus von Appen
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer 
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef __FreeBSD__
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#endif

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#ifndef _MSC_VER
#include <unistd.h>
#endif
#include <locale.h>
#include <errno.h>

#include "asmgraph.h"

/* Forward declarations. */
static void usage (void);

/**
 * Displays the usage command of the application.
 */
static void
usage (void)
{
    fprintf (stderr,
        "usage: asmgraph [-acgnr] [-d num] [-i incl] [-R root] file ...\n");
    exit (EXIT_FAILURE);
}

/**
 * Entry point for the asmgraph application. Creates a flow graph for
 * GNU as and nasm assembler files according to the POSIX specification.
 *
 * \param argc The argument count.
 * \param argv The argument array.
 */
int
main (int argc, char *argv[])
{
    FILE *fp;
    bool_t statics = FALSE;
    bool_t privates = FALSE;
    char *root = NULL;       /* Root function to use. */
    graph_t graph;           /* Actual graph to process. */
    int ch;                  /* Option to parse. */
    int i;                   /* Counter. */
    int depth = INT_MAX;     /* Depth to traverse. */
    int parser = NASM_LEXER; 
    bool_t graphviz = FALSE;
    bool_t complete = FALSE;
    bool_t reversed = FALSE;

    setlocale (LC_ALL, "");

    while ((ch = getopt (argc, argv, "acd:i:gnrR:")) != -1)
    {
        switch (ch)
        {
        case 'a':
            parser = AS_LEXER;
            break;
        case 'c':
            complete = TRUE;
            break;
        case 'd':
        {
            long val = strtol (optarg, NULL, 10);
            if (val == 0 && (errno == ERANGE || errno == EINVAL))
                usage();
            if (val > INT_MAX)
                usage();
            depth = (int) val;
            break;
        }
        case 'g':
            graphviz = TRUE;
            break;
        case 'i':
            if (strlen (optarg) > 1)
                usage ();
            if (optarg[0] == 'x')
                statics = TRUE;
            else if (optarg[0] == '_')
                privates = TRUE;
            else
                usage ();
            break;
        case 'n':
            parser = NASM_LEXER;
            break;
        case 'r':
            reversed = TRUE;
            break;
        case 'R':
            root = strdup (optarg);
            if (!root)
                exit (EXIT_FAILURE);
            break;
        }
    }

    /* Defaults are parsed, now get through the files. */
    argc -= optind;
    argv += optind;

    if (argc <= 0) /* No more arguments? */
        usage ();

    graph.root = (root) ? root : "main";
    graph.rootnode = NULL;
    graph.defines = NULL;
    graph.defcount = 0;
    graph.excludes = NULL;
    graph.statics = statics;
    graph.privates = privates;
    graph.depth = depth;
    graph.complete = complete;
    graph.reversed = reversed;

    /* Go through all the files and create the graph for each of it. */
    for (i = 0; i < argc; i++)
    {
        bool_t retval = FALSE;
        /* Open the file and create the graph struct to pass around. */
        fp = fopen (argv[i], "r");
        if (!fp)
        {
            perror (argv[i]);
            return 1;
        }
        /* Create the graphs. */
        switch (parser)
        {
        case AS_LEXER:
            retval = as_lex_create_graph (&graph, fp, argv[i]);
            break;
        case NASM_LEXER:
        default:
            retval = nasm_lex_create_graph (&graph, fp, argv[i]);
            break;
        }
        fclose (fp);
        if (!retval)
            return 1;
    }
    if (!graphviz)
        print_graph (&graph);
    else
        print_graphviz_graph (&graph);
    free_nodes (graph.excludes);
    free_g_nodes (graph.defines);
    return 0;
}
