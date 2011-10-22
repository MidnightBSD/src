/*-
 * Copyright (c) 2007, Marcus von Appen
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
 *
 * $FreeBSD$
 *
 */

#ifndef ASMGRAPH_H
#define ASMGRAPH_H

#include "graph.h"

/*
 * Possible token types.
 */
enum
{
    UNKNOWN,      /* Unknown token type. */
    ENDOFFILE,    /* End of file marker, set by EOF. */

    EXTERN,       /* 'extern' keyword. */
    GLOBAL,       /* 'global' keyword. */

    SECTION,      /* section ... */
    VAR_SECTION,  /* .bss .data */
    CMD_SECTION,  /* .text */

    LABEL,        /* XXX: */
    CALL,         /* 'call' keyword */

    SEMICOLON,    /* ; */
    COLON,        /* : */
    COMMA,        /* , */

    IDENTIFIER    /* An identifier like 'i' or 'strcmp'. */
};

/*
 * Assembler parser to invoke.
 */
enum
{
    NASM_LEXER, /* Use the parser for NASM syntax. */
    AS_LEXER    /* Use the parser for GNU as syntax. */
};

bool_t nasm_lex_create_graph (graph_t *graph, FILE *fp, char *filename);
bool_t as_lex_create_graph (graph_t *graph, FILE *fp, char *filename);

#endif /* ASMGRAPH_H */
