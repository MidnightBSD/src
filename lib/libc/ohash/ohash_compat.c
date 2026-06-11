/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 1999, 2004 Marc Espie <espie@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/cdefs.h>

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct ohash_info_compat {
	ptrdiff_t key_offset;
	void *data;
	void *(*halloc)(size_t, void *);
	void (*hfree)(void *, size_t, void *);
	void *(*alloc)(size_t, void *);
};

struct ohash_record_compat {
	uint32_t hv;
	const char *p;
};

struct ohash_compat {
	struct ohash_record_compat *t;
	struct ohash_info_compat info;
	unsigned int size;
	unsigned int total;
	unsigned int deleted;
};

#define DELETED ((const char *)h)
#define NONE (h->size)
#define MINSIZE (1UL << 4)
#define MAXSIZE ((UINT_MAX >> 1U) + 1U)
#define MINDELETED 4

void *ohash_create_entry_compat(struct ohash_info_compat *, const char *,
    const char **);
void ohash_delete_compat(struct ohash_compat *);
unsigned int ohash_lookup_string_compat(struct ohash_compat *, const char *,
    uint32_t);
unsigned int ohash_lookup_interval_compat(struct ohash_compat *, const char *,
    const char *, uint32_t);
unsigned int ohash_lookup_memory_compat(struct ohash_compat *, const char *,
    size_t, uint32_t);
void *ohash_find_compat(struct ohash_compat *, unsigned int);
void *ohash_remove_compat(struct ohash_compat *, unsigned int);
void *ohash_insert_compat(struct ohash_compat *, unsigned int, void *);
void *ohash_first_compat(struct ohash_compat *, unsigned int *);
void *ohash_next_compat(struct ohash_compat *, unsigned int *);
unsigned int ohash_entries_compat(struct ohash_compat *);
void ohash_init_compat(struct ohash_compat *, unsigned int,
    struct ohash_info_compat *);
uint32_t ohash_interval_compat(const char *, const char **);
unsigned int ohash_qlookupi_compat(struct ohash_compat *, const char *,
    const char **);
unsigned int ohash_qlookup_compat(struct ohash_compat *, const char *);

static void ohash_resize_compat(struct ohash_compat *);
static size_t ohash_table_size_compat(size_t);

static size_t
ohash_table_size_compat(size_t size)
{
	if (size > SIZE_MAX / sizeof(struct ohash_record_compat))
		return 0;
	return sizeof(struct ohash_record_compat) * size;
}

void *
ohash_create_entry_compat(struct ohash_info_compat *i, const char *start,
    const char **end)
{
	char *p;
	ptrdiff_t len;
	size_t key_offset;
	size_t sz;

	if (!*end)
		*end = start + strlen(start);
	len = *end - start;
	if (len < 0 || i->key_offset < 0)
		return NULL;
	key_offset = (size_t)i->key_offset;
	if ((size_t)len > SIZE_MAX - key_offset - 1)
		return NULL;
	sz = key_offset + (size_t)len + 1;
	p = (i->alloc)(sz, i->data);
	if (p) {
		// NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
		memcpy(p + key_offset, start, (size_t)len);
		p[key_offset + (size_t)len] = '\0';
	}
	return (void *)p;
}

void
ohash_delete_compat(struct ohash_compat *h)
{
	(h->info.hfree)(h->t, ohash_table_size_compat(h->size), h->info.data);
#ifndef NDEBUG
	h->t = NULL;
#endif
}

static void
ohash_resize_compat(struct ohash_compat *h)
{
	struct ohash_record_compat *n;
	unsigned int ns, j;
	unsigned int i, incr;

	if (4 * h->deleted < h->total) {
		if (h->size >= MAXSIZE)
			ns = MAXSIZE;
		else
			ns = h->size << 1U;
	} else if (3 * h->deleted > 2 * h->total)
		ns = h->size >> 1;
	else
		ns = h->size;
	if (ns < MINSIZE)
		ns = MINSIZE;
	if (ohash_table_size_compat(ns) == 0)
		return;
#ifdef STATS_HASH
	STAT_HASH_EXPAND++;
	STAT_HASH_SIZE += ns - h->size;
#endif
	n = (h->info.halloc)(ohash_table_size_compat(ns), h->info.data);
	if (!n)
		return;

	for (j = 0; j < h->size; j++) {
		if (h->t[j].p != NULL && h->t[j].p != DELETED) {
			i = h->t[j].hv % ns;
			incr = ((h->t[j].hv % (ns - 2)) & ~1) + 1;
			while (n[i].p != NULL) {
				i += incr;
				if (i >= ns)
					i -= ns;
			}
			n[i].hv = h->t[j].hv;
			n[i].p = h->t[j].p;
		}
	}
	(h->info.hfree)(h->t, ohash_table_size_compat(h->size), h->info.data);
	h->t = n;
	h->size = ns;
	h->total -= h->deleted;
	h->deleted = 0;
}

void *
ohash_remove_compat(struct ohash_compat *h, unsigned int i)
{
	void *result = (void *)h->t[i].p;

	if (result == NULL || result == DELETED)
		return NULL;

#ifdef STATS_HASH
	STAT_HASH_ENTRIES--;
#endif
	h->t[i].p = DELETED;
	h->deleted++;
	if (h->deleted >= MINDELETED && 4 * h->deleted > h->total)
		ohash_resize_compat(h);
	return result;
}

void *
ohash_find_compat(struct ohash_compat *h, unsigned int i)
{
	if (h->t[i].p == DELETED)
		return NULL;
	else
		return (void *)h->t[i].p;
}

void *
ohash_insert_compat(struct ohash_compat *h, unsigned int i, void *p)
{
#ifdef STATS_HASH
	STAT_HASH_ENTRIES++;
#endif
	if (h->t[i].p == DELETED) {
		h->deleted--;
		h->t[i].p = p;
	} else {
		h->t[i].p = p;
		/* Arbitrary resize boundary.  Tweak if not efficient enough. */
		if (++h->total * 4 > h->size * 3)
			ohash_resize_compat(h);
	}
	return p;
}

unsigned int
ohash_entries_compat(struct ohash_compat *h)
{
	return h->total - h->deleted;
}

void *
ohash_first_compat(struct ohash_compat *h, unsigned int *pos)
{
	*pos = 0;
	return ohash_next_compat(h, pos);
}

void *
ohash_next_compat(struct ohash_compat *h, unsigned int *pos)
{
	for (; *pos < h->size; (*pos)++)
		if (h->t[*pos].p != DELETED && h->t[*pos].p != NULL)
			return (void *)h->t[(*pos)++].p;
	return NULL;
}

void
ohash_init_compat(struct ohash_compat *h, unsigned int size,
    struct ohash_info_compat *info)
{
	if (size >= sizeof(h->size) * CHAR_BIT - 1)
		h->size = MAXSIZE;
	else
		h->size = 1U << size;
	if (h->size < MINSIZE)
		h->size = MINSIZE;
#ifdef STATS_HASH
	STAT_HASH_CREATION++;
	STAT_HASH_SIZE += h->size;
#endif
	/* Copy info so that caller may free it. */
	h->info.key_offset = info->key_offset;
	h->info.halloc = info->halloc;
	h->info.hfree = info->hfree;
	h->info.alloc = info->alloc;
	h->info.data = info->data;
	if (ohash_table_size_compat(h->size) == 0) {
		h->t = NULL;
		h->size = 0;
		h->total = h->deleted = 0;
		return;
	}
	h->t = (h->info.halloc)(ohash_table_size_compat(h->size), h->info.data);
	h->total = h->deleted = 0;
}

uint32_t
ohash_interval_compat(const char *s, const char **e)
{
	uint32_t k;

	if (!*e)
		*e = s + strlen(s);
	if (s == *e)
		k = 0;
	else
		/* NOLINTNEXTLINE(bugprone-signed-char-misuse,cert-str34-c) */
		k = *s++;
	while (s != *e)
		k = ((k << 2) | (k >> 30)) ^ *s++;
	return k;
}

unsigned int
ohash_lookup_interval_compat(struct ohash_compat *h, const char *start,
    const char *end, uint32_t hv)
{
	unsigned int i, incr;
	unsigned int empty;

#ifdef STATS_HASH
	STAT_HASH_LOOKUP++;
#endif
	empty = NONE;
	i = hv % h->size;
	incr = ((hv % (h->size - 2)) & ~1) + 1;
	while (h->t[i].p != NULL) {
#ifdef STATS_HASH
		STAT_HASH_LENGTH++;
#endif
		if (h->t[i].p == DELETED) {
			if (empty == NONE)
				empty = i;
		} else if (h->t[i].hv == hv &&
		    strncmp(h->t[i].p + h->info.key_offset, start,
			end - start) == 0 &&
		    (h->t[i].p + h->info.key_offset)[end - start] == '\0') {
			if (empty != NONE) {
				h->t[empty].hv = hv;
				h->t[empty].p = h->t[i].p;
				h->t[i].p = DELETED;
				return empty;
			} else {
#ifdef STATS_HASH
				STAT_HASH_POSITIVE++;
#endif
				return i;
			}
		}
		i += incr;
		if (i >= h->size)
			i -= h->size;
	}

	/* Found an empty position. */
	if (empty != NONE)
		i = empty;
	h->t[i].hv = hv;
	return i;
}

unsigned int
ohash_lookup_memory_compat(struct ohash_compat *h, const char *k, size_t size,
    uint32_t hv)
{
	unsigned int i, incr;
	unsigned int empty;

#ifdef STATS_HASH
	STAT_HASH_LOOKUP++;
#endif
	empty = NONE;
	i = hv % h->size;
	incr = ((hv % (h->size - 2)) & ~1) + 1;
	while (h->t[i].p != NULL) {
#ifdef STATS_HASH
		STAT_HASH_LENGTH++;
#endif
		if (h->t[i].p == DELETED) {
			if (empty == NONE)
				empty = i;
		} else if (h->t[i].hv == hv &&
		    memcmp(h->t[i].p + h->info.key_offset, k, size) == 0) {
			if (empty != NONE) {
				h->t[empty].hv = hv;
				h->t[empty].p = h->t[i].p;
				h->t[i].p = DELETED;
				return empty;
			} else {
#ifdef STATS_HASH
				STAT_HASH_POSITIVE++;
#endif
			}
			return i;
		}
		i += incr;
		if (i >= h->size)
			i -= h->size;
	}

	/* Found an empty position. */
	if (empty != NONE)
		i = empty;
	h->t[i].hv = hv;
	return i;
}

unsigned int
ohash_lookup_string_compat(struct ohash_compat *h, const char *s, uint32_t hv)
{
	const char *e;

	e = s + strlen(s);
	return ohash_lookup_interval_compat(h, s, e, hv);
}

unsigned int
ohash_qlookup_compat(struct ohash_compat *h, const char *s)
{
	const char *e = NULL;
	return ohash_qlookupi_compat(h, s, &e);
}

unsigned int
ohash_qlookupi_compat(struct ohash_compat *h, const char *s, const char **e)
{
	uint32_t hv;

	hv = ohash_interval_compat(s, e);
	return ohash_lookup_interval_compat(h, s, *e, hv);
}

__sym_compat(ohash_init, ohash_init_compat, FBSD_1.0);
__sym_compat(ohash_delete, ohash_delete_compat, FBSD_1.0);
__sym_compat(ohash_lookup_string, ohash_lookup_string_compat, FBSD_1.0);
__sym_compat(ohash_lookup_interval, ohash_lookup_interval_compat, FBSD_1.0);
__sym_compat(ohash_lookup_memory, ohash_lookup_memory_compat, FBSD_1.0);
__sym_compat(ohash_find, ohash_find_compat, FBSD_1.0);
__sym_compat(ohash_remove, ohash_remove_compat, FBSD_1.0);
__sym_compat(ohash_insert, ohash_insert_compat, FBSD_1.0);
__sym_compat(ohash_first, ohash_first_compat, FBSD_1.0);
__sym_compat(ohash_next, ohash_next_compat, FBSD_1.0);
__sym_compat(ohash_entries, ohash_entries_compat, FBSD_1.0);
__sym_compat(ohash_create_entry, ohash_create_entry_compat, FBSD_1.0);
__sym_compat(ohash_interval, ohash_interval_compat, FBSD_1.0);
__sym_compat(ohash_qlookupi, ohash_qlookupi_compat, FBSD_1.0);
__sym_compat(ohash_qlookup, ohash_qlookup_compat, FBSD_1.0);
