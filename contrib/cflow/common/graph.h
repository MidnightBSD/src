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
 *
 * $FreeBSD$
 *
 */

#ifndef GRAPH_H
#define GRAPH_H

#include <stdio.h>
#ifdef WIN32
#include "wincompat.h"
#endif

/* Boolean recognition. */
typedef int bool_t;
#define FALSE (0)
#define TRUE (!FALSE)

/* Node lists. */
typedef struct _node
{
    struct _node *next; /* Pointer to next one in list. */
    char         *name; /* Name of the current node. */
} node_t;

typedef enum 
{
    VARIABLE,
    FUNCTION
} NodeType;

/* An enhanced node, that is suitable for the POSIX definition of a
 * graph. Those ones are used for the main tree of the graph, containing
 * all functions at depth 0, which means declarations, definitions,
 * globals, externs and so on.
 */
typedef struct _g_node
{
    struct _g_node    *next;    /* Pointer to next main node in list. */
    struct _g_subnode *list;    /* Calls within the function. */
    struct _g_subnode *callers; /* Callers of the function. */
    char              *name;    /* Name of the current node. */
    int                namelen; /* Length of the name. */
    char              *type;    /* Type of the current node. */
    char              *file;    /* Definition/declaration file. */
    int                line;    /* Line where defined, not declared. */
    NodeType           ntype;   /* Type of the node. */
    bool_t             private; /* Indicates the scope of that node. */
    bool_t             printed; /* Indicates, whether the node was printed. */
} g_node_t;

/* To conserve space for the directed graph we use a subnode definition
 * within the g_node_t struct, which consists of a singly linked list
 * with pointers to the according g_node_t element.
 */
typedef struct _g_subnode
{
    struct _g_subnode *next;    /* The next node in the list. */
    struct _g_node    *content; /* The according g_node_t for this entry. */
} g_subnode_t;

/* File struct for graphs. */
typedef struct _graph
{
    node_t     *excludes; /* Excluded keywords. */
    g_node_t   *defines;  /* Associated definition list. */
    long int    defcount; /* Amount of defines. */
    bool_t      statics;  /* Include externals and static data (-i x). */
    bool_t      privates; /* Include data with a leading underscore (-i _). */
    int         depth;    /* Maximum depth. */
    const char *root;     /* The root node to use, default is "main" */
    g_node_t   *rootnode; /* The root node to use, default is "main" */
    bool_t      complete; /* Shall all nodes be printed? */
    bool_t      reversed; /* Shall it be printed in reverse order? */
    
} graph_t;

/* Keyword flags for bitwise ORs of the keywords to exclude. */
enum
{
    NO_ANSI_KWDS =  1,
    NO_POSIX_KWDS = 2,
    NO_C99_KWDS =   4,
    NO_GCC_KWDS =   8
};

/* Graph functions, defined in graph.c. */
node_t* add_node (node_t *list, const char *name);
g_subnode_t *create_sub_node (g_node_t *node);
g_node_t* create_g_node (char *name, char *type, char *file, int line);
g_node_t* get_definition_node (g_node_t *list, char *name, char *filename);
g_node_t* add_g_node (graph_t *graph, NodeType ntype, char *name, char* type,
                      char *file, int line);
bool_t add_to_call_stack (graph_t *graph, char *function, char *filename,
                          g_subnode_t *calls);
void free_nodes (node_t *list);
void free_g_nodes (g_node_t *list);
void free_graph (graph_t *graph);
node_t* create_excludes (node_t *list, int excludes);

/* Printing functions, defined in printgraph.c. */
void print_graph (graph_t *graph);
void print_graphviz_graph (graph_t *graph);

#endif /* GRAPH_H */
