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

#include "cgraph.h"

#define C_DEBUG 0

/* Marker for the current file offset(s). */
static int offset = 0;
static int line = 0;

/* Forward declarations. */
static int skip_whitespaces (FILE *fp);
static inline int skip_strings (FILE *fp, int delim);
static inline int skip_brackets (FILE *fp, int delim);
static char* get_name (FILE *fp, int ch);
static int is_reserved (char *name);
static bool_t is_c_keyword (char *name);
static bool_t is_excluded (graph_t *graph, char *name);
static int parse_cpp (FILE *fp, int ch);
static int get_next_token (graph_t *graph, FILE *fp, char **name);

/**
 * Skips whitespaces, tabs and comments within a file buffer.
 *
 * \param fp The graph to read and skip the whitespaces from.
 * \return The character value or EOF, if the end of the file was reached.
 */
static int
skip_whitespaces (FILE *fp)
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
                offset = 0;
                line++;

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
static inline int
skip_strings (FILE *fp, int delim)
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
 * Skips characters until a matching closing bracket for the passed opening
 * bracket is reached.
 *
 * \param fp The file to read and skip the characters from.
 * \param delim The opening bracket to use as delimiter. Only '(' and
 *        '[' are recognized.
 * \return The next character after the delimiter or EOF.
 */
static inline int
skip_brackets (FILE *fp, int delim)
{
    int ch = '\0';
    int close = (delim == '(') ? ')' : ']';
     
    while (ch != close && ch != EOF)
    {
        ch = fgetc (fp);
        offset++;
    }
    return ch;
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
get_name (FILE *fp, int ch)
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
 * Checks whether the passed name is a reserved name.
 *
 * \param The NUL-terminated name to check.
 * \return The type of the bane or UNKNOWN in case it could not be
 *         determined.
 */
static int
is_reserved (char *name)
{
    if (strcmp ("static", name) == 0)
        return STATIC;
    else if (strcmp ("typedef", name) == 0)
        return TYPEDEF;
    else if (strcmp ("extern", name) == 0)
        return EXTERN;
    else if (strcmp ("struct", name) == 0 || strcmp ("union", name) == 0)
        return STRUCT;
    else if (strcmp ("enum", name) == 0)
        return ENUM;
    return UNKNOWN;
}

/**
 * Checks whether the passed name is a C keyword.
 *
 * \param name The NUL-terminated name to check.
 * \return TRUE, if the name is a C keyword, FALSE otherwise.
 */
static bool_t
is_c_keyword (char *name)
{
    int i;
    static const char* keywords[] = {
        "auto", "break", "case", "const", "continue", "default", "do",
        "else", "for", "goto", "if", "inline", "return", "sizeof", "switch",
        "volatile", "while", NULL
    };

    for (i = 0; keywords[i] != NULL; i++)
        if (strcmp (keywords[i], name) == 0)
            return TRUE;
    return FALSE;
}

/**
 * Checks whether the passed name is excluded by the graph.
 *
 * \param The graph to check for the exvclusion.
 * \param The name to check for the exvclusion.
 * \return TRUE, if the name is excluded, FALSE otherwise.
 */
static bool_t
is_excluded (graph_t *graph, char *name)
{
    node_t *cur;
    for (cur = graph->excludes; cur != NULL; cur = cur->next)
        if (strcmp (cur->name, name) == 0)
            return TRUE;
    
    
    return FALSE;
}

/**
 * Parses a C preprocessor directive. The graph's file reader offset
 * will be advanced to the first character after the directive.
 *
 * Additionally the function will update the line offset and current
 * filename scope for preprocessed files.
 *
 * \param fp The file to parse the directive from.
 * \param ch The # of the directive.
 * \return ENDOFFILE on reaching the EOF value of the file reader, or
 *         UNKNOWN, once the end of the directive is reached.
 */
static int
parse_cpp (FILE *fp, int ch)
{

    /* A directive. Treat those specially. */
    if (ch != '#')
        return UNKNOWN;

    ch = skip_whitespaces (fp);
    if (isdigit (ch))
    {
        int i = 0;
        char file[PATH_MAX] = { '\0' };
        
        /* We got some # nn expression - update the line no. */
        line = ch - '0';
        ch = fgetc (fp);
        while (isdigit (ch))
        {
            line = line * 10 + ch - '0';
            ch = fgetc (fp);
            if (ch == EOF)
                return ENDOFFILE;
        }
        
        /* If it is an expanded include
         *        # nn "xxx.xxx" ...
         * we need to preserve the filename.
         */
        while (ch != EOF && ch != '"')
            ch = fgetc (fp);
        if (ch == EOF)
            return ENDOFFILE;
        
        /* Get the filename. */
        while ((ch = fgetc (fp)) != '"' && ch != EOF && i < PATH_MAX - 1)
            file[i++] = ch;

        if (ch == EOF)
            return ENDOFFILE;
    }

    while (ch != '\n' && ch != EOF)
    {
        if (ch == '\\')
        {
            ch = fgetc (fp);
            line++;
        }
        ch = fgetc (fp);
    }
    if (ch == '\n')
        line++;
    return UNKNOWN;
}

/**
 * Gets the next valid token type from the file.
 *
 * \param graph The graph to get the next token for.
 * \param fp The file to get the next token from.
 * \return An enum value indicating the type of token.
 */
static int
get_next_token (graph_t *graph, FILE *fp, char **name)
{
    int ch;
    char *curname = NULL;

    do
    {
        ch = skip_whitespaces (fp);
        switch (ch)
        {
        case EOF:
            return ENDOFFILE;
        case '{':
            return BODYSTART;
        case '}':
            return BODYEND;

        case '"':
        case '\'':
            /* Skip string or char literals. */
            ch = skip_strings (fp, ch);
            if (ch == EOF)
                return ENDOFFILE;
            return get_next_token (graph, fp, name);

        case '[':
            return ARRAYSTART;
        case ']':
            return ARRAYEND;

        case '(':
            return ARGSTART;
        case ')':
            return ARGEND;
        case '#':
            if (parse_cpp (fp, ch) == ENDOFFILE)
                return ENDOFFILE;
            break;
        default:
            if (isalpha (ch) || ch == '_')
            {
                /* [A-Z] or _ are allowed values for a name identifier. */
                int token = UNKNOWN;

                if (*name)
                {
                    free (*name);
                    *name = NULL;
                }

                curname = get_name (fp, ch);

                /* Check for a builtin keyword. */
                if (is_c_keyword (curname))
                {
                    free (curname);
                    *name = NULL;
                    break;
                }

                token = is_reserved (curname);
                if (token != UNKNOWN) /* A reserved word was found. */
                {
                    free (curname);
                    *name = NULL;
                    return token;
                }

                /* Check, if the name is excluded. */
                if (!is_excluded (graph, curname))
                {
                    *name = curname;
                    return IDENTIFIER;
                }

                free (curname);
                *name = NULL;
                return token;
            }
            else if (ch == '=')
            {
                ch = fgetc (fp);
                if (ch != EOF && ch == '=')
                    return OPERATOR;
                else
                {
                    ungetc (ch, fp);
                    return ASSIGN;
                }
            }
            else if (ch == '-')
            {
                ch = fgetc (fp);
                if (ch != EOF && ch == '>')
                    return REFERENCE; /* -> */
                else
                {
                    if (ch != '=') /* Sikp -= */
                        ungetc (ch, fp);
                    return OPERATOR;
                }
            }
            else if (ch == '+' || ch == '/' || ch == '%' || ch == '&' ||
                     ch == '~' || ch == '>' || ch == '<' || ch == '^' ||
                     ch == '|' || ch == '!')
            {
                ch = fgetc (fp);
                if (ch != EOF && ch == '=')
                    return OPERATOR; /* +=, -= ... */
                else
                    ungetc (ch, fp);
                return OPERATOR;
            }
            else if (ch == '*')
                return POINTER;
            else if (ch == ':')
                return COLON;
            else if (ch == ';')
                return SEMICOLON;
            else if (ch == ',')
                return COMMA;
            else if (ch == '.')
                return REFERENCE;
            break;
        }
    }
    while (TRUE);
}

/**
 * Creates the output graph from the passed graph object.
 *
 * \param graph The graph object to create the output graph for.
 * \param fp The file to create the graph for.
 * \param filename The name of the file.
 * \return TRUE on success, FALSE on error.
 */
bool_t
lex_create_graph (graph_t *graph, FILE *fp, char *filename)
{
    char *curtype = NULL;
    char *curname = NULL; 
    char *curfunc = NULL;
    char *name = NULL;       /* The current node name. */
    int prev = SEMICOLON;    /* Previous token. */
    int token = SEMICOLON;   /* Current token. */
    int modifier = -1;       /* STATIC, EXTERN ... */
    int level = 0;           /* Scope level. */
    int arglevel = 0;        /* Argument level */
    int lastarglevel = 0;
    int funcline = -1;
    bool_t maybeknr = FALSE;  /* K&R func declaration */
    bool_t istypedef = FALSE; /* typedef indicator. */

    g_subnode_t *calls = NULL;
    line = 1;
    
    while (prev = token,
        (token = get_next_token (graph, fp, &name)) != ENDOFFILE)
    {
        /* Scope change. */
        if (token == BODYSTART || token == BODYEND)
        {
            (token == BODYSTART) ? level++ : level--;
            if (level < 0)
            {
                /* That should not happen. */
                fprintf (stderr, "%s: Brace level mismatch at line %d\n",
                    filename, line);
                goto error;
            }
            if (!level && curfunc)
            {
                free (curfunc);
                curfunc = NULL;
            }
        }

        /* Argument level. */
        if (token == ARGSTART || token == ARGEND)
        {
            (token == ARGSTART) ? arglevel++ : arglevel--;
            if (arglevel < 0)
            {
                /* That should not happen. */
                fprintf (stderr, "%s: Brace level mismatch at line %d\n",
                    filename, line);
                goto error;
            }
        }

        /* Preserve the variable/function modifier OR
         * TYPEDEF XXX - no need to know about it. */
        if (token == STATIC || token == EXTERN || token == TYPEDEF
            || token == ENUM)
        {
            modifier = token;
            if (token == TYPEDEF)
                istypedef = TRUE;
            continue;
        }
        if (token == STRUCT)
        {
            /* STRUCT XXX = ... || STRUCT XXX { ... } ||
             * void foo (..., arg) STRUCT ... ; { ... */
            modifier = token;
            if (prev == ARGEND && curtype && curname)
                maybeknr = TRUE;
            continue;
        }
        if (token == ASSIGN)
        {
            /* Reset the modifier environment on assignments */
            modifier = -1;
        }

        /* A NAME, but not in an enum, arg list or a redefinition. */
        if (token == IDENTIFIER && !arglevel && !istypedef && modifier != ENUM)
        {
            /* Exclude references. */
            if (prev == REFERENCE)
                modifier = REFERENCE;

            /* name can be a local function variable. */
            if (!level)
            {
                /* TYPE or NAME */
                if (!maybeknr && (prev == IDENTIFIER || prev == POINTER))
                {
                    if (curtype)
                    {
                        if (modifier == STRUCT)
                        {
                            /* STRUCT NAME ... */
                            char *tmp;
                            int len = strlen (curtype);
                            char *foo = strdup (curtype);
                            if (!foo)
                                goto memerror;
                            
                            tmp = realloc (curtype, sizeof (char) * (len + 8));
                            if (!tmp)
                                goto memerror;
                            curtype = tmp;
                            strcpy (curtype, "struct ");
                            strcpy (curtype + 7, foo);
                            curtype[len + 7] = '\0';
                            free (foo);
                        }
                        
                        if (strcmp (curtype, "unsigned") == 0)
                        {
                            /* unsigned modifier */
                            int len = strlen (name);
                            char *tmp = realloc
                                (curtype, sizeof (char) * (len + 10));
                            if (!tmp)
                                goto memerror;
                            curtype = tmp;
                            strcpy (curtype, "unsigned ");
                            strcpy (curtype + 9, name);
                            curtype[len + 9] = '\0';
                        }
                        else if (prev == POINTER)
                        {
                            /* TYPE* NAME construct */
                            int len = strlen (curtype);
                            char *tmp = realloc
                                (curtype, sizeof (char) * (len + 2));
                            if (!tmp)
                                goto memerror;
                            curtype = tmp;
                            curtype[len] = '*';
                            curtype[len + 1] = '\0';
                        }
                    }

                    /* NAME */
                    if (curname)
                    {
                        if (curtype)
                        {
                            /* There is already a type - it's possibly a 
                             * FOO int bar (); construct - move the prev
                             * name into the type. */
                            free (curtype);
                            curtype = strdup (curname);
                            if (!curtype)
                                goto memerror;
                        }
                        free (curname);
                    }
                    curname = strdup (name);
                    if (!curname)
                        goto memerror;

                    /* Save the current line for the later function
                     * addition. */
                    funcline = line;
                }
                else if (prev == ARGEND && curtype && curname)
                {
                    /* Possibly a K&R definition. */
                    maybeknr = TRUE;
                }
                else if (!maybeknr)
                {
                    /* TYPE */
                    if (curtype)
                        free (curtype);
                    curtype = strdup (name);
                    if (!curtype)
                        goto memerror;
                }
#if C_DEBUG
                if (curtype && curname)
                    printf
                        ("Found '%s %s' at line %d\n", curtype, curname, line);
#endif
            }
        }
        
        /* This could be a function call or a K&R style function
         * declaration. */
        if (name && prev == IDENTIFIER && token == ARGSTART &&
            modifier != REFERENCE)
        {
            if (level)
                lastarglevel = arglevel;
            else if (arglevel)
            {
                /* Not in a function - we are in a function
                 * declaration/definition argument list, e.g.
                 * void foo (void (*) bar);
                 *                 ^
                 */
                continue;
            }
            
            if (curname)
                free (curname);
            curname = strdup (name);
            if (!curname)
                goto memerror;
        }
        else if (token == ARGEND && curname && curfunc && 
                 arglevel == lastarglevel - 1)
        {
            /* { ... NAME ARGSTART ARGEND ... } - closed args of a
             * function call or a K&R declaration. */

            if (level)
            {
                /* Function call, create the call node for the temporary
                 * call stack. */
                g_subnode_t *sub = NULL;
                g_node_t *call = get_definition_node (graph->defines, curname,
                                                      filename);
                if (!call)
                {
                    call = add_g_node (graph, FUNCTION, curname, curtype,
                                       filename, -1);
                    if (!call)
                        goto memerror;
                    call->private = (modifier == STATIC) ? TRUE : FALSE;
                    call->ntype = FUNCTION;
                }
                sub = create_sub_node (call);
                if (!sub)
                    goto memerror;
                if (calls)
                    sub->next = calls;
                calls = sub;
#if C_DEBUG
                printf ("Adding function call '%s' in func '%s', %d\n", curname,
                        curfunc, line);
#endif
                free (curname);
                curname = NULL;
            }
        }

        if (prev == ARGEND && token == SEMICOLON && !level)
        {
            g_node_t *func;

            /* TYPE NAME ARGS SEMICOLON - this seems to be a function
             * declaration. */
            if (!curname || !curtype)
                continue;
            func = add_g_node (graph, FUNCTION, curname, curtype, filename, -1);
            if (!func)
                goto memerror;
#if C_DEBUG
            printf ("Adding function declaration %s\n", curname);
#endif
            func->private = (modifier == STATIC) ? TRUE : FALSE;
            free (curname);
            free (curtype);
            curname = NULL;
            curtype = NULL;
            
        }
        else if ((prev == ARGEND && token == BODYSTART) ||
                 (token == BODYSTART && maybeknr))
        {
            g_node_t *func;

            /* TYPE NAME ARGS BODY - this seems to be a function definition. */
            if (!curname || !curtype)
                continue;
            func = add_g_node (graph, FUNCTION, curname, curtype, filename,
                               funcline);
            if (!func)
                goto memerror;
#if C_DEBUG
            printf ("Adding function definition %s\n", curname);
#endif
            func->private = (modifier == STATIC) ? TRUE : FALSE;
            
            if (curfunc)
                free (curfunc);
            curfunc = strdup (curname);
            if (!curfunc)
                goto memerror;

            free (curname);
            free (curtype);
            curname = NULL;
            curtype = NULL;
            funcline = -1;
            maybeknr = FALSE;
        }
        else if (prev == IDENTIFIER && !level && !arglevel &&
                 (token == ASSIGN || token == SEMICOLON || token == COMMA ||
                  token == ARRAYSTART) && !maybeknr)
        {
            g_node_t *var;

            /* TYPE NAME [ASSIGN, SEMICOLON, COMMA] - global variable */
            if (!curname || !curtype)
                continue;
            var = add_g_node (graph, VARIABLE, curname, curtype, filename,
                              line); 
            if (!var)
                goto memerror;
#if C_DEBUG
            printf ("Adding global variable %s\n", curname);
#endif
            var->private = (modifier == STATIC) ? TRUE : FALSE;
            free (curname);
            free (curtype);
            curname = NULL;
            curtype = NULL;
        }
        
        /* { ... NAME ... } - possible variable reference. */ 
        if (token == IDENTIFIER && level && curfunc)
        {
            g_node_t *node = get_definition_node (graph->defines, name,
                                                  filename);
            if (node && node->ntype == VARIABLE)
            {
                g_subnode_t *sub = create_sub_node (node);
                if (!sub)
                    goto memerror;

                /* Found a global variable reference, add it to the call
                 * stack. */
                if (calls)
                    sub->next = calls;
                calls = sub;
            }
        }
            
        if (token == SEMICOLON || (token == ARGEND && lastarglevel > arglevel))
        {
            if (level && curfunc)
            {
                /* Reached the end of a statement, reset modifiers and
                   add the calls from the stack to the current
                   function. */
                if (calls)
                {
                    if (!add_to_call_stack (graph, curfunc, filename, calls))
                        goto error;
                }
                calls = NULL;
            }
        }

        if (token == SEMICOLON)
        {
            if (!maybeknr)
            {
                modifier = -1;
                if (curname)
                    free (curname);
                if (curtype)
                    free (curtype);
                curname = NULL;
                curtype = NULL;
            }
            
            istypedef = FALSE;
        }
    }

    if (name)
        free (name);
    return TRUE;

memerror:
    fprintf (stderr, "%s: Memory allocation error for line %d\n", filename,
        line);
error:
    return FALSE;
}
