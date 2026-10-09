/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Lucas Holt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <err.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "mport.h"
#include "mport_private.h"

struct pkgmeta_dependency_edge {
	int to;
	struct pkgmeta_dependency_edge *next;
};

static void free_edges(struct pkgmeta_dependency_edge **, int);

/* Splint does not model ownership transfer for the sparse edge-list cleanup. */
/*@ignore@*/
static void
free_edges(struct pkgmeta_dependency_edge **adj, int package_count)
{
	struct pkgmeta_dependency_edge *edge;
	struct pkgmeta_dependency_edge *next;
	int i;

	if (adj == NULL)
		return;

	for (i = 0; i < package_count; i++) {
		edge = adj[i];
		while (edge != NULL) {
			next = edge->next;
			free(edge);
			edge = next;
		}
	}

	free(adj);
}
/*@end@*/

/* record from -> to once; returns -1 on allocation failure */
static int
add_edge(struct pkgmeta_dependency_edge **adj, int *in_degree, int from, int to)
{
	const struct pkgmeta_dependency_edge *curr;
	struct pkgmeta_dependency_edge *new_edge;

	for (curr = adj[from]; curr != NULL; curr = curr->next) {
		if (curr->to == to)
			return 0;
	}

	new_edge = malloc(sizeof(struct pkgmeta_dependency_edge));
	if (new_edge == NULL) {
		warnx("Out of memory");
		return -1;
	}
	new_edge->to = to;
	new_edge->next = adj[from];
	adj[from] = new_edge;
	in_degree[to]++;

	return 0;
}

/* Splint hits an internal constraint bug on this cleanup path. */
/*@ignore@*/
mportPackageMeta **
mport_pkgmeta_sort_dependencies(
    mportInstance *mport, mportPackageMeta **flat_packs, int package_count, bool reverse_edges)
{
	mportPackageMeta **sorted_packs;
	mportPackageMeta **result;
	mportPackageMeta **downdeps;
	mportPackageMeta **providers;
	mportPackageMeta **d;
	struct pkgmeta_dependency_edge **adj;
	const struct pkgmeta_dependency_edge *curr;
	int *in_degree;
	bool *queued;
	int i;
	int j;
	int sorted_count;

	result = NULL;
	if (flat_packs == NULL || package_count <= 0)
		return calloc(1, sizeof(mportPackageMeta *));

	sorted_packs = calloc((size_t)(package_count + 1), sizeof(mportPackageMeta *));
	in_degree = calloc((size_t)package_count, sizeof(int));
	queued = calloc((size_t)package_count, sizeof(bool));
	adj = calloc((size_t)package_count, sizeof(struct pkgmeta_dependency_edge *));

	if (sorted_packs == NULL || in_degree == NULL || adj == NULL || queued == NULL) {
		warnx("Out of memory");
		goto error;
	}

	for (i = 0; i < package_count; i++) {
		downdeps = NULL;
		if (mport_pkgmeta_get_downdepends(mport, flat_packs[i], &downdeps) != MPORT_OK) {
			warnx("Error getting dependencies for %s: %s", flat_packs[i]->name,
			    mport_err_string());
			goto error;
		}

		if (downdeps == NULL)
			continue;

		for (d = downdeps; *d != NULL; d++) {
			for (j = 0; j < package_count; j++) {
				if (i == j)
					continue;
				if (strcmp((*d)->name, flat_packs[j]->name) != 0)
					continue;
				if (add_edge(adj, in_degree, reverse_edges ? j : i,
					reverse_edges ? i : j) != 0) {
					mport_pkgmeta_vec_free(downdeps);
					goto error;
				}
				break;
			}
		}
		mport_pkgmeta_vec_free(downdeps);
	}

	/*
	 * A package that links against a library another package provides
	 * depends on that package whether or not the port declared it.  The
	 * registry records both sides, so order providers like dependencies.
	 */
	for (i = 0; i < package_count; i++) {
		providers = NULL;
		if (mport_pkgmeta_search_master(mport, &providers,
			"pkg IN (SELECT DISTINCT p.pkg FROM shlibs_required r "
			"JOIN shlibs_provided p ON p.name = r.name "
			"WHERE r.pkg=%Q AND p.pkg != r.pkg)",
			flat_packs[i]->name) != MPORT_OK) {
			warnx("Error getting library providers for %s: %s", flat_packs[i]->name,
			    mport_err_string());
			goto error;
		}
		if (providers == NULL)
			continue;

		for (d = providers; *d != NULL; d++) {
			for (j = 0; j < package_count; j++) {
				if (i == j)
					continue;
				if (strcmp((*d)->name, flat_packs[j]->name) != 0)
					continue;
				if (add_edge(adj, in_degree, reverse_edges ? j : i,
					reverse_edges ? i : j) != 0) {
					mport_pkgmeta_vec_free(providers);
					goto error;
				}
				break;
			}
		}
		mport_pkgmeta_vec_free(providers);
	}

	sorted_count = 0;
	while (sorted_count < package_count) {
		for (i = 0; i < package_count; i++) {
			if (!queued[i] && in_degree[i] == 0)
				break;
		}

		if (i == package_count) {
			warnx(
			    "Dependency cycle detected among packages. Ordering may be sub-optimal.");
			for (i = 0; i < package_count; i++) {
				if (!queued[i])
					break;
			}
		}

		queued[i] = true;
		sorted_packs[sorted_count++] = flat_packs[i];

		curr = adj[i];
		while (curr != NULL) {
			in_degree[curr->to]--;
			curr = curr->next;
		}
	}

	result = sorted_packs;
	sorted_packs = NULL;

error:
	free_edges(adj, package_count);
	free(sorted_packs);
	free(in_degree);
	free(queued);
	return result;
}
/*@end@*/
