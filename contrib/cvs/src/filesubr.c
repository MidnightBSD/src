/* filesubr.c --- subroutines for dealing with files
   Jim Blandy <jimb@cyclic.com>

   This file is part of GNU CVS.

   GNU CVS is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2, or (at your option) any
   later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.  */

/* These functions were moved out of subr.c because they need different
   definitions under operating systems (like, say, Windows NT) with different
   file system semantics.  */

#include <assert.h>
#include "cvs.h"

#include "xsize.h"

static int deep_remove_dir PROTO((const char *path));

/*
 * Copies "from" to "to".
 */
void
copy_file (from, to)
    const char *from;
    const char *to;
{
    struct stat sb;
    struct utimbuf t;
    int fdin, fdout;

    if (trace)
	(void) fprintf (stderr, "%s-> copy(%s,%s)\n",
			CLIENT_SERVER_STR, from, to);
    if (noexec)
	return;

    /* If the file to be copied is a link or a device, then just create
       the new link or device appropriately. */
    if (islink (from))
    {
	char *source = xreadlink (from);
	symlink (source, to);
	free (source);
	return;
    }

    if (isdevice (from))
    {
#if defined(HAVE_MKNOD) && defined(HAVE_STRUCT_STAT_ST_RDEV)
	if (stat (from, &sb) < 0)
	    error (1, errno, "cannot stat %s", from);
	mknod (to, sb.st_mode, sb.st_rdev);
#else
	error (1, 0, "cannot copy device files on this system (%s)", from);
#endif
    }
    else
    {
	/* Not a link or a device... probably a regular file. */
	if ((fdin = open (from, O_RDONLY)) < 0)
	    error (1, errno, "cannot open %s for copying", from);
	if (fstat (fdin, &sb) < 0)
	    error (1, errno, "cannot fstat %s", from);
	if ((fdout = creat (to, (int) sb.st_mode & 07777)) < 0)
	    error (1, errno, "cannot create %s for copying", to);
	if (sb.st_size > 0)
	{
	    char buf[BUFSIZ];
	    int n;
	    
	    for (;;) 
	    {
		n = read (fdin, buf, sizeof(buf));
		if (n == -1)
		{
#ifdef EINTR
		    if (errno == EINTR)
			continue;
#endif
		    error (1, errno, "cannot read file %s for copying", from);
		}
		else if (n == 0) 
		    break;
		
		if (write(fdout, buf, n) != n) {
		    error (1, errno, "cannot write file %s for copying", to);
		}
	    }

#ifdef HAVE_FSYNC
	    if (fsync (fdout)) 
		error (1, errno, "cannot fsync file %s after copying", to);
#endif
	}

	if (close (fdin) < 0) 
	    error (0, errno, "cannot close %s", from);
	if (close (fdout) < 0)
	    error (1, errno, "cannot close %s", to);
    }

    /* preserve last access & modification times */
    memset ((char *) &t, 0, sizeof (t));
    t.actime = sb.st_atime;
    t.modtime = sb.st_mtime;
    (void) utime (to, &t);
}

/* FIXME-krp: these functions would benefit from caching the char * &
   stat buf.  */

/*
 * Returns non-zero if the argument file is a directory, or is a symbolic
 * link which points to a directory.
 */
int
isdir (file)
    const char *file;
{
    struct stat sb;

    if (stat (file, &sb) < 0)
	return (0);
    return (S_ISDIR (sb.st_mode));
}

/*
 * Returns non-zero if the argument file is a symbolic link.
 */
int
islink (file)
    const char *file;
{
#ifdef S_ISLNK
    struct stat sb;

    if (CVS_LSTAT (file, &sb) < 0)
	return (0);
    return (S_ISLNK (sb.st_mode));
#else
    return (0);
#endif
}

/*
 * Returns non-zero if the argument file is a block or
 * character special device.
 */
int
isdevice (file)
    const char *file;
{
    struct stat sb;

    if (CVS_LSTAT (file, &sb) < 0)
	return (0);
#ifdef S_ISBLK
    if (S_ISBLK (sb.st_mode))
	return 1;
#endif
#ifdef S_ISCHR
    if (S_ISCHR (sb.st_mode))
	return 1;
#endif
    return 0;
}

/*
 * Returns non-zero if the argument file exists.
 */
int
isfile (file)
    const char *file;
{
    return isaccessible(file, F_OK);
}

/*
 * Returns non-zero if the argument file is readable.
 */
int
isreadable (file)
    const char *file;
{
    return isaccessible(file, R_OK);
}

/*
 * Returns non-zero if the argument file is writable.
 */
int
iswritable (file)
    const char *file;
{
    return isaccessible(file, W_OK);
}

/*
 * Returns non-zero if the argument file is accessable according to
 * mode.  If compiled with SETXID_SUPPORT also works if cvs has setxid
 * bits set.
 */
int
isaccessible (file, mode)
    const char *file;
    const int mode;
{
#ifdef SETXID_SUPPORT
    struct stat sb;
    int umask = 0;
    int gmask = 0;
    int omask = 0;
    int uid, mask;
    
    if (stat(file, &sb) == -1)
	return 0;
    if (mode == F_OK)
	return 1;

    uid = geteuid();
    if (uid == 0)		/* superuser */
    {
	if (!(mode & X_OK) || (sb.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH)))
	    return 1;

	errno = EACCES;
	return 0;
    }
	
    if (mode & R_OK)
    {
	umask |= S_IRUSR;
	gmask |= S_IRGRP;
	omask |= S_IROTH;
    }
    if (mode & W_OK)
    {
	umask |= S_IWUSR;
	gmask |= S_IWGRP;
	omask |= S_IWOTH;
    }
    if (mode & X_OK)
    {
	umask |= S_IXUSR;
	gmask |= S_IXGRP;
	omask |= S_IXOTH;
    }

    mask = sb.st_uid == uid ? umask : sb.st_gid == getegid() ? gmask : omask;
    if ((sb.st_mode & mask) == mask)
	return 1;
    errno = EACCES;
    return 0;
#else
    return access(file, mode) == 0;
#endif
}

/*
 * Open a file and die if it fails
 */
FILE *
open_file (name, mode)
    const char *name;
    const char *mode;
{
    FILE *fp;

    if ((fp = fopen (name, mode)) == NULL)
	error (1, errno, "cannot open %s", name);
    return (fp);
}

/*
 * Make a directory and die if it fails
 */
void
make_directory (name)
    const char *name;
{
    struct stat sb;

    if (stat (name, &sb) == 0 && (!S_ISDIR (sb.st_mode)))
	    error (0, 0, "%s already exists but is not a directory", name);
    if (!noexec && mkdir (name, 0777) < 0)
	error (1, errno, "cannot make directory %s", name);
}

/*
 * Make a path to the argument directory, printing a message if something
 * goes wrong.
 */
void
make_directories (name)
    const char *name;
{
    char *cp;

    if (noexec)
	return;

    if (mkdir (name, 0777) == 0 || errno == EEXIST)
	return;
    if (! existence_error (errno))
    {
	error (0, errno, "cannot make path to %s", name);
	return;
    }
    if ((cp = strrchr (name, '/')) == NULL)
	return;
    *cp = '\0';
    make_directories (name);
    *cp++ = '/';
    if (*cp == '\0')
	return;
    (void) mkdir (name, 0777);
}

/* Create directory NAME if it does not already exist; fatal error for
   other errors.  Returns 0 if directory was created; 1 if it already
   existed.  */
int
mkdir_if_needed (name)
    const char *name;
{
    if (mkdir (name, 0777) < 0)
    {
	int save_errno = errno;
	if (save_errno != EEXIST && !isdir (name))
	    error (1, save_errno, "cannot make directory %s", name);
	return 1;
    }
    return 0;
}

/*
 * Change the mode of a file, either adding write permissions, or removing
 * all write permissions.  Either change honors the current umask setting.
 *
 * Don't do anything if PreservePermissions is set to `yes'.  This may
 * have unexpected consequences for some uses of xchmod.
 */
void
xchmod (fname, writable)
    const char *fname;
    int writable;
{
    struct stat sb;
    mode_t mode, oumask;

    if (preserve_perms)
	return;

    if (stat (fname, &sb) < 0)
    {
	if (!noexec)
	    error (0, errno, "cannot stat %s", fname);
	return;
    }
    oumask = umask (0);
    (void) umask (oumask);
    if (writable)
    {
	mode = sb.st_mode | (~oumask
			     & (((sb.st_mode & S_IRUSR) ? S_IWUSR : 0)
				| ((sb.st_mode & S_IRGRP) ? S_IWGRP : 0)
				| ((sb.st_mode & S_IROTH) ? S_IWOTH : 0)));
    }
    else
    {
	mode = sb.st_mode & ~(S_IWRITE | S_IWGRP | S_IWOTH) & ~oumask;
    }

    if (trace)
	(void) fprintf (stderr, "%s-> chmod(%s,%o)\n",
			CLIENT_SERVER_STR, fname,
			(unsigned int) mode);
    if (noexec)
	return;

    if (chmod (fname, mode) < 0)
	error (0, errno, "cannot change mode of file %s", fname);
}

/*
 * Rename a file and die if it fails
 */
void
rename_file (from, to)
    const char *from;
    const char *to;
{
    if (trace)
	(void) fprintf (stderr, "%s-> rename(%s,%s)\n",
			CLIENT_SERVER_STR, from, to);
    if (noexec)
	return;

    if (rename (from, to) < 0)
	error (1, errno, "cannot rename file %s to %s", from, to);
}

/*
 * unlink a file, if possible.
 */
int
unlink_file (f)
    const char *f;
{
    if (trace)
	(void) fprintf (stderr, "%s-> unlink_file(%s)\n",
			CLIENT_SERVER_STR, f);
    if (noexec)
	return (0);

    return (CVS_UNLINK (f));
}

/*
 * Unlink a file or dir, if possible.  If it is a directory do a deep
 * removal of all of the files in the directory.  Return -1 on error
 * (in which case errno is set).
 */
int
unlink_file_dir (f)
    const char *f;
{
    struct stat sb;

    /* This is called by the server parent process in contexts where
       it is not OK to send output (e.g. after we sent "ok" to the
       client).  */
    if (trace && !server_active)
	(void) fprintf (stderr, "-> unlink_file_dir(%s)\n", f);

    if (noexec)
	return (0);

    /* For at least some unices, if root tries to unlink() a directory,
       instead of doing something rational like returning EISDIR,
       the system will gleefully go ahead and corrupt the filesystem.
       So we first call stat() to see if it is OK to call unlink().  This
       doesn't quite work--if someone creates a directory between the
       call to stat() and the call to unlink(), we'll still corrupt
       the filesystem.  Where is the Unix Haters Handbook when you need
       it?  */
    if (stat (f, &sb) < 0)
    {
	if (existence_error (errno))
	{
	    /* The file or directory doesn't exist anyhow.  */
	    return -1;
	}
    }
    else if (S_ISDIR (sb.st_mode))
	return deep_remove_dir (f);

    return CVS_UNLINK (f);
}

/* Remove a directory and everything it contains.  Returns 0 for
 * success, -1 for failure (in which case errno is set).
 */

static int
deep_remove_dir (path)
    const char *path;
{
    DIR		  *dirp;
    struct dirent *dp;

    if (rmdir (path) != 0)
    {
	if (errno == ENOTEMPTY
	    || errno == EEXIST
	    /* Ugly workaround for ugly AIX 4.1 (and 3.2) header bug
	       (it defines ENOTEMPTY and EEXIST to 17 but actually
	       returns 87).  */
	    || (ENOTEMPTY == 17 && EEXIST == 17 && errno == 87))
	{
	    if ((dirp = CVS_OPENDIR (path)) == NULL)
		/* If unable to open the directory return
		 * an error
		 */
		return -1;

	    errno = 0;
	    while ((dp = CVS_READDIR (dirp)) != NULL)
	    {
		char *buf;

		if (strcmp (dp->d_name, ".") == 0 ||
			    strcmp (dp->d_name, "..") == 0)
		    continue;

		buf = xmalloc (strlen (path) + strlen (dp->d_name) + 5);
		sprintf (buf, "%s/%s", path, dp->d_name);

		/* See comment in unlink_file_dir explanation of why we use
		   isdir instead of just calling unlink and checking the
		   status.  */
		if (isdir(buf)) 
		{
		    if (deep_remove_dir(buf))
		    {
			CVS_CLOSEDIR(dirp);
			free (buf);
			return -1;
		    }
		}
		else
		{
		    if (CVS_UNLINK (buf) != 0)
		    {
			CVS_CLOSEDIR(dirp);
			free (buf);
			return -1;
		    }
		}
		free (buf);

		errno = 0;
	    }
	    if (errno != 0)
	    {
		int save_errno = errno;
		CVS_CLOSEDIR (dirp);
		errno = save_errno;
		return -1;
	    }
	    CVS_CLOSEDIR (dirp);
	    return rmdir (path);
	}
	else
	    return -1;
    }

    /* Was able to remove the directory return 0 */
    return 0;
}

/* Read NCHARS bytes from descriptor FD into BUF.
   Return the number of characters successfully read.
   The number returned is always NCHARS unless end-of-file or error.  */
static size_t
block_read (fd, buf, nchars)
    int fd;
    char *buf;
    size_t nchars;
{
    char *bp = buf;
    size_t nread;

    do 
    {
	nread = read (fd, bp, nchars);
	if (nread == (size_t)-1)
	{
#ifdef EINTR
	    if (errno == EINTR)
		continue;
#endif
	    return (size_t)-1;
	}

	if (nread == 0)
	    break; 

	bp += nread;
	nchars -= nread;
    } while (nchars != 0);

    return bp - buf;
} 

    
/*
 * Compare "file1" to "file2". Return non-zero if they don't compare exactly.
 * If FILE1 and FILE2 are special files, compare their salient characteristics
 * (i.e. major/minor device numbers, links, etc.
 */
int
xcmp (file1, file2)
    const char *file1;
    const char *file2;
{
    char *buf1, *buf2;
    struct stat sb1, sb2;
    int fd1, fd2;
    int ret;

    if (CVS_LSTAT (file1, &sb1) < 0)
	error (1, errno, "cannot lstat %s", file1);
    if (CVS_LSTAT (file2, &sb2) < 0)
	error (1, errno, "cannot lstat %s", file2);

    /* If FILE1 and FILE2 are not the same file type, they are unequal. */
    if ((sb1.st_mode & S_IFMT) != (sb2.st_mode & S_IFMT))
	return 1;

    /* If FILE1 and FILE2 are symlinks, they are equal if they point to
       the same thing. */
#ifdef S_ISLNK
    if (S_ISLNK (sb1.st_mode) && S_ISLNK (sb2.st_mode))
    {
	int result;
	buf1 = xreadlink (file1);
	buf2 = xreadlink (file2);
	result = (strcmp (buf1, buf2) == 0);
	free (buf1);
	free (buf2);
	return result;
    }
#endif

    /* If FILE1 and FILE2 are devices, they are equal if their device
       numbers match. */
    if (S_ISBLK (sb1.st_mode) || S_ISCHR (sb1.st_mode))
    {
#ifdef HAVE_STRUCT_STAT_ST_RDEV
	if (sb1.st_rdev == sb2.st_rdev)
	    return 0;
	else
	    return 1;
#else
	error (1, 0, "cannot compare device files on this system (%s and %s)",
	       file1, file2);
#endif
    }

    if ((fd1 = open (file1, O_RDONLY)) < 0)
	error (1, errno, "cannot open file %s for comparing", file1);
    if ((fd2 = open (file2, O_RDONLY)) < 0)
	error (1, errno, "cannot open file %s for comparing", file2);

    /* A generic file compare routine might compare st_dev & st_ino here 
       to see if the two files being compared are actually the same file.
       But that won't happen in CVS, so we won't bother. */

    if (sb1.st_size != sb2.st_size)
	ret = 1;
    else if (sb1.st_size == 0)
	ret = 0;
    else
    {
	/* FIXME: compute the optimal buffer size by computing the least
	   common multiple of the files st_blocks field */
	size_t buf_size = 8 * 1024;
	size_t read1;
	size_t read2;

	buf1 = xmalloc (buf_size);
	buf2 = xmalloc (buf_size);

	do 
	{
	    read1 = block_read (fd1, buf1, buf_size);
	    if (read1 == (size_t)-1)
		error (1, errno, "cannot read file %s for comparing", file1);

	    read2 = block_read (fd2, buf2, buf_size);
	    if (read2 == (size_t)-1)
		error (1, errno, "cannot read file %s for comparing", file2);

	    /* assert (read1 == read2); */

	    ret = memcmp(buf1, buf2, read1);
	} while (ret == 0 && read1 == buf_size);

	free (buf1);
	free (buf2);
    }
	
    (void) close (fd1);
    (void) close (fd2);
    return (ret);
}

/* Generate a unique temporary filename.  Returns a pointer to a newly
 * malloc'd string containing the name.  Returns successfully or not at
 * all.
 *
 *     THIS FUNCTION IS DEPRECATED!!!  USE cvs_temp_file INSTEAD!!!
 *
 * and yes, I know about the way the rcs commands use temp files.  I think
 * they should be converted too but I don't have time to look into it right
 * now.
 */
char *
cvs_temp_name ()
{
    char *fn;
    FILE *fp;

    fp = cvs_temp_file (&fn);
    if (fp == NULL)
	error (1, errno, "Failed to create temporary file %s",
	       fn ? fn : "(null)");
    if (fclose (fp) == EOF)
	error (0, errno, "Failed to close temporary file %s", fn);
    return fn;
}

/* Generate a unique temporary filename and return an open file stream
 * to the truncated file by that name
 *
 *  INPUTS
 *	filename	where to place the pointer to the newly allocated file
 *   			name string
 *
 *  OUTPUTS
 *	filename	dereferenced, will point to the newly allocated file
 *			name string.  This value is undefined if the function
 *			returns an error.
 *
 *  RETURNS
 *	An open file pointer to a read/write mode empty temporary file with the
 *	unique file name or NULL on failure.
 *
 *  ERRORS
 *	on error, errno will be set to some value either by CVS_FOPEN or
 *	whatever system function is called to generate the temporary file name
 */
/* There are at least four functions for generating temporary
 * filenames.  We use mkstemp (BSD 4.3) if possible, else tempnam (SVID 3),
 * else mktemp (BSD 4.3), and as last resort tmpnam (POSIX).  Reason is that
 * mkstemp, tempnam, and mktemp both allow to specify the directory in which
 * the temporary file will be created.
 *
 * And the _correct_ way to use the deprecated functions probably involves
 * opening file descriptors using O_EXCL & O_CREAT and even doing the annoying
 * NFS locking thing, but until I hear of more problems, I'm not going to
 * bother.
 */
FILE *
cvs_temp_file (filename)
    char **filename;
{
    char *fn;
    FILE *fp;

    /* FIXME - I'd like to be returning NULL here in noexec mode, but I think
     * some of the rcs & diff functions which rely on a temp file run in
     * noexec mode too.
     */

    assert (filename != NULL);

#ifdef HAVE_MKSTEMP

    {
    int fd;

    fn = xmalloc (strlen (Tmpdir) + 11);
    sprintf (fn, "%s/%s", Tmpdir, "cvsXXXXXX" );
    fd = mkstemp (fn);

    /* a NULL return will be interpreted by callers as an error and
     * errno should still be set
     */
    if (fd == -1) fp = NULL;
    else if ((fp = CVS_FDOPEN (fd, "w+")) == NULL)
    {
	/* Attempt to close and unlink the file since mkstemp returned
	 * sucessfully and we believe it's been created and opened.
	 */
 	int save_errno = errno;
	if (close (fd))
	    error (0, errno, "Failed to close temporary file %s", fn);
	if (CVS_UNLINK (fn))
	    error (0, errno, "Failed to unlink temporary file %s", fn);
	errno = save_errno;
    }

    if (fp == NULL)
    {
	free (fn);
	fn = NULL;
    }
    /* mkstemp is defined to open mode 0600 using glibc 2.0.7+ */
    /* FIXME - configure can probably tell us which version of glibc we are
     * linking to and not chmod for 2.0.7+
     */
    else chmod (fn, 0600);

    }

#elif HAVE_TEMPNAM

    /* tempnam has been deprecated due to under-specification */

    fn = tempnam (Tmpdir, "cvs");
    if (fn == NULL) fp = NULL;
    else if ((fp = CVS_FOPEN (fn, "w+")) == NULL)
    {
	free (fn);
	fn = NULL;
    }
    else chmod (fn, 0600);

    /* tempnam returns a pointer to a newly malloc'd string, so there's
     * no need for a xstrdup
     */

#elif HAVE_MKTEMP

    /* mktemp has been deprecated due to the BSD 4.3 specification specifying
     * that XXXXXX will be replaced by a PID and a letter, creating only 26
     * possibilities, a security risk, and a race condition.
     */

    {
    char *ifn;

    ifn = xmalloc (strlen (Tmpdir) + 11);
    sprintf (ifn, "%s/%s", Tmpdir, "cvsXXXXXX" );
    fn = mktemp (ifn);

    if (fn == NULL) fp = NULL;
    else fp = CVS_FOPEN (fn, "w+");

    if (fp == NULL) free (ifn);
    else chmod (fn, 0600);

    }

#else	/* use tmpnam if all else fails */

    /* tmpnam is deprecated */

    {
    char ifn[L_tmpnam + 1];

    fn = tmpnam (ifn);

    if (fn == NULL) fp = NULL;
    else if ((fp = CVS_FOPEN (ifn, "w+")) != NULL)
    {
	fn = xstrdup (ifn);
	chmod (fn, 0600);
    }

    }

#endif

    *filename = fn;
    if (fn == NULL && fp != NULL)
    {
	fclose (fp);
	fp = NULL;
    }
    return fp;
}



#ifdef HAVE_READLINK
/* char *
 * xreadlink ( const char *link )
 *
 * Like the X/OPEN and 4.4BSD readlink() function, but allocates and returns
 * its own buf.
 *
 * INPUTS
 *  link	The original path.
 *
 * RETURNS
 *  The resolution of the final symbolic link in the path.
 *
 * ERRORS
 *  This function exits with a fatal error if it fails to read the link for
 *  any reason.
 */
#define MAXSIZE (SIZE_MAX < SSIZE_MAX ? SIZE_MAX : SSIZE_MAX)

char *
xreadlink (link)
    const char *link;
{
    char *file = NULL;
    size_t buflen = 128;

    /* Get the name of the file to which `from' is linked. */
    while (1)
    {
	ssize_t r;
	size_t link_name_len;

	file = xrealloc (file, buflen);
	r = readlink (link, file, buflen);
	link_name_len = r;

	if (r < 0
#ifdef ERANGE
	    /* AIX 4 and HP-UX report ERANGE if the buffer is too small. */
	    && errno != ERANGE
#endif
	    )
	    error (1, errno, "cannot readlink %s", link);

	/* If there is space for the NUL byte, set it and return. */
	if (r >= 0 && link_name_len < buflen)
	{
	    file[link_name_len] = '\0';
	    return file;
	}

	if (buflen <= MAXSIZE / 2)
	    buflen *= 2;
	else if (buflen < MAXSIZE)
	    buflen = MAXSIZE;
	else
	    /* Our buffer cannot grow any bigger.  */
	    error (1, ENAMETOOLONG, "cannot readlink %s", link);
    }
}
#endif /* HAVE_READLINK */



/* char *
 * xresolvepath ( const char *path )
 *
 * Like xreadlink(), but resolve all links in a path.
 *
 * INPUTS
 *  path	The original path.
 *
 * RETURNS
 *  The path with any symbolic links expanded.
 *
 * ERRORS
 *  This function exits with a fatal error if it fails to read the link for
 *  any reason.
 */
char *
xresolvepath ( path )
    const char *path;
{
    char *hardpath;
    char *owd;

    assert ( isdir ( path ) );

    /* FIXME - If HAVE_READLINK is defined, we should probably walk the path
     * bit by bit calling xreadlink().
     */

    owd = xgetwd();
    if ( CVS_CHDIR ( path ) < 0)
	error ( 1, errno, "cannot chdir to %s", path );
    if ( ( hardpath = xgetwd() ) == NULL )
	error (1, errno, "cannot getwd in %s", path);
    if ( CVS_CHDIR ( owd ) < 0)
	error ( 1, errno, "cannot chdir to %s", owd );
    free (owd);
    return hardpath;
}



/* Return a pointer into PATH's last component.  */
const char *
last_component (path)
    const char *path;
{
    const char *last = strrchr (path, '/');

    assert (path);
    if (last && (last != path))
        return last + 1;
    else
        return path;
}

/* Return the home directory.  Returns a pointer to storage
   managed by this function or its callees (currently getenv).
   This function will return the same thing every time it is
   called.  Returns NULL if there is no home directory.

   Note that for a pserver server, this may return root's home
   directory.  What typically happens is that upon being started from
   inetd, before switching users, the code in cvsrc.c calls
   get_homedir which remembers root's home directory in the static
   variable.  Then the switch happens and get_homedir might return a
   directory that we don't even have read or execute permissions for
   (which is bad, when various parts of CVS try to read there).  One
   fix would be to make the value returned by get_homedir only good
   until the next call (which would free the old value).  Another fix
   would be to just always malloc our answer, and let the caller free
   it (that is best, because some day we may need to be reentrant).

   The workaround is to put -f in inetd.conf which means that
   get_homedir won't get called until after the switch in user ID.

   The whole concept of a "home directory" on the server is pretty
   iffy, although I suppose some people probably are relying on it for
   .cvsrc and such, in the cases where it works.  */
char *
get_homedir ()
{
    static char *home = NULL;
    char *env;
    struct passwd *pw;

    if (home != NULL)
	return home;

    if (!server_active && (env = getenv ("HOME")) != NULL)
	home = env;
    else if ((pw = (struct passwd *) getpwuid (getuid ()))
	     && pw->pw_dir)
	home = xstrdup (pw->pw_dir);
    else
	return 0;

    return home;
}

/* Compose a path to a file in the home directory.  This is necessary because
 * of different behavior on UNIX and VMS.  See the notes in vms/filesubr.c.
 *
 * A more clean solution would be something more along the lines of a
 * "join a directory to a filename" kind of thing which was not specific to
 * the homedir.  This should aid portability between UNIX, Mac, Windows, VMS,
 * and possibly others.  This is already handled by Perl - it might be
 * interesting to see how much of the code was written in C since Perl is under
 * the GPL and the Artistic license - we might be able to use it.
 */
char *
strcat_filename_onto_homedir (dir, file)
    const char *dir;
    const char *file;
{
    char *path = xmalloc (strlen (dir) + 1 + strlen(file) + 1);
    sprintf (path, "%s/%s", dir, file);
    return path;
}

/* See cvs.h for description.  On unix this does nothing, because the
   shell expands the wildcards.  */
void
expand_wild (argc, argv, pargc, pargv)
    int argc;
    char **argv;
    int *pargc;
    char ***pargv;
{
    int i;
    assert (argv || !argc);
    if (size_overflow_p (xtimes (argc, sizeof (char *)))) {
	*pargc = 0;
	*pargv = NULL;
	error (0, 0, "expand_wild: too many arguments");
	return;
    }
    *pargc = argc;
    *pargv = xmalloc (xtimes (argc, sizeof (char *)));
    for (i = 0; i < argc; ++i)
	(*pargv)[i] = xstrdup (argv[i]);
}



#ifdef SERVER_SUPPORT
/* Case-insensitive string compare.  I know that some systems
   have such a routine, but I'm not sure I see any reasons for
   dealing with the hair of figuring out whether they do (I haven't
   looked into whether this is a performance bottleneck; I would guess
   not).  */
int
cvs_casecmp (str1, str2)
    const char *str1;
    const char *str2;
{
    const char *p;
    const char *q;
    int pqdiff;

    p = str1;
    q = str2;
    while ((pqdiff = tolower (*p) - tolower (*q)) == 0)
    {
	if (*p == '\0')
	    return 0;
	++p;
	++q;
    }
    return pqdiff;
}
#endif /* SERVER_SUPPORT */
