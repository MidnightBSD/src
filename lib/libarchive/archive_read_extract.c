/*-
 * Copyright (c) 2003-2007 Tim Kientzle
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "archive_platform.h"
__FBSDID("$FreeBSD: src/lib/libarchive/archive_read_extract.c,v 1.41.2.4 2007/03/06 08:03:17 kientzle Exp $");

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_ACL_H
#include <sys/acl.h>
#endif
#ifdef HAVE_ATTR_XATTR_H
#include <attr/xattr.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#ifdef HAVE_EXT2FS_EXT2_FS_H
#include <ext2fs/ext2_fs.h>	/* for Linux file flags */
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_GRP_H
#include <grp.h>
#endif
#ifdef HAVE_LINUX_EXT2_FS_H
#include <linux/ext2_fs.h>	/* for Linux file flags */
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "archive.h"
#include "archive_string.h"
#include "archive_entry.h"
#include "archive_private.h"

struct fixup_entry {
	struct fixup_entry	*next;
	mode_t			 mode;
	int64_t			 mtime;
	int64_t			 atime;
	unsigned long		 mtime_nanos;
	unsigned long		 atime_nanos;
	unsigned long		 fflags_set;
	int			 fixup; /* bitmask of what needs fixing */
	char			*name;
};

#define	FIXUP_MODE	1
#define	FIXUP_TIMES	2
#define	FIXUP_FFLAGS	4

struct bucket {
	char	*name;
	int	 hash;
	id_t	 id;
};

struct extract {
	mode_t			 umask;
	mode_t			 default_dir_mode_initial;
	mode_t			 default_dir_mode_final;
	struct archive_string	 create_parent_dir;
	struct fixup_entry	*fixup_list;
	struct fixup_entry	*current_fixup;

	struct bucket ucache[127];
	struct bucket gcache[127];

	/*
	 * Cached stat data from disk for the current entry.
	 * If this is valid, pst points to st.  Otherwise,
	 * pst is null.
	 */
	struct stat		 st;
	struct stat		*pst;
};

/* Default mode for dirs created automatically (will be modified by umask). */
#define DEFAULT_DIR_MODE 0777
/*
 * Mode to use for newly-created dirs during extraction; the correct
 * mode will be set at the end of the extraction.
 */
#define MINIMUM_DIR_MODE 0700
#define MAXIMUM_DIR_MODE 0775

static int	archive_extract_cleanup(struct archive *);
static int	create_extract(struct archive *a);
static int	extract_block_device(struct archive *,
		    struct archive_entry *, int);
static int	extract_char_device(struct archive *,
		    struct archive_entry *, int);
static int	extract_device(struct archive *,
		    struct archive_entry *, int flags, mode_t mode);
static int	extract_dir(struct archive *, struct archive_entry *, int);
static int	extract_fifo(struct archive *, struct archive_entry *, int);
static int	extract_file(struct archive *, struct archive_entry *, int);
static int	extract_hard_link(struct archive *, struct archive_entry *, int);
static int	extract_symlink(struct archive *, struct archive_entry *, int);
static unsigned int	hash(const char *);
static gid_t	lookup_gid(struct archive *, const char *uname, gid_t);
static uid_t	lookup_uid(struct archive *, const char *uname, uid_t);
static int	create_dir(struct archive *, const char *, int flags);
static int	create_dir_mutable(struct archive *, char *, int flags);
static int	create_dir_recursive(struct archive *, char *, int flags);
static int	create_parent_dir(struct archive *, const char *, int flags);
static int	create_parent_dir_mutable(struct archive *, char *, int flags);
static int	restore_metadata(struct archive *, int fd,
		    struct archive_entry *, int flags);
#ifdef HAVE_POSIX_ACL
static int	set_acl(struct archive *, int fd, struct archive_entry *,
		    acl_type_t, int archive_entry_acl_type, const char *tn);
#endif
static int	set_acls(struct archive *, int fd, struct archive_entry *);
static int	set_xattrs(struct archive *, int fd, struct archive_entry *);
static int	set_fflags(struct archive *, int fd, const char *name, mode_t,
		    unsigned long fflags_set, unsigned long fflags_clear);
static int	set_ownership(struct archive *, int fd, struct archive_entry *,
		    int flags);
static int	set_perm(struct archive *, int fd, struct archive_entry *,
		    int mode, int flags);
static int	set_time(struct archive *, int fd, struct archive_entry *, int);
static struct fixup_entry *sort_dir_list(struct fixup_entry *p);


/*
 * Extract this entry to disk.
 *
 * TODO: Validate hardlinks.  According to the standards, we're
 * supposed to check each extracted hardlink and squawk if it refers
 * to a file that we didn't restore.  I'm not entirely convinced this
 * is a good idea, but more importantly: Is there any way to validate
 * hardlinks without keeping a complete list of filenames from the
 * entire archive?? Ugh.
 *
 */
int
archive_read_extract(struct archive *a, struct archive_entry *entry, int flags)
{
	mode_t mode;
	struct extract *extract;
	int ret;
	int restore_pwd;
	char *original_filename;

	if (a->extract == NULL) {
		ret = create_extract(a);
		if (ret)
			return (ret);
	}
	extract = a->extract;
	extract->pst = NULL;
	extract->current_fixup = NULL;
	restore_pwd = -1;
	original_filename = NULL;

	/* The following is not possible without fchdir.  <sigh> */
#ifdef HAVE_FCHDIR
	/*
	 * If pathname is longer than PATH_MAX, record starting directory
	 * and chdir to a suitable intermediate dir.
	 */
	if (strlen(archive_entry_pathname(entry)) > PATH_MAX) {
		char *intdir, *tail;

		restore_pwd = open(".", O_RDONLY);
		if (restore_pwd < 0) {
				archive_set_error(a, errno,
				    "Unable to restore long pathname");
				return (ARCHIVE_WARN);
		}

		/*
		 * Yes, the copy here is necessary because we edit
		 * the pathname in-place to create intermediate dirnames.
		 */
		original_filename = strdup(archive_entry_pathname(entry));

		/*
		 * "intdir" points to the initial dir section we're going
		 * to remove, "tail" points to the remainder of the path.
		 */
		intdir = tail = original_filename;
		while (strlen(tail) > PATH_MAX) {
			intdir = tail;

			/* Locate a dir prefix shorter than PATH_MAX. */
			tail = intdir + PATH_MAX - 8;
			while (tail > intdir && *tail != '/')
				tail--;
			if (tail <= intdir) {
				archive_set_error(a, EPERM,
				    "Path element too long");
				ret = ARCHIVE_WARN;
				goto cleanup;
			}

			/* Create intdir and chdir to it. */
			*tail = '\0'; /* Terminate dir portion */
			ret = create_dir(a, intdir, flags);
			if (ret == ARCHIVE_OK && chdir(intdir) != 0) {
				archive_set_error(a, errno, "Couldn't chdir");
				ret = ARCHIVE_WARN;
			}
			*tail = '/'; /* Restore the / we removed. */
			if (ret != ARCHIVE_OK)
				goto cleanup;
			tail++;
		}
		archive_entry_set_pathname(entry, tail);
	}
#endif

	if (stat(archive_entry_pathname(entry), &extract->st) == 0)
		extract->pst = &extract->st;
	extract->umask = umask(0); /* Set the umask to zero, record old one. */

	if (extract->pst != NULL &&
	    extract->pst->st_dev == a->skip_file_dev &&
	    extract->pst->st_ino == a->skip_file_ino) {
		archive_set_error(a, 0, "Refusing to overwrite archive");
		ret = ARCHIVE_WARN;
	} else if (archive_entry_hardlink(entry) != NULL)
		ret = extract_hard_link(a, entry, flags);
	else {
		mode = archive_entry_mode(entry);
		switch (mode & S_IFMT) {
		default:
			/* Fall through, as required by POSIX. */
		case S_IFREG:
			ret = extract_file(a, entry, flags);
			break;
		case S_IFLNK:	/* Symlink */
			ret = extract_symlink(a, entry, flags);
			break;
		case S_IFCHR:
			ret = extract_char_device(a, entry, flags);
			break;
		case S_IFBLK:
			ret = extract_block_device(a, entry, flags);
			break;
		case S_IFDIR:
			ret = extract_dir(a, entry, flags);
			break;
		case S_IFIFO:
			ret = extract_fifo(a, entry, flags);
			break;
		}
	}

	umask(extract->umask); /* Restore umask. */

cleanup:
#ifdef HAVE_FCHDIR
	/* If we changed directory above, restore it here. */
	if (restore_pwd >= 0 && original_filename != NULL) {
		fchdir(restore_pwd);
		close(restore_pwd);
		archive_entry_copy_pathname(entry, original_filename);
		free(original_filename);
	}
#endif

	return (ret);
}


static int
create_extract(struct archive *a)
{
	struct extract *extract;

	extract = (struct extract *)malloc(sizeof(*extract));
	if (extract == NULL) {
		archive_set_error(a, ENOMEM, "Can't extract");
		return (ARCHIVE_FATAL);
	}
	a->cleanup_archive_extract = archive_extract_cleanup;
	memset(extract, 0, sizeof(*extract));
	umask(extract->umask = umask(0)); /* Read the current umask. */
	/* Final permission for default dirs. */
	extract->default_dir_mode_final
	    = DEFAULT_DIR_MODE & ~extract->umask;
	/* Temporary permission for default dirs during extract. */
	extract->default_dir_mode_initial
	    = extract->default_dir_mode_final;
	extract->default_dir_mode_initial |= MINIMUM_DIR_MODE;
	extract->default_dir_mode_initial &= MAXIMUM_DIR_MODE;
	/* If the two permissions above are different, then
	 * the "final" permissions will be applied in the
	 * post-extract fixup pass. */
	a->extract = extract;
	return (ARCHIVE_OK);
}

/*
 * Cleanup function for archive_extract.  Mostly, this involves processing
 * the fixup list, which is used to address a number of problems:
 *   * Dir permissions might prevent us from restoring a file in that
 *     dir, so we restore the dir 0700 first, then correct the
 *     mode at the end.
 *   * Similarly, the act of restoring a file touches the directory
 *     and changes the timestamp on the dir, so we have to touch-up dir
 *     timestamps at the end as well.
 *   * Some file flags can interfere with the restore by, for example,
 *     preventing the creation of hardlinks to those files.
 *
 * Note that tar/cpio do not require that archives be in a particular
 * order; there is no way to know when the last file has been restored
 * within a directory, so there's no way to optimize the memory usage
 * here by fixing up the directory any earlier than the
 * end-of-archive.
 *
 * XXX TODO: Directory ACLs should be restored here, for the same
 * reason we set directory perms here. XXX
 *
 * Registering this function (rather than calling it explicitly by
 * name from archive_read_finish) reduces static link pollution, since
 * applications that don't use this API won't get this file linked in.
 */
static int
archive_extract_cleanup(struct archive *a)
{
	struct fixup_entry *next, *p;
	struct extract *extract;

	/* Sort dir list so directories are fixed up in depth-first order. */
	extract = a->extract;
	p = sort_dir_list(extract->fixup_list);

	while (p != NULL) {
		extract->pst = NULL; /* Mark stat cache as out-of-date. */
		if (p->fixup & FIXUP_TIMES) {
			struct timeval times[2];
			times[1].tv_sec = p->mtime;
			times[1].tv_usec = p->mtime_nanos / 1000;
			times[0].tv_sec = p->atime;
			times[0].tv_usec = p->atime_nanos / 1000;
			utimes(p->name, times);
		}
		if (p->fixup & FIXUP_MODE)
			chmod(p->name, p->mode);

		if (p->fixup & FIXUP_FFLAGS)
			set_fflags(a, -1, p->name, p->mode, p->fflags_set, 0);

		next = p->next;
		free(p->name);
		free(p);
		p = next;
	}
	extract->fixup_list = NULL;
	archive_string_free(&extract->create_parent_dir);
	free(a->extract);
	a->extract = NULL;
	return (ARCHIVE_OK);
}

/*
 * Simple O(n log n) merge sort to order the fixup list.  In
 * particular, we want to restore dir timestamps depth-first.
 */
static struct fixup_entry *
sort_dir_list(struct fixup_entry *p)
{
	struct fixup_entry *a, *b, *t;

	if (p == NULL)
		return (NULL);
	/* A one-item list is already sorted. */
	if (p->next == NULL)
		return (p);

	/* Step 1: split the list. */
	t = p;
	a = p->next->next;
	while (a != NULL) {
		/* Step a twice, t once. */
		a = a->next;
		if (a != NULL)
			a = a->next;
		t = t->next;
	}
	/* Now, t is at the mid-point, so break the list here. */
	b = t->next;
	t->next = NULL;
	a = p;

	/* Step 2: Recursively sort the two sub-lists. */
	a = sort_dir_list(a);
	b = sort_dir_list(b);

	/* Step 3: Merge the returned lists. */
	/* Pick the first element for the merged list. */
	if (strcmp(a->name, b->name) > 0) {
		t = p = a;
		a = a->next;
	} else {
		t = p = b;
		b = b->next;
	}

	/* Always put the later element on the list first. */
	while (a != NULL && b != NULL) {
		if (strcmp(a->name, b->name) > 0) {
			t->next = a;
			a = a->next;
		} else {
			t->next = b;
			b = b->next;
		}
		t = t->next;
	}

	/* Only one list is non-empty, so just splice it on. */
	if (a != NULL)
		t->next = a;
	if (b != NULL)
		t->next = b;

	return (p);
}

/*
 * Returns a new, initialized fixup entry.
 *
 * TODO: Reduce the memory requirements for this list by using a tree
 * structure rather than a simple list of names.
 */
static struct fixup_entry *
new_fixup(struct archive *a, const char *pathname)
{
	struct extract *extract;
	struct fixup_entry *fe;

	extract = a->extract;
	fe = (struct fixup_entry *)malloc(sizeof(struct fixup_entry));
	if (fe == NULL)
		return (NULL);
	fe->next = extract->fixup_list;
	extract->fixup_list = fe;
	fe->fixup = 0;
	fe->name = strdup(pathname);
	return (fe);
}

/*
 * Returns a fixup structure for the current entry.
 */
static struct fixup_entry *
current_fixup(struct archive *a, const char *pathname)
{
	struct extract *extract;

	extract = a->extract;
	if (extract->current_fixup == NULL)
		extract->current_fixup = new_fixup(a, pathname);
	return (extract->current_fixup);
}

static int
extract_file(struct archive *a, struct archive_entry *entry, int flags)
{
	struct extract *extract;
	const char *name;
	mode_t mode;
	int fd, r, r2;

	extract = a->extract;
	name = archive_entry_pathname(entry);
	mode = archive_entry_mode(entry) & 0777;
	r = ARCHIVE_OK;

	/*
	 * If we're not supposed to overwrite pre-existing files,
	 * use O_EXCL.  Otherwise, use O_TRUNC.
	 */
	if (flags & (ARCHIVE_EXTRACT_UNLINK | ARCHIVE_EXTRACT_NO_OVERWRITE))
		fd = open(name, O_WRONLY | O_CREAT | O_EXCL, mode);
	else
		fd = open(name, O_WRONLY | O_CREAT | O_TRUNC, mode);

	/* Try removing a pre-existing file. */
	if (fd < 0 && !(flags & ARCHIVE_EXTRACT_NO_OVERWRITE)) {
		unlink(name);
		fd = open(name, O_WRONLY | O_CREAT | O_EXCL, mode);
	}

	/* Might be a non-existent parent dir; try fixing that. */
	if (fd < 0) {
		create_parent_dir(a, name, flags);
		fd = open(name, O_WRONLY | O_CREAT | O_EXCL, mode);
	}
	if (fd < 0) {
		archive_set_error(a, errno, "Can't open '%s'", name);
		return (ARCHIVE_WARN);
	}
	r = archive_read_data_into_fd(a, fd);
	extract->pst = NULL; /* Cached stat data no longer valid. */
	r2 = restore_metadata(a, fd, entry, flags);
	close(fd);
	return (err_combine(r, r2));
}

static int
extract_dir(struct archive *a, struct archive_entry *entry, int flags)
{
	struct extract *extract;
	struct fixup_entry *fe;
	char *path, *p;
	mode_t restore_mode, final_mode;

	extract = a->extract;
	extract->pst = NULL; /* Invalidate cached stat data. */

	/* Copy path to mutable storage. */
	archive_strcpy(&(extract->create_parent_dir),
	    archive_entry_pathname(entry));
	path = extract->create_parent_dir.s;

	if (*path == '\0') {
		archive_set_error(a, ARCHIVE_ERRNO_MISC,
		    "Invalid empty pathname");
		return (ARCHIVE_WARN);
	}

	/* Deal with any troublesome trailing path elements. */
	/* TODO: Someday, generalize this to remove '//' or '/./' from
	 * the middle of paths.  But, it should not compress '..' from
	 * the middle of paths.  It's a feature that restoring
	 * "a/../b" creates both 'a' and 'b' directories. */
	for (;;) {
		/* Locate last element. */
		p = strrchr(path, '/');
		if (p != NULL)
			p++;
		else
			p = path;
		/* Trim trailing '/' unless that's the entire path. */
		if (p[0] == '\0' && p - 1 > path) {
			p[-1] = '\0';
			continue;
		}
		/* Trim trailing '.' unless that's the entire path. */
		if (p > path && p[0] == '.' && p[1] == '\0') {
			p[0] = '\0';
			continue;
		}
		/* Just exit on trailing '..'. */
		if (p[0] == '.' && p[1] == '.' && p[2] == '\0') {
			archive_set_error(a, ARCHIVE_ERRNO_MISC,
			    "Can't restore directory '..'");
			return (ARCHIVE_WARN);
		}
		break;
	}

	final_mode = archive_entry_mode(entry) &
	    (S_ISUID | S_ISGID | S_ISVTX | S_IRWXU | S_IRWXG | S_IRWXO);
	if ((flags & ARCHIVE_EXTRACT_PERM) == 0)
		final_mode &= ~extract->umask;
	/* Constrain the permissions in effect during the restore. */
	restore_mode = final_mode;
	restore_mode |= MINIMUM_DIR_MODE;
	restore_mode &= MAXIMUM_DIR_MODE;

	if (mkdir(path, restore_mode) == 0)
		goto success;

	if (extract->pst == NULL && stat(path, &extract->st) == 0)
		extract->pst = &extract->st;

	if (extract->pst != NULL) {
		extract->pst = &extract->st;
		/* If dir already exists, don't reset permissions. */
		if (S_ISDIR(extract->pst->st_mode))
			return (ARCHIVE_OK);
		/* It exists but isn't a dir. */
		if ((flags & ARCHIVE_EXTRACT_UNLINK))
			unlink(path);
	} else {
		/* Doesn't already exist; try building the parent path. */
		if (create_parent_dir_mutable(a, path, flags) != ARCHIVE_OK)
			return (ARCHIVE_WARN);
	}

	/* One final attempt to create the dir. */
	if (mkdir(path, restore_mode) != 0) {
		archive_set_error(a, errno, "Can't create directory");
		return (ARCHIVE_WARN);
	}

success:
	/* Add this dir to the fixup list. */
	if (final_mode != restore_mode) {
		fe = current_fixup(a, path);
		fe->fixup |= FIXUP_MODE;
		fe->mode = final_mode;
	}
	if (flags & ARCHIVE_EXTRACT_TIME) {
		fe = current_fixup(a, path);
		fe->fixup |= FIXUP_TIMES;
		fe->mtime = archive_entry_mtime(entry);
		fe->mtime_nanos = archive_entry_mtime_nsec(entry);
		fe->atime = archive_entry_atime(entry);
		fe->atime_nanos = archive_entry_atime_nsec(entry);
	}
	return (restore_metadata(a, -1, entry, flags));
}


/*
 * Create the parent of the specified path.  Copy the provided
 * path into mutable storage first.
 */
static int
create_parent_dir(struct archive *a, const char *path, int flags)
{
	int r;

	/* Copy path to mutable storage. */
	archive_strcpy(&(a->extract->create_parent_dir), path);
	r = create_parent_dir_mutable(a, a->extract->create_parent_dir.s, flags);
	return (r);
}

/*
 * Like create_parent_dir, but creates the dir actually requested, not
 * the parent.
 */
static int
create_dir(struct archive *a, const char *path, int flags)
{
	int r;
	/* Copy path to mutable storage. */
	archive_strcpy(&(a->extract->create_parent_dir), path);
	r = create_dir_mutable(a, a->extract->create_parent_dir.s, flags);
	return (r);
}

/*
 * Create the parent directory of the specified path, assuming path
 * is already in mutable storage.
 */
static int
create_parent_dir_mutable(struct archive *a, char *path, int flags)
{
	char *slash;
	int r;

	/* Remove tail element to obtain parent name. */
	slash = strrchr(path, '/');
	if (slash == NULL)
		return (ARCHIVE_OK);
	*slash = '\0';
	r = create_dir_mutable(a, path, flags);
	*slash = '/';
	return (r);
}

/*
 * Create the specified dir, assuming path is already in
 * mutable storage.
 */
static int
create_dir_mutable(struct archive *a, char *path, int flags)
{
	int r;

	r = create_dir_recursive(a, path, flags);
	return (r);
}

/*
 * Create the specified dir, recursing to create parents as necessary.
 *
 * Returns ARCHIVE_OK if the path exists when we're done here.
 * Otherwise, returns ARCHIVE_WARN.
 */
static int
create_dir_recursive(struct archive *a, char *path, int flags)
{
	struct stat st;
	struct extract *extract;
	struct fixup_entry *le;
	char *slash, *base;
	int r;

	extract = a->extract;
	r = ARCHIVE_OK;

	/* Check for special names and just skip them. */
	slash = strrchr(path, '/');
	base = strrchr(path, '/');
	if (slash == NULL)
		base = path;
	else
		base = slash + 1;

	if (base[0] == '\0' ||
	    (base[0] == '.' && base[1] == '\0') ||
	    (base[0] == '.' && base[1] == '.' && base[2] == '\0')) {
		/* Don't bother trying to create null path, '.', or '..'. */
		if (slash != NULL) {
			*slash = '\0';
			r = create_dir_recursive(a, path, flags);
			*slash = '/';
			return (r);
		}
		return (ARCHIVE_OK);
	}

	/*
	 * Yes, this should be stat() and not lstat().  Using lstat()
	 * here loses the ability to extract through symlinks.  Also note
	 * that this should not use the extract->st cache.
	 */
	if (stat(path, &st) == 0) {
		if (S_ISDIR(st.st_mode))
			return (ARCHIVE_OK);
		if ((flags & ARCHIVE_EXTRACT_NO_OVERWRITE)) {
			archive_set_error(a, EEXIST,
			    "Can't create directory '%s'", path);
			return (ARCHIVE_WARN);
		}
		if (unlink(path) != 0) {
			archive_set_error(a, errno,
			    "Can't create directory '%s': "
			    "Conflicting file cannot be removed");
			return (ARCHIVE_WARN);
		}
	} else if (errno != ENOENT && errno != ENOTDIR) {
		/* Stat failed? */
		archive_set_error(a, errno, "Can't test directory '%s'", path);
		return (ARCHIVE_WARN);
	} else if (slash != NULL) {
		*slash = '\0';
		r = create_dir_recursive(a, path, flags);
		*slash = '/';
		if (r != ARCHIVE_OK)
			return (r);
	}

	if (mkdir(path, extract->default_dir_mode_initial) == 0) {
		if (extract->default_dir_mode_initial
		    != extract->default_dir_mode_final) {
			le = new_fixup(a, path);
			le->fixup |= FIXUP_MODE;
			le->mode = extract->default_dir_mode_final;
		}
		return (ARCHIVE_OK);
	}

	/*
	 * Without the following check, a/b/../b/c/d fails at the
	 * second visit to 'b', so 'd' can't be created.  Note that we
	 * don't add it to the fixup list here, as it's already been
	 * added.
	 */
	if (stat(path, &st) == 0 && S_ISDIR(st.st_mode))
		return (ARCHIVE_OK);

	archive_set_error(a, errno, "Failed to create dir '%s'", path);
	return (ARCHIVE_WARN);
}

static int
extract_hard_link(struct archive *a, struct archive_entry *entry, int flags)
{
	struct extract *extract;
	int r;
	const char *pathname;
	const char *linkname;

	extract = a->extract;
	pathname = archive_entry_pathname(entry);
	linkname = archive_entry_hardlink(entry);

	/* Just remove any pre-existing file with this name. */
	if (!(flags & ARCHIVE_EXTRACT_NO_OVERWRITE))
		unlink(pathname);

	r = link(linkname, pathname);
	extract->pst = NULL; /* Invalidate cached stat data. */

	if (r != 0) {
		/* Might be a non-existent parent dir; try fixing that. */
		create_parent_dir(a, pathname, flags);
		r = link(linkname, pathname);
	}

	if (r != 0) {
		/* XXX Better error message here XXX */
		archive_set_error(a, errno,
		    "Can't restore hardlink to '%s'", linkname);
		return (ARCHIVE_WARN);
	}

	/* Set ownership, time, permission information. */
	r = restore_metadata(a, -1, entry, flags);
	return (r);
}

static int
extract_symlink(struct archive *a, struct archive_entry *entry, int flags)
{
	struct extract *extract;
	int r;
	const char *pathname;
	const char *linkname;

	extract = a->extract;
	pathname = archive_entry_pathname(entry);
	linkname = archive_entry_symlink(entry);

	/* Just remove any pre-existing file with this name. */
	if (!(flags & ARCHIVE_EXTRACT_NO_OVERWRITE))
		unlink(pathname);

	r = symlink(linkname, pathname);
	extract->pst = NULL; /* Invalidate cached stat data. */

	if (r != 0) {
		/* Might be a non-existent parent dir; try fixing that. */
		create_parent_dir(a, pathname, flags);
		r = symlink(linkname, pathname);
	}

	if (r != 0) {
		/* XXX Better error message here XXX */
		archive_set_error(a, errno,
		    "Can't restore symlink to '%s'", linkname);
		return (ARCHIVE_WARN);
	}

	r = restore_metadata(a, -1, entry, flags);
	return (r);
}

static int
extract_device(struct archive *a, struct archive_entry *entry,
    int flags, mode_t mode)
{
	struct extract *extract;
	int r;

	extract = a->extract;
	/* Just remove any pre-existing file with this name. */
	if (!(flags & ARCHIVE_EXTRACT_NO_OVERWRITE))
		unlink(archive_entry_pathname(entry));

	r = mknod(archive_entry_pathname(entry), mode,
	    archive_entry_rdev(entry));
	extract->pst = NULL; /* Invalidate cached stat data. */

	/* Might be a non-existent parent dir; try fixing that. */
	if (r != 0 && errno == ENOENT) {
		create_parent_dir(a, archive_entry_pathname(entry), flags);
		r = mknod(archive_entry_pathname(entry), mode,
		    archive_entry_rdev(entry));
	}

	if (r != 0) {
		archive_set_error(a, errno, "Can't restore device node");
		return (ARCHIVE_WARN);
	}

	r = restore_metadata(a, -1, entry, flags);
	return (r);
}

static int
extract_char_device(struct archive *a, struct archive_entry *entry, int flags)
{
	mode_t mode;

	mode = (archive_entry_mode(entry) & ~S_IFMT) | S_IFCHR;
	return (extract_device(a, entry, flags, mode));
}

static int
extract_block_device(struct archive *a, struct archive_entry *entry, int flags)
{
	mode_t mode;

	mode = (archive_entry_mode(entry) & ~S_IFMT) | S_IFBLK;
	return (extract_device(a, entry, flags, mode));
}

static int
extract_fifo(struct archive *a, struct archive_entry *entry, int flags)
{
	struct extract *extract;
	int r;

	extract = a->extract;
	/* Just remove any pre-existing file with this name. */
	if (!(flags & ARCHIVE_EXTRACT_NO_OVERWRITE))
		unlink(archive_entry_pathname(entry));

	r = mkfifo(archive_entry_pathname(entry),
	    archive_entry_mode(entry));
	extract->pst = NULL; /* Invalidate cached stat data. */

	/* Might be a non-existent parent dir; try fixing that. */
	if (r != 0 && errno == ENOENT) {
		create_parent_dir(a, archive_entry_pathname(entry), flags);
		r = mkfifo(archive_entry_pathname(entry),
		    archive_entry_mode(entry));
	}

	if (r != 0) {
		archive_set_error(a, errno, "Can't restore fifo");
		return (ARCHIVE_WARN);
	}

	r = restore_metadata(a, -1, entry, flags);
	return (r);
}

static int
restore_metadata(struct archive *a, int fd, struct archive_entry *entry, int flags)
{
	int r, r2;

	r = set_ownership(a, fd, entry, flags);
	r2 = set_time(a, fd, entry, flags);
	r = err_combine(r, r2);
	r2 = set_perm(a, fd, entry, archive_entry_mode(entry), flags);
	return (err_combine(r, r2));
}

static int
set_ownership(struct archive *a, int fd,
    struct archive_entry *entry, int flags)
{
	uid_t uid;
	gid_t gid;

	/* Not changed. */
	if ((flags & ARCHIVE_EXTRACT_OWNER) == 0)
		return (ARCHIVE_OK);

	uid = lookup_uid(a, archive_entry_uname(entry),
	    archive_entry_uid(entry));
	gid = lookup_gid(a, archive_entry_gname(entry),
	    archive_entry_gid(entry));

	/* If we know we can't change it, don't bother trying. */
	if (a->user_uid != 0  &&  a->user_uid != uid)
		return (ARCHIVE_OK);

#ifdef HAVE_FCHOWN
	if (fd >= 0 && fchown(fd, uid, gid) == 0)
		return (ARCHIVE_OK);
#endif

#ifdef HAVE_LCHOWN
	if (lchown(archive_entry_pathname(entry), uid, gid))
#else
	if (!S_ISLNK(archive_entry_mode(entry))
	    && chown(archive_entry_pathname(entry), uid, gid) != 0)
#endif
	{
		archive_set_error(a, errno,
		    "Can't set user=%d/group=%d for %s", uid, gid,
		    archive_entry_pathname(entry));
		return (ARCHIVE_WARN);
	}
	return (ARCHIVE_OK);
}

static int
set_time(struct archive *a, int fd, struct archive_entry *entry, int flags)
{
	const struct stat *st;
	struct timeval times[2];

	(void)a; /* UNUSED */
	st = archive_entry_stat(entry);

	if ((flags & ARCHIVE_EXTRACT_TIME) == 0)
		return (ARCHIVE_OK);
	/* It's a waste of time to mess with dir timestamps here. */
	if (S_ISDIR(archive_entry_mode(entry)))
		return (ARCHIVE_OK);

	times[1].tv_sec = st->st_mtime;
	times[1].tv_usec = ARCHIVE_STAT_MTIME_NANOS(st) / 1000;

	times[0].tv_sec = st->st_atime;
	times[0].tv_usec = ARCHIVE_STAT_ATIME_NANOS(st) / 1000;

#ifdef HAVE_FUTIMES
	if (fd >= 0 && futimes(fd, times) == 0)
		return (ARCHIVE_OK);
#endif

#ifdef HAVE_LUTIMES
	if (lutimes(archive_entry_pathname(entry), times) != 0) {
#else
	if ((archive_entry_mode(entry) & S_IFMT) != S_IFLNK &&
	    utimes(archive_entry_pathname(entry), times) != 0) {
#endif
		archive_set_error(a, errno, "Can't update time for %s",
		    archive_entry_pathname(entry));
		return (ARCHIVE_WARN);
	}

	/*
	 * Note: POSIX does not provide a portable way to restore ctime.
	 * (Apart from resetting the system clock, which is distasteful.)
	 * So, any restoration of ctime will necessarily be OS-specific.
	 */

	/* XXX TODO: Can FreeBSD restore ctime? XXX */

	return (ARCHIVE_OK);
}

static int
set_perm(struct archive *a, int fd, struct archive_entry *entry,
    int mode, int flags)
{
	struct extract *extract;
	struct fixup_entry *le;
	const char *name;
	unsigned long	 set, clear;
	int		 r;
	int		 critical_flags;

	extract = a->extract;

	/* Obey umask unless ARCHIVE_EXTRACT_PERM. */
	if ((flags & ARCHIVE_EXTRACT_PERM) == 0)
		mode &= ~extract->umask; /* Enforce umask. */
	name = archive_entry_pathname(entry);

	if (mode & (S_ISUID | S_ISGID)) {
		if (extract->pst != NULL) {
			/* Already have stat() data available. */
#ifdef HAVE_FSTAT
		} else if (fd >= 0 && fstat(fd, &extract->st) == 0) {
			extract->pst = &extract->st;
#endif
		} else if (stat(name, &extract->st) == 0) {
			extract->pst = &extract->st;
		} else {
			archive_set_error(a, errno,
			    "Couldn't stat file");
			return (ARCHIVE_WARN);
		}

		/*
		 * TODO: Use the uid/gid looked up in set_ownership
		 * above rather than the uid/gid stored in the entry.
		 */
		if (extract->pst->st_uid != archive_entry_uid(entry))
			mode &= ~ S_ISUID;
		if (extract->pst->st_gid != archive_entry_gid(entry))
			mode &= ~ S_ISGID;
	}

	if (S_ISLNK(archive_entry_mode(entry))) {
#ifdef HAVE_LCHMOD
		/*
		 * If this is a symlink, use lchmod().  If the
		 * platform doesn't support lchmod(), just skip it as
		 * permissions on symlinks are actually ignored on
		 * most platforms.
		 */
		if (lchmod(name, mode) != 0) {
			archive_set_error(a, errno, "Can't set permissions");
			return (ARCHIVE_WARN);
		}
#endif
	} else if (!S_ISDIR(archive_entry_mode(entry))) {
		/*
		 * If it's not a symlink and not a dir, then use
		 * fchmod() or chmod(), depending on whether we have
		 * an fd.  Dirs get their perms set during the
		 * post-extract fixup, which is handled elsewhere.
		 */
#ifdef HAVE_FCHMOD
		if (fd >= 0) {
			if (fchmod(fd, mode) != 0) {
				archive_set_error(a, errno,
				    "Can't set permissions");
				return (ARCHIVE_WARN);
			}
		} else
#endif
			/* If this platform lacks fchmod(), then
			 * we'll just use chmod(). */
			if (chmod(name, mode) != 0) {
				archive_set_error(a, errno,
				    "Can't set permissions");
				return (ARCHIVE_WARN);
			}
	}

	if (flags & ARCHIVE_EXTRACT_ACL) {
		r = set_acls(a, fd, entry);
		if (r != ARCHIVE_OK)
			return (r);
	}

	if (flags & ARCHIVE_EXTRACT_XATTR) {
		r = set_xattrs(a, fd, entry);
		if (r != ARCHIVE_OK)
			return (r);
	}

	/*
	 * Make 'critical_flags' hold all file flags that can't be
	 * immediately restored.  For example, on BSD systems,
	 * SF_IMMUTABLE prevents hardlinks from being created, so
	 * should not be set until after any hardlinks are created.  To
	 * preserve some semblance of portability, this uses #ifdef
	 * extensively.  Ugly, but it works.
	 *
	 * Yes, Virginia, this does create a security race.  It's mitigated
	 * somewhat by the practice of creating dirs 0700 until the extract
	 * is done, but it would be nice if we could do more than that.
	 * People restoring critical file systems should be wary of
	 * other programs that might try to muck with files as they're
	 * being restored.
	 */
	/* Hopefully, the compiler will optimize this mess into a constant. */
	critical_flags = 0;
#ifdef SF_IMMUTABLE
	critical_flags |= SF_IMMUTABLE;
#endif
#ifdef UF_IMMUTABLE
	critical_flags |= UF_IMMUTABLE;
#endif
#ifdef SF_APPEND
	critical_flags |= SF_APPEND;
#endif
#ifdef UF_APPEND
	critical_flags |= UF_APPEND;
#endif
#ifdef EXT2_APPEND_FL
	critical_flags |= EXT2_APPEND_FL;
#endif
#ifdef EXT2_IMMUTABLE_FL
	critical_flags |= EXT2_IMMUTABLE_FL;
#endif

	if (flags & ARCHIVE_EXTRACT_FFLAGS) {
		archive_entry_fflags(entry, &set, &clear);

		/*
		 * The first test encourages the compiler to eliminate
		 * all of this if it's not necessary.
		 */
		if ((critical_flags != 0)  &&  (set & critical_flags)) {
			le = current_fixup(a, archive_entry_pathname(entry));
			le->fixup |= FIXUP_FFLAGS;
			le->fflags_set = set;
			/* Store the mode if it's not already there. */
			if ((le->fixup & FIXUP_MODE) == 0)
				le->mode = mode;
		} else {
			r = set_fflags(a, fd, archive_entry_pathname(entry),
			    mode, set, clear);
			if (r != ARCHIVE_OK)
				return (r);
		}
	}
	return (ARCHIVE_OK);
}


#if ( defined(HAVE_LCHFLAGS) || defined(HAVE_CHFLAGS) || defined(HAVE_FCHFLAGS) ) && !defined(__linux)
static int
set_fflags(struct archive *a, int fd, const char *name, mode_t mode,
    unsigned long set, unsigned long clear)
{
	struct extract *extract;

	extract = a->extract;
	if (set == 0  && clear == 0)
		return (ARCHIVE_OK);

	(void)mode; /* UNUSED */
	/*
	 * XXX Is the stat here really necessary?  Or can I just use
	 * the 'set' flags directly?  In particular, I'm not sure
	 * about the correct approach if we're overwriting an existing
	 * file that already has flags on it. XXX
	 */
	if (extract->pst != NULL) {
		/* Already have stat() data available. */
	} else if (fd >= 0 && fstat(fd, &extract->st) == 0)
		extract->pst = &extract->st;
	else if (stat(name, &extract->st) == 0)
		extract->pst = &extract->st;
	else {
		archive_set_error(a, errno,
		    "Couldn't stat file");
		return (ARCHIVE_WARN);
	}

	extract->st.st_flags &= ~clear;
	extract->st.st_flags |= set;
#ifdef HAVE_FCHFLAGS
	/* If platform has fchflags() and we were given an fd, use it. */
	if (fd >= 0 && fchflags(fd, extract->st.st_flags) == 0)
		return (ARCHIVE_OK);
#endif
	/*
	 * If we can't use the fd to set the flags, we'll use the
	 * pathname to set flags.  We prefer lchflags() but will use
	 * chflags() if we must.
	 */
#ifdef HAVE_LCHFLAGS
	if (lchflags(name, extract->st.st_flags) == 0)
		return (ARCHIVE_OK);
#elif defined(HAVE_CHFLAGS)
	if (chflags(name, extract->st.st_flags) == 0)
		return (ARCHIVE_OK);
#endif
	archive_set_error(a, errno,
	    "Failed to set file flags");
	return (ARCHIVE_WARN);
}

#elif defined(__linux) && defined(EXT2_IOC_GETFLAGS) && defined(EXT2_IOC_SETFLAGS)

/*
 * Linux has flags too, but uses ioctl() to access them instead of
 * having a separate chflags() system call.
 */
static int
set_fflags(struct archive *a, int fd, const char *name, mode_t mode,
    unsigned long set, unsigned long clear)
{
	struct extract *extract;
	int		 ret;
	int		 myfd = fd;
	unsigned long newflags, oldflags;
	unsigned long sf_mask = 0;

	extract = a->extract;
	if (set == 0  && clear == 0)
		return (ARCHIVE_OK);
	/* Only regular files and dirs can have flags. */
	if (!S_ISREG(mode) && !S_ISDIR(mode))
		return (ARCHIVE_OK);

	/* If we weren't given an fd, open it ourselves. */
	if (myfd < 0)
		myfd = open(name, O_RDONLY|O_NONBLOCK);
	if (myfd < 0)
		return (ARCHIVE_OK);

	/*
	 * Linux has no define for the flags that are only settable by
	 * the root user.  This code may seem a little complex, but
	 * there seem to be some Linux systems that lack these
	 * defines. (?)  The code below degrades reasonably gracefully
	 * if sf_mask is incomplete.
	 */
#ifdef EXT2_IMMUTABLE_FL
	sf_mask |= EXT2_IMMUTABLE_FL;
#endif
#ifdef EXT2_APPEND_FL
	sf_mask |= EXT2_APPEND_FL;
#endif
	/*
	 * XXX As above, this would be way simpler if we didn't have
	 * to read the current flags from disk. XXX
	 */
	ret = ARCHIVE_OK;
	/* Try setting the flags as given. */
	if (ioctl(myfd, EXT2_IOC_GETFLAGS, &oldflags) >= 0) {
		newflags = (oldflags & ~clear) | set;
		if (ioctl(myfd, EXT2_IOC_SETFLAGS, &newflags) >= 0)
			goto cleanup;
		if (errno != EPERM)
			goto fail;
	}
	/* If we couldn't set all the flags, try again with a subset. */
	if (ioctl(myfd, EXT2_IOC_GETFLAGS, &oldflags) >= 0) {
		newflags &= ~sf_mask;
		oldflags &= sf_mask;
		newflags |= oldflags;
		if (ioctl(myfd, EXT2_IOC_SETFLAGS, &newflags) >= 0)
			goto cleanup;
	}
	/* We couldn't set the flags, so report the failure. */
fail:
	archive_set_error(a, errno,
	    "Failed to set file flags");
	ret = ARCHIVE_WARN;
cleanup:
	if (fd < 0)
		close(myfd);
	return (ret);
}

#else /* Not HAVE_CHFLAGS && Not __linux */

/*
 * Of course, some systems have neither BSD chflags() nor Linux' flags
 * support through ioctl().
 */
static int
set_fflags(struct archive *a, int fd, const char *name, mode_t mode,
    unsigned long set, unsigned long clear)
{
	(void)a;
	(void)fd;
	(void)name;
	(void)mode;
	(void)set;
	(void)clear;
	return (ARCHIVE_OK);
}

#endif /* __linux */

#ifndef HAVE_POSIX_ACL
/* Default empty function body to satisfy mainline code. */
static int
set_acls(struct archive *a, int fd, struct archive_entry *entry)
{
	(void)a;
	(void)fd;
	(void)entry;

	return (ARCHIVE_OK);
}

#else

/*
 * XXX TODO: What about ACL types other than ACCESS and DEFAULT?
 */
static int
set_acls(struct archive *a, int fd, struct archive_entry *entry)
{
	int		 ret;

	ret = set_acl(a, fd, entry, ACL_TYPE_ACCESS,
	    ARCHIVE_ENTRY_ACL_TYPE_ACCESS, "access");
	if (ret != ARCHIVE_OK)
		return (ret);
	ret = set_acl(a, fd, entry, ACL_TYPE_DEFAULT,
	    ARCHIVE_ENTRY_ACL_TYPE_DEFAULT, "default");
	return (ret);
}


static int
set_acl(struct archive *a, int fd, struct archive_entry *entry,
    acl_type_t acl_type, int ae_requested_type, const char *tname)
{
	acl_t		 acl;
	acl_entry_t	 acl_entry;
	acl_permset_t	 acl_permset;
	int		 ret;
	int		 ae_type, ae_permset, ae_tag, ae_id;
	uid_t		 ae_uid;
	gid_t		 ae_gid;
	const char	*ae_name;
	int		 entries;
	const char	*name;

	ret = ARCHIVE_OK;
	entries = archive_entry_acl_reset(entry, ae_requested_type);
	if (entries == 0)
		return (ARCHIVE_OK);
	acl = acl_init(entries);
	while (archive_entry_acl_next(entry, ae_requested_type, &ae_type,
		   &ae_permset, &ae_tag, &ae_id, &ae_name) == ARCHIVE_OK) {
		acl_create_entry(&acl, &acl_entry);

		switch (ae_tag) {
		case ARCHIVE_ENTRY_ACL_USER:
			acl_set_tag_type(acl_entry, ACL_USER);
			ae_uid = lookup_uid(a, ae_name, ae_id);
			acl_set_qualifier(acl_entry, &ae_uid);
			break;
		case ARCHIVE_ENTRY_ACL_GROUP:
			acl_set_tag_type(acl_entry, ACL_GROUP);
			ae_gid = lookup_gid(a, ae_name, ae_id);
			acl_set_qualifier(acl_entry, &ae_gid);
			break;
		case ARCHIVE_ENTRY_ACL_USER_OBJ:
			acl_set_tag_type(acl_entry, ACL_USER_OBJ);
			break;
		case ARCHIVE_ENTRY_ACL_GROUP_OBJ:
			acl_set_tag_type(acl_entry, ACL_GROUP_OBJ);
			break;
		case ARCHIVE_ENTRY_ACL_MASK:
			acl_set_tag_type(acl_entry, ACL_MASK);
			break;
		case ARCHIVE_ENTRY_ACL_OTHER:
			acl_set_tag_type(acl_entry, ACL_OTHER);
			break;
		default:
			/* XXX */
			break;
		}

		acl_get_permset(acl_entry, &acl_permset);
		acl_clear_perms(acl_permset);
		if (ae_permset & ARCHIVE_ENTRY_ACL_EXECUTE)
			acl_add_perm(acl_permset, ACL_EXECUTE);
		if (ae_permset & ARCHIVE_ENTRY_ACL_WRITE)
			acl_add_perm(acl_permset, ACL_WRITE);
		if (ae_permset & ARCHIVE_ENTRY_ACL_READ)
			acl_add_perm(acl_permset, ACL_READ);
	}

	name = archive_entry_pathname(entry);

	/* Try restoring the ACL through 'fd' if we can. */
#if HAVE_ACL_SET_FD
	if (fd >= 0 && acl_type == ACL_TYPE_ACCESS && acl_set_fd(fd, acl) == 0)
		ret = ARCHIVE_OK;
	else
#else
#if HAVE_ACL_SET_FD_NP
	if (fd >= 0 && acl_set_fd_np(fd, acl, acl_type) == 0)
		ret = ARCHIVE_OK;
	else
#endif
#endif
	if (acl_set_file(name, acl_type, acl) != 0) {
		archive_set_error(a, errno, "Failed to set %s acl", tname);
		ret = ARCHIVE_WARN;
	}
	acl_free(acl);
	return (ret);
}
#endif

#if HAVE_LSETXATTR
/*
 * Restore extended attributes -  Linux implementation
 */
static int
set_xattrs(struct archive *a, int fd, struct archive_entry *entry)
{
	static int warning_done = 0;
	int ret = ARCHIVE_OK;
	int i = archive_entry_xattr_reset(entry);

	while (i--) {
		const char *name;
		const void *value;
		size_t size;
		archive_entry_xattr_next(entry, &name, &value, &size);
		if (name != NULL &&
				strncmp(name, "xfsroot.", 8) != 0 &&
				strncmp(name, "system.", 7) != 0) {
			int e;
#if HAVE_FSETXATTR
			if (fd >= 0)
				e = fsetxattr(fd, name, value, size, 0);
			else
#endif
			{
				e = lsetxattr(archive_entry_pathname(entry),
				    name, value, size, 0);
			}
			if (e == -1) {
				if (errno == ENOTSUP) {
					if (!warning_done) {
						warning_done = 1;
						archive_set_error(a, errno,
						    "Cannot restore extended "
						    "attributes on this file "
						    "system");
					}
				} else
					archive_set_error(a, errno,
					    "Failed to set extended attribute");
				ret = ARCHIVE_WARN;
			}
		} else {
			archive_set_error(a, ARCHIVE_ERRNO_FILE_FORMAT,
			    "Invalid extended attribute encountered");
			ret = ARCHIVE_WARN;
		}
	}
	return (ret);
}
#else
/*
 * Restore extended attributes - stub implementation for unsupported systems
 */
static int
set_xattrs(struct archive *a, int fd, struct archive_entry *entry)
{
	static int warning_done = 0;
	(void)a; /* UNUSED */
	(void)fd; /* UNUSED */

	/* If there aren't any extended attributes, then it's okay not
	 * to extract them, otherwise, issue a single warning. */
	if (archive_entry_xattr_count(entry) != 0 && !warning_done) {
		warning_done = 1;
		archive_set_error(a, ARCHIVE_ERRNO_FILE_FORMAT,
		    "Cannot restore extended attributes on this system");
		return (ARCHIVE_WARN);
	}
	/* Warning was already emitted; suppress further warnings. */
	return (ARCHIVE_OK);
}
#endif

/*
 * The following routines do some basic caching of uname/gname
 * lookups.  All such lookups go through these routines, including ACL
 * conversions.  Even a small cache here provides an enormous speedup,
 * especially on systems using NIS, LDAP, or a similar networked
 * directory system.
 *
 * TODO: Provide an API for clients to override these routines.
 */
static gid_t
lookup_gid(struct archive *a, const char *gname, gid_t gid)
{
	struct group	*grent;
	struct extract *extract;
	int h;
	struct bucket *b;
	int cache_size;

	extract = a->extract;
	cache_size = sizeof(extract->gcache) / sizeof(extract->gcache[0]);

	/* If no gname, just use the gid provided. */
	if (gname == NULL || *gname == '\0')
		return (gid);

	/* Try to find gname in the cache. */
	h = hash(gname);
	b = &extract->gcache[h % cache_size ];
	if (b->name != NULL && b->hash == h && strcmp(gname, b->name) == 0)
		return ((gid_t)b->id);

	/* Free the cache slot for a new entry. */
	if (b->name != NULL)
		free(b->name);
	b->name = strdup(gname);
	/* Note: If strdup fails, that's okay; we just won't cache. */
	b->hash = h;
	grent = getgrnam(gname);
	if (grent != NULL)
		gid = grent->gr_gid;
	b->id = gid;

	return (gid);
}

static uid_t
lookup_uid(struct archive *a, const char *uname, uid_t uid)
{
	struct passwd	*pwent;
	struct extract *extract;
	int h;
	struct bucket *b;
	int cache_size;

	extract = a->extract;
	cache_size = sizeof(extract->ucache) / sizeof(extract->ucache[0]);

	/* If no uname, just use the uid provided. */
	if (uname == NULL || *uname == '\0')
		return (uid);

	/* Try to find uname in the cache. */
	h = hash(uname);
	b = &extract->ucache[h % cache_size ];
	if (b->name != NULL && b->hash == h && strcmp(uname, b->name) == 0)
		return ((uid_t)b->id);

	/* Free the cache slot for a new entry. */
	if (b->name != NULL)
		free(b->name);
	b->name = strdup(uname);
	/* Note: If strdup fails, that's okay; we just won't cache. */
	b->hash = h;
	pwent = getpwnam(uname);
	if (pwent != NULL)
		uid = pwent->pw_uid;
	b->id = uid;

	return (uid);
}

static unsigned int
hash(const char *p)
{
	/* A 32-bit version of Peter Weinberger's (PJW) hash algorithm,
	   as used by ELF for hashing function names. */
	unsigned g, h = 0;
	while (*p != '\0') {
		h = ( h << 4 ) + *p++;
		if (( g = h & 0xF0000000 )) {
			h ^= g >> 24;
			h &= 0x0FFFFFFF;
		}
	}
	return h;
}

void
archive_read_extract_set_progress_callback(struct archive *a,
    void (*progress_func)(void *), void *user_data)
{
	a->extract_progress = progress_func;
	a->extract_progress_user_data = user_data;
}
