/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Lucas Holt
 * All rights reserved.
 */

#include <sys/cdefs.h>

#include "mport.h"
#include "mport_private.h"

/* SPLINT_SKIP_FILE: Splint crashes/noises on query formatter vector ownership and libc models. */

#include <ctype.h>
#include <errno.h>
#include <fnmatch.h>
#include <libutil.h>
#include <regex.h>
#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	char **items;
	size_t count;
} query_list;

static bool match_pattern(const char *, const char *, const mportQueryOptions *);
static bool eval_expression(mportInstance *, mportPackageMeta *, const char *);
static char *field_value(mportInstance *, mportPackageMeta *, const char);
static char *annotation_value(mportInstance *, mportPackageMeta *, const char *);
static char *index_value(mportInstance *, mportPackageMeta *, const char *);
static int append_value(FILE *, const char *);
static int print_list(mportInstance *, mportPackageMeta *, char, FILE *);
static int list_for_code(mportInstance *, mportPackageMeta *, char, query_list *);
static void query_list_free(query_list *);
static int query_list_add(query_list *, const char *);
static char *xstrdup(const char *);
static char *lowercase_dup(const char *);
static bool truthy(const char *);
static bool compare_values(const char *, const char *, const char *);
static char *trim(char *);

/* Splint's constraint engine crashes on this vector ownership-transfer loop. */
/*@ignore@*/
MPORT_PUBLIC_API int
mport_query_installed(mportInstance *mport, const mportQueryOptions *opts, mportPackageMeta ***ref)
{
	mportPackageMeta **all_packs = NULL;
	mportPackageMeta **out = NULL;
	mportPackageMeta **pack;
	size_t count = 0;
	size_t kept = 0;
	int i;

	if (mport == NULL || opts == NULL || ref == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "Invalid arguments to mport_query_installed");

	*ref = NULL;

	if (mport_pkgmeta_list(mport, &all_packs) != MPORT_OK)
		RETURN_CURRENT_ERROR;

	if (all_packs == NULL)
		return MPORT_OK;

	for (pack = all_packs; *pack != NULL; pack++)
		count++;

	out = calloc(count + 1, sizeof(mportPackageMeta *));
	if (out == NULL) {
		mport_pkgmeta_vec_free(all_packs);
		RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory");
	}

	for (pack = all_packs; *pack != NULL; pack++) {
		bool matched = opts->all || opts->pattern_count == 0;

		for (i = 0; !matched && i < opts->pattern_count; i++)
			matched = match_pattern((*pack)->name, opts->patterns[i], opts);

		if (matched && opts->expression != NULL && opts->expression[0] != '\0')
			matched = eval_expression(mport, *pack, opts->expression);

		if (matched) {
			out[kept++] = *pack;
		} else {
			mport_pkgmeta_free(*pack);
		}
	}
	out[kept] = NULL;
	free(all_packs);

	if (kept == 0) {
		free(out);
	} else {
		*ref = out;
	}

	return MPORT_OK;
}
/*@end@*/

MPORT_PUBLIC_API int
mport_query_print(mportInstance *mport, mportPackageMeta **packs, const char *format, FILE *out)
{
	mportPackageMeta **pack;

	if (mport == NULL || format == NULL || out == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "Invalid arguments to mport_query_print");

	for (pack = packs; pack != NULL && *pack != NULL; pack++) {
		if (mport_query_format_package(mport, *pack, format, out) != MPORT_OK)
			RETURN_CURRENT_ERROR;
		fputc('\n', out);
	}

	return MPORT_OK;
}

MPORT_PUBLIC_API int
mport_query_format_package(
    mportInstance *mport, mportPackageMeta *pack, const char *format, FILE *out)
{
	const char *p;

	if (mport == NULL || pack == NULL || format == NULL || out == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "Invalid arguments to mport_query_format_package");

	for (p = format; *p != '\0'; p++) {
		char *value = NULL;
		query_list list = { 0 };
		char code;

		if (*p != '%') {
			fputc(*p, out);
			continue;
		}

		p++;
		if (*p == '\0')
			RETURN_ERROR(MPORT_ERR_FATAL, "Incomplete query format");

		if (*p == '%') {
			fputc('%', out);
			continue;
		}

		if (*p == '#' || *p == '?') {
			bool is_count = *p == '#';
			p++;
			if (*p == '\0')
				RETURN_ERROR(MPORT_ERR_FATAL, "Incomplete query list modifier");
			if (list_for_code(mport, pack, *p, &list) != MPORT_OK)
				RETURN_CURRENT_ERROR;
			fprintf(out, "%zu",
			    is_count ? list.count : (list.count > 0 ? (size_t)1 : (size_t)0));
			query_list_free(&list);
			continue;
		}

		code = *p;
		if (code == 's' && p[1] == 'b')
			p++;
		else if (code == 's' && p[1] == 'h') {
			code = 'h';
			p++;
		}
		switch (code) {
		case 'A':
		case 'C':
		case 'D':
		case 'F':
		case 'L':
		case 'O':
		case 'd':
		case 'r':
			if (print_list(mport, pack, code, out) != MPORT_OK)
				RETURN_CURRENT_ERROR;
			break;
		default:
			value = field_value(mport, pack, code);
			if (value == NULL)
				RETURN_ERRORX(
				    MPORT_ERR_FATAL, "Unsupported query format code %%%c", code);
			if (append_value(out, value) != MPORT_OK) {
				free(value);
				RETURN_CURRENT_ERROR;
			}
			free(value);
			break;
		}
	}

	return MPORT_OK;
}

MPORT_PUBLIC_API char *
mport_query_pkg_message(mportInstance *mport, mportPackageMeta *pkg)
{
	mportPackageMessage packageMessage;

	if (mport == NULL || pkg == NULL)
		return NULL;

	memset(&packageMessage, 0, sizeof(packageMessage));
	packageMessage.type = PKG_MESSAGE_ALWAYS;

	if (mport_pkg_message_load(mport, pkg, &packageMessage) != MPORT_OK)
		return NULL;

	free(packageMessage.minimum_version);
	free(packageMessage.maximum_version);

	if (packageMessage.str == NULL)
		return xstrdup("");

	return packageMessage.str;
}

static bool
match_pattern(const char *value, const char *pattern, const mportQueryOptions *opts)
{
	int flags = 0;
	regex_t re;
	int ret;

	if (value == NULL || pattern == NULL)
		return false;

	if (opts->match == MPORT_QUERY_MATCH_REGEX) {
		flags = REG_NOSUB | REG_EXTENDED;
		if (!opts->case_sensitive)
			flags |= REG_ICASE;
		if (regcomp(&re, pattern, flags) != 0)
			return false;
		ret = regexec(&re, value, 0, NULL, 0);
		regfree(&re);
		return ret == 0;
	}

	if (opts->match == MPORT_QUERY_MATCH_GLOB) {
#ifdef FNM_CASEFOLD
		if (!opts->case_sensitive)
			flags |= FNM_CASEFOLD;
		return fnmatch(pattern, value, flags) == 0;
#else
		bool matched;

		if (opts->case_sensitive)
			return fnmatch(pattern, value, flags) == 0;
		char *lower_pattern = lowercase_dup(pattern);
		char *lower_value = lowercase_dup(value);
		if (lower_pattern == NULL || lower_value == NULL) {
			free(lower_pattern);
			free(lower_value);
			return false;
		}
		matched = fnmatch(lower_pattern, lower_value, flags) == 0;
		free(lower_pattern);
		free(lower_value);
		return matched;
#endif
	}

	if (opts->case_sensitive)
		return strcmp(value, pattern) == 0;

	return strcasecmp(value, pattern) == 0;
}

static bool
eval_expression(mportInstance *mport, mportPackageMeta *pack, const char *expression)
{
	static const char *ops[] = { ">=", "<=", "!=", "~", "=", ">", "<", NULL };
	char *copy = NULL;
	char *or_save = NULL;
	char *or_term;
	bool result = false;
	int i;

	copy = strdup(expression);
	if (copy == NULL)
		return false;

	for (or_term = strtok_r(copy, "|", &or_save); or_term != NULL;
	    or_term = strtok_r(NULL, "|", &or_save)) {
		char *and_save = NULL;
		char *and_term;
		bool and_result = true;

		if (*or_term == '|')
			or_term++;

		or_term = trim(or_term);
		if (*or_term == '\0')
			continue;

		for (and_term = strtok_r(or_term, "&", &and_save); and_term != NULL;
		    and_term = strtok_r(NULL, "&", &and_save)) {
			char *term = trim(and_term);
			bool had_operator = false;
			bool negate = false;
			bool term_result = false;

			if (*term == '&')
				term++;
			term = trim(term);
			if (*term == '\0')
				continue;
			if (*term == '!') {
				negate = true;
				term = trim(term + 1);
			}
			if (*term == '\0')
				continue;

			for (i = 0; ops[i] != NULL; i++) {
				char *op = strstr(term, ops[i]);
				if (op != NULL) {
					char *left;
					char *right;
					char *value;

					had_operator = true;
					*op = '\0';
					left = trim(term);
					right = trim(op + strlen(ops[i]));
					if (left[0] == '%')
						left++;
					value = field_value(mport, pack, left[0]);
					if (value != NULL) {
						term_result = compare_values(value, ops[i], right);
						free(value);
					}
					break;
				}
			}

			if (!had_operator && !term_result && strchr(term, '=') == NULL &&
			    strchr(term, '~') == NULL && strchr(term, '<') == NULL &&
			    strchr(term, '>') == NULL) {
				char *value;
				if (term[0] == '%')
					term++;
				value = field_value(mport, pack, term[0]);
				term_result = truthy(value);
				free(value);
			}

			if (negate)
				term_result = !term_result;
			and_result = and_result && term_result;
		}

		result = result || and_result;
	}

	free(copy);
	return result;
}

static char *
field_value(mportInstance *mport, mportPackageMeta *pack, const char code)
{
	char *buf = NULL;
	char sizebuf[8];

	switch (code) {
	case 'n':
		return xstrdup(pack->name);
	case 'v':
		return xstrdup(pack->version);
	case 'o':
		return xstrdup(pack->origin);
	case 'p':
		return xstrdup(pack->prefix);
	case 'm':
		return annotation_value(mport, pack, "maintainer");
	case 'c':
		return xstrdup(pack->comment);
	case 'e':
		return xstrdup(pack->desc);
	case 'w':
		return annotation_value(mport, pack, "www");
	case 'a':
		return xstrdup(pack->automatic == MPORT_AUTOMATIC ? "1" : "0");
	case 'k':
		return xstrdup(pack->locked ? "1" : "0");
	case 't':
		if (asprintf(&buf, "%lld", (long long)pack->install_date) == -1)
			return NULL;
		return buf;
	case 's':
		if (asprintf(&buf, "%lld", (long long)pack->flatsize) == -1)
			return NULL;
		return buf;
	case 'h':
		humanize_number(sizebuf, sizeof(sizebuf), pack->flatsize, "B", HN_AUTOSCALE,
		    HN_DECIMAL | HN_IEC_PREFIXES);
		return xstrdup(sizebuf);
	case 'M':
		return mport_query_pkg_message(mport, pack);
	case 'X':
		return index_value(mport, pack, "hash");
	case 'l':
		return index_value(mport, pack, "license");
	case 'q':
		return xstrdup(MPORT_ARCH);
	case 'P':
		return mport_purl_uri(pack);
	default:
		return NULL;
	}
}

static char *
annotation_value(mportInstance *mport, mportPackageMeta *pack, const char *tag)
{
	char *value = NULL;

	if (mport_annotation_get(mport, pack->name, tag, &value) == MPORT_OK && value != NULL)
		return value;

	return xstrdup("");
}

static char *
index_value(mportInstance *mport, mportPackageMeta *pack, const char *field)
{
	mportIndexEntry **entries = NULL;
	char *value = NULL;

	if ((mport->flags & MPORT_INST_HAVE_INDEX) == 0)
		return xstrdup("");

	if (mport_index_lookup_pkgname(mport, pack->name, &entries) != MPORT_OK ||
	    entries == NULL || entries[0] == NULL) {
		mport_index_entry_free_vec(entries);
		return xstrdup("");
	}

	if (strcmp(field, "license") == 0)
		value = xstrdup(entries[0]->license);
	else if (strcmp(field, "hash") == 0)
		value = xstrdup(entries[0]->hash);
	else
		value = xstrdup("");

	mport_index_entry_free_vec(entries);
	return value;
}

static int
append_value(FILE *out, const char *value)
{
	if (value == NULL)
		return MPORT_OK;
	if (fputs(value, out) == EOF)
		RETURN_ERROR(MPORT_ERR_FATAL, strerror(errno));
	return MPORT_OK;
}

static int
print_list(mportInstance *mport, mportPackageMeta *pack, char code, FILE *out)
{
	query_list list = { 0 };
	size_t i;

	if (list_for_code(mport, pack, code, &list) != MPORT_OK)
		RETURN_CURRENT_ERROR;

	for (i = 0; i < list.count; i++) {
		if (i > 0)
			fputc(',', out);
		if (list.items[i] != NULL)
			fputs(list.items[i], out);
	}

	query_list_free(&list);
	return MPORT_OK;
}

static int
list_for_code(mportInstance *mport, mportPackageMeta *pack, char code, query_list *list)
{
	sqlite3_stmt *stmt = NULL;

	switch (code) {
	case 'C':
		if (mport_db_prepare(mport->db, &stmt,
			"SELECT category FROM categories WHERE pkg=%Q ORDER BY category",
			pack->name) != MPORT_OK)
			RETURN_CURRENT_ERROR;
		break;
	case 'd':
		if (mport_db_prepare(mport->db, &stmt,
			"SELECT depend_pkgname FROM depends WHERE pkg=%Q ORDER BY depend_pkgname",
			pack->name) != MPORT_OK)
			RETURN_CURRENT_ERROR;
		break;
	case 'r':
		if (mport_db_prepare(mport->db, &stmt,
			"SELECT pkg FROM depends WHERE depend_pkgname=%Q ORDER BY pkg",
			pack->name) != MPORT_OK)
			RETURN_CURRENT_ERROR;
		break;
	case 'F':
		if (mport_db_prepare(mport->db, &stmt,
			"SELECT data FROM assets WHERE pkg=%Q AND type IN (%d,%d,%d,%d) ORDER BY data",
			pack->name, ASSET_FILE, ASSET_SAMPLE, ASSET_SHELL, ASSET_INFO) != MPORT_OK)
			RETURN_CURRENT_ERROR;
		break;
	case 'D':
		if (mport_db_prepare(mport->db, &stmt,
			"SELECT data FROM assets WHERE pkg=%Q AND type IN (%d,%d,%d,%d) ORDER BY data",
			pack->name, ASSET_DIR, ASSET_DIRRM, ASSET_DIRRMTRY,
			ASSET_DIR_OWNER_MODE) != MPORT_OK)
			RETURN_CURRENT_ERROR;
		break;
	case 'A':
		if (mport_db_prepare(mport->db, &stmt,
			"SELECT tag || '=' || val FROM annotation WHERE pkg=%Q ORDER BY tag",
			pack->name) != MPORT_OK)
			RETURN_CURRENT_ERROR;
		break;
	case 'L': {
		char *license = index_value(mport, pack, "license");
		char *save = NULL;
		char *tok;
		for (tok = strtok_r(license, " ,", &save); tok != NULL;
		    tok = strtok_r(NULL, " ,", &save)) {
			if (query_list_add(list, tok) != MPORT_OK) {
				free(license);
				query_list_free(list);
				RETURN_CURRENT_ERROR;
			}
		}
		free(license);
		return MPORT_OK;
	}
	case 'O': {
		char *options = xstrdup(pack->options);
		char *save = NULL;
		char *tok;
		for (tok = strtok_r(options, " \n\t,", &save); tok != NULL;
		    tok = strtok_r(NULL, " \n\t,", &save)) {
			if (query_list_add(list, tok) != MPORT_OK) {
				free(options);
				query_list_free(list);
				RETURN_CURRENT_ERROR;
			}
		}
		free(options);
		return MPORT_OK;
	}
	default:
		RETURN_ERRORX(MPORT_ERR_FATAL, "Unsupported query list code %%%c", code);
	}

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		const unsigned char *value = sqlite3_column_text(stmt, 0);
		if (value != NULL && query_list_add(list, (const char *)value) != MPORT_OK) {
			sqlite3_finalize(stmt);
			query_list_free(list);
			RETURN_CURRENT_ERROR;
		}
	}
	sqlite3_finalize(stmt);
	return MPORT_OK;
}

static void
query_list_free(query_list *list)
{
	size_t i;

	for (i = 0; i < list->count; i++)
		free(list->items[i]);
	free(list->items);
	list->items = NULL;
	list->count = 0;
}

static int
query_list_add(query_list *list, const char *value)
{
	char **items;

	items = reallocarray(list->items, list->count + 1, sizeof(char *));
	if (items == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory");

	list->items = items;
	list->items[list->count] = xstrdup(value);
	if (list->items[list->count] == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory");
	list->count++;

	return MPORT_OK;
}

static char *
xstrdup(const char *value)
{
	if (value == NULL)
		value = "";
	return strdup(value);
}

static char *
lowercase_dup(const char *value)
{
	char *copy = xstrdup(value);
	char *p;

	if (copy == NULL)
		return NULL;

	for (p = copy; *p != '\0'; p++)
		*p = (char)tolower((unsigned char)*p);

	return copy;
}

static bool
truthy(const char *value)
{
	return value != NULL && value[0] != '\0' && strcmp(value, "0") != 0;
}

static bool
compare_values(const char *left, const char *op, const char *right)
{
	const char *right_cmp = right;
	char *right_buf = NULL;
	bool result;

	if (left == NULL)
		left = "";
	if (right_cmp == NULL)
		right_cmp = "";

	if (right_cmp[0] != '\0' && (right_cmp[0] == '\'' || right_cmp[0] == '"')) {
		size_t len = strlen(right_cmp);

		if (len >= 2 && right_cmp[len - 1] == right_cmp[0]) {
			right_buf = malloc(len - 1);
			if (right_buf != NULL) {
				memcpy(right_buf, right_cmp + 1, len - 2);
				right_buf[len - 2] = '\0';
				right_cmp = right_buf;
			}
		}
	}

	if (strcmp(op, "~") == 0)
		result = fnmatch(right_cmp, left, 0) == 0;
	else if (strcmp(op, "=") == 0)
		result = strcmp(left, right_cmp) == 0;
	else if (strcmp(op, "!=") == 0)
		result = strcmp(left, right_cmp) != 0;
	else if (strcmp(op, ">") == 0)
		result = mport_version_cmp(left, right_cmp) > 0;
	else if (strcmp(op, ">=") == 0)
		result = mport_version_cmp(left, right_cmp) >= 0;
	else if (strcmp(op, "<") == 0)
		result = mport_version_cmp(left, right_cmp) < 0;
	else if (strcmp(op, "<=") == 0)
		result = mport_version_cmp(left, right_cmp) <= 0;
	else
		result = false;

	free(right_buf);
	return result;
}

static char *
trim(char *value)
{
	char *end;

	while (isspace((unsigned char)*value))
		value++;
	if (*value == '\0')
		return value;

	end = value + strlen(value) - 1;
	while (end > value && isspace((unsigned char)*end))
		*end-- = '\0';

	return value;
}
