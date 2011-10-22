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
#include "ansi_keywords.h"
#include "posix_keywords.h"
#include "c99_keywords.h"
#include "gcc_keywords.h"

static void free_g_node (g_node_t *node);
static inline node_t* add_excludes (node_t *excludes, const char* keywords[]);

/**
 * Frees the passed g_node_t.
 *
 * \param node The g_node_t to free.
 */
static void
free_g_node (g_node_t *node)
{
    g_subnode_t *tmp = node->list;
    g_subnode_t *prev = NULL;

    free (node->name);
    if (node->type)
        free (node->type);
    if (node->file)
        free (node->file);
    while (tmp)
    {
        prev = tmp;
        tmp = tmp->next;
        free (prev);
    }

    tmp = node->callers;
    while (tmp)
    {
        prev = tmp;
        tmp = tmp->next;
        free (prev);
    }

    free (node);
}

/**
 * Creates a new g_node_t node. The return value has to be freed by the
 * caller.
 *
 * \param name The name of the node.
 * \param type The type of the node.
 * \param file The file the node belongs to.
 * \param line The line the name was found in.
 * \return A new g_node_t or NULL in case of an error.
 */
g_node_t*
create_g_node (char *name, char *type, char *file, int line)
{
    g_node_t *new = malloc (sizeof (g_node_t));
    if (!new)
        return NULL;

    new->name = strdup (name);
    if (!new->name)
    {
        free (new);
        return NULL;
    }
    new->namelen = strlen (name);

    new->type = NULL;
    if (type)
    { 
        new->type = strdup (type);
        if (!new->type)
        {
            free (new->name);
            free (new);
            return NULL;
        }
    }

    if (file)
    { 
        new->file = strdup (file);
        if (!new->file)
        {
            free (new->name);
            if (new->type)
                free (new->type);
            free (new);
            return NULL;
        }
    }

    new->line = line;
    new->next = NULL;
    new->list = NULL;
    new->callers = NULL;
    new->private = FALSE;
    new->printed = FALSE;
    return new;
}

/**
 * Creates a subnode entry for a certain g_node_t. The return value has
 * to be freed by the caller.
 *
 * \param node The g_node_t to create the subnode for.
 * \return A new g_subnode_t or NULL in case of an error.
 */
g_subnode_t*
create_sub_node (g_node_t *node)
{
    g_subnode_t *sub = malloc (sizeof (g_subnode_t));
    if (!sub)
        return NULL;
    sub->next = NULL;
    sub->content = node;
    return sub;
}

/**
 * Gets the g_node_t from the list, that has the passed name.
 *
 * \param list The list to get the node from.
 * \param name The NUL-terminated name to check for.
 * \param filename The definition filename.
 * \return A g_node_t with the name or NULL if none was found.
 */
g_node_t*
get_definition_node (g_node_t *list, char *name, char *filename)
{
    g_node_t *cur = list;
    while (cur)
    {
        if (strcmp (cur->name, name) == 0)
        {
            if (cur->private)
            {
                if (strcmp (cur->file, filename) == 0)
                    return cur;
                else
                    cur = cur->next;
            }
            else
                return cur;
        }
        cur = cur->next;
    }
    return NULL;
}

/**
 * Adds a new node to a given node_t list.
 *
 * \param list The list to add the node to or NULL for a new list.
 * \param name The name of the new node.
 * \return The (new) list.
 */
node_t*
add_node (node_t *list, const char *name)
{
    node_t *add = NULL;
    node_t *tmp = NULL;
     
    add = malloc (sizeof (node_t));
    if (!add)
        return NULL;

    /* Copy name. */
    add->name = strdup (name);
    if (!add->name)
    {
        free (add);
        return NULL;
    }
    add->next = NULL;

    /* First node. */
    if (!list)
        return add;
     
    /* Get to the end of the list. */
    tmp = list;
    while (tmp->next)
        tmp = tmp->next;
    tmp->next = add;
    return list;
}

/**
 * Frees a list of node_t nodes.
 *
 * \param list The list of nodes to free.
 */
void
free_nodes (node_t *list)
{
    node_t *cur = list;
    node_t *tmp = NULL;

    while (cur)
    {
        tmp = cur->next;
        free (cur->name);
        free (cur);
        cur = tmp;
    }
}

/**
 * Adds a new definition node to the graph's node list.
 *
 * \param graph The graph to add the node to.
 * \param ntype The type of the node.
 * \param name The name of the node.
 * \param type The type of the node.
 * \param file The file the node occured in.
 * \param line The line the node was found at.
 * \return The newly created node or NULL in case of an error.
 */
g_node_t*
add_g_node (graph_t *graph, NodeType ntype, char *name, char *type, char *file,
            int line)
{
    g_node_t *add = NULL;
    g_node_t *tmp = graph->defines;

    if (line != -1)
    {
        add = get_definition_node (graph->defines, name, file);
        if (add && add->line == -1)
        {
            /* Node was created from a call earlier. Set its type and
             * line. */
            add->line = line;
            if (type && !add->type)
            {
                add->type = strdup (type);
                if (!add->type)
                    return NULL;
            }
            if (file)
            {
                if (add->file)
                    free (add->file);
                add->file = strdup (file);
                if (!add->file)
                    return NULL;
            }
            add->ntype = ntype;
            return add;
        }
    }
     
    add = create_g_node (name, type, file, line);
    if (!add)
        return NULL;
    add->ntype = ntype;

    if (strcmp (name, graph->root) == 0)
        graph->rootnode = add;

    /* Increase the amount of existing nodes. */
    graph->defcount++;

    /* First node. */
    if (!graph->defines)
    {
        graph->defines = add;
        return add;
    }

    /* Get to the end of the list. */
    while (tmp && tmp->next)
        tmp = tmp->next;
    tmp->next = add;

    return add;
}

/**
 * Adds a list of nodes to the call list of the passed function node.
 *
 * \param graph The graph to add the call list to.
 * \param function The name of the node to add the calls to.
 * \param filename The definition filename.
 * \param calls The list of calls to add.
 * \return TRUE on success, FALSE on error.
 */
bool_t
add_to_call_stack (graph_t *graph, char *function, char *filename,
    g_subnode_t *calls)
{
    bool_t hascaller = FALSE;
    g_subnode_t *plist = NULL;
    g_subnode_t *tmp = NULL;
    g_subnode_t *unlink = NULL;
    g_subnode_t *prev = NULL;
    g_node_t *parent = get_definition_node (graph->defines, function, filename);

    if (!graph->complete)
    {
        /* We do not want to see redundant functions. Remove them from the
         * passed call stack. */
        tmp = calls;
        while (tmp)
        {
            prev = tmp;
            unlink = NULL;
            plist = tmp->next;
            while (plist)
            {
                if (tmp->content == plist->content ||
                    strcmp (tmp->content->name, plist->content->name) == 0)
                {
                    unlink = plist;
                    break;
                }
                prev = plist;
                plist = plist->next;
            }

            if (unlink)
            {
                prev->next = unlink->next;
                free (unlink);
            }
            tmp = tmp->next;
        }
        
        /* Now eliminate those, which are already in the call list of
         * the parent. */
        plist = parent->list;
        while (plist)
        {
            tmp = calls;
            prev = NULL;
            while (tmp)
            {
                if (tmp->content == plist->content ||
                    strcmp (tmp->content->name, plist->content->name) == 0)
                {
                    if (tmp == calls)
                        calls = tmp->next;
                    if (prev)
                        prev->next = tmp->next;
                    unlink = tmp;
                    tmp = tmp->next;
                    free (unlink);
                }
                else
                {
                    prev = tmp;
                    tmp = tmp->next;
                }
            }
            plist = plist->next;
        }
    }

    /* The callees need to know about their caller. */
    tmp = calls;
    while (tmp)
    {
        if (!tmp->content->callers)
        {
            tmp->content->callers = create_sub_node (parent);
            if (!tmp->content->callers)
            {
                fprintf (stderr, "Memory alloation error\n");
                return FALSE;
            }
            tmp = tmp->next;
            continue;
        }

        plist = tmp->content->callers;
        hascaller = FALSE;
        while (plist->next)
        {
            if (plist->content == parent)
            {
                hascaller = TRUE;
                break;
            }
            plist = plist->next;
        }

        if (!hascaller)
        {
            plist->next = create_sub_node (parent);
            if (!plist->next)
            {
                fprintf (stderr, "Memory allocation error\n");
                return FALSE;
            }
        }
        tmp = tmp->next;
    }

    plist = parent->list;
    if (plist)
    {
        while (plist->next)
            plist = plist->next;
        plist->next = calls;
    }
    else
        parent->list = calls;
    return TRUE;
}

/**
 * Frees a list of g_node_t nodes.
 *
 * \param list The list of nodes to free.
 */
void
free_g_nodes (g_node_t *list)
{
    g_node_t *cur = list;
    g_node_t *tmp = NULL;

    while (cur)
    {
        tmp = cur->next;
        free_g_node (cur);
        cur = tmp;
    }
}

/**
 * Frees a graph_t.
 *
 * \param graph The graph_t to free.
 */
void
free_graph (graph_t *graph)
{
    free_nodes (graph->excludes);
    free_g_nodes (graph->defines);
    free (graph);
}

/**
 * Adds both, specific types and keywords (like function definitions) to
 * the static exclude lists. This will ensure, that the user can
 * 'filter' the wanted output with some limited mechanisms.
 *  The caller has to free the exclude list
 *
 * \param excludes The excludes to add the keywords to.
 * \param keywords An array of keywords to add to the excludes.
 *
 * \return The new start of the excludes list or NULL in case of an
 *         error. If NULL is returned, the passed excludes are freed
 *         automatically.
 */
static inline node_t*
add_excludes (node_t *excludes, const char* keywords[])
{
    const char **cur;
    node_t *list = excludes;

    for (cur = keywords; *cur != NULL; cur++)
    {
        list = add_node (list, *cur);
        if (!list)
        {
            free_nodes (excludes);
            return NULL;
        }
    }
    return list;
}

/**
 * Creates the statically kept exclude lists based on the user input.
 *
 * \param list The list to add the excludes to.
 * \param excludes The type of excludes to add.
 * \return The updated excludes list or NULL in case of an error. If
 *         NULL is returned, the passed excludes are freed
 *         automatically.
 */
node_t*
create_excludes (node_t *list, int excludes)
{
    if ((excludes & NO_ANSI_KWDS) == NO_ANSI_KWDS)
        list = add_excludes (list, ansi_keywords);
    if ((excludes & NO_POSIX_KWDS) == NO_POSIX_KWDS)
        list = add_excludes (list, posix_keywords);
    if ((excludes & NO_C99_KWDS) == NO_C99_KWDS)
        list = add_excludes (list, c99_keywords);
    if ((excludes & NO_GCC_KWDS) == NO_GCC_KWDS)
        list = add_excludes (list, gcc_keywords);
    return list;
}
