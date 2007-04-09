/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS kit.  */

#include "cvs.h"
#include "getline.h"
#include "history.h"
#include "savecwd.h"

#ifndef DBLKSIZ
#define	DBLKSIZ	4096			/* since GNU ndbm doesn't define it */
#endif

static int checkout_file PROTO((char *file, char *temp));
static char *make_tempfile PROTO((void));
static void rename_rcsfile PROTO((char *temp, char *real));

#ifndef MY_NDBM
static void rename_dbmfile PROTO((char *temp));
static void write_dbmfile PROTO((char *temp));
#endif				/* !MY_NDBM */

/* Structure which describes an administrative file.  */
struct admin_file {
   /* Name of the file, within the CVSROOT directory.  */
   char *filename;

   /* This is a one line description of what the file is for.  It is not
      currently used, although one wonders whether it should be, somehow.
      If NULL, then don't process this file in mkmodules (FIXME?: a bit of
      a kludge; probably should replace this with a flags field).  */
   char *errormsg;

   /* Contents which the file should have in a new repository.  To avoid
      problems with brain-dead compilers which choke on long string constants,
      this is a pointer to an array of char * terminated by NULL--each of
      the strings is concatenated.

      If this field is NULL, the file is not created in a new
      repository, but it can be added with "cvs add" (just as if one
      had created the repository with a version of CVS which didn't
      know about the file) and the checked-out copy will be updated
      without having to add it to checkoutlist.  */
   const char * const *contents;
};

static const char *const loginfo_contents[] = {
    "# The \"loginfo\" file controls where \"cvs commit\" log information\n",
    "# is sent.  The first entry on a line is a regular expression which must match\n",
    "# the directory that the change is being made to, relative to the\n",
    "# $CVSROOT.  If a match is found, then the remainder of the line is a filter\n",
    "# program that should expect log information on its standard input.\n",
    "#\n",
    "# If the repository name does not match any of the regular expressions in this\n",
    "# file, the \"DEFAULT\" line is used, if it is specified.\n",
    "#\n",
    "# If the name ALL appears as a regular expression it is always used\n",
    "# in addition to the first matching regex or DEFAULT.\n",
    "#\n",
    "# You may specify a format string as part of the\n",
    "# filter.  The string is composed of a `%' followed\n",
    "# by a single format character, or followed by a set of format\n",
    "# characters surrounded by `{' and `}' as separators.  The format\n",
    "# characters are:\n",
    "#\n",
    "#   s = file name\n",
    "#   V = old version number (pre-checkin)\n",
    "#   v = new version number (post-checkin)\n",
    "#\n",
    "# For example:\n",
    "#DEFAULT (echo \"\"; id; echo %s; date; cat) >> $CVSROOT/CVSROOT/commitlog\n",
    "# or\n",
    "#DEFAULT (echo \"\"; id; echo %{sVv}; date; cat) >> $CVSROOT/CVSROOT/commitlog\n",
    NULL
};

static const char *const rcsinfo_contents[] = {
    "# The \"rcsinfo\" file is used to control templates with which the editor\n",
    "# is invoked on commit and import.\n",
    "#\n",
    "# The first entry on a line is a regular expression which is tested\n",
    "# against the directory that the change is being made to, relative to the\n",
    "# $CVSROOT.  For the first match that is found, then the remainder of the\n",
    "# line is the name of the file that contains the template.\n",
    "#\n",
    "# If the repository name does not match any of the regular expressions in this\n",
    "# file, the \"DEFAULT\" line is used, if it is specified.\n",
    "#\n",
    "# If the name \"ALL\" appears as a regular expression it is always used\n",
    "# in addition to the first matching regex or \"DEFAULT\".\n",
    NULL
};

static const char *const editinfo_contents[] = {
    "# The \"editinfo\" file is used to allow verification of logging\n",
    "# information.  It works best when a template (as specified in the\n",
    "# rcsinfo file) is provided for the logging procedure.  Given a\n",
    "# template with locations for, a bug-id number, a list of people who\n",
    "# reviewed the code before it can be checked in, and an external\n",
    "# process to catalog the differences that were code reviewed, the\n",
    "# following test can be applied to the code:\n",
    "#\n",
    "#   Making sure that the entered bug-id number is correct.\n",
    "#   Validating that the code that was reviewed is indeed the code being\n",
    "#       checked in (using the bug-id number or a seperate review\n",
    "#       number to identify this particular code set.).\n",
    "#\n",
    "# If any of the above test failed, then the commit would be aborted.\n",
    "#\n",
    "# Actions such as mailing a copy of the report to each reviewer are\n",
    "# better handled by an entry in the loginfo file.\n",
    "#\n",
    "# One thing that should be noted is the the ALL keyword is not\n",
    "# supported.  There can be only one entry that matches a given\n",
    "# repository.\n",
    NULL
};

static const char *const verifymsg_contents[] = {
    "# The \"verifymsg\" file is used to allow verification of logging\n",
    "# information.  It works best when a template (as specified in the\n",
    "# rcsinfo file) is provided for the logging procedure.  Given a\n",
    "# template with locations for, a bug-id number, a list of people who\n",
    "# reviewed the code before it can be checked in, and an external\n",
    "# process to catalog the differences that were code reviewed, the\n",
    "# following test can be applied to the code:\n",
    "#\n",
    "#   Making sure that the entered bug-id number is correct.\n",
    "#   Validating that the code that was reviewed is indeed the code being\n",
    "#       checked in (using the bug-id number or a seperate review\n",
    "#       number to identify this particular code set.).\n",
    "#\n",
    "# If any of the above test failed, then the commit would be aborted.\n",
    "#\n",
    "# Actions such as mailing a copy of the report to each reviewer are\n",
    "# better handled by an entry in the loginfo file.\n",
    "#\n",
    "# One thing that should be noted is the the ALL keyword is not\n",
    "# supported.  There can be only one entry that matches a given\n",
    "# repository.\n",
    NULL
};

static const char *const commitinfo_contents[] = {
    "# The \"commitinfo\" file is used to control pre-commit checks.\n",
    "# The filter on the right is invoked with the repository and a list \n",
    "# of files to check.  A non-zero exit of the filter program will \n",
    "# cause the commit to be aborted.\n",
    "#\n",
    "# The first entry on a line is a regular expression which is tested\n",
    "# against the directory that the change is being committed to, relative\n",
    "# to the $CVSROOT.  For the first match that is found, then the remainder\n",
    "# of the line is the name of the filter to run.\n",
    "#\n",
    "# If the repository name does not match any of the regular expressions in this\n",
    "# file, the \"DEFAULT\" line is used, if it is specified.\n",
    "#\n",
    "# If the name \"ALL\" appears as a regular expression it is always used\n",
    "# in addition to the first matching regex or \"DEFAULT\".\n",
    NULL
};

static const char *const taginfo_contents[] = {
    "# The \"taginfo\" file is used to control pre-tag checks.\n",
    "# The filter on the right is invoked with the following arguments:\n",
    "#\n",
    "# $1 -- tagname\n",
    "# $2 -- operation \"add\" for tag, \"mov\" for tag -F, and \"del\" for tag -d\n",
    "# $3 -- repository\n",
    "# $4->  file revision [file revision ...]\n",
    "#\n",
    "# A non-zero exit of the filter program will cause the tag to be aborted.\n",
    "#\n",
    "# The first entry on a line is a regular expression which is tested\n",
    "# against the directory that the change is being committed to, relative\n",
    "# to the $CVSROOT.  For the first match that is found, then the remainder\n",
    "# of the line is the name of the filter to run.\n",
    "#\n",
    "# If the repository name does not match any of the regular expressions in this\n",
    "# file, the \"DEFAULT\" line is used, if it is specified.\n",
    "#\n",
    "# If the name \"ALL\" appears as a regular expression it is always used\n",
    "# in addition to the first matching regex or \"DEFAULT\".\n",
    NULL
};

static const char *const checkoutlist_contents[] = {
    "# The \"checkoutlist\" file is used to support additional version controlled\n",
    "# administrative files in $CVSROOT/CVSROOT, such as template files.\n",
    "#\n",
    "# The first entry on a line is a filename which will be checked out from\n",
    "# the corresponding RCS file in the $CVSROOT/CVSROOT directory.\n",
    "# The remainder of the line is an error message to use if the file cannot\n",
    "# be checked out.\n",
    "#\n",
    "# File format:\n",
    "#\n",
    "#	[<whitespace>]<filename>[<whitespace><error message>]<end-of-line>\n",
    "#\n",
    "# comment lines begin with '#'\n",
    NULL
};

static const char *const cvswrappers_contents[] = {
    "# This file affects handling of files based on their names.\n",
    "#\n",
#if 0    /* see comments in wrap_add in wrapper.c */
    "# The -t/-f options allow one to treat directories of files\n",
    "# as a single file, or to transform a file in other ways on\n",
    "# its way in and out of CVS.\n",
    "#\n",
#endif
    "# The -m option specifies whether CVS attempts to merge files.\n",
    "#\n",
    "# The -k option specifies keyword expansion (e.g. -kb for binary).\n",
    "#\n",
    "# Format of wrapper file ($CVSROOT/CVSROOT/cvswrappers or .cvswrappers)\n",
    "#\n",
    "#  wildcard	[option value][option value]...\n",
    "#\n",
    "#  where option is one of\n",
    "#  -f		from cvs filter		value: path to filter\n",
    "#  -t		to cvs filter		value: path to filter\n",
    "#  -m		update methodology	value: MERGE or COPY\n",
    "#  -k		expansion mode		value: b, o, kkv, &c\n",
    "#\n",
    "#  and value is a single-quote delimited value.\n",
    "# For example:\n",
    "#*.gif -k 'b'\n",
    NULL
};

static const char *const notify_contents[] = {
    "# The \"notify\" file controls where notifications from watches set by\n",
    "# \"cvs watch add\" or \"cvs edit\" are sent.  The first entry on a line is\n",
    "# a regular expression which is tested against the directory that the\n",
    "# change is being made to, relative to the $CVSROOT.  If it matches,\n",
    "# then the remainder of the line is a filter program that should contain\n",
    "# one occurrence of %s for the user to notify, and information on its\n",
    "# standard input.\n",
    "#\n",
    "# \"ALL\" or \"DEFAULT\" can be used in place of the regular expression.\n",
    "#\n",
    "# For example:\n",
    "#ALL mail -s \"CVS notification\" %s\n",
    NULL
};

static const char *const modules_contents[] = {
    "# Three different line formats are valid:\n",
    "#	key	-a    aliases...\n",
    "#	key [options] directory\n",
    "#	key [options] directory files...\n",
    "#\n",
    "# Where \"options\" are composed of:\n",
    "#	-i prog		Run \"prog\" on \"cvs commit\" from top-level of module.\n",
    "#	-o prog		Run \"prog\" on \"cvs checkout\" of module.\n",
    "#	-e prog		Run \"prog\" on \"cvs export\" of module.\n",
    "#	-t prog		Run \"prog\" on \"cvs rtag\" of module.\n",
    "#	-u prog		Run \"prog\" on \"cvs update\" of module.\n",
    "#	-d dir		Place module in directory \"dir\" instead of module name.\n",
    "#	-l		Top-level directory only -- do not recurse.\n",
    "#\n",
    "# NOTE:  If you change any of the \"Run\" options above, you'll have to\n",
    "# release and re-checkout any working directories of these modules.\n",
    "#\n",
    "# And \"directory\" is a path to a directory relative to $CVSROOT.\n",
    "#\n",
    "# The \"-a\" option specifies an alias.  An alias is interpreted as if\n",
    "# everything on the right of the \"-a\" had been typed on the command line.\n",
    "#\n",
    "# You can encode a module within a module by using the special '&'\n",
    "# character to interpose another module into the current module.  This\n",
    "# can be useful for creating a module that consists of many directories\n",
    "# spread out over the entire source repository.\n",
    NULL
};

static const char *const config_contents[] = {
    "# Set this to \"no\" if pserver shouldn't check system users/passwords\n",
    "#SystemAuth=no\n",
    "\n",
    "# Put CVS lock files in this directory rather than directly in the repository.\n",
    "#LockDir=/var/lock/cvs\n",
    "\n",
#ifdef PRESERVE_PERMISSIONS_SUPPORT
    "# Set `PreservePermissions' to `yes' to save file status information\n",
    "# in the repository.\n",
    "#PreservePermissions=no\n",
    "\n",
#endif
    "# Set `TopLevelAdmin' to `yes' to create a CVS directory at the top\n",
    "# level of the new working directory when using the `cvs checkout'\n",
    "# command.\n",
    "#TopLevelAdmin=no\n",
    "\n",
    "# Set `LogHistory' to `all' or `" ALL_HISTORY_REC_TYPES "' to log all transactions to the\n",
    "# history file, or a subset as needed (ie `TMAR' logs all write operations)\n",
    "#LogHistory=" ALL_HISTORY_REC_TYPES "\n",
    "\n",
    "# Set `RereadLogAfterVerify' to `always' (the default) to allow the verifymsg\n",
    "# script to change the log message.  Set it to `stat' to force CVS to verify",
    "# that the file has changed before reading it (this can take up to an extra\n",
    "# second per directory being committed, so it is not recommended for large\n",
    "# repositories.  Set it to `never' (the previous CVS behavior) to prevent\n",
    "# verifymsg scripts from changing the log message.\n",
    "#RereadLogAfterVerify=always\n",
    NULL
};

static const struct admin_file filelist[] = {
    {CVSROOTADM_LOGINFO, 
	"no logging of 'cvs commit' messages is done without a %s file",
	&loginfo_contents[0]},
    {CVSROOTADM_RCSINFO,
	"a %s file can be used to configure 'cvs commit' templates",
	rcsinfo_contents},
    {CVSROOTADM_EDITINFO,
	"a %s file can be used to validate log messages",
	editinfo_contents},
    {CVSROOTADM_VERIFYMSG,
	"a %s file can be used to validate log messages",
	verifymsg_contents},
    {CVSROOTADM_COMMITINFO,
	"a %s file can be used to configure 'cvs commit' checking",
	commitinfo_contents},
    {CVSROOTADM_TAGINFO,
	"a %s file can be used to configure 'cvs tag' checking",
	taginfo_contents},
    {CVSROOTADM_IGNORE,
	"a %s file can be used to specify files to ignore",
	NULL},
    {CVSROOTADM_CHECKOUTLIST,
	"a %s file can specify extra CVSROOT files to auto-checkout",
	checkoutlist_contents},
    {CVSROOTADM_WRAPPER,
	"a %s file can be used to specify files to treat as wrappers",
	cvswrappers_contents},
    {CVSROOTADM_NOTIFY,
	"a %s file can be used to specify where notifications go",
	notify_contents},
    {CVSROOTADM_MODULES,
	/* modules is special-cased in mkmodules.  */
	NULL,
	modules_contents},
    {CVSROOTADM_READERS,
	"a %s file specifies read-only users",
	NULL},
    {CVSROOTADM_WRITERS,
	"a %s file specifies read/write users",
	NULL},

    /* Some have suggested listing CVSROOTADM_PASSWD here too.  This
       would mean that CVS commands which operate on the
       CVSROOTADM_PASSWD file would transmit hashed passwords over the
       net.  This might seem to be no big deal, as pserver normally
       transmits cleartext passwords, but the difference is that
       CVSROOTADM_PASSWD contains *all* passwords, not just the ones
       currently being used.  For example, it could be too easy to
       accidentally give someone readonly access to CVSROOTADM_PASSWD
       (e.g. via anonymous CVS or cvsweb), and then if there are any
       guessable passwords for read/write access (usually there will be)
       they get read/write access.

       Another worry is the implications of storing old passwords--if
       someone used a password in the past they might be using it
       elsewhere, using a similar password, etc, and so saving old
       passwords, even hashed, is probably not a good idea.  */

    {CVSROOTADM_CONFIG,
	 "a %s file configures various behaviors",
	 config_contents},
    {NULL, NULL, NULL}
};

/* Rebuild the checked out administrative files in directory DIR.  */
int
mkmodules (dir)
    char *dir;
{
    struct saved_cwd cwd;
    char *temp;
    char *cp, *last, *fname;
#ifdef MY_NDBM
    DBM *db;
#endif
    FILE *fp;
    char *line = NULL;
    size_t line_allocated = 0;
    const struct admin_file *fileptr;

    if (noexec)
	return 0;

    if (save_cwd (&cwd))
	error_exit ();

    if ( CVS_CHDIR (dir) < 0)
	error (1, errno, "cannot chdir to %s", dir);

    /*
     * First, do the work necessary to update the "modules" database.
     */
    temp = make_tempfile ();
    switch (checkout_file (CVSROOTADM_MODULES, temp))
    {

	case 0:			/* everything ok */
#ifdef MY_NDBM
	    /* open it, to generate any duplicate errors */
	    if ((db = dbm_open (temp, O_RDONLY, 0666)) != NULL)
		dbm_close (db);
#else
	    write_dbmfile (temp);
	    rename_dbmfile (temp);
#endif
	    rename_rcsfile (temp, CVSROOTADM_MODULES);
	    break;

	default:
	    error (0, 0,
		"'cvs checkout' is less functional without a %s file",
		CVSROOTADM_MODULES);
	    break;
    }					/* switch on checkout_file() */

    if (unlink_file (temp) < 0
	&& !existence_error (errno))
	error (0, errno, "cannot remove %s", temp);
    free (temp);

    /* Checkout the files that need it in CVSROOT dir */
    for (fileptr = filelist; fileptr && fileptr->filename; fileptr++) {
	if (fileptr->errormsg == NULL)
	    continue;
	temp = make_tempfile ();
	if (checkout_file (fileptr->filename, temp) == 0)
	    rename_rcsfile (temp, fileptr->filename);
#if 0
	/*
	 * If there was some problem other than the file not existing,
	 * checkout_file already printed a real error message.  If the
	 * file does not exist, it is harmless--it probably just means
	 * that the repository was created with an old version of CVS
	 * which didn't have so many files in CVSROOT.
	 */
	else if (fileptr->errormsg)
	    error (0, 0, fileptr->errormsg, fileptr->filename);
#endif
	if (unlink_file (temp) < 0
	    && !existence_error (errno))
	    error (0, errno, "cannot remove %s", temp);
	free (temp);
    }

    fp = CVS_FOPEN (CVSROOTADM_CHECKOUTLIST, "r");
    if (fp)
    {
	/*
	 * File format:
	 *  [<whitespace>]<filename>[<whitespace><error message>]<end-of-line>
	 *
	 * comment lines begin with '#'
	 */
	while (getline (&line, &line_allocated, fp) >= 0)
	{
	    /* skip lines starting with # */
	    if (line[0] == '#')
		continue;

	    if ((last = strrchr (line, '\n')) != NULL)
		*last = '\0';			/* strip the newline */

	    /* Skip leading white space. */
	    for (fname = line;
		 *fname && isspace ((unsigned char) *fname);
		 fname++)
		;

	    /* Find end of filename. */
	    for (cp = fname; *cp && !isspace ((unsigned char) *cp); cp++)
		;
	    *cp = '\0';

	    temp = make_tempfile ();
	    if (checkout_file (fname, temp) == 0)
	    {
		rename_rcsfile (temp, fname);
	    }
	    else
	    {
		/* Skip leading white space before the error message.  */
		for (cp++;
		     cp < last && *cp && isspace ((unsigned char) *cp);
		     cp++)
		    ;
		if (cp < last && *cp)
		    error (0, 0, "%s", cp);
	    }
	    if (unlink_file (temp) < 0
		&& !existence_error (errno))
		error (0, errno, "cannot remove %s", temp);
	    free (temp);
	}
	if (line)
	    free (line);
	if (ferror (fp))
	    error (0, errno, "cannot read %s", CVSROOTADM_CHECKOUTLIST);
	if (fclose (fp) < 0)
	    error (0, errno, "cannot close %s", CVSROOTADM_CHECKOUTLIST);
    }
    else
    {
	/* Error from CVS_FOPEN.  */
	if (!existence_error (errno))
	    error (0, errno, "cannot open %s", CVSROOTADM_CHECKOUTLIST);
    }

    if (restore_cwd (&cwd, NULL))
	error_exit ();
    free_cwd (&cwd);

    return (0);
}

/*
 * Yeah, I know, there are NFS race conditions here.
 */
static char *
make_tempfile ()
{
    static int seed = 0;
    int fd;
    char *temp;

    if (seed == 0)
	seed = getpid ();
    temp = xmalloc (sizeof (BAKPREFIX) + 40);
    while (1)
    {
	(void) sprintf (temp, "%s%d", BAKPREFIX, seed++);
	if ((fd = CVS_OPEN (temp, O_CREAT|O_EXCL|O_RDWR, 0666)) != -1)
	    break;
	if (errno != EEXIST)
	    error (1, errno, "cannot create temporary file %s", temp);
    }
    if (close(fd) < 0)
	error(1, errno, "cannot close temporary file %s", temp);
    return temp;
}

/* Get a file.  If the file does not exist, return 1 silently.  If
   there is an error, print a message and return 1 (FIXME: probably
   not a very clean convention).  On success, return 0.  */

static int
checkout_file (file, temp)
    char *file;
    char *temp;
{
    char *rcs;
    RCSNode *rcsnode;
    int retcode = 0;

    if (noexec)
	return 0;

    rcs = xmalloc (strlen (file) + 5);
    strcpy (rcs, file);
    strcat (rcs, RCSEXT);
    if (!isfile (rcs))
    {
	free (rcs);
	return (1);
    }
    rcsnode = RCS_parsercsfile (rcs);
    retcode = RCS_checkout (rcsnode, NULL, NULL, NULL, NULL, temp,
			    (RCSCHECKOUTPROC) NULL, (void *) NULL);
    if (retcode != 0)
    {
	/* Probably not necessary (?); RCS_checkout already printed a
	   message.  */
	error (0, 0, "failed to check out %s file",
	       file);
    }
    freercsnode (&rcsnode);
    free (rcs);
    return (retcode);
}

#ifndef MY_NDBM

static void
write_dbmfile (temp)
    char *temp;
{
    char line[DBLKSIZ], value[DBLKSIZ];
    FILE *fp;
    DBM *db;
    char *cp, *vp;
    datum key, val;
    int len, cont, err = 0;

    fp = open_file (temp, "r");
    if ((db = dbm_open (temp, O_RDWR | O_CREAT | O_TRUNC, 0666)) == NULL)
	error (1, errno, "cannot open dbm file %s for creation", temp);
    for (cont = 0; fgets (line, sizeof (line), fp) != NULL;)
    {
	if ((cp = strrchr (line, '\n')) != NULL)
	    *cp = '\0';			/* strip the newline */

	/*
	 * Add the line to the value, at the end if this is a continuation
	 * line; otherwise at the beginning, but only after any trailing
	 * backslash is removed.
	 */
	vp = value;
	if (cont)
	    vp += strlen (value);

	/*
	 * See if the line we read is a continuation line, and strip the
	 * backslash if so.
	 */
	len = strlen (line);
	if (len > 0)
	    cp = &line[len - 1];
	else
	    cp = line;
	if (*cp == '\\')
	{
	    cont = 1;
	    *cp = '\0';
	}
	else
	{
	    cont = 0;
	}
	(void) strcpy (vp, line);
	if (value[0] == '#')
	    continue;			/* comment line */
	vp = value;
	while (*vp && isspace ((unsigned char) *vp))
	    vp++;
	if (*vp == '\0')
	    continue;			/* empty line */

	/*
	 * If this was not a continuation line, add the entry to the database
	 */
	if (!cont)
	{
	    key.dptr = vp;
	    while (*vp && !isspace ((unsigned char) *vp))
		vp++;
	    key.dsize = vp - key.dptr;
	    *vp++ = '\0';		/* NULL terminate the key */
	    while (*vp && isspace ((unsigned char) *vp))
		vp++;			/* skip whitespace to value */
	    if (*vp == '\0')
	    {
		error (0, 0, "warning: NULL value for key `%s'", key.dptr);
		continue;
	    }
	    val.dptr = vp;
	    val.dsize = strlen (vp);
	    if (dbm_store (db, key, val, DBM_INSERT) == 1)
	    {
		error (0, 0, "duplicate key found for `%s'", key.dptr);
		err++;
	    }
	}
    }
    dbm_close (db);
    if (fclose (fp) < 0)
	error (0, errno, "cannot close %s", temp);
    if (err)
    {
	/* I think that the size of the buffer needed here is
	   just determined by sizeof (CVSROOTADM_MODULES), the
	   filenames created by make_tempfile, and other things that won't
	   overflow.  */
	char dotdir[50], dotpag[50], dotdb[50];

	(void) sprintf (dotdir, "%s.dir", temp);
	(void) sprintf (dotpag, "%s.pag", temp);
	(void) sprintf (dotdb, "%s.db", temp);
	if (unlink_file (dotdir) < 0
	    && !existence_error (errno))
	    error (0, errno, "cannot remove %s", dotdir);
	if (unlink_file (dotpag) < 0
	    && !existence_error (errno))
	    error (0, errno, "cannot remove %s", dotpag);
	if (unlink_file (dotdb) < 0
	    && !existence_error (errno))
	    error (0, errno, "cannot remove %s", dotdb);
	error (1, 0, "DBM creation failed; correct above errors");
    }
}

static void
rename_dbmfile (temp)
    char *temp;
{
    /* I think that the size of the buffer needed here is
       just determined by sizeof (CVSROOTADM_MODULES), the
       filenames created by make_tempfile, and other things that won't
       overflow.  */
    char newdir[50], newpag[50], newdb[50];
    char dotdir[50], dotpag[50], dotdb[50];
    char bakdir[50], bakpag[50], bakdb[50];

    int dir1_errno = 0, pag1_errno = 0, db1_errno = 0;
    int dir2_errno = 0, pag2_errno = 0, db2_errno = 0;
    int dir3_errno = 0, pag3_errno = 0, db3_errno = 0;

    (void) sprintf (dotdir, "%s.dir", CVSROOTADM_MODULES);
    (void) sprintf (dotpag, "%s.pag", CVSROOTADM_MODULES);
    (void) sprintf (dotdb, "%s.db", CVSROOTADM_MODULES);
    (void) sprintf (bakdir, "%s%s.dir", BAKPREFIX, CVSROOTADM_MODULES);
    (void) sprintf (bakpag, "%s%s.pag", BAKPREFIX, CVSROOTADM_MODULES);
    (void) sprintf (bakdb, "%s%s.db", BAKPREFIX, CVSROOTADM_MODULES);
    (void) sprintf (newdir, "%s.dir", temp);
    (void) sprintf (newpag, "%s.pag", temp);
    (void) sprintf (newdb, "%s.db", temp);

    (void) chmod (newdir, 0666);
    (void) chmod (newpag, 0666);
    (void) chmod (newdb, 0666);

    /* don't mess with me */
    SIG_beginCrSect ();

    /* rm .#modules.dir .#modules.pag */
    if (unlink_file (bakdir) < 0)
	dir1_errno = errno;
    if (unlink_file (bakpag) < 0)
	pag1_errno = errno;
    if (unlink_file (bakdb) < 0)
	db1_errno = errno;

    /* mv modules.dir .#modules.dir */
    if (CVS_RENAME (dotdir, bakdir) < 0)
	dir2_errno = errno;
    /* mv modules.pag .#modules.pag */
    if (CVS_RENAME (dotpag, bakpag) < 0)
	pag2_errno = errno;
    /* mv modules.db .#modules.db */
    if (CVS_RENAME (dotdb, bakdb) < 0)
	db2_errno = errno;

    /* mv "temp".dir modules.dir */
    if (CVS_RENAME (newdir, dotdir) < 0)
	dir3_errno = errno;
    /* mv "temp".pag modules.pag */
    if (CVS_RENAME (newpag, dotpag) < 0)
	pag3_errno = errno;
    /* mv "temp".db modules.db */
    if (CVS_RENAME (newdb, dotdb) < 0)
	db3_errno = errno;

    /* OK -- make my day */
    SIG_endCrSect ();

    /* I didn't want to call error() when we had signals blocked
       (unnecessary?), but do it now.  */
    if (dir1_errno && !existence_error (dir1_errno))
	error (0, dir1_errno, "cannot remove %s", bakdir);
    if (pag1_errno && !existence_error (pag1_errno))
	error (0, pag1_errno, "cannot remove %s", bakpag);
    if (db1_errno && !existence_error (db1_errno))
	error (0, db1_errno, "cannot remove %s", bakdb);

    if (dir2_errno && !existence_error (dir2_errno))
	error (0, dir2_errno, "cannot remove %s", bakdir);
    if (pag2_errno && !existence_error (pag2_errno))
	error (0, pag2_errno, "cannot remove %s", bakpag);
    if (db2_errno && !existence_error (db2_errno))
	error (0, db2_errno, "cannot remove %s", bakdb);

    if (dir3_errno && !existence_error (dir3_errno))
	error (0, dir3_errno, "cannot remove %s", bakdir);
    if (pag3_errno && !existence_error (pag3_errno))
	error (0, pag3_errno, "cannot remove %s", bakpag);
    if (db3_errno && !existence_error (db3_errno))
	error (0, db3_errno, "cannot remove %s", bakdb);
}

#endif				/* !MY_NDBM */

static void
rename_rcsfile (temp, real)
    char *temp;
    char *real;
{
    char *bak;
    struct stat statbuf;
    char *rcs;

    /* Set "x" bits if set in original. */
    rcs = xmalloc (strlen (real) + sizeof (RCSEXT) + 10);
    (void) sprintf (rcs, "%s%s", real, RCSEXT);
    statbuf.st_mode = 0; /* in case rcs file doesn't exist, but it should... */
    if (CVS_STAT (rcs, &statbuf) < 0
	&& !existence_error (errno))
	error (0, errno, "cannot stat %s", rcs);
    free (rcs);

    if (chmod (temp, 0444 | (statbuf.st_mode & 0111)) < 0)
	error (0, errno, "warning: cannot chmod %s", temp);
    bak = xmalloc (strlen (real) + sizeof (BAKPREFIX) + 10);
    (void) sprintf (bak, "%s%s", BAKPREFIX, real);

    /* rm .#loginfo */
    if (unlink_file (bak) < 0
	&& !existence_error (errno))
	error (0, errno, "cannot remove %s", bak);

    /* mv loginfo .#loginfo */
    if (CVS_RENAME (real, bak) < 0
	&& !existence_error (errno))
	error (0, errno, "cannot rename %s to %s", real, bak);

    /* mv "temp" loginfo */
    if (CVS_RENAME (temp, real) < 0
	&& !existence_error (errno))
	error (0, errno, "cannot rename %s to %s", temp, real);

    free (bak);
}

const char *const init_usage[] = {
    "Usage: %s %s\n",
    "(Specify the --help global option for a list of other help options)\n",
    NULL
};

int
init (argc, argv)
    int argc;
    char **argv;
{
    /* Name of CVSROOT directory.  */
    char *adm;
    /* Name of this administrative file.  */
    char *info;
    /* Name of ,v file for this administrative file.  */
    char *info_v;
    /* Exit status.  */
    int err = 0;

    const struct admin_file *fileptr;

    umask (cvsumask);

    if (argc == -1 || argc > 1)
	usage (init_usage);

#ifdef CLIENT_SUPPORT
    if (current_parsed_root->isremote)
    {
	start_server ();

	ign_setup ();
	send_init_command ();
	return get_responses_and_close ();
    }
#endif /* CLIENT_SUPPORT */

    /* Note: we do *not* create parent directories as needed like the
       old cvsinit.sh script did.  Few utilities do that, and a
       non-existent parent directory is as likely to be a typo as something
       which needs to be created.  */
    mkdir_if_needed (current_parsed_root->directory);

    adm = xmalloc (strlen (current_parsed_root->directory) + sizeof (CVSROOTADM) + 2);
    sprintf (adm, "%s/%s", current_parsed_root->directory, CVSROOTADM);
    mkdir_if_needed (adm);

    /* This is needed because we pass "fileptr->filename" not "info"
       to add_rcs_file below.  I think this would be easy to change,
       thus nuking the need for CVS_CHDIR here, but I haven't looked
       closely (e.g. see wrappers calls within add_rcs_file).  */
    if ( CVS_CHDIR (adm) < 0)
	error (1, errno, "cannot change to directory %s", adm);

    /* Make Emptydir so it's there if we need it */
    mkdir_if_needed (CVSNULLREPOS);

    /* 80 is long enough for all the administrative file names, plus
       "/" and so on.  */
    info = xmalloc (strlen (adm) + 80);
    info_v = xmalloc (strlen (adm) + 80);
    for (fileptr = filelist; fileptr && fileptr->filename; ++fileptr)
    {
	if (fileptr->contents == NULL)
	    continue;
	strcpy (info, adm);
	strcat (info, "/");
	strcat (info, fileptr->filename);
	strcpy (info_v, info);
	strcat (info_v, RCSEXT);
	if (isfile (info_v))
	    /* We will check out this file in the mkmodules step.
	       Nothing else is required.  */
	    ;
	else
	{
	    int retcode;

	    if (!isfile (info))
	    {
		FILE *fp;
		const char * const *p;

		fp = open_file (info, "w");
		for (p = fileptr->contents; *p != NULL; ++p)
		    if (fputs (*p, fp) < 0)
			error (1, errno, "cannot write %s", info);
		if (fclose (fp) < 0)
		    error (1, errno, "cannot close %s", info);
	    }
	    /* The message used to say " of " and fileptr->filename after
	       "initial checkin" but I fail to see the point as we know what
	       file it is from the name.  */
	    retcode = add_rcs_file ("initial checkin", info_v,
				    fileptr->filename, "1.1", NULL,

				    /* No vendor branch.  */
				    NULL, NULL, 0, NULL,

				    NULL, 0, NULL);
	    if (retcode != 0)
		/* add_rcs_file already printed an error message.  */
		err = 1;
	}
    }

    /* Turn on history logging by default.  The user can remove the file
       to disable it.  */
    strcpy (info, adm);
    strcat (info, "/");
    strcat (info, CVSROOTADM_HISTORY);
    if (!isfile (info))
    {
	FILE *fp;

	fp = open_file (info, "w");
	if (fclose (fp) < 0)
	    error (1, errno, "cannot close %s", info);
 
        /* Make the new history file world-writeable, since every CVS
           user will need to be able to write to it.  We use chmod()
           because xchmod() is too shy. */
        chmod (info, 0666);
    }

    /* Make an empty val-tags file to prevent problems creating it later.  */
    strcpy (info, adm);
    strcat (info, "/");
    strcat (info, CVSROOTADM_VALTAGS);
    if (!isfile (info))
    {
	FILE *fp;

	fp = open_file (info, "w");
	if (fclose (fp) < 0)
	    error (1, errno, "cannot close %s", info);
 
        /* Make the new val-tags file world-writeable, since every CVS
           user will need to be able to write to it.  We use chmod()
           because xchmod() is too shy. */
        chmod (info, 0666);
    }

    free (info);
    free (info_v);

    mkmodules (adm);

    free (adm);
    return err;
}
