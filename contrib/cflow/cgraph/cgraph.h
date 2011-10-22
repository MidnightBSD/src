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

#ifndef CGRAPH_H
#define CGRAPH_H

#include "graph.h"

/*
 * Possible token types. The C language specs define several constructs,
 * which have to be taken care of.
 */
enum
{
    UNKNOWN,      /* Unknown token type. */
    ENDOFFILE,    /* End of file marker, set by EOF. */

    ARGSTART,     /* Argument list of something. */
    ARGEND,       /* Argument list of something. */

    BODYSTART,    /* Body of an object, enclosed by brackets. */
    BODYEND,      /* Body of an object, enclosed by brackets. */

    STATIC,       /* 'static' keyword. */
    EXTERN,       /* 'extern' keyword. */
    TYPEDEF,      /* 'typedef' keyword. */
    STRUCT,       /* 'struct'or 'union'. */
    ENUM,         /* 'enum'. */ 
     
    ARRAYSTART,   /* Array access, like e.g str[7] or foo[get_no()]. */
    ARRAYEND,     /* Array access, like e.g str[7] or foo[get_no()]. */
    OPERATOR,     /* Operator, e.g. +, -, /, ... */
    SEMICOLON,    /* ; */
    COLON,        /* : */
    COMMA,        /* , */
    ASSIGN,       /* Assignment of an variable, e.g. = or combines. */
    REFERENCE,    /* Reference access through pointers, e.g. -> or '.'. */
    POINTER,      /* Pointer '*' */

    IDENTIFIER    /* An identifier like 'i' or 'strcmp'. */
};

bool_t lex_create_graph (graph_t *graph, FILE *fp, char *filename);

#endif /* CGRAPH_H */
