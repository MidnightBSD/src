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

#include <string.h>
#include <stdlib.h>

#include "graph.h"

 /* Spacing after the function name for children:
  * foo: int(), <test.c 17>
  *    ^^^bar: void(), <test.c 23>
  */     
#define INDENT 3

static int compare_gnodes (const void *a, const void *b);
static bool_t nodes_contain (node_t* list, char *name);
static void print_node (g_node_t *node, int pad, size_t maxlen, int count);
static void print_preorder (graph_t *graph, g_node_t *node, int depth,
                            size_t maxlen, int pad, int *count);
static void print_callers (graph_t *graph, g_node_t *node, int depth,
                           size_t maxlen, int pad, int *count);
static void print_graphviz_node (g_node_t *node);
static void print_graphviz_preorder (graph_t *graph, g_node_t *node, int depth);
static void print_graphviz_callers (graph_t *graph, g_node_t *node, int depth);

/**
 * Qsort comparer that compares the names of two passed g_node_t
 * pointers.
 *
 * \param a The first g_node_t to compare.
 * \param b The second g_node_t to compare.
 * \return A strcmp() value.
 */
static int
compare_gnodes (const void *a, const void *b)
{
    return strcmp ((*(g_node_t* const*)a)->name, (*(g_node_t* const*)b)->name);
}

/**
 * Checks whether a node with the specified name exists in the passed
 * list.
 *
 * \param list The node_t list to check.
 * \param name The NUL-terminated name to check for.
 * \return TRUE, if a node with the name exists, FALSE otherwise.
 */
static bool_t
nodes_contain (node_t* list, char *name)
{
    while (list)
    {
        if (strcmp (list->name, name) == 0)
            return TRUE;
        list = list->next;
    }
    return FALSE;
}

/**
 * Prints a g_node_t node.
 * 
 * \param node The node to print.
 * \param pad The left handed padding to use.
 * \param maxlen The maximum length of all nodes on that level.
 * \param count The padding modifier.
 */
static void
print_node (g_node_t *node, int pad, size_t maxlen, int count)
{
    if (node->line != -1)
    {
        if (node->ntype == VARIABLE)
        {
            if (!node->type)
                printf ("%*d %*s: <%s %d>\n", pad, count, (int)maxlen,
                        node->name, node->file, node->line);
            else
                printf ("%*d %*s: %s, <%s %d>\n", pad, count, (int)maxlen,
                        node->name, node->type, node->file, node->line);
        }
        else
        {
            if (!node->type)
                printf ("%*d %*s: (), <%s %d>\n", pad, count, (int)maxlen,
                        node->name, node->file, node->line);
            else
                printf ("%*d %*s: %s(), <%s %d>\n", pad, count, (int)maxlen,
                        node->name, node->type, node->file, node->line);
        }
    }
    else
        printf ("%*d %*s: <>\n", pad, count, (int)maxlen, node->name);
}

/**
 * Prints the graph nodes using a preorder walkthrough. 
 *
 * \param graph The graph_t to print.
 * \param node The g_node_t to start from.
 * \param depth The node depth related to its position.
 * \param maxlen The maximum name length for the indentation on this
 *               depth.
 * \param pad The additional padding for the line numbers to print.
 * \param count The amount of nodes printed already (= line number).
 */
static void
print_preorder (graph_t *graph, g_node_t *node, int depth, size_t maxlen,
                int pad, int *count)
{
    int sublen = 0;
    g_subnode_t *sub = NULL;

    /* Skip functions and data starting with an underscore on demand. */
    if (!graph->privates && node->name[0] == '_')
        return;
    if (!graph->statics && node->ntype == VARIABLE)
        return;

    /* Skip excluded keywords. */
    if (nodes_contain (graph->excludes, node->name))
        return;

    print_node (node, pad, maxlen, *count);

    (*count)++;
    if (node->printed)
        return;
    node->printed = TRUE;
    
    if (depth >= graph->depth)
        return;

    sub = node->list;
    while (sub)
    {
        if (sub->content->namelen > sublen)
            sublen = sub->content->namelen;
        sub = sub->next;
    }

    sub = node->list;
    while (sub)
    {
        print_preorder (graph, sub->content, depth + 1,
            maxlen + sublen + INDENT, pad, count);
        sub = sub->next;
    }
}


/**
 * Prints the graph nodes in a caller<->callee order.
 *
 * \param graph The graph_t to print.
 * \param node The g_node_t to start from.
 * \param depth The node depth related to its position.
 * \param maxlen The maximum name length for the indentation on this
 *               depth.
 * \param pad The additional padding for the line numbers to print.
 * \param count The amount of nodes printed already (= line number).
 */
static void
print_callers (graph_t *graph, g_node_t *node, int depth, size_t maxlen,
               int pad, int *count)
{
    int sublen = 0;
    g_subnode_t *sub = NULL;

    /* Skip functions and data starting with an underscore on demand. */
    if (!graph->privates && node->name[0] == '_')
        return;
    if (!graph->statics && node->ntype == VARIABLE)
        return;

    /* Skip excluded keywords. */
    if (nodes_contain (graph->excludes, node->name))
        return;

    print_node (node, pad, maxlen, *count);

    (*count)++;
    
    if (depth >= graph->depth)
        return;

    sub = node->callers;
    while (sub)
    {
        if (sub->content->namelen > sublen)
            sublen = sub->content->namelen;
        sub = sub->next;
    }

    sub = node->callers;
    while (sub)
    {
        /* Skip functions and data starting with an underscore on demand. */
        if (!graph->privates && sub->content->name[0] == '_')
            return;
        if (!graph->statics && sub->content->ntype == VARIABLE)
            return;
        print_node (sub->content, pad, maxlen + sublen + 1, *count);
        (*count)++;
        sub = sub->next;
    }
}

/**
 * Prints a graph.
 *
 * \param graph The graph_t to print.
 */
void
print_graph (graph_t *graph)
{
    g_node_t *cur = NULL;
    g_subnode_t *sub = NULL;
    int count = 0;
    size_t maxlen = 0;
    int pad = 0;

    /* Get the maximum name length. */
    cur = graph->defines;
    while (cur)
    {
        /* Only use the height of the top root nodes. */
        if (!cur->callers && (size_t) cur->namelen > maxlen)
            maxlen = cur->namelen;

        sub = cur->list;
        while (sub)
        {
            count++;
            sub = sub->next;
        }
        count++;
        cur = cur->next;
    }

    /* Add an additional padding for the line numbers. */
    while (count > 0)
    {
        count /= 10;
        pad++;
    }

    count = 1;
    cur = graph->defines;
    if (!graph->reversed)
    {
        /* Usual preorder run. */
        if (graph->rootnode)
            print_preorder (graph, graph->rootnode, 0,
                strlen (graph->rootnode->name), pad, &count);
        else
        {
            while (cur)
            {
                if (!cur->printed)
                    print_preorder (graph, cur, 0, maxlen, pad, &count);
                cur = cur->next;
            }
        }
    }
    else
    {
        /* Print a reversed callee:caller graph. */
        int i = 0;
        g_node_t **rev = malloc (sizeof (g_node_t *) * graph->defcount);
        if (!rev)
        {
            fprintf (stderr, "Memory allocation error\n");
            return;
        }
        while (cur)
        {
            rev[i] = cur;
            cur = cur->next;
            i++;
        }
        qsort (rev, (size_t) graph->defcount, sizeof (g_node_t*),
            compare_gnodes);
        for (i = 0; i < graph->defcount; i++)
            print_callers(graph, rev[i], 0, maxlen, pad, &count);
        free (rev);
    }
}

/**
 * Prints a node using the graphvis conventiosn.
 *
 * \param node The g_node_t to print.
 */
static void
print_graphviz_node (g_node_t *node)
{
    (node->ntype == VARIABLE) ? printf ("%s_var", node->name) :
        printf ("%s", node->name);
}


/**
 * Prints the graph nodes using a preorder walkthrough using the
 * graphviz conventions.
 *
 * \param graph The graph_t to print.
 * \param node The g_node_t to start from.
 * \param depth The node depth related to its position.
 */
static void
print_graphviz_preorder (graph_t *graph, g_node_t *node, int depth)
{
    g_subnode_t *sub = NULL;
    long int count = 0;

    /* Skip functions and data starting with an underscore on demand. */
    if (!graph->privates && node->name[0] == '_')
        return;
    if (!graph->statics && node->ntype == VARIABLE)
        return;

    /* Skip excluded keywords. */
    if (nodes_contain (graph->excludes, node->name))
        return;

    if (node->printed)
        return;
    node->printed = TRUE;

    if (depth >= graph->depth)
        return;

    /* Create the graphviz node links. */
    sub = node->list;
    while (sub)
    {
        /* If the subnode matches the exclude criteria, do not print it. */
        if ((!graph->privates && sub->content->name[0] == '_') ||
            (!graph->statics && sub->content->ntype == VARIABLE) ||
            nodes_contain (graph->excludes, sub->content->name))
        {
            sub = sub->next;
            continue;
        }

        count++;

        /* Link the node. */
        printf ("  ");
        print_graphviz_node (node);
        printf (" -> ");
        print_graphviz_node (sub->content);
        printf (" [label=\"%ld\"];\n", count);

        sub = sub->next;
    }

    /* Down the tree in a preorder traversal. */
    sub = node->list;
    while (sub)
    {
        print_graphviz_preorder (graph, sub->content, depth + 1);
        sub = sub->next;
    }

}

/**
 * Prints the graph nodes in a caller<->callee order using the graphviz
 * conventions.
 *
 * \param graph The graph_t to print.
 * \param node The g_node_t to start from.
 * \param depth The node depth related to its position.
 */
static void
print_graphviz_callers (graph_t *graph, g_node_t *node, int depth)
{
    g_subnode_t *sub = NULL;
    long int count = 0;

    /* Skip functions and data starting with an underscore on demand. */
    if (!graph->privates && node->name[0] == '_')
        return;
    if (!graph->statics && node->ntype == VARIABLE)
        return;

    /* Skip excluded keywords. */
    if (nodes_contain (graph->excludes, node->name))
        return;

    if (node->printed)
        return;
    node->printed = TRUE;

    if (depth >= graph->depth)
        return;

    /* Create the graphviz node links. */
    sub = node->callers;
    depth += 1;
    while (sub)
    {

        /* If the subnode matches the exclude criteria, do not print it. */
        if ((!graph->privates && sub->content->name[0] == '_') ||
            (!graph->statics && sub->content->ntype == VARIABLE) ||
            nodes_contain (graph->excludes, sub->content->name))
        {
            sub = sub->next;
            continue;
        }

        count++;

        /* Link the node. */
        printf ("  ");
        print_graphviz_node (node);
        printf (" -> ");
        print_graphviz_node (sub->content);
        printf (" [label=\"%ld\"];\n", count);

        sub = sub->next;
    }
}

/**
 * Prints a graph using the graphviz conventions.
 *
 * \param graph The graph_t to print.
 */
void
print_graphviz_graph (graph_t *graph)
{
    g_node_t *cur = NULL;

    printf ("digraph \"%s\" {\n", "TODO");

    cur = graph->defines;
    if (!graph->reversed)
    {
        /* Usual preorder run. */
        if (graph->rootnode)
            print_graphviz_preorder (graph, graph->rootnode, 0);
        else
        {
            while (cur)
            {
                if (!cur->printed)
                    print_graphviz_preorder (graph, cur, 0);
                cur = cur->next;
            }
        }
    }
    else
    {
        /* Print a reversed callee:caller graph. */
        int i = 0;
        g_node_t **rev = malloc (sizeof (g_node_t *) * graph->defcount);
        if (!rev)
        {
            fprintf (stderr, "Memory allocation error\n");
            return;
        }
        while (cur)
        {
            rev[i] = cur;
            cur = cur->next;
            i++;
        }
        qsort (rev, (size_t) graph->defcount, sizeof (g_node_t*),
            compare_gnodes);
        for (i = 0; i < graph->defcount; i++)
            print_graphviz_callers (graph, rev[i], 0);
        free (rev);
    }

    /* Create all node descriptions first */
    cur = graph->defines;
    while (cur)
    {
        /* 
         * If the subnode matches the exclude criteria, do not print it.
         * If it was not printed already, do not show it as well as it
         * is unlikely that it was referenced by another node.
         */
        if ((!graph->privates && cur->name[0] == '_') ||
            (!graph->statics && cur->ntype == VARIABLE) ||
            nodes_contain (graph->excludes, cur->name) ||
            !cur->printed)
        {
            cur = cur->next;
            continue;
        }

        if (cur->ntype == VARIABLE)
            printf ("  %s_var [label=\"%s\",shape=box];\n", cur->name,
                cur->name);
        else
            printf ("  %s [label=\"%s\"];\n", cur->name, cur->name);
        cur = cur->next;
    }

    printf ("}\n");

}
