/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Lucas Holt
 */

#include <sys/cdefs.h>
#include <sys/param.h>

#include <ctype.h>
#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mport.h"
#include "mport_private.h"

/* Splint does not model mtree_dir_list's growable buffer or BSD string APIs well. */
/*@-boundsread -boundswrite -branchstate -compdef -compmempass -matchanyintegral@*/
/*@-nullstate -unqualifiedtrans -unrecog -usereleased@*/

typedef struct {
	char **items;
	size_t count;
	size_t cap;
} mtree_dir_list;

static const char *fallback_system_dirs[] = {
	"/boot",
	_PATH_ETC,
	"/etc/rc.d",
	"/root",
	_PATH_TMP,
	"/usr/lib",
	"/usr/bin",
	"/usr/sbin",
	_PATH_MAN,
	_PATH_LOCALE,
	_PATH_FIRMWARE,
	"/usr/share",
	"/usr/local",
	"/usr/local/bin",
	"/usr/local/sbin",
	"/usr/local/share",
	"/usr/local/lib",
	"/usr/local/libexec",
	"/usr/local/include",
	"/var",
	"/var/lib",
	"/var/log",
	"/var/spool",
	"/var/empty",
	_PATH_VARDB,
	_PATH_VARRUN,
	_PATH_VARTMP,
	_PATH_MAILDIR,
};

static const char *
mtree_dir_path(void)
{
	const char *mtree_dir;

	mtree_dir = getenv("MPORT_MTREE_DIR");
	/*@-boundsread@*/
	if (mtree_dir == NULL || *mtree_dir == '\0')
		mtree_dir = "/etc/mtree";
	/*@=boundsread@*/

	return mtree_dir;
}

static /*@observer@*/ /*@null@*/ const char *
mtree_root_for_file(const char *filename)
{
	if (strcmp(filename, "BSD.root.dist") == 0 || strcmp(filename, "BSD.sendmail.dist") == 0)
		return "/";
	if (strcmp(filename, "BSD.var.dist") == 0)
		return "/var";
	if (strcmp(filename, "BSD.local.dist") == 0)
		return "/usr/local";
	if (strcmp(filename, "BSD.usr.dist") == 0 || strcmp(filename, "BSD.include.dist") == 0 ||
	    strcmp(filename, "BSD.lib32.dist") == 0 || strcmp(filename, "BSD.debug.dist") == 0 ||
	    strcmp(filename, "BSD.tests.dist") == 0 ||
	    strncmp(filename, "BSD.x11", strlen("BSD.x11")) == 0)
		return "/usr";

	return NULL;
}

static void
normalize_path(char *path)
{
	size_t len;

	if (path == NULL)
		return;

	len = strlen(path);
	while (len > 1 && path[len - 1] == '/') {
		/*@-boundswrite@*/
		path[len - 1] = '\0';
		/*@=boundswrite@*/
		len--;
	}
}

static bool
list_contains(const mtree_dir_list *list, const char *path)
{
	size_t i;

	if (list == NULL || path == NULL)
		return false;

	for (i = 0; i < list->count; i++) {
		/*@-boundsread@*/
		if (strcmp(list->items[i], path) == 0)
			return true;
		/*@=boundsread@*/
	}

	return false;
}

static int
list_add(mtree_dir_list *list, const char *path)
{
	char **items;
	char *copy;
	size_t pathlen;

	if (list_contains(list, path))
		return MPORT_OK;

	if (list->count == list->cap) {
		size_t newcap;

		if (list->cap == 0)
			newcap = 32;
		else
			newcap = list->cap * 2;
		items = realloc(list->items, newcap * sizeof(char *));
		if (items == NULL)
			return MPORT_ERR_FATAL;
		list->items = items;
		list->cap = newcap;
	}

	pathlen = strlen(path) + 1;
	copy = malloc(pathlen);
	if (copy == NULL)
		return MPORT_ERR_FATAL;
	(void)strlcpy(copy, path, pathlen);

	/*@-boundswrite@*/
	list->items[list->count] = copy;
	/*@=boundswrite@*/
	list->count++;

	return MPORT_OK;
}

static void
list_free(mtree_dir_list *list)
{
	size_t i;

	for (i = 0; i < list->count; i++) {
		/*@-boundsread -unqualifiedtrans@*/
		free(list->items[i]);
		/*@=boundsread =unqualifiedtrans@*/
	}
	free(list->items);
	list->items = NULL;
	list->count = 0;
	list->cap = 0;
}

static void
path_pop(char *path)
{
	char *slash;

	normalize_path(path);
	if (strcmp(path, "/") == 0)
		return;

	slash = strrchr(path, '/');
	if (slash == NULL || slash == path)
		(void)strlcpy(path, "/", MAXPATHLEN);
	else
		*slash = '\0';
}

static void
path_append(char *out, size_t outsz, const char *base, const char *entry)
{
	const char *name = entry;

	/*@-boundsread@*/
	while (name[0] == '.' && name[1] == '/')
		name += 2;
	while (*name == '/')
		name++;
	/*@=boundsread@*/

	if (*name == '\0') {
		(void)strlcpy(out, base, outsz);
		normalize_path(out);
		return;
	}

	if (strcmp(base, "/") == 0)
		(void)snprintf(out, outsz, "/%s", name);
	else {
		/*@-boundswrite@*/
		(void)snprintf(out, outsz, "%s/%s", base, name);
		/*@=boundswrite@*/
	}
	normalize_path(out);
}

static int
parse_mtree_file(const char *path, const char *root, mtree_dir_list *list)
{
	FILE *fp;
	char line[MAXPATHLEN * 2];
	char cwd[MAXPATHLEN];

	fp = fopen(path, "re");
	if (fp == NULL)
		return MPORT_ERR_FATAL;

	(void)strlcpy(cwd, root, sizeof(cwd));
	normalize_path(cwd);
	if (list_add(list, cwd) != MPORT_OK) {
		(void)fclose(fp);
		return MPORT_ERR_FATAL;
	}

	/*@-matchanyintegral@*/
	while (fgets(line, sizeof(line), fp) != NULL) {
		/*@=matchanyintegral@*/
		char *token;
		char *p = line;
		char child[MAXPATHLEN];

		while (isspace((unsigned char)*p))
			p++;
		if (*p == '\0' || *p == '#')
			continue;
		if (strncmp(p, "/set", 4) == 0 || strncmp(p, "/unset", 6) == 0)
			continue;

		token = strsep(&p, " \t\r\n");
		/*@-boundsread@*/
		if (token == NULL || *token == '\0')
			continue;
		/*@=boundsread@*/
		if (strcmp(token, ".") == 0) {
			(void)strlcpy(cwd, root, sizeof(cwd));
			normalize_path(cwd);
			(void)list_add(list, cwd);
			continue;
		}
		if (strcmp(token, "..") == 0) {
			path_pop(cwd);
			continue;
		}

		if (strchr(token, '/') != NULL)
			path_append(child, sizeof(child), root, token);
		else
			path_append(child, sizeof(child), cwd, token);
		if (list_add(list, child) != MPORT_OK) {
			(void)fclose(fp);
			return MPORT_ERR_FATAL;
		}
		(void)strlcpy(cwd, child, sizeof(cwd));
	}

	(void)fclose(fp);
	return MPORT_OK;
}

static int
load_mtree_dirs(mtree_dir_list *list)
{
	const char *mtree_dir;
	DIR *dir;
	struct dirent *de;
	int loaded = 0;

	mtree_dir = mtree_dir_path();

	dir = opendir(mtree_dir);
	if (dir == NULL)
		return MPORT_ERR_FATAL;

	while ((de = readdir(dir)) != NULL) {
		const char *root = mtree_root_for_file(de->d_name);
		char path[MAXPATHLEN];

		if (root == NULL)
			continue;

		(void)snprintf(path, sizeof(path), "%s/%s", mtree_dir, de->d_name);
		if (parse_mtree_file(path, root, list) == MPORT_OK)
			loaded++;
	}

	(void)closedir(dir);
	return loaded > 0 ? MPORT_OK : MPORT_ERR_FATAL;
}

static bool
is_fallback_system_dir(const char *path)
{
	size_t i;

	for (i = 0; i < sizeof(fallback_system_dirs) / sizeof(fallback_system_dirs[0]); i++) {
		/*@-boundsread@*/
		if (strcmp(path, fallback_system_dirs[i]) == 0)
			return true;
		/*@=boundsread@*/
	}

	return false;
}

bool
mport_is_system_mtree_dir(const char *path)
{
	static mtree_dir_list list = { NULL, 0, 0 };
	static bool loaded = false;
	static bool using_fallback = false;
	static char cached_mtree_dir[MAXPATHLEN];
	char normalized[MAXPATHLEN];
	const char *current_mtree_dir;

	if (path == NULL)
		return false;

	current_mtree_dir = mtree_dir_path();
	if (loaded && strcmp(cached_mtree_dir, current_mtree_dir) != 0) {
		list_free(&list);
		loaded = false;
		using_fallback = false;
	}

	(void)strlcpy(normalized, path, sizeof(normalized));
	normalize_path(normalized);

	if (!loaded) {
		(void)strlcpy(cached_mtree_dir, current_mtree_dir, sizeof(cached_mtree_dir));
		if (load_mtree_dirs(&list) != MPORT_OK) {
			list_free(&list);
			using_fallback = true;
		}
		loaded = true;
	}

	if (using_fallback)
		return is_fallback_system_dir(normalized);

	return list_contains(&list, normalized);
}
