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
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "asmgraph.h"

#define AS_DEBUG 0

/* Marker for the current file offset(s). */
static int offset = 0;
static int line = 0;

/* Forward declarations. */
static int as_skip_whitespaces (FILE *fp);
inline int as_skip_strings (FILE *fp, int delim);
static int as_check_valid_section (char *name);
static int as_check_keyword (char *name);
static bool_t as_is_function_call (char *name);
static char* as_get_name (FILE *fp, int ch);
static int as_get_next_token (graph_t *graph, FILE *fp, char **name);

/**
 * Skips whitespaces, tabs and comments within a file buffer.
 *
 * \param fp The file to read and skip the whitespaces from.
 * \return The character value or EOF, if the end of the file was reached.
 */
static int
as_skip_whitespaces (FILE *fp)
{
    int ch;
    int next;

    if (feof (fp))
        return EOF;

    do
    {
        ch = fgetc (fp);

        if (isspace (ch))
        {
            if (ch == '\n')
            {
                line++; /* new line, increase line marker. */
                offset = 0;
            }
        }
        else if (ch == '/')
        {
            /* Possible comment block. */
            next = fgetc (fp);
            offset++;
            if (next == '/')
            {
                /* Single line comment, skip until a newline. */
                while (next != '\n' && next != EOF)
                {
                    next = fgetc (fp);
                    offset++;
                }

                if (next == EOF)
                    return EOF;
                    
                /* Line increment. */
                offset = 1;
                line++;

                ch = fgetc (fp);
            }
            else if (next == '*')
            {
                /* Multiline comment, skip anything until we reached
                 * its end. */
                while (ch != '*' || next != '/')
                {
                    ch = next;
                    next = fgetc (fp);
                    if (next == EOF)
                        return EOF;
                    offset++;

                    if (next == '\n')
                    {
                        offset = 0;
                        line++;
                    }
                }
            }
            else
            {
                ungetc (next, fp);
                return ch;
            }
        } /* if (ch == '/') */
        else
            return ch;
    }
    while (!feof (fp));
    return EOF;
}

/**
 * Skips characters until the certain delimiter is reached.
 *
 * \param fp The file to read and skip the strings from.
 * \param delim The delimiter character.
 * \return The next character after the delimiter or EOF.
 */
inline int
as_skip_strings (FILE *fp, int delim)
{
    int ch = '\0';
     
    while (ch != EOF && ch != delim)
    {
        if (ch == '\\')
            ch = fgetc (fp);
        ch = fgetc (fp);
        if (ch == '\n')
        {
            line++;
            offset = 0;
        }
        offset++;
    }
    return ch;
}

/**
 * Checks, whether the name matches a valid section identifier.
 *
 * \param name The name to check.
 * \return VAR_SECTION or CMD_SECTION, if the name matches, UNKNOWN, if
 *         not.
 */
static int
as_check_valid_section (char *name)
{
    if (strcmp (name, ".bss") == 0)
        return VAR_SECTION;
    if (strcmp (name, ".data") == 0)
        return VAR_SECTION;
    if (strcmp (name, ".text") == 0)
        return CMD_SECTION;
    return UNKNOWN;
}

/**
 * Checks, whether the name is a relevant keyword.
 *
 * \param name The name to check.
 * \return SECTION, GLOBAL or EXTERN, or UNKNOWN if name is not relevant.
 */
static int
as_check_keyword (char *name)
{
    if (strcmp (name, ".section") == 0)
        return SECTION;
    if (strcmp (name, ".global") == 0 || strcmp (name, ".globl") == 0)
        return GLOBAL;
    /* Superfluous */
    if (strcmp (name, ".extern") == 0)
        return EXTERN;
    return UNKNOWN;
}

/**
 * Checks, whether the name is a valid function call identifier.
 *
 * \param name The name to check.
 * \return TRUE, if name is a valid function call identifier, FALSE
 *         otherwise.
 */
static bool_t
as_is_function_call (char *name)
{
    return strcmp (name, "call") == 0;
}

/**
 * Reads and returns a name. The return value has to be freed by the
 * caller.
 * 
 * \param fp The file to read the name from.
 * \param ch The character to start with.
 * \return A valid C identifier name.
 */
static char*
as_get_name (FILE *fp, int ch)
{
    char *name = NULL;
    char *tmp = NULL;
    int i = 0;

    name = malloc (sizeof (char));
    if (!name)
        return NULL;
    name[0] = '\0';

    do
    {
        tmp = realloc (name, sizeof (char) * (i + 2));
        if (!tmp)
        {
            if (name)
                free (name);
            return NULL;
        }
        name = tmp;
        name[i] = ch;
        name[i + 1] = '\0';
        i++;

        ch = fgetc (fp);
        if (ch == EOF)
        {
            free (name);
            return NULL;
        }
        offset++;
    }
    while (isalnum (ch) || ch == '_');
    ungetc (ch, fp); /* Unget the last one. It's not the name. */
    return name;
}

/**
 * Gets the next valid token type from the file.
 *
 * \param graph The graph to get the next token for.
 * \param fp The file to get the next token from.
 * \return An enum value indicating the type of token.
 */
static int
as_get_next_token (graph_t *graph, FILE *fp, char **name)
{
    int ch;

    do
    {
        ch = as_skip_whitespaces (fp);
        switch (ch)
        {
        case EOF:
            return ENDOFFILE;
        case '"':
        case '\'':
            /* Skip string or char literals. */
            ch = as_skip_strings (fp, ch);
            if (ch == EOF)
                return ENDOFFILE;
            return as_get_next_token (graph, fp, name);
        case '.':
        {
            /* A possible section name or local label */
            int token = UNKNOWN;
            if (*name)
                free (*name);
            *name = as_get_name (fp, ch);
            
            /* check for .bss, .data, .text */
            token = as_check_valid_section (*name);
            if (token != UNKNOWN)
                return token;

            /* Check for .section, .global, .globl, .extern */
            token = as_check_keyword (*name);
            if (token != UNKNOWN)
                return token;

            /* Each other is a local label */
            return UNKNOWN;
        }            
        default:
            if (isalpha (ch) || ch == '_')
            {
                /* [A-Z] or _ are allowed values for a name identifier. */
                if (*name)
                    free (*name);

                *name = as_get_name (fp, ch);
                
                /* Check for call instructions. */
                if (as_is_function_call (*name))
                    return CALL;

                /* Check for a label. */
                ch = fgetc (fp);
                if (ch == EOF)
                    return ENDOFFILE;
                if (ch == ':')
                    return LABEL;

                ungetc (ch, fp);

                return IDENTIFIER;
            }
            break;
        }
    }
    while (TRUE);
}

/**
 * Creates the output graph from the passed graph object.
 *
 * \param graph The graph object to create the output grah for.
 * \return TRUE on success, FALSE on error.
 */
bool_t
as_lex_create_graph (graph_t *graph, FILE *fp, char *filename)
{
    char *curname = NULL; 
    char *curfunc = NULL;
    NodeType nodetype = FUNCTION;
    char *name = NULL;       /* The current node name. */
    int prev = SEMICOLON;    /* Previous token. */
    int token = SEMICOLON;   /* Current token. */
    int funcline = -1;

    line = 1;
    
    while (prev = token,
        (token = as_get_next_token (graph, fp, &name)) != ENDOFFILE)
    {
        /* SECTION .XXXX: ... */
        if (token == SECTION)
        {
        }

        /* SECTION .bss: || SECTION .data: */
        if (token == VAR_SECTION)
        {
            nodetype = VARIABLE;
        }

        /* SECTION .text: */
        if (token == CMD_SECTION)
        {
            nodetype = FUNCTION;
        }

        if (nodetype == VARIABLE && token == LABEL)
        {
            if (!add_g_node (graph, VARIABLE, name, NULL, filename, line))
                goto memerror;
#if AS_DEBUG
                printf ("Adding variable declaration %s\n", name);
#endif
        }

        if (nodetype == FUNCTION && prev == GLOBAL && token == IDENTIFIER)
        {
            /* SECTION .text:
             * ...
             * GLOBAL NAME
             */

            /* Copy the function name. */
            if (name)
            {
                if (curname)
                    free (curname);
                curname = strdup (name);
                if (!curname)
                    goto memerror;
            }
        }

        if (prev == IDENTIFIER && token == LABEL)
        {
            /* NAME: */
            if (curname && strcmp (curname, name) == 0)
            {
                /*       global NAME
                 * NAME:
                 * We seem to be in the correct function.
                 */
                funcline = line;
                if (!add_g_node (graph, FUNCTION, curname, NULL, filename,
                                 line))
                    goto memerror;
#if AS_DEBUG
                printf ("Adding function declaration %s\n", curname);
#endif
                if (curfunc)
                    free (curfunc);

                curfunc = strdup (curname);
                if (!curfunc)
                    goto memerror;
                free (curname);
                curname = NULL;
            }
        }

        if (prev == CALL && token == IDENTIFIER && curfunc != NULL)
        {
            /* Function call. */
            g_subnode_t *sub = NULL;
            g_node_t *call = get_definition_node (graph->defines, name,
                                                  filename);
            if (!call)
            {
                call = add_g_node (graph, FUNCTION, name, NULL, filename, -1);
                if (!call)
                    goto memerror;
                call->ntype = FUNCTION;
            }
            sub = create_sub_node (call);
            if (!sub)
                goto memerror;
            if (!add_to_call_stack (graph, curfunc, filename, sub))
                goto error;
#if AS_DEBUG
                printf ("Adding function call '%s' in func '%s', %d\n", name,
                        curfunc, line);
#endif
        }
        else if ((prev == IDENTIFIER || prev == COMMA) &&
                 token == IDENTIFIER && curfunc && name)
        {
            /* Is this a call to a variable? */
            g_node_t *call = get_definition_node (graph->defines, name,
                                                  filename);
            if (call && call->ntype == VARIABLE)
            {
                g_subnode_t *sub = create_sub_node (call);
                if (!sub)
                    goto memerror;
                if (!add_to_call_stack (graph, curfunc, filename, sub))
                    goto error;
#if AS_DEBUG
            printf ("Adding global variable call %s\n", name);
#endif
            }
        }
    }
    
    if (curfunc)
        free (curfunc);
    if (name)
        free (name);
    return TRUE;

memerror:
    fprintf (stderr, "%s: Memory allocation error for line %d\n", filename,
        line);
error:
    return FALSE;
}
