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

/*
 * Shared library analysis at package creation.
 *
 * Every regular file staged for the package is read as ELF.  An object with
 * DT_SONAME provides that library; every DT_NEEDED is a library the package
 * requires.  The rules follow FreeBSD pkg(8) so the same port knobs behave
 * the same way:
 *
 *   SHLIB_PROVIDE_PATHS_NATIVE, _COMPAT_32, _COMPAT_LINUX, _COMPAT_LINUX_32
 *       Comma separated directories.  A library counts as provided only
 *       when the directory it is installed into is one of them (chosen by
 *       the object's OS and word size).  An unset or empty list disables
 *       the filter.  Libraries elsewhere are private to the package and
 *       still cancel the package's own requirements.
 *   SHLIB_PROVIDE_IGNORE_GLOB, SHLIB_PROVIDE_IGNORE_REGEX
 *   SHLIB_REQUIRE_IGNORE_GLOB, SHLIB_REQUIRE_IGNORE_REGEX
 *       Comma separated patterns dropped from the respective list.
 *
 * Names carry the same suffixes pkg uses for non-native objects: ":32",
 * ":Linux" and ":Linux:32".  The ports framework exports these variables
 * in PKG_ENV when it runs mport.create(1).
 */

#include <sys/endian.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <gelf.h>
#include <libelf.h>
#include <regex.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mport.h"
#include "mport_private.h"

/* a compiled SHLIB_*_IGNORE_REGEX pattern */
struct shlib_regex {
	regex_t re;
	struct shlib_regex *next;
};

struct mport_shlib_scan {
	stringlist_t paths[MPORT_SHLIB_NFLAGS];
	stringlist_t provide_ignore_glob;
	stringlist_t require_ignore_glob;
	/*@null@*/ struct shlib_regex *provide_ignore_regex;
	/*@null@*/ struct shlib_regex *require_ignore_regex;

	stringlist_t provided; /* regular files in a provide path */
	stringlist_t maybe_provided; /* symlinks in a provide path */
	stringlist_t internal; /* sonames found outside the provide paths */
	stringlist_t required; /* every DT_NEEDED seen */
	stringlist_t basenames; /* of every regular file, for the self-check */
	int elfclass; /* word size of the target: ABI_FILE's, else the host's */
};

/* comma separated, surrounding blanks dropped; a pattern cannot contain a comma */
static int
split_env_list(const char *name, stringlist_t *out)
{
	const char *value = getenv(name);
	char *copy, *tok, *save;
	char *item;
	size_t len;

	if (value == NULL || value[0] == '\0')
		return MPORT_OK;
	if ((copy = strdup(value)) == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory");
	for (tok = strtok_r(copy, ",", &save); tok != NULL; tok = strtok_r(NULL, ",", &save)) {
		while (*tok == ' ' || *tok == '\t')
			tok++;
		len = strlen(tok);
		while (len > 0 && (tok[len - 1] == ' ' || tok[len - 1] == '\t'))
			tok[--len] = '\0';
		if (len == 0)
			continue;
		item = strdup(tok);
		if (item == NULL) {
			free(copy);
			RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory");
		}
		tll_push_back(*out, item);
	}
	free(copy);
	return MPORT_OK;
}

/* compile every pattern in an environment list; a bad one is an error */
static int
compile_env_regexes(const char *name, /*@out@*/ struct shlib_regex **head)
{
	stringlist_t patterns = tll_init();
	int ret;

	*head = NULL;
	if ((ret = split_env_list(name, &patterns)) != MPORT_OK)
		return ret;

	tll_foreach(patterns, it)
	{
		struct shlib_regex *node = calloc(1, sizeof(*node));
		int rc;

		if (node == NULL) {
			tll_free_and_free(patterns, free);
			RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory");
		}
		rc = regcomp(&node->re, it->item, REG_EXTENDED | REG_NOSUB);
		if (rc != 0) {
			char msg[128];

			(void)regerror(rc, &node->re, msg, sizeof(msg));
			free(node);
			ret = SET_ERRORX(
			    MPORT_ERR_FATAL, "%s: bad pattern '%s': %s", name, it->item, msg);
			tll_free_and_free(patterns, free);
			return ret;
		}
		node->next = *head;
		*head = node;
	}
	tll_free_and_free(patterns, free);
	return MPORT_OK;
}

static void
free_regexes(/*@null@*/ /*@only@*/ struct shlib_regex *head)
{
	while (head != NULL) {
		struct shlib_regex *next = head->next;

		regfree(&head->re);
		free(head);
		head = next;
	}
}

static bool
list_contains(const stringlist_t *list, const char *s)
{
	tll_foreach(*list, it)
	{
		if (strcmp(it->item, s) == 0)
			return true;
	}
	return false;
}

static void
list_add_unique(stringlist_t *list, char *s)
{
	if (s == NULL)
		return;
	if (list_contains(list, s)) {
		free(s);
		return;
	}
	tll_push_back(*list, s);
}

static int
cmp_str(const char *a, const char *b)
{
	return strcmp(a, b);
}

/* pattern lists: fnmatch(3) globs and the compiled extended regexps */
static bool
matches_ignore(const char *name, const stringlist_t *globs, const struct shlib_regex *regexps)
{
	tll_foreach(*globs, it)
	{
		if (fnmatch(it->item, name, 0) == 0)
			return true;
	}
	for (; regexps != NULL; regexps = regexps->next) {
		if (regexec(&regexps->re, name, 0, NULL, 0) == 0)
			return true;
	}
	return false;
}

/*
 * Lexically normalise an installed path: leading slash, no empty or "."
 * components, ".." folded.  The plist can spell a path as lib/../lib/x or
 * with doubled slashes, and the provide-path filter compares directories.
 */
static int
normalise_path(const char *in, char *out, size_t outlen)
{
	const char *p = in;
	size_t len = 0;

	if (outlen < 2)
		return -1;
	out[len++] = '/';
	out[len] = '\0';

	while (*p != '\0') {
		const char *end;
		size_t clen;

		while (*p == '/')
			p++;
		if (*p == '\0')
			break;
		end = strchr(p, '/');
		clen = (end == NULL) ? strlen(p) : (size_t)(end - p);

		if (clen == 1 && p[0] == '.') {
			/* nothing */
		} else if (clen == 2 && p[0] == '.' && p[1] == '.') {
			while (len > 1 && out[len - 1] != '/')
				len--;
			if (len > 1)
				len--;
			out[len] = '\0';
		} else {
			if (len > 1) {
				if (len + 1 >= outlen)
					return -1;
				out[len++] = '/';
			}
			if (len + clen >= outlen)
				return -1;
			memcpy(out + len, p, clen);
			len += clen;
			out[len] = '\0';
		}
		p += clen;
	}
	return 0;
}

/* strip a trailing slash so "/usr/local/lib/" and "/usr/local/lib" agree */
static bool
dir_paths_equal(const char *a, const char *b)
{
	size_t la = strlen(a), lb = strlen(b);

	while (la > 1 && a[la - 1] == '/')
		la--;
	while (lb > 1 && b[lb - 1] == '/')
		lb--;
	return la == lb && strncmp(a, b, la) == 0;
}

/* whether the directory holding installed_path is one of the listed paths */
static bool
in_provide_paths(const stringlist_t *paths, const char *installed_path)
{
	char dir[PATH_MAX];
	const char *slash;
	size_t len;

	if (tll_length(*paths) == 0)
		return true; /* unset: no filtering, as pkg does */

	slash = strrchr(installed_path, '/');
	if (slash == NULL)
		return false;
	len = (slash == installed_path) ? 1 : (size_t)(slash - installed_path);
	if (len >= sizeof(dir))
		return false;
	memcpy(dir, installed_path, len);
	dir[len] = '\0';

	tll_foreach(*paths, it)
	{
		if (dir_paths_equal(dir, it->item))
			return true;
	}
	return false;
}

/*@null@*/ /*@only@*/ char *
mport_shlib_name_with_flags(const char *name, int flags)
{
	const char *os = (flags & MPORT_SHLIB_LINUX) ? ":Linux" : "";
	const char *arch = (flags & MPORT_SHLIB_COMPAT_32) ? ":32" : "";
	char *out;

	if (asprintf(&out, "%s%s%s", name, os, arch) == -1)
		return NULL;
	return out;
}

/*
 * Read one file.  On return *provided holds the soname (caller frees) or
 * NULL, *flags the object's class, and required gains every DT_NEEDED.
 * Non-ELF files, static objects and objects for an architecture this host
 * cannot run are silently skipped.  Only libelf failures return an error.
 */
/*
 * The OS an object was built for, from its ABI note when it has one: a
 * FreeBSD or MidnightBSD NT_FREEBSD_ABI_TAG is native, a GNU NT_GNU_ABI_TAG
 * is Linux.  *os is -1 when no note says.  *osreldate is the value of a
 * MidnightBSD tag (__MidnightBSD_version, MMmmppp), or 0 without one; a
 * FreeBSD tag counts FreeBSD releases, so it does not name ours.
 */
static void
elf_abi_from_notes(Elf *elf, unsigned char encoding, /*@out@*/ int *os,
    /*@out@*/ uint32_t *osreldate)
{
	Elf_Scn *scn = NULL;

	*os = -1;
	*osreldate = 0;

	while ((scn = elf_nextscn(elf, scn)) != NULL) {
		GElf_Shdr shdr;
		Elf_Data *data;
		size_t off = 0;

		if (gelf_getshdr(scn, &shdr) != &shdr || shdr.sh_type != SHT_NOTE)
			continue;
		if ((data = elf_getdata(scn, NULL)) == NULL || data->d_buf == NULL)
			continue;

		/* Elf_Note is the same three words in both classes; name and
		 * descriptor are each padded to four bytes */
		while (off + sizeof(Elf_Note) <= data->d_size) {
			const Elf_Note *note = (const Elf_Note *)((const char *)data->d_buf + off);
			const char *owner = (const char *)(note + 1);
			size_t namesz = note->n_namesz;
			size_t descsz = note->n_descsz;
			size_t name_pad = (namesz + 3) & ~(size_t)3;
			size_t desc_pad = (descsz + 3) & ~(size_t)3;

			if (off + sizeof(Elf_Note) + name_pad + desc_pad > data->d_size ||
			    namesz == 0)
				break;
			if (owner[namesz - 1] == '\0') {
				if (note->n_type == NT_FREEBSD_ABI_TAG &&
				    (strcmp(owner, "FreeBSD") == 0 ||
					strcmp(owner, "MidnightBSD") == 0)) {
					/* the descriptor is not translated to host order */
					if (strcmp(owner, "MidnightBSD") == 0 &&
					    descsz >= sizeof(uint32_t)) {
						const void *desc = owner + name_pad;
						*osreldate = encoding == ELFDATA2MSB ?
						    be32dec(desc) :
						    le32dec(desc);
					}
					*os = MPORT_SHLIB_NATIVE;
					return;
				}
				if (note->n_type == NT_GNU_ABI_TAG && strcmp(owner, "GNU") == 0) {
					*os = MPORT_SHLIB_LINUX;
					return;
				}
			}
			off += sizeof(Elf_Note) + name_pad + desc_pad;
		}
	}
}

/*
 * Read the target ABI from a binary, as ABI_FILE names one for cross builds:
 * *osrelease is the release ("4.1") and *osreldate the raw tag from a
 * MidnightBSD ABI note, or NULL and 0 when the file has none, and *elfclass
 * its word size.  Any output may be NULL.  A file that cannot be read or is
 * not ELF is an error: guessing would record the build host's ABI.
 */
int
mport_abi_file_read(/*@notnull@*/ const char *path, /*@null@*/ /*@out@*/ char **osrelease,
    /*@null@*/ /*@out@*/ uint32_t *osreldate, /*@null@*/ /*@out@*/ int *elfclass)
{
	int fd;
	Elf *elf;
	GElf_Ehdr ehdr;
	struct stat sb;
	uint32_t tag;
	int os;
	int ret = MPORT_OK;

	if (osrelease != NULL)
		*osrelease = NULL;
	if (osreldate != NULL)
		*osreldate = 0;
	if (elfclass != NULL)
		*elfclass = ELFCLASSNONE;

	if (elf_version(EV_CURRENT) == EV_NONE)
		RETURN_ERRORX(
		    MPORT_ERR_FATAL, "ELF library initialization failed: %s", elf_errmsg(-1));

	if ((fd = open(path, O_RDONLY | O_CLOEXEC)) < 0)
		RETURN_ERRORX(MPORT_ERR_FATAL, "ABI_FILE %s: %s", path, strerror(errno));
	if (fstat(fd, &sb) != 0 || !S_ISREG(sb.st_mode)) {
		close(fd);
		RETURN_ERRORX(MPORT_ERR_FATAL, "ABI_FILE %s is not a regular file", path);
	}
	if ((elf = elf_begin(fd, ELF_C_READ, NULL)) == NULL) {
		close(fd);
		RETURN_ERRORX(MPORT_ERR_FATAL, "ABI_FILE %s is not an ELF file", path);
	}

	if (elf_kind(elf) != ELF_K_ELF || gelf_getehdr(elf, &ehdr) == NULL ||
	    (ehdr.e_ident[EI_CLASS] != ELFCLASS32 && ehdr.e_ident[EI_CLASS] != ELFCLASS64)) {
		ret = SET_ERRORX(MPORT_ERR_FATAL, "ABI_FILE %s is not an ELF file", path);
		goto out;
	}

	elf_abi_from_notes(elf, ehdr.e_ident[EI_DATA], &os, &tag);
	if (elfclass != NULL)
		*elfclass = ehdr.e_ident[EI_CLASS];
	if (osreldate != NULL)
		*osreldate = tag;
	if (osrelease != NULL && tag != 0 &&
	    asprintf(osrelease, "%u.%u", tag / 100000, (tag / 1000) % 100) == -1) {
		*osrelease = NULL;
		ret = SET_ERROR(MPORT_ERR_FATAL, "Out of memory");
	}

out:
	elf_end(elf);
	close(fd);
	return ret;
}

int
mport_shlib_analyse_elf(
    const char *path, /*@out@*/ char **provided, /*@out@*/ int *flags, stringlist_t *required)
{
	return mport_shlib_analyse_elf_for(MPORT_HOST_ELFCLASS, path, provided, flags, required);
}

/* As above, judging word size against a target of elfclass, not the host. */
int
mport_shlib_analyse_elf_for(int elfclass, const char *path, /*@out@*/ char **provided,
    /*@out@*/ int *flags, stringlist_t *required)
{
	int fd;
	Elf *elf;
	GElf_Ehdr ehdr;
	Elf_Scn *scn = NULL;
	Elf_Scn *dynamic = NULL;
	size_t sh_link = 0;
	size_t numdyn = 0;
	Elf_Data *data;
	struct stat sb;
	size_t i;
	uint32_t osreldate;
	int os;
	int ret = MPORT_OK;

	*provided = NULL;
	*flags = MPORT_SHLIB_NATIVE;

	if (elf_version(EV_CURRENT) == EV_NONE)
		RETURN_ERRORX(
		    MPORT_ERR_FATAL, "ELF library initialization failed: %s", elf_errmsg(-1));

	/* regular files only; the caller resolves symlinks inside the stage */
	if ((fd = open(path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC)) < 0)
		return MPORT_OK;
	if (fstat(fd, &sb) != 0 || !S_ISREG(sb.st_mode) || sb.st_size == 0) {
		close(fd);
		return MPORT_OK;
	}

	if ((elf = elf_begin(fd, ELF_C_READ, NULL)) == NULL) {
		close(fd);
		return MPORT_OK;
	}

	if (elf_kind(elf) != ELF_K_ELF || gelf_getehdr(elf, &ehdr) == NULL ||
	    (ehdr.e_type != ET_DYN && ehdr.e_type != ET_EXEC))
		goto out;

	/*
	 * OS: the ABI note first, then the header's OSABI (the MidnightBSD
	 * toolchain tags every object ELFOSABI_FREEBSD), and otherwise Linux,
	 * the same fallback the kernel's image activator applies.  Class: a
	 * 32-bit object on a 64-bit target is a compat32 object; a 64-bit
	 * object on a 32-bit target cannot run there and is skipped.
	 */
	elf_abi_from_notes(elf, ehdr.e_ident[EI_DATA], &os, &osreldate);
	if (os == MPORT_SHLIB_LINUX || (os < 0 && ehdr.e_ident[EI_OSABI] != ELFOSABI_FREEBSD))
		*flags |= MPORT_SHLIB_LINUX;
	if (ehdr.e_ident[EI_CLASS] != elfclass) {
		if (elfclass == ELFCLASS64 && ehdr.e_ident[EI_CLASS] == ELFCLASS32)
			*flags |= MPORT_SHLIB_COMPAT_32;
		else
			goto out;
	}

	while ((scn = elf_nextscn(elf, scn)) != NULL) {
		GElf_Shdr shdr;

		if (gelf_getshdr(scn, &shdr) != &shdr)
			continue;
		if (shdr.sh_type == SHT_DYNAMIC) {
			if (shdr.sh_entsize == 0)
				goto out;
			dynamic = scn;
			sh_link = shdr.sh_link;
			numdyn = shdr.sh_size / shdr.sh_entsize;
			break;
		}
	}
	if (dynamic == NULL || (data = elf_getdata(dynamic, NULL)) == NULL)
		goto out; /* statically linked */

	for (i = 0; i < numdyn; i++) {
		GElf_Dyn dyn;
		const char *name;
		char *named;

		if (gelf_getdyn(data, (int)i, &dyn) != &dyn)
			break;
		if (dyn.d_tag == DT_NULL)
			break;
		if (dyn.d_tag != DT_SONAME && dyn.d_tag != DT_NEEDED)
			continue;

		name = elf_strptr(elf, sh_link, dyn.d_un.d_val);
		if (name == NULL || strncmp(name, "lib", 3) != 0)
			continue; /* pkg ignores anything not named lib*; so do we */

		if (dyn.d_tag == DT_SONAME) {
			if (*provided != NULL) {
				free(*provided);
				*provided = NULL;
				ret = SET_ERRORX(MPORT_ERR_FATAL,
				    "malformed ELF file %s has multiple DT_SONAME entries", path);
				goto out;
			}
			if ((*provided = strdup(name)) == NULL) {
				ret = SET_ERROR(MPORT_ERR_FATAL, "Out of memory");
				goto out;
			}
		} else if (name[0] != '/') {
			/* a few builds record a full path; pkg skips those too */
			named = mport_shlib_name_with_flags(name, *flags);
			if (named == NULL) {
				free(*provided);
				*provided = NULL;
				ret = SET_ERROR(MPORT_ERR_FATAL, "Out of memory");
				goto out;
			}
			list_add_unique(required, named);
		}
	}

out:
	elf_end(elf);
	close(fd);
	return ret;
}

/* NULL with the mport error set: out of memory or a bad ignore pattern */
/*@null@*/ /*@only@*/ mportShlibScan *
mport_shlib_scan_new(void)
{
	mportShlibScan *scan = calloc(1, sizeof(*scan));
	stringlist_t empty = tll_init();
	const char *abi_file;
	int i;

	if (scan == NULL) {
		SET_ERROR(MPORT_ERR_FATAL, "Out of memory");
		return NULL;
	}
	for (i = 0; i < MPORT_SHLIB_NFLAGS; i++)
		scan->paths[i] = empty;
	scan->provide_ignore_glob = empty;
	scan->require_ignore_glob = empty;
	scan->provided = empty;
	scan->maybe_provided = empty;
	scan->internal = empty;
	scan->required = empty;
	scan->basenames = empty;

	if (split_env_list("SHLIB_PROVIDE_PATHS_NATIVE", &scan->paths[MPORT_SHLIB_NATIVE]) !=
		MPORT_OK ||
	    split_env_list("SHLIB_PROVIDE_PATHS_COMPAT_32", &scan->paths[MPORT_SHLIB_COMPAT_32]) !=
		MPORT_OK ||
	    split_env_list("SHLIB_PROVIDE_PATHS_COMPAT_LINUX", &scan->paths[MPORT_SHLIB_LINUX]) !=
		MPORT_OK ||
	    split_env_list("SHLIB_PROVIDE_PATHS_COMPAT_LINUX_32",
		&scan->paths[MPORT_SHLIB_LINUX | MPORT_SHLIB_COMPAT_32]) != MPORT_OK ||
	    split_env_list("SHLIB_PROVIDE_IGNORE_GLOB", &scan->provide_ignore_glob) != MPORT_OK ||
	    split_env_list("SHLIB_REQUIRE_IGNORE_GLOB", &scan->require_ignore_glob) != MPORT_OK ||
	    compile_env_regexes("SHLIB_PROVIDE_IGNORE_REGEX", &scan->provide_ignore_regex) !=
		MPORT_OK ||
	    compile_env_regexes("SHLIB_REQUIRE_IGNORE_REGEX", &scan->require_ignore_regex) !=
		MPORT_OK) {
		mport_shlib_scan_free(scan);
		return NULL;
	}

	scan->elfclass = MPORT_HOST_ELFCLASS;
	abi_file = getenv("ABI_FILE");
	if (abi_file != NULL && abi_file[0] != '\0' &&
	    mport_abi_file_read(abi_file, NULL, NULL, &scan->elfclass) != MPORT_OK) {
		mport_shlib_scan_free(scan);
		return NULL;
	}

	return scan;
}

void
mport_shlib_scan_free(/*@null@*/ /*@only@*/ mportShlibScan *scan)
{
	int i;

	if (scan == NULL)
		return;
	for (i = 0; i < MPORT_SHLIB_NFLAGS; i++)
		tll_free_and_free(scan->paths[i], free);
	tll_free_and_free(scan->provide_ignore_glob, free);
	tll_free_and_free(scan->require_ignore_glob, free);
	free_regexes(scan->provide_ignore_regex);
	free_regexes(scan->require_ignore_regex);
	tll_free_and_free(scan->provided, free);
	tll_free_and_free(scan->maybe_provided, free);
	tll_free_and_free(scan->internal, free);
	tll_free_and_free(scan->required, free);
	tll_free_and_free(scan->basenames, free);
	free(scan);
}

/*
 * Record one staged file.  staged_path is where the file is now,
 * installed_path where the package puts it (absolute, used for the
 * provide-path filter).
 */
/*
 * Where a staged symlink points, as a path inside the stage: an absolute
 * target is taken relative to the stage root (the package installs it
 * relative to the system root), a relative one relative to the link.  A
 * target that leaves the stage, or does not exist in it, yields -1: the
 * build host's own libraries must never be read in its place.
 */
static int
resolve_staged_link(const char *staged_path, const char *installed_path, char *out, size_t outlen)
{
	char target[PATH_MAX];
	char candidate[PATH_MAX];
	char root_path[PATH_MAX];
	char root_real[PATH_MAX];
	char cand_real[PATH_MAX];
	size_t staged_len = strlen(staged_path);
	size_t inst_len = strlen(installed_path);
	size_t root_len;
	ssize_t n;

	/* the stage root is the staged path less the installed path */
	if (staged_len <= inst_len ||
	    strcmp(staged_path + staged_len - inst_len, installed_path) != 0)
		return -1;
	root_len = staged_len - inst_len;

	n = readlink(staged_path, target, sizeof(target) - 1);
	if (n < 0)
		return -1;
	target[n] = '\0';

	if (target[0] == '/') {
		if (snprintf(candidate, sizeof(candidate), "%.*s%s", (int)root_len, staged_path,
			target) >= (int)sizeof(candidate))
			return -1;
	} else {
		const char *slash = strrchr(staged_path, '/');
		int dirlen = (slash == NULL) ? 0 : (int)(slash - staged_path);

		if (snprintf(candidate, sizeof(candidate), "%.*s/%s", dirlen, staged_path,
			target) >= (int)sizeof(candidate))
			return -1;
	}

	if (snprintf(root_path, sizeof(root_path), "%.*s", (int)root_len, staged_path) >=
	    (int)sizeof(root_path))
		return -1;
	if (realpath(root_path, root_real) == NULL || realpath(candidate, cand_real) == NULL)
		return -1;
	root_len = strlen(root_real);
	while (root_len > 1 && root_real[root_len - 1] == '/')
		root_len--;
	if (strncmp(cand_real, root_real, root_len) != 0 || cand_real[root_len] != '/')
		return -1;

	if (strlcpy(out, cand_real, outlen) >= outlen)
		return -1;
	return 0;
}

int
mport_shlib_scan_file(mportShlibScan *scan, const char *staged_path, const char *installed_raw)
{
	char *provided = NULL;
	char *named;
	int flags = MPORT_SHLIB_NATIVE;
	struct stat sb;
	const char *base;
	const char *object = staged_path;
	char installed_path[PATH_MAX];
	char resolved[PATH_MAX];

	if (scan == NULL)
		return MPORT_OK;

	if (normalise_path(installed_raw, installed_path, sizeof(installed_path)) != 0)
		RETURN_ERRORX(MPORT_ERR_FATAL, "Path too long: %s", installed_raw);

	if (lstat(staged_path, &sb) != 0)
		return MPORT_OK;

	if (S_ISREG(sb.st_mode)) {
		char *copy;

		base = strrchr(installed_path, '/');
		base = (base == NULL) ? installed_path : base + 1;
		if ((copy = strdup(base)) == NULL)
			RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory");
		list_add_unique(&scan->basenames, copy);
	} else if (S_ISLNK(sb.st_mode)) {
		if (resolve_staged_link(staged_path, installed_path, resolved, sizeof(resolved)) !=
		    0)
			return MPORT_OK; /* leaves the stage, or dangling: not ours to read */
		object = resolved;
	} else {
		return MPORT_OK;
	}

	if (mport_shlib_analyse_elf_for(
		scan->elfclass, object, &provided, &flags, &scan->required) != MPORT_OK)
		return mport_err_code();

	if (provided == NULL)
		return MPORT_OK;

	named = mport_shlib_name_with_flags(provided, flags);
	free(provided);
	if (named == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory");

	if (in_provide_paths(&scan->paths[flags & (MPORT_SHLIB_NFLAGS - 1)], installed_path)) {
		if (S_ISREG(sb.st_mode))
			list_add_unique(&scan->provided, named);
		else
			list_add_unique(&scan->maybe_provided, named);
	} else {
		list_add_unique(&scan->internal, named);
	}

	return MPORT_OK;
}

/*
 * Settle the lists and hand them to the package: provided sonames the
 * package exports, required sonames it needs from elsewhere.  Both are
 * sorted and unique.  no_provide_shlib is set when the package exports
 * nothing; a value the caller already set stands.
 */
int
mport_shlib_scan_finish(mportShlibScan *scan, mportPackageMeta *pack)
{
	if (scan == NULL || pack == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "Invalid shared library scan");

	/*
	 * A symlink in a provide path whose target is a private copy of the
	 * same soname (libfoo.so.1 -> ../private/libfoo.so.1.2) exports that
	 * soname after all.
	 */
	tll_foreach(scan->maybe_provided, mp)
	{
		tll_foreach(scan->internal, in)
		{
			if (strcmp(mp->item, in->item) == 0) {
				char *copy = strdup(mp->item);

				if (copy == NULL)
					RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory");
				list_add_unique(&scan->provided, copy);
				tll_remove_and_free(scan->internal, in, free);
				break;
			}
		}
	}

	/* requirements the package meets itself are not requirements */
	tll_foreach(scan->required, it)
	{
		const char *s = it->item;
		const char *colon = strchr(s, ':');
		size_t plain_len = (colon == NULL) ? strlen(s) : (size_t)(colon - s);
		bool self = list_contains(&scan->provided, s) ||
		    list_contains(&scan->internal, s) || list_contains(&scan->maybe_provided, s);

		if (!self) {
			tll_foreach(scan->basenames, bn)
			{
				if (strlen(bn->item) == plain_len &&
				    strncmp(bn->item, s, plain_len) == 0) {
					self = true;
					break;
				}
			}
		}
		if (self ||
		    matches_ignore(s, &scan->require_ignore_glob, scan->require_ignore_regex)) {
			tll_remove_and_free(scan->required, it, free);
			continue;
		}
	}

	tll_foreach(scan->provided, it)
	{
		if (matches_ignore(
			it->item, &scan->provide_ignore_glob, scan->provide_ignore_regex))
			tll_remove_and_free(scan->provided, it, free);
	}

	tll_free_and_free(pack->shlibs_provided, free);
	tll_free_and_free(pack->shlibs_required, free);
	tll_foreach(scan->provided, it)
	{
		char *copy = strdup(it->item);

		if (copy == NULL)
			RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory");
		tll_push_back(pack->shlibs_provided, copy);
	}
	tll_foreach(scan->required, it)
	{
		char *copy = strdup(it->item);

		if (copy == NULL)
			RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory");
		tll_push_back(pack->shlibs_required, copy);
	}
	tll_sort(pack->shlibs_provided, cmp_str);
	tll_sort(pack->shlibs_required, cmp_str);

	if (tll_length(pack->shlibs_provided) == 0)
		pack->no_provide_shlib = 1;

	return MPORT_OK;
}

/* ---- registry ---------------------------------------------------------- */

static bool
stub_has_table(mportInstance *mport, const char *table)
{
	int count = 0;

	if (mport_db_count(mport->db, &count,
		"SELECT count(*) FROM stub.sqlite_master WHERE type='table' AND name=%Q", table) !=
	    MPORT_OK)
		return false;
	return count > 0;
}

/*
 * Copy the package file's lists into the registry.  Packages built before
 * the scan existed carry neither table and register nothing, which reads
 * as "unknown" rather than "none" to the consumers.
 */
int
mport_shlibs_register(mportInstance *mport, mportPackageMeta *pkg)
{
	if (pkg == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "pkg is null");
	if (mport->db == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "mport database is not initialized");

	if (stub_has_table(mport, "shlibs_provided") &&
	    mport_db_do(mport->db,
		"INSERT INTO shlibs_provided (pkg, name) SELECT pkg, name FROM stub.shlibs_provided WHERE pkg=%Q",
		pkg->name) != MPORT_OK)
		RETURN_CURRENT_ERROR;

	if (stub_has_table(mport, "shlibs_required") &&
	    mport_db_do(mport->db,
		"INSERT INTO shlibs_required (pkg, name) SELECT pkg, name FROM stub.shlibs_required WHERE pkg=%Q",
		pkg->name) != MPORT_OK)
		RETURN_CURRENT_ERROR;

	return MPORT_OK;
}

static int
load_names(mportInstance *mport, const char *table, const char *pkgname, stringlist_t *out)
{
	sqlite3_stmt *stmt = NULL;
	int rc;

	if (mport_db_prepare(mport->db, &stmt,
		"SELECT name FROM %s WHERE pkg=%Q ORDER BY name", table, pkgname) != MPORT_OK) {
		sqlite3_finalize(stmt);
		RETURN_CURRENT_ERROR;
	}
	while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		const char *name = (const char *)sqlite3_column_text(stmt, 0);
		char *copy;

		if (name == NULL)
			continue;
		if ((copy = strdup(name)) == NULL) {
			sqlite3_finalize(stmt);
			RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory");
		}
		tll_push_back(*out, copy);
	}
	sqlite3_finalize(stmt);
	if (rc != SQLITE_DONE)
		RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(mport->db));

	return MPORT_OK;
}

/* the registered lists for an installed package; either output may be NULL */
MPORT_PUBLIC_API int
mport_shlibs_get(
    mportInstance *mport, const char *pkgname, stringlist_t *provided, stringlist_t *required)
{
	if (mport == NULL || pkgname == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "Invalid arguments");

	if (provided != NULL && load_names(mport, "shlibs_provided", pkgname, provided) != MPORT_OK)
		RETURN_CURRENT_ERROR;
	if (required != NULL && load_names(mport, "shlibs_required", pkgname, required) != MPORT_OK)
		RETURN_CURRENT_ERROR;

	return MPORT_OK;
}

/* "libfoo.so.1" from "/usr/local/lib/libfoo.so.1"; the flag suffix is kept */
static const char *
library_name(const char *library)
{
	const char *slash = strrchr(library, '/');

	return slash == NULL ? library : slash + 1;
}

MPORT_PUBLIC_API int
mport_shlib_providers(mportInstance *mport, const char *library, mportPackageMeta ***packs)
{
	if (mport == NULL || library == NULL || packs == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "Invalid arguments");

	return mport_pkgmeta_search_master(mport, packs,
	    "pkg IN (SELECT pkg FROM shlibs_provided WHERE name=%Q) ORDER BY pkg",
	    library_name(library));
}

MPORT_PUBLIC_API int
mport_shlib_requirers(mportInstance *mport, const char *library, mportPackageMeta ***packs)
{
	if (mport == NULL || library == NULL || packs == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "Invalid arguments");

	return mport_pkgmeta_search_master(mport, packs,
	    "pkg IN (SELECT pkg FROM shlibs_required WHERE name=%Q) ORDER BY pkg",
	    library_name(library));
}

/*
 * Whether the base system ships a library, by looking in the directories
 * the run-time linker searches for the object's class.  The base system is
 * not a package here, the way pkgbase is on FreeBSD, so this stands in for
 * a provider.
 */
MPORT_PUBLIC_API bool
mport_shlib_in_base(const char *name)
{
	static const char *const native[] = { "/lib", "/usr/lib", NULL };
	static const char *const compat32[] = { "/usr/lib32", NULL };
	static const char *const linux64[] = { "/compat/linux/lib64", "/compat/linux/usr/lib64",
		NULL };
	static const char *const linux32[] = { "/compat/linux/lib", "/compat/linux/usr/lib",
		NULL };
	const char *const *dirs = native;
	const char *colon;
	char plain[PATH_MAX];
	char path[PATH_MAX];
	size_t len;
	struct stat sb;

	if (name == NULL || name[0] == '\0')
		return false;
	name = library_name(name);

	colon = strchr(name, ':');
	len = (colon == NULL) ? strlen(name) : (size_t)(colon - name);
	if (len == 0 || len >= sizeof(plain))
		return false;
	memcpy(plain, name, len);
	plain[len] = '\0';

	if (colon != NULL) {
		bool is_linux = strstr(colon, ":Linux") != NULL;
		bool is_32 = strstr(colon, ":32") != NULL;

		if (is_linux)
			dirs = is_32 ? linux32 : linux64;
		else if (is_32)
			dirs = compat32;
	}

	for (; *dirs != NULL; dirs++) {
		if (snprintf(path, sizeof(path), "%s/%s", *dirs, plain) >= (int)sizeof(path))
			continue;
		if (stat(path, &sb) == 0 && S_ISREG(sb.st_mode))
			return true;
	}
	return false;
}

/*
 * Report every registered requirement that no installed package provides
 * and the base system does not ship.  Returns the count, or -1 on error.
 */
MPORT_PUBLIC_API int
mport_check_missing_shlibs(mportInstance *mport)
{
	sqlite3_stmt *stmt = NULL;
	int missing = 0;
	int rc;

	if (mport == NULL)
		return -1;

	if (mport_db_prepare(mport->db, &stmt,
		"SELECT r.pkg, r.name FROM shlibs_required r "
		"WHERE EXISTS (SELECT 1 FROM packages p WHERE p.pkg = r.pkg) "
		"AND NOT EXISTS (SELECT 1 FROM shlibs_provided s WHERE s.name = r.name) "
		"ORDER BY r.pkg, r.name") != MPORT_OK) {
		sqlite3_finalize(stmt);
		return -1;
	}

	while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		const char *pkg = (const char *)sqlite3_column_text(stmt, 0);
		const char *name = (const char *)sqlite3_column_text(stmt, 1);

		if (pkg == NULL || name == NULL || mport_shlib_in_base(name))
			continue;
		mport_call_msg_cb(mport,
		    "Missing shared library: %s needs %s, which no installed package provides", pkg,
		    name);
		missing++;
	}
	sqlite3_finalize(stmt);

	if (rc != SQLITE_DONE) {
		SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(mport->db));
		return -1;
	}

	return missing;
}

/*
 * Whether an installed package's provided libraries are all also provided
 * by some other package registered for the given release.  Used to judge a
 * dependency left over from an older release: if every soname it exports is
 * exported again by a current build, consumers link against the current one
 * and the old copy is harmless.  Returns 1 when the package has recorded
 * libraries and each is superseded, 0 otherwise, -1 on error.  A package
 * with no recorded libraries is not superseded; it is unknown.
 */
int
mport_shlibs_superseded(mportInstance *mport, const char *pkgname, const char *os_release)
{
	int total = 0;
	int unmatched = 0;

	if (mport == NULL || pkgname == NULL || os_release == NULL)
		return -1;

	if (mport_db_count(mport->db, &total, "SELECT count(*) FROM shlibs_provided WHERE pkg=%Q",
		pkgname) != MPORT_OK)
		return -1;
	if (total == 0)
		return 0;

	if (mport_db_count(mport->db, &unmatched,
		"SELECT count(*) FROM shlibs_provided sp WHERE sp.pkg=%Q AND NOT EXISTS ("
		"SELECT 1 FROM shlibs_provided o JOIN packages p ON p.pkg = o.pkg "
		"WHERE o.name = sp.name AND o.pkg != sp.pkg AND p.status='clean' AND p.os_release=%Q)",
		pkgname, os_release) != MPORT_OK)
		return -1;

	return unmatched == 0 ? 1 : 0;
}

/*
 * Say which libraries the package file about to be installed needs that
 * no installed package provides and the base system does not ship.  A
 * warning rather than a refusal: packages registered before the scan
 * existed have no provided list, so on such a registry a missing provider
 * usually means "not recorded", not "not installed".  Returns the count.
 */
int
mport_shlibs_warn_missing(mportInstance *mport, mportPackageMeta *pkg)
{
	sqlite3_stmt *stmt = NULL;
	int missing = 0;
	int rc;

	if (mport == NULL || pkg == NULL)
		return 0;
	if (!stub_has_table(mport, "shlibs_required"))
		return 0;

	if (mport_db_prepare(mport->db, &stmt,
		"SELECT r.name FROM stub.shlibs_required r WHERE r.pkg=%Q "
		"AND NOT EXISTS (SELECT 1 FROM shlibs_provided s WHERE s.name = r.name) "
		"ORDER BY r.name",
		pkg->name) != MPORT_OK) {
		sqlite3_finalize(stmt);
		return 0;
	}

	while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		const char *name = (const char *)sqlite3_column_text(stmt, 0);

		if (name == NULL || mport_shlib_in_base(name))
			continue;
		mport_call_msg_cb(mport, "Warning: %s-%s needs %s, which no installed package provides",
		    pkg->name, pkg->version, name);
		missing++;
	}
	sqlite3_finalize(stmt);

	return missing;
}

/* whether a package file's stub database is attached to this connection */
static bool
stub_attached(mportInstance *mport)
{
	sqlite3_stmt *stmt = NULL;
	bool attached;

	attached = sqlite3_prepare_v2(mport->db, "SELECT 1 FROM stub.sqlite_master LIMIT 1", -1,
			&stmt, NULL) == SQLITE_OK;
	sqlite3_finalize(stmt);
	return attached;
}

/*
 * Fill pkg->shlibs_provided and pkg->shlibs_required if they are empty:
 * from the package file being installed when its stub is attached and
 * carries rows for the package, otherwise from the registry.  Used before
 * handing the lists to package scripts, which run both ways.
 */
int
mport_shlibs_load(mportInstance *mport, mportPackageMeta *pkg)
{
	int count = 0;

	if (mport == NULL || pkg == NULL || pkg->name == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "Invalid arguments");
	if (tll_length(pkg->shlibs_provided) > 0 || tll_length(pkg->shlibs_required) > 0)
		return MPORT_OK;

	if (stub_attached(mport) && stub_has_table(mport, "shlibs_provided") &&
	    stub_has_table(mport, "shlibs_required")) {
		if (mport_db_count(mport->db, &count,
			"SELECT count(*) FROM stub.shlibs_provided WHERE pkg=%Q "
			"UNION ALL SELECT count(*) FROM stub.shlibs_required WHERE pkg=%Q",
			pkg->name, pkg->name) == MPORT_OK &&
		    count > 0) {
			if (load_names(mport, "stub.shlibs_provided", pkg->name,
				&pkg->shlibs_provided) != MPORT_OK ||
			    load_names(mport, "stub.shlibs_required", pkg->name,
				&pkg->shlibs_required) != MPORT_OK)
				RETURN_CURRENT_ERROR;
			return MPORT_OK;
		}
	}

	return mport_shlibs_get(mport, pkg->name, &pkg->shlibs_provided, &pkg->shlibs_required);
}
