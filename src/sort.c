/* sort - sort lines of text (with all kinds of options).
   Copyright (C) 1988, 1991-2007 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

   Written December 1988 by Mike Haertel.
   The author may be reached (Email) at the address mike@gnu.ai.mit.edu,
   or (US mail) as Mike Haertel c/o Free Software Foundation.

   Ørn E. Hansen added NLS support in 1997.  */

#include <config.h>

#include <getopt.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include "system.h"
#include "argmatch.h"
#include "error.h"
#include "hard-locale.h"
#include "hash.h"
#include "inttostr.h"
#include "md5.h"
#include "physmem.h"
#include "posixver.h"
#include "quote.h"
#include "randread.h"
#include "stdio--.h"
#include "stdlib--.h"
#include "strnumcmp.h"
#include "xmemcoll.h"
#include "xmemxfrm.h"
#include "xstrtol.h"

#if HAVE_SYS_RESOURCE_H
# include <sys/resource.h>
#endif
#ifndef RLIMIT_DATA
struct rlimit { size_t rlim_cur; };
# define getrlimit(Resource, Rlp) (-1)
#endif

/* The official name of this program (e.g., no `g' prefix).  */
#define PROGRAM_NAME "sort"

#define AUTHORS "Mike Haertel", "Paul Eggert"

#if HAVE_LANGINFO_CODESET
# include <langinfo.h>
#endif

/* Use SA_NOCLDSTOP as a proxy for whether the sigaction machinery is
   present.  */
#ifndef SA_NOCLDSTOP
# define SA_NOCLDSTOP 0
/* No sigprocmask.  Always 'return' zero. */
# define sigprocmask(How, Set, Oset) (0)
# define sigset_t int
# if ! HAVE_SIGINTERRUPT
#  define siginterrupt(sig, flag) /* empty */
# endif
#endif

#ifndef STDC_HEADERS
double strtod ();
#endif

#define UCHAR_LIM (UCHAR_MAX + 1)

#ifndef DEFAULT_TMPDIR
# define DEFAULT_TMPDIR "/tmp"
#endif

/* Exit statuses.  */
enum
  {
    /* POSIX says to exit with status 1 if invoked with -c and the
       input is not properly sorted.  */
    SORT_OUT_OF_ORDER = 1,

    /* POSIX says any other irregular exit must exit with a status
       code greater than 1.  */
    SORT_FAILURE = 2
  };

enum
  {
    /* The number of times we should try to fork a compression process
       (we retry if the fork call fails).  We don't _need_ to compress
       temp files, this is just to reduce disk access, so this number
       can be small.  */
    MAX_FORK_TRIES_COMPRESS = 2,

    /* The number of times we should try to fork a decompression process.
       If we can't fork a decompression process, we can't sort, so this
       number should be big.  */
    MAX_FORK_TRIES_DECOMPRESS = 8
  };

/* The representation of the decimal point in the current locale.  */
static int decimal_point;

/* Thousands separator; if -1, then there isn't one.  */
static int thousands_sep;

/* Nonzero if the corresponding locales are hard.  */
static bool hard_LC_COLLATE;
#if HAVE_NL_LANGINFO
static bool hard_LC_TIME;
#endif

#define NONZERO(x) ((x) != 0)

/* The kind of blanks for '-b' to skip in various options. */
enum blanktype { bl_start, bl_end, bl_both };

/* The character marking end of line. Default to \n. */
static char eolchar = '\n';

/* Lines are held in core as counted strings. */
struct line
{
  char *text;			/* Text of the line. */
  size_t length;		/* Length including final newline. */
  char *keybeg;			/* Start of first key. */
  char *keylim;			/* Limit of first key. */
};

/* Input buffers. */
struct buffer
{
  char *buf;			/* Dynamically allocated buffer,
				   partitioned into 3 regions:
				   - input data;
				   - unused area;
				   - an array of lines, in reverse order.  */
  size_t used;			/* Number of bytes used for input data.  */
  size_t nlines;		/* Number of lines in the line array.  */
  size_t alloc;			/* Number of bytes allocated. */
  size_t left;			/* Number of bytes left from previous reads. */
  size_t line_bytes;		/* Number of bytes to reserve for each line. */
  bool eof;			/* An EOF has been read.  */
};

struct keyfield
{
  size_t sword;			/* Zero-origin 'word' to start at. */
  size_t schar;			/* Additional characters to skip. */
  size_t eword;			/* Zero-origin first word after field. */
  size_t echar;			/* Additional characters in field. */
  bool const *ignore;		/* Boolean array of characters to ignore. */
  char const *translate;	/* Translation applied to characters. */
  bool skipsblanks;		/* Skip leading blanks when finding start.  */
  bool skipeblanks;		/* Skip leading blanks when finding end.  */
  bool numeric;			/* Flag for numeric comparison.  Handle
				   strings of digits with optional decimal
				   point, but no exponential notation. */
  bool random;			/* Sort by random hash of key.  */
  bool general_numeric;		/* Flag for general, numeric comparison.
				   Handle numbers in exponential notation. */
  bool month;			/* Flag for comparison by month name. */
  bool reverse;			/* Reverse the sense of comparison. */
  struct keyfield *next;	/* Next keyfield to try. */
};

struct month
{
  char const *name;
  int val;
};

/* The name this program was run with. */
char *program_name;

/* FIXME: None of these tables work with multibyte character sets.
   Also, there are many other bugs when handling multibyte characters.
   One way to fix this is to rewrite `sort' to use wide characters
   internally, but doing this with good performance is a bit
   tricky.  */

/* Table of blanks.  */
static bool blanks[UCHAR_LIM];

/* Table of non-printing characters. */
static bool nonprinting[UCHAR_LIM];

/* Table of non-dictionary characters (not letters, digits, or blanks). */
static bool nondictionary[UCHAR_LIM];

/* Translation table folding lower case to upper.  */
static char fold_toupper[UCHAR_LIM];

#define MONTHS_PER_YEAR 12

/* Table mapping month names to integers.
   Alphabetic order allows binary search. */
static struct month monthtab[] =
{
  {"APR", 4},
  {"AUG", 8},
  {"DEC", 12},
  {"FEB", 2},
  {"JAN", 1},
  {"JUL", 7},
  {"JUN", 6},
  {"MAR", 3},
  {"MAY", 5},
  {"NOV", 11},
  {"OCT", 10},
  {"SEP", 9}
};

/* During the merge phase, the number of files to merge at once. */
#define NMERGE 16

/* Minimum size for a merge or check buffer.  */
#define MIN_MERGE_BUFFER_SIZE (2 + sizeof (struct line))

/* Minimum sort size; the code might not work with smaller sizes.  */
#define MIN_SORT_SIZE (NMERGE * MIN_MERGE_BUFFER_SIZE)

/* The number of bytes needed for a merge or check buffer, which can
   function relatively efficiently even if it holds only one line.  If
   a longer line is seen, this value is increased.  */
static size_t merge_buffer_size = MAX (MIN_MERGE_BUFFER_SIZE, 256 * 1024);

/* The approximate maximum number of bytes of main memory to use, as
   specified by the user.  Zero if the user has not specified a size.  */
static size_t sort_size;

/* The guessed size for non-regular files.  */
#define INPUT_FILE_SIZE_GUESS (1024 * 1024)

/* Array of directory names in which any temporary files are to be created. */
static char const **temp_dirs;

/* Number of temporary directory names used.  */
static size_t temp_dir_count;

/* Number of allocated slots in temp_dirs.  */
static size_t temp_dir_alloc;

/* Flag to reverse the order of all comparisons. */
static bool reverse;

/* Flag for stable sort.  This turns off the last ditch bytewise
   comparison of lines, and instead leaves lines in the same order
   they were read if all keys compare equal.  */
static bool stable;

/* If TAB has this value, blanks separate fields.  */
enum { TAB_DEFAULT = CHAR_MAX + 1 };

/* Tab character separating fields.  If TAB_DEFAULT, then fields are
   separated by the empty string between a non-blank character and a blank
   character. */
static int tab = TAB_DEFAULT;

/* Flag to remove consecutive duplicate lines from the output.
   Only the last of a sequence of equal lines will be output. */
static bool unique;

/* Nonzero if any of the input files are the standard input. */
static bool have_read_stdin;

/* List of key field comparisons to be tried.  */
static struct keyfield *keylist;

/* Program used to (de)compress temp files.  Must accept -d.  */
static char const *compress_program;

static void sortlines_temp (struct line *, size_t, struct line *);

/* Report MESSAGE for FILE, then clean up and exit.
   If FILE is null, it represents standard output.  */

static void die (char const *, char const *) ATTRIBUTE_NORETURN;
static void
die (char const *message, char const *file)
{
  error (0, errno, "%s: %s", message, file ? file : _("standard output"));
  exit (SORT_FAILURE);
}

void
usage (int status)
{
  if (status != EXIT_SUCCESS)
    fprintf (stderr, _("Try `%s --help' for more information.\n"),
	     program_name);
  else
    {
      printf (_("\
Usage: %s [OPTION]... [FILE]...\n\
"),
	      program_name);
      fputs (_("\
Write sorted concatenation of all FILE(s) to standard output.\n\
\n\
"), stdout);
      fputs (_("\
Mandatory arguments to long options are mandatory for short options too.\n\
"), stdout);
      fputs (_("\
Ordering options:\n\
\n\
"), stdout);
      fputs (_("\
  -b, --ignore-leading-blanks  ignore leading blanks\n\
  -d, --dictionary-order      consider only blanks and alphanumeric characters\n\
  -f, --ignore-case           fold lower case to upper case characters\n\
"), stdout);
      fputs (_("\
  -g, --general-numeric-sort  compare according to general numerical value\n\
  -i, --ignore-nonprinting    consider only printable characters\n\
  -M, --month-sort            compare (unknown) < `JAN' < ... < `DEC'\n\
  -n, --numeric-sort          compare according to string numerical value\n\
  -R, --random-sort           sort by random hash of keys\n\
      --random-source=FILE    get random bytes from FILE (default /dev/urandom)\n\
  -r, --reverse               reverse the result of comparisons\n\
\n\
"), stdout);
      fputs (_("\
Other options:\n\
\n\
  -c, --check, --check=diagnose-first  check for sorted input; do not sort\n\
  -C, --check=quiet, --check=silent  like -c, but do not report first bad line\n\
      --compress-program=PROG  compress temporaries with PROG;\n\
                              decompress them with PROG -d\n\
  -k, --key=POS1[,POS2]     start a key at POS1, end it at POS2 (origin 1)\n\
  -m, --merge               merge already sorted files; do not sort\n\
"), stdout);
      fputs (_("\
  -o, --output=FILE         write result to FILE instead of standard output\n\
  -s, --stable              stabilize sort by disabling last-resort comparison\n\
  -S, --buffer-size=SIZE    use SIZE for main memory buffer\n\
"), stdout);
      printf (_("\
  -t, --field-separator=SEP  use SEP instead of non-blank to blank transition\n\
  -T, --temporary-directory=DIR  use DIR for temporaries, not $TMPDIR or %s;\n\
                              multiple options specify multiple directories\n\
  -u, --unique              with -c, check for strict ordering;\n\
                              without -c, output only the first of an equal run\n\
"), DEFAULT_TMPDIR);
      fputs (_("\
  -z, --zero-terminated     end lines with 0 byte, not newline\n\
"), stdout);
      fputs (HELP_OPTION_DESCRIPTION, stdout);
      fputs (VERSION_OPTION_DESCRIPTION, stdout);
      fputs (_("\
\n\
POS is F[.C][OPTS], where F is the field number and C the character position\n\
in the field; both are origin 1.  If neither -t nor -b is in effect, characters\n\
in a field are counted from the beginning of the preceding whitespace.  OPTS is\n\
one or more single-letter ordering options, which override global ordering\n\
options for that key.  If no key is given, use the entire line as the key.\n\
\n\
SIZE may be followed by the following multiplicative suffixes:\n\
"), stdout);
      fputs (_("\
% 1% of memory, b 1, K 1024 (default), and so on for M, G, T, P, E, Z, Y.\n\
\n\
With no FILE, or when FILE is -, read standard input.\n\
\n\
*** WARNING ***\n\
The locale specified by the environment affects sort order.\n\
Set LC_ALL=C to get the traditional sort order that uses\n\
native byte values.\n\
"), stdout );
      printf (_("\nReport bugs to <%s>.\n"), PACKAGE_BUGREPORT);
    }

  exit (status);
}

/* For long options that have no equivalent short option, use a
   non-character as a pseudo short option, starting with CHAR_MAX + 1.  */
enum
{
  CHECK_OPTION = CHAR_MAX + 1,
  COMPRESS_PROGRAM_OPTION,
  RANDOM_SOURCE_OPTION
};

static char const short_options[] = "-bcCdfgik:mMno:rRsS:t:T:uy:z";

static struct option const long_options[] =
{
  {"ignore-leading-blanks", no_argument, NULL, 'b'},
  {"check", optional_argument, NULL, CHECK_OPTION},
  {"compress-program", required_argument, NULL, COMPRESS_PROGRAM_OPTION},
  {"dictionary-order", no_argument, NULL, 'd'},
  {"ignore-case", no_argument, NULL, 'f'},
  {"general-numeric-sort", no_argument, NULL, 'g'},
  {"ignore-nonprinting", no_argument, NULL, 'i'},
  {"key", required_argument, NULL, 'k'},
  {"merge", no_argument, NULL, 'm'},
  {"month-sort", no_argument, NULL, 'M'},
  {"numeric-sort", no_argument, NULL, 'n'},
  {"random-sort", no_argument, NULL, 'R'},
  {"random-source", required_argument, NULL, RANDOM_SOURCE_OPTION},
  {"output", required_argument, NULL, 'o'},
  {"reverse", no_argument, NULL, 'r'},
  {"stable", no_argument, NULL, 's'},
  {"buffer-size", required_argument, NULL, 'S'},
  {"field-separator", required_argument, NULL, 't'},
  {"temporary-directory", required_argument, NULL, 'T'},
  {"unique", no_argument, NULL, 'u'},
  {"zero-terminated", no_argument, NULL, 'z'},
  {GETOPT_HELP_OPTION_DECL},
  {GETOPT_VERSION_OPTION_DECL},
  {NULL, 0, NULL, 0},
};

static char const *const check_args[] =
{
  "quiet", "silent", "diagnose-first", NULL
};
static char const check_types[] =
{
  'C', 'C', 'c'
};
ARGMATCH_VERIFY (check_args, check_types);

/* The set of signals that are caught.  */
static sigset_t caught_signals;

/* Critical section status.  */
struct cs_status
{
  bool valid;
  sigset_t sigs;
};

/* Enter a critical section.  */
static struct cs_status
cs_enter (void)
{
  struct cs_status status;
  status.valid = (sigprocmask (SIG_BLOCK, &caught_signals, &status.sigs) == 0);
  return status;
}

/* Leave a critical section.  */
static void
cs_leave (struct cs_status status)
{
  if (status.valid)
    {
      /* Ignore failure when restoring the signal mask. */
      sigprocmask (SIG_SETMASK, &status.sigs, NULL);
    }
}

/* The list of temporary files. */
struct tempnode
{
  struct tempnode *volatile next;
  pid_t pid;     /* If compressed, the pid of compressor, else zero */
  char name[1];  /* Actual size is 1 + file name length.  */
};
static struct tempnode *volatile temphead;
static struct tempnode *volatile *temptail = &temphead;

struct sortfile
{
  char const *name;
  pid_t pid;     /* If compressed, the pid of compressor, else zero */
};

/* A table where we store compression process states.  We clean up all
   processes in a timely manner so as not to exhaust system resources,
   so we store the info on whether the process is still running, or has
   been reaped here.  */
static Hash_table *proctab;

enum { INIT_PROCTAB_SIZE = 47 };

enum procstate { ALIVE, ZOMBIE };

/* A proctab entry.  The COUNT field is there in case we fork a new
   compression process that has the same PID as an old zombie process
   that is still in the table (because the process to decompress the
   temp file it was associated with hasn't started yet).  */
struct procnode
{
  pid_t pid;
  enum procstate state;
  size_t count;
};

static size_t
proctab_hasher (const void *entry, size_t tabsize)
{
  const struct procnode *node = entry;
  return node->pid % tabsize;
}

static bool
proctab_comparator (const void *e1, const void *e2)
{
  const struct procnode *n1 = e1, *n2 = e2;
  return n1->pid == n2->pid;
}

/* The total number of forked processes (compressors and decompressors)
   that have not been reaped yet. */
static size_t nprocs;

/* The number of child processes we'll allow before we try to reap some. */
enum { MAX_PROCS_BEFORE_REAP = 2 };

/* If 0 < PID, wait for the child process with that PID to exit.
   If PID is -1, clean up a random child process which has finished and
   return the process ID of that child.  If PID is -1 and no processes
   have quit yet, return 0 without waiting.  */

static pid_t
reap (pid_t pid)
{
  int status;
  pid_t cpid = waitpid (pid, &status, pid < 0 ? WNOHANG : 0);

  if (cpid < 0)
    error (SORT_FAILURE, errno, _("waiting for %s [-d]"),
           compress_program);
  else if (0 < cpid)
    {
      if (! WIFEXITED (status) || WEXITSTATUS (status))
	error (SORT_FAILURE, 0, _("%s [-d] terminated abnormally"),
	       compress_program);
      --nprocs;
    }

  return cpid;
}

/* Add the PID of a running compression process to proctab, or update
   the entry COUNT and STATE fields if it's already there.  This also
   creates the table for us the first time it's called.  */

static void
register_proc (pid_t pid)
{
  struct procnode test, *node;

  if (! proctab)
    {
      proctab = hash_initialize (INIT_PROCTAB_SIZE, NULL,
				 proctab_hasher,
				 proctab_comparator,
				 free);
      if (! proctab)
	xalloc_die ();
    }

  test.pid = pid;
  node = hash_lookup (proctab, &test);
  if (node)
    {
      node->state = ALIVE;
      ++node->count;
    }
  else
    {
      node = xmalloc (sizeof *node);
      node->pid = pid;
      node->state = ALIVE;
      node->count = 1;
      hash_insert (proctab, node);
    }
}

/* This is called when we reap a random process.  We don't know
   whether we have reaped a compression process or a decompression
   process until we look in the table.  If there's an ALIVE entry for
   it, then we have reaped a compression process, so change the state
   to ZOMBIE.  Otherwise, it's a decompression processes, so ignore it.  */

static void
update_proc (pid_t pid)
{
  struct procnode test, *node;

  test.pid = pid;
  node = hash_lookup (proctab, &test);
  if (node)
    node->state = ZOMBIE;
}

/* This is for when we need to wait for a compression process to exit.
   If it has a ZOMBIE entry in the table then it's already dead and has
   been reaped.  Note that if there's an ALIVE entry for it, it still may
   already have died and been reaped if a second process was created with
   the same PID.  This is probably exceedingly rare, but to be on the safe
   side we will have to wait for any compression process with this PID.  */

static void
wait_proc (pid_t pid)
{
  struct procnode test, *node;

  test.pid = pid;
  node = hash_lookup (proctab, &test);
  if (node->state == ALIVE)
    reap (pid);

  node->state = ZOMBIE;
  if (! --node->count)
    {
      hash_delete (proctab, node);
      free (node);
    }
}

/* Keep reaping finished children as long as there are more to reap.
   This doesn't block waiting for any of them, it only reaps those
   that are already dead.  */

static void
reap_some (void)
{
  pid_t pid;

  while (0 < nprocs && (pid = reap (-1)))
    update_proc (pid);
}

/* Clean up any remaining temporary files.  */

static void
cleanup (void)
{
  struct tempnode const *node;

  for (node = temphead; node; node = node->next)
    unlink (node->name);
  temphead = NULL;
}

/* Cleanup actions to take when exiting.  */

static void
exit_cleanup (void)
{
  if (temphead)
    {
      /* Clean up any remaining temporary files in a critical section so
	 that a signal handler does not try to clean them too.  */
      struct cs_status cs = cs_enter ();
      cleanup ();
      cs_leave (cs);
    }

  close_stdout ();
}

/* Create a new temporary file, returning its newly allocated tempnode.
   Store into *PFD the file descriptor open for writing.  */

static struct tempnode *
create_temp_file (int *pfd)
{
  static char const slashbase[] = "/sortXXXXXX";
  static size_t temp_dir_index;
  int fd;
  int saved_errno;
  char const *temp_dir = temp_dirs[temp_dir_index];
  size_t len = strlen (temp_dir);
  struct tempnode *node =
    xmalloc (offsetof (struct tempnode, name) + len + sizeof slashbase);
  char *file = node->name;
  struct cs_status cs;

  memcpy (file, temp_dir, len);
  memcpy (file + len, slashbase, sizeof slashbase);
  node->next = NULL;
  node->pid = 0;
  if (++temp_dir_index == temp_dir_count)
    temp_dir_index = 0;

  /* Create the temporary file in a critical section, to avoid races.  */
  cs = cs_enter ();
  fd = mkstemp (file);
  if (0 <= fd)
    {
      *temptail = node;
      temptail = &node->next;
    }
  saved_errno = errno;
  cs_leave (cs);
  errno = saved_errno;

  if (fd < 0)
    die (_("cannot create temporary file"), file);

  *pfd = fd;
  return node;
}

/* Return a stream for FILE, opened with mode HOW.  A null FILE means
   standard output; HOW should be "w".  When opening for input, "-"
   means standard input.  To avoid confusion, do not return file
   descriptors STDIN_FILENO, STDOUT_FILENO, or STDERR_FILENO when
   opening an ordinary FILE.  */

static FILE *
xfopen (const char *file, const char *how)
{
  FILE *fp;

  if (!file)
    fp = stdout;
  else if (STREQ (file, "-") && *how == 'r')
    {
      have_read_stdin = true;
      fp = stdin;
    }
  else
    {
      fp = fopen (file, how);
      if (! fp)
	die (_("open failed"), file);
    }

  return fp;
}

/* Close FP, whose name is FILE, and report any errors.  */

static void
xfclose (FILE *fp, char const *file)
{
  switch (fileno (fp))
    {
    case STDIN_FILENO:
      /* Allow reading stdin from tty more than once.  */
      if (feof (fp))
	clearerr (fp);
      break;

    case STDOUT_FILENO:
      /* Don't close stdout just yet.  close_stdout does that.  */
      if (fflush (fp) != 0)
	die (_("fflush failed"), file);
      break;

    default:
      if (fclose (fp) != 0)
	die (_("close failed"), file);
      break;
    }
}

static void
dup2_or_die (int oldfd, int newfd)
{
  if (dup2 (oldfd, newfd) < 0)
    error (SORT_FAILURE, errno, _("dup2 failed"));
}

/* Fork a child process for piping to and do common cleanup.  The
   TRIES parameter tells us how many times to try to fork before
   giving up.  Return the PID of the child or -1 if fork failed.  */

static pid_t
pipe_fork (int pipefds[2], size_t tries)
{
#if HAVE_WORKING_FORK
  struct tempnode *saved_temphead;
  int saved_errno;
  unsigned int wait_retry = 1;
  pid_t pid IF_LINT (= -1);
  struct cs_status cs;

  if (pipe (pipefds) < 0)
    return -1;

  while (tries--)
    {
      /* This is so the child process won't delete our temp files
	 if it receives a signal before exec-ing.  */
      cs = cs_enter ();
      saved_temphead = temphead;
      temphead = NULL;

      pid = fork ();
      saved_errno = errno;
      if (pid)
	temphead = saved_temphead;

      cs_leave (cs);
      errno = saved_errno;

      if (0 <= pid || errno != EAGAIN)
	break;
      else
	{
	  sleep (wait_retry);
	  wait_retry *= 2;
	  reap_some ();
	}
    }

  if (pid < 0)
    {
      close (pipefds[0]);
      close (pipefds[1]);
    }
  else if (pid == 0)
    {
      close (STDIN_FILENO);
      close (STDOUT_FILENO);
    }
  else
    ++nprocs;

  return pid;

#else  /* ! HAVE_WORKING_FORK */
  return -1;
#endif
}

/* Create a temporary file and start a compression program to filter output
   to that file.  Set *PFP to the file handle and if *PPID is non-NULL,
   set it to the PID of the newly-created process.  */

static char *
create_temp (FILE **pfp, pid_t *ppid)
{
  int tempfd;
  struct tempnode *node = create_temp_file (&tempfd);
  char *name = node->name;

  if (compress_program)
    {
      int pipefds[2];

      node->pid = pipe_fork (pipefds, MAX_FORK_TRIES_COMPRESS);
      if (0 < node->pid)
	{
	  close (tempfd);
	  close (pipefds[0]);
	  tempfd = pipefds[1];

	  register_proc (node->pid);
	}
      else if (node->pid == 0)
	{
	  close (pipefds[1]);
	  dup2_or_die (tempfd, STDOUT_FILENO);
	  close (tempfd);
	  dup2_or_die (pipefds[0], STDIN_FILENO);
	  close (pipefds[0]);

	  if (execlp (compress_program, compress_program, (char *) NULL) < 0)
	    error (SORT_FAILURE, errno, _("couldn't execute %s"),
		   compress_program);
	}
      else
	node->pid = 0;
    }

  *pfp = fdopen (tempfd, "w");
  if (! *pfp)
    die (_("couldn't create temporary file"), name);

  if (ppid)
    *ppid = node->pid;

  return name;
}

/* Open a compressed temp file and start a decompression process through
   which to filter the input.  PID must be the valid processes ID of the
   process used to compress the file.  */

static FILE *
open_temp (const char *name, pid_t pid)
{
  int tempfd, pipefds[2];
  pid_t child_pid;
  FILE *fp;

  wait_proc (pid);

  tempfd = open (name, O_RDONLY);
  if (tempfd < 0)
    die (_("couldn't open temporary file"), name);

  child_pid = pipe_fork (pipefds, MAX_FORK_TRIES_DECOMPRESS);
  if (0 < child_pid)
    {
      close (tempfd);
      close (pipefds[1]);
    }
  else if (child_pid == 0)
    {
      close (pipefds[0]);
      dup2_or_die (tempfd, STDIN_FILENO);
      close (tempfd);
      dup2_or_die (pipefds[1], STDOUT_FILENO);
      close (pipefds[1]);

      if (execlp (compress_program, compress_program, "-d", (char *) NULL) < 0)
	error (SORT_FAILURE, errno, _("couldn't execute %s -d"),
	       compress_program);
    }
  else
    error (SORT_FAILURE, errno, _("couldn't create process for %s -d"),
	   compress_program);

  fp = fdopen (pipefds[0], "r");
  if (! fp)
    die (_("couldn't create temporary file"), name);

  return fp;
}

static void
write_bytes (const char *buf, size_t n_bytes, FILE *fp, const char *output_file)
{
  if (fwrite (buf, 1, n_bytes, fp) != n_bytes)
    die (_("write failed"), output_file);
}

/* Append DIR to the array of temporary directory names.  */
static void
add_temp_dir (char const *dir)
{
  if (temp_dir_count == temp_dir_alloc)
    temp_dirs = X2NREALLOC (temp_dirs, &temp_dir_alloc);

  temp_dirs[temp_dir_count++] = dir;
}

/* Remove NAME from the list of temporary files.  */

static void
zaptemp (const char *name)
{
  struct tempnode *volatile *pnode;
  struct tempnode *node;
  struct tempnode *next;
  int unlink_status;
  int unlink_errno = 0;
  struct cs_status cs;

  for (pnode = &temphead; (node = *pnode)->name != name; pnode = &node->next)
    continue;

  /* Unlink the temporary file in a critical section to avoid races.  */
  next = node->next;
  cs = cs_enter ();
  unlink_status = unlink (name);
  unlink_errno = errno;
  *pnode = next;
  cs_leave (cs);

  if (unlink_status != 0)
    error (0, unlink_errno, _("warning: cannot remove: %s"), name);
  if (! next)
    temptail = pnode;
  free (node);
}

#if HAVE_NL_LANGINFO

static int
struct_month_cmp (const void *m1, const void *m2)
{
  struct month const *month1 = m1;
  struct month const *month2 = m2;
  return strcmp (month1->name, month2->name);
}

#endif

/* Initialize the character class tables. */

static void
inittables (void)
{
  size_t i;

  for (i = 0; i < UCHAR_LIM; ++i)
    {
      blanks[i] = !! isblank (i);
      nonprinting[i] = ! isprint (i);
      nondictionary[i] = ! isalnum (i) && ! isblank (i);
      fold_toupper[i] = toupper (i);
    }

#if HAVE_NL_LANGINFO
  /* If we're not in the "C" locale, read different names for months.  */
  if (hard_LC_TIME)
    {
      for (i = 0; i < MONTHS_PER_YEAR; i++)
	{
	  char const *s;
	  size_t s_len;
	  size_t j;
	  char *name;

	  s = (char *) nl_langinfo (ABMON_1 + i);
	  s_len = strlen (s);
	  monthtab[i].name = name = xmalloc (s_len + 1);
	  monthtab[i].val = i + 1;

	  for (j = 0; j < s_len; j++)
	    name[j] = fold_toupper[to_uchar (s[j])];
	  name[j] = '\0';
	}
      qsort ((void *) monthtab, MONTHS_PER_YEAR,
	     sizeof *monthtab, struct_month_cmp);
    }
#endif
}

/* Specify the amount of main memory to use when sorting.  */
static void
specify_sort_size (char const *s)
{
  uintmax_t n;
  char *suffix;
  enum strtol_error e = xstrtoumax (s, &suffix, 10, &n, "EgGkKmMPtTYZ");

  /* The default unit is KiB.  */
  if (e == LONGINT_OK && ISDIGIT (suffix[-1]))
    {
      if (n <= UINTMAX_MAX / 1024)
	n *= 1024;
      else
	e = LONGINT_OVERFLOW;
    }

  /* A 'b' suffix means bytes; a '%' suffix means percent of memory.  */
  if (e == LONGINT_INVALID_SUFFIX_CHAR && ISDIGIT (suffix[-1]) && ! suffix[1])
    switch (suffix[0])
      {
      case 'b':
	e = LONGINT_OK;
	break;

      case '%':
	{
	  double mem = physmem_total () * n / 100;

	  /* Use "<", not "<=", to avoid problems with rounding.  */
	  if (mem < UINTMAX_MAX)
	    {
	      n = mem;
	      e = LONGINT_OK;
	    }
	  else
	    e = LONGINT_OVERFLOW;
	}
	break;
      }

  if (e == LONGINT_OK)
    {
      /* If multiple sort sizes are specified, take the maximum, so
	 that option order does not matter.  */
      if (n < sort_size)
	return;

      sort_size = n;
      if (sort_size == n)
	{
	  sort_size = MAX (sort_size, MIN_SORT_SIZE);
	  return;
	}

      e = LONGINT_OVERFLOW;
    }

  STRTOL_FATAL_ERROR (s, _("sort size"), e);
}

/* Return the default sort size.  */
static size_t
default_sort_size (void)
{
  /* Let MEM be available memory or 1/8 of total memory, whichever
     is greater.  */
  double avail = physmem_available ();
  double total = physmem_total ();
  double mem = MAX (avail, total / 8);
  struct rlimit rlimit;

  /* Let SIZE be MEM, but no more than the maximum object size or
     system resource limits.  Avoid the MIN macro here, as it is not
     quite right when only one argument is floating point.  Don't
     bother to check for values like RLIM_INFINITY since in practice
     they are not much less than SIZE_MAX.  */
  size_t size = SIZE_MAX;
  if (mem < size)
    size = mem;
  if (getrlimit (RLIMIT_DATA, &rlimit) == 0 && rlimit.rlim_cur < size)
    size = rlimit.rlim_cur;
#ifdef RLIMIT_AS
  if (getrlimit (RLIMIT_AS, &rlimit) == 0 && rlimit.rlim_cur < size)
    size = rlimit.rlim_cur;
#endif

  /* Leave a large safety margin for the above limits, as failure can
     occur when they are exceeded.  */
  size /= 2;

#ifdef RLIMIT_RSS
  /* Leave a 1/16 margin for RSS to leave room for code, stack, etc.
     Exceeding RSS is not fatal, but can be quite slow.  */
  if (getrlimit (RLIMIT_RSS, &rlimit) == 0 && rlimit.rlim_cur / 16 * 15 < size)
    size = rlimit.rlim_cur / 16 * 15;
#endif

  /* Use no less than the minimum.  */
  return MAX (size, MIN_SORT_SIZE);
}

/* Return the sort buffer size to use with the input files identified
   by FPS and FILES, which are alternate names of the same files.
   NFILES gives the number of input files; NFPS may be less.  Assume
   that each input line requires LINE_BYTES extra bytes' worth of line
   information.  Do not exceed the size bound specified by the user
   (or a default size bound, if the user does not specify one).  */

static size_t
sort_buffer_size (FILE *const *fps, size_t nfps,
		  char *const *files, size_t nfiles,
		  size_t line_bytes)
{
  /* A bound on the input size.  If zero, the bound hasn't been
     determined yet.  */
  static size_t size_bound;

  /* In the worst case, each input byte is a newline.  */
  size_t worst_case_per_input_byte = line_bytes + 1;

  /* Keep enough room for one extra input line and an extra byte.
     This extra room might be needed when preparing to read EOF.  */
  size_t size = worst_case_per_input_byte + 1;

  size_t i;

  for (i = 0; i < nfiles; i++)
    {
      struct stat st;
      off_t file_size;
      size_t worst_case;

      if ((i < nfps ? fstat (fileno (fps[i]), &st)
	   : STREQ (files[i], "-") ? fstat (STDIN_FILENO, &st)
	   : stat (files[i], &st))
	  != 0)
	die (_("stat failed"), files[i]);

      if (S_ISREG (st.st_mode))
	file_size = st.st_size;
      else
	{
	  /* The file has unknown size.  If the user specified a sort
	     buffer size, use that; otherwise, guess the size.  */
	  if (sort_size)
	    return sort_size;
	  file_size = INPUT_FILE_SIZE_GUESS;
	}

      if (! size_bound)
	{
	  size_bound = sort_size;
	  if (! size_bound)
	    size_bound = default_sort_size ();
	}

      /* Add the amount of memory needed to represent the worst case
	 where the input consists entirely of newlines followed by a
	 single non-newline.  Check for overflow.  */
      worst_case = file_size * worst_case_per_input_byte + 1;
      if (file_size != worst_case / worst_case_per_input_byte
	  || size_bound - size <= worst_case)
	return size_bound;
      size += worst_case;
    }

  return size;
}

/* Initialize BUF.  Reserve LINE_BYTES bytes for each line; LINE_BYTES
   must be at least sizeof (struct line).  Allocate ALLOC bytes
   initially.  */

static void
initbuf (struct buffer *buf, size_t line_bytes, size_t alloc)
{
  /* Ensure that the line array is properly aligned.  If the desired
     size cannot be allocated, repeatedly halve it until allocation
     succeeds.  The smaller allocation may hurt overall performance,
     but that's better than failing.  */
  for (;;)
    {
      alloc += sizeof (struct line) - alloc % sizeof (struct line);
      buf->buf = malloc (alloc);
      if (buf->buf)
	break;
      alloc /= 2;
      if (alloc <= line_bytes + 1)
	xalloc_die ();
    }

  buf->line_bytes = line_bytes;
  buf->alloc = alloc;
  buf->used = buf->left = buf->nlines = 0;
  buf->eof = false;
}

/* Return one past the limit of the line array.  */

static inline struct line *
buffer_linelim (struct buffer const *buf)
{
  return (struct line *) (buf->buf + buf->alloc);
}

/* Return a pointer to the first character of the field specified
   by KEY in LINE. */

static char *
begfield (const struct line *line, const struct keyfield *key)
{
  char *ptr = line->text, *lim = ptr + line->length - 1;
  size_t sword = key->sword;
  size_t schar = key->schar;
  size_t remaining_bytes;

  /* The leading field separator itself is included in a field when -t
     is absent.  */

  if (tab != TAB_DEFAULT)
    while (ptr < lim && sword--)
      {
	while (ptr < lim && *ptr != tab)
	  ++ptr;
	if (ptr < lim)
	  ++ptr;
      }
  else
    while (ptr < lim && sword--)
      {
	while (ptr < lim && blanks[to_uchar (*ptr)])
	  ++ptr;
	while (ptr < lim && !blanks[to_uchar (*ptr)])
	  ++ptr;
      }

  if (key->skipsblanks)
    while (ptr < lim && blanks[to_uchar (*ptr)])
      ++ptr;

  /* Advance PTR by SCHAR (if possible), but no further than LIM.  */
  remaining_bytes = lim - ptr;
  if (schar < remaining_bytes)
    ptr += schar;
  else
    ptr = lim;

  return ptr;
}

/* Return the limit of (a pointer to the first character after) the field
   in LINE specified by KEY. */

static char *
limfield (const struct line *line, const struct keyfield *key)
{
  char *ptr = line->text, *lim = ptr + line->length - 1;
  size_t eword = key->eword, echar = key->echar;
  size_t remaining_bytes;

  /* Move PTR past EWORD fields or to one past the last byte on LINE,
     whichever comes first.  If there are more than EWORD fields, leave
     PTR pointing at the beginning of the field having zero-based index,
     EWORD.  If a delimiter character was specified (via -t), then that
     `beginning' is the first character following the delimiting TAB.
     Otherwise, leave PTR pointing at the first `blank' character after
     the preceding field.  */
  if (tab != TAB_DEFAULT)
    while (ptr < lim && eword--)
      {
	while (ptr < lim && *ptr != tab)
	  ++ptr;
	if (ptr < lim && (eword | echar))
	  ++ptr;
      }
  else
    while (ptr < lim && eword--)
      {
	while (ptr < lim && blanks[to_uchar (*ptr)])
	  ++ptr;
	while (ptr < lim && !blanks[to_uchar (*ptr)])
	  ++ptr;
      }

#ifdef POSIX_UNSPECIFIED
  /* The following block of code makes GNU sort incompatible with
     standard Unix sort, so it's ifdef'd out for now.
     The POSIX spec isn't clear on how to interpret this.
     FIXME: request clarification.

     From: kwzh@gnu.ai.mit.edu (Karl Heuer)
     Date: Thu, 30 May 96 12:20:41 -0400
     [Translated to POSIX 1003.1-2001 terminology by Paul Eggert.]

     [...]I believe I've found another bug in `sort'.

     $ cat /tmp/sort.in
     a b c 2 d
     pq rs 1 t
     $ textutils-1.15/src/sort -k1.7,1.7 </tmp/sort.in
     a b c 2 d
     pq rs 1 t
     $ /bin/sort -k1.7,1.7 </tmp/sort.in
     pq rs 1 t
     a b c 2 d

     Unix sort produced the answer I expected: sort on the single character
     in column 7.  GNU sort produced different results, because it disagrees
     on the interpretation of the key-end spec "M.N".  Unix sort reads this
     as "skip M-1 fields, then N-1 characters"; but GNU sort wants it to mean
     "skip M-1 fields, then either N-1 characters or the rest of the current
     field, whichever comes first".  This extra clause applies only to
     key-ends, not key-starts.
     */

  /* Make LIM point to the end of (one byte past) the current field.  */
  if (tab != TAB_DEFAULT)
    {
      char *newlim;
      newlim = memchr (ptr, tab, lim - ptr);
      if (newlim)
	lim = newlim;
    }
  else
    {
      char *newlim;
      newlim = ptr;
      while (newlim < lim && blanks[to_uchar (*newlim)])
	++newlim;
      while (newlim < lim && !blanks[to_uchar (*newlim)])
	++newlim;
      lim = newlim;
    }
#endif

  /* If we're ignoring leading blanks when computing the End
     of the field, don't start counting bytes until after skipping
     past any leading blanks. */
  if (key->skipeblanks)
    while (ptr < lim && blanks[to_uchar (*ptr)])
      ++ptr;

  /* Advance PTR by ECHAR (if possible), but no further than LIM.  */
  remaining_bytes = lim - ptr;
  if (echar < remaining_bytes)
    ptr += echar;
  else
    ptr = lim;

  return ptr;
}

/* Fill BUF reading from FP, moving buf->left bytes from the end
   of buf->buf to the beginning first.  If EOF is reached and the
   file wasn't terminated by a newline, supply one.  Set up BUF's line
   table too.  FILE is the name of the file corresponding to FP.
   Return true if some input was read.  */

static bool
fillbuf (struct buffer *buf, FILE *fp, char const *file)
{
  struct keyfield const *key = keylist;
  char eol = eolchar;
  size_t line_bytes = buf->line_bytes;
  size_t mergesize = merge_buffer_size - MIN_MERGE_BUFFER_SIZE;

  if (buf->eof)
    return false;

  if (buf->used != buf->left)
    {
      memmove (buf->buf, buf->buf + buf->used - buf->left, buf->left);
      buf->used = buf->left;
      buf->nlines = 0;
    }

  for (;;)
    {
      char *ptr = buf->buf + buf->used;
      struct line *linelim = buffer_linelim (buf);
      struct line *line = linelim - buf->nlines;
      size_t avail = (char *) linelim - buf->nlines * line_bytes - ptr;
      char *line_start = buf->nlines ? line->text + line->length : buf->buf;

      while (line_bytes + 1 < avail)
	{
	  /* Read as many bytes as possible, but do not read so many
	     bytes that there might not be enough room for the
	     corresponding line array.  The worst case is when the
	     rest of the input file consists entirely of newlines,
	     except that the last byte is not a newline.  */
	  size_t readsize = (avail - 1) / (line_bytes + 1);
	  size_t bytes_read = fread (ptr, 1, readsize, fp);
	  char *ptrlim = ptr + bytes_read;
	  char *p;
	  avail -= bytes_read;

	  if (bytes_read != readsize)
	    {
	      if (ferror (fp))
		die (_("read failed"), file);
	      if (feof (fp))
		{
		  buf->eof = true;
		  if (buf->buf == ptrlim)
		    return false;
		  if (ptrlim[-1] != eol)
		    *ptrlim++ = eol;
		}
	    }

	  /* Find and record each line in the just-read input.  */
	  while ((p = memchr (ptr, eol, ptrlim - ptr)))
	    {
	      ptr = p + 1;
	      line--;
	      line->text = line_start;
	      line->length = ptr - line_start;
	      mergesize = MAX (mergesize, line->length);
	      avail -= line_bytes;

	      if (key)
		{
		  /* Precompute the position of the first key for
		     efficiency.  */
		  line->keylim = (key->eword == SIZE_MAX
				  ? p
				  : limfield (line, key));

		  if (key->sword != SIZE_MAX)
		    line->keybeg = begfield (line, key);
		  else
		    {
		      if (key->skipsblanks)
			while (blanks[to_uchar (*line_start)])
			  line_start++;
		      line->keybeg = line_start;
		    }
		}

	      line_start = ptr;
	    }

	  ptr = ptrlim;
	  if (buf->eof)
	    break;
	}

      buf->used = ptr - buf->buf;
      buf->nlines = buffer_linelim (buf) - line;
      if (buf->nlines != 0)
	{
	  buf->left = ptr - line_start;
	  merge_buffer_size = mergesize + MIN_MERGE_BUFFER_SIZE;
	  return true;
	}

      /* The current input line is too long to fit in the buffer.
	 Double the buffer size and try again.  */
      buf->buf = X2REALLOC (buf->buf, &buf->alloc);
    }
}

/* Compare strings A and B as numbers without explicitly converting them to
   machine numbers.  Comparatively slow for short strings, but asymptotically
   hideously fast. */

static int
numcompare (const char *a, const char *b)
{
  while (blanks[to_uchar (*a)])
    a++;
  while (blanks[to_uchar (*b)])
    b++;

  return strnumcmp (a, b, decimal_point, thousands_sep);
}

static int
general_numcompare (const char *sa, const char *sb)
{
  /* FIXME: add option to warn about failed conversions.  */
  /* FIXME: maybe add option to try expensive FP conversion
     only if A and B can't be compared more cheaply/accurately.  */

  char *ea;
  char *eb;
  double a = strtod (sa, &ea);
  double b = strtod (sb, &eb);

  /* Put conversion errors at the start of the collating sequence.  */
  if (sa == ea)
    return sb == eb ? 0 : -1;
  if (sb == eb)
    return 1;

  /* Sort numbers in the usual way, where -0 == +0.  Put NaNs after
     conversion errors but before numbers; sort them by internal
     bit-pattern, for lack of a more portable alternative.  */
  return (a < b ? -1
	  : a > b ? 1
	  : a == b ? 0
	  : b == b ? -1
	  : a == a ? 1
	  : memcmp ((char *) &a, (char *) &b, sizeof a));
}

/* Return an integer in 1..12 of the month name MONTH with length LEN.
   Return 0 if the name in S is not recognized.  */

static int
getmonth (char const *month, size_t len)
{
  size_t lo = 0;
  size_t hi = MONTHS_PER_YEAR;
  char const *monthlim = month + len;

  for (;;)
    {
      if (month == monthlim)
	return 0;
      if (!blanks[to_uchar (*month)])
	break;
      ++month;
    }

  do
    {
      size_t ix = (lo + hi) / 2;
      char const *m = month;
      char const *n = monthtab[ix].name;

      for (;; m++, n++)
	{
	  if (!*n)
	    return monthtab[ix].val;
	  if (m == monthlim || fold_toupper[to_uchar (*m)] < to_uchar (*n))
	    {
	      hi = ix;
	      break;
	    }
	  else if (fold_toupper[to_uchar (*m)] > to_uchar (*n))
	    {
	      lo = ix + 1;
	      break;
	    }
	}
    }
  while (lo < hi);

  return 0;
}

/* A source of random data.  */
static struct randread_source *randread_source;

/* Return the Ith randomly-generated state.  The caller must invoke
   random_state (H) for all H less than I before invoking random_state
   (I).  */

static struct md5_ctx
random_state (size_t i)
{
  /* An array of states resulting from the random data, and counts of
     its used and allocated members.  */
  static struct md5_ctx *state;
  static size_t used;
  static size_t allocated;

  struct md5_ctx *s = &state[i];

  if (used <= i)
    {
      unsigned char buf[MD5_DIGEST_SIZE];

      used++;

      if (allocated <= i)
	{
	  state = X2NREALLOC (state, &allocated);
	  s = &state[i];
	}

      randread (randread_source, buf, sizeof buf);
      md5_init_ctx (s);
      md5_process_bytes (buf, sizeof buf, s);
    }

  return *s;
}

/* Compare the hashes of TEXTA with length LENGTHA to those of TEXTB
   with length LENGTHB.  Return negative if less, zero if equal,
   positive if greater.  */

static int
cmp_hashes (char const *texta, size_t lena,
	    char const *textb, size_t lenb)
{
  /* Try random hashes until a pair of hashes disagree.  But if the
     first pair of random hashes agree, check whether the keys are
     identical and if so report no difference.  */
  int diff;
  size_t i;
  for (i = 0; ; i++)
    {
      uint32_t dig[2][MD5_DIGEST_SIZE / sizeof (uint32_t)];
      struct md5_ctx s[2];
      s[0] = s[1] = random_state (i);
      md5_process_bytes (texta, lena, &s[0]);  md5_finish_ctx (&s[0], dig[0]);
      md5_process_bytes (textb, lenb, &s[1]);  md5_finish_ctx (&s[1], dig[1]);
      diff = memcmp (dig[0], dig[1], sizeof dig[0]);
      if (diff != 0)
	break;
      if (i == 0 && lena == lenb && memcmp (texta, textb, lena) == 0)
	break;
    }

  return diff;
}

/* Compare the keys TEXTA (of length LENA) and TEXTB (of length LENB)
   using one or more random hash functions.  */

static int
compare_random (char *restrict texta, size_t lena,
		char *restrict textb, size_t lenb)
{
  int diff;

  if (! hard_LC_COLLATE)
    diff = cmp_hashes (texta, lena, textb, lenb);
  else
    {
      /* Transform the text into the basis of comparison, so that byte
	 strings that would otherwise considered to be equal are
	 considered equal here even if their bytes differ.  */

      char *buf = NULL;
      char stackbuf[4000];
      size_t tlena = xmemxfrm (stackbuf, sizeof stackbuf, texta, lena);
      bool a_fits = tlena <= sizeof stackbuf;
      size_t tlenb = xmemxfrm ((a_fits ? stackbuf + tlena : NULL),
			       (a_fits ? sizeof stackbuf - tlena : 0),
			       textb, lenb);

      if (a_fits && tlena + tlenb <= sizeof stackbuf)
	buf = stackbuf;
      else
	{
	  /* Adding 1 to the buffer size lets xmemxfrm run a bit
	     faster by avoiding the need for an extra buffer copy.  */
	  buf = xmalloc (tlena + tlenb + 1);
	  xmemxfrm (buf, tlena + 1, texta, lena);
	  xmemxfrm (buf + tlena, tlenb + 1, textb, lenb);
	}

      diff = cmp_hashes (buf, tlena, buf + tlena, tlenb);

      if (buf != stackbuf)
	free (buf);
    }

  return diff;
}

/* Compare two lines A and B trying every key in sequence until there
   are no more keys or a difference is found. */

static int
keycompare (const struct line *a, const struct line *b)
{
  struct keyfield const *key = keylist;

  /* For the first iteration only, the key positions have been
     precomputed for us. */
  char *texta = a->keybeg;
  char *textb = b->keybeg;
  char *lima = a->keylim;
  char *limb = b->keylim;

  int diff;

  for (;;)
    {
      char const *translate = key->translate;
      bool const *ignore = key->ignore;

      /* Find the lengths. */
      size_t lena = lima <= texta ? 0 : lima - texta;
      size_t lenb = limb <= textb ? 0 : limb - textb;

      /* Actually compare the fields. */

      if (key->random)
	diff = compare_random (texta, lena, textb, lenb);
      else if (key->numeric | key->general_numeric)
	{
	  char savea = *lima, saveb = *limb;

	  *lima = *limb = '\0';
	  diff = ((key->numeric ? numcompare : general_numcompare)
		  (texta, textb));
	  *lima = savea, *limb = saveb;
	}
      else if (key->month)
	diff = getmonth (texta, lena) - getmonth (textb, lenb);
      /* Sorting like this may become slow, so in a simple locale the user
	 can select a faster sort that is similar to ascii sort.  */
      else if (hard_LC_COLLATE)
	{
	  if (ignore || translate)
	    {
	      char buf[4000];
	      size_t size = lena + 1 + lenb + 1;
	      char *copy_a = (size <= sizeof buf ? buf : xmalloc (size));
	      char *copy_b = copy_a + lena + 1;
	      size_t new_len_a, new_len_b, i;

	      /* Ignore and/or translate chars before comparing.  */
	      for (new_len_a = new_len_b = i = 0; i < MAX (lena, lenb); i++)
		{
		  if (i < lena)
		    {
		      copy_a[new_len_a] = (translate
					   ? translate[to_uchar (texta[i])]
					   : texta[i]);
		      if (!ignore || !ignore[to_uchar (texta[i])])
			++new_len_a;
		    }
		  if (i < lenb)
		    {
		      copy_b[new_len_b] = (translate
					   ? translate[to_uchar (textb[i])]
					   : textb [i]);
		      if (!ignore || !ignore[to_uchar (textb[i])])
			++new_len_b;
		    }
		}

	      diff = xmemcoll (copy_a, new_len_a, copy_b, new_len_b);

	      if (sizeof buf < size)
		free (copy_a);
	    }
	  else if (lena == 0)
	    diff = - NONZERO (lenb);
	  else if (lenb == 0)
	    goto greater;
	  else
	    diff = xmemcoll (texta, lena, textb, lenb);
	}
      else if (ignore)
	{
#define CMP_WITH_IGNORE(A, B)						\
  do									\
    {									\
	  for (;;)							\
	    {								\
	      while (texta < lima && ignore[to_uchar (*texta)])		\
		++texta;						\
	      while (textb < limb && ignore[to_uchar (*textb)])		\
		++textb;						\
	      if (! (texta < lima && textb < limb))			\
		break;							\
	      diff = to_uchar (A) - to_uchar (B);			\
	      if (diff)							\
		goto not_equal;						\
	      ++texta;							\
	      ++textb;							\
	    }								\
									\
	  diff = (texta < lima) - (textb < limb);			\
    }									\
  while (0)

	  if (translate)
	    CMP_WITH_IGNORE (translate[to_uchar (*texta)],
			     translate[to_uchar (*textb)]);
	  else
	    CMP_WITH_IGNORE (*texta, *textb);
	}
      else if (lena == 0)
	diff = - NONZERO (lenb);
      else if (lenb == 0)
	goto greater;
      else
	{
	  if (translate)
	    {
	      while (texta < lima && textb < limb)
		{
		  diff = (to_uchar (translate[to_uchar (*texta++)])
			  - to_uchar (translate[to_uchar (*textb++)]));
		  if (diff)
		    goto not_equal;
		}
	    }
	  else
	    {
	      diff = memcmp (texta, textb, MIN (lena, lenb));
	      if (diff)
		goto not_equal;
	    }
	  diff = lena < lenb ? -1 : lena != lenb;
	}

      if (diff)
	goto not_equal;

      key = key->next;
      if (! key)
	break;

      /* Find the beginning and limit of the next field.  */
      if (key->eword != SIZE_MAX)
	lima = limfield (a, key), limb = limfield (b, key);
      else
	lima = a->text + a->length - 1, limb = b->text + b->length - 1;

      if (key->sword != SIZE_MAX)
	texta = begfield (a, key), textb = begfield (b, key);
      else
	{
	  texta = a->text, textb = b->text;
	  if (key->skipsblanks)
	    {
	      while (texta < lima && blanks[to_uchar (*texta)])
		++texta;
	      while (textb < limb && blanks[to_uchar (*textb)])
		++textb;
	    }
	}
    }

  return 0;

 greater:
  diff = 1;
 not_equal:
  return key->reverse ? -diff : diff;
}

/* Compare two lines A and B, returning negative, zero, or positive
   depending on whether A compares less than, equal to, or greater than B. */

static int
compare (const struct line *a, const struct line *b)
{
  int diff;
  size_t alen, blen;

  /* First try to compare on the specified keys (if any).
     The only two cases with no key at all are unadorned sort,
     and unadorned sort -r. */
  if (keylist)
    {
      diff = keycompare (a, b);
      if (diff | unique | stable)
	return diff;
    }

  /* If the keys all compare equal (or no keys were specified)
     fall through to the default comparison.  */
  alen = a->length - 1, blen = b->length - 1;

  if (alen == 0)
    diff = - NONZERO (blen);
  else if (blen == 0)
    diff = 1;
  else if (hard_LC_COLLATE)
    diff = xmemcoll (a->text, alen, b->text, blen);
  else if (! (diff = memcmp (a->text, b->text, MIN (alen, blen))))
    diff = alen < blen ? -1 : alen != blen;

  return reverse ? -diff : diff;
}

/* Check that the lines read from FILE_NAME come in order.  Return
   true if they are in order.  If CHECKONLY == 'c', also print a
   diagnostic (FILE_NAME, line number, contents of line) to stderr if
   they are not in order.  */

static bool
check (char const *file_name, char checkonly)
{
  FILE *fp = xfopen (file_name, "r");
  struct buffer buf;		/* Input buffer. */
  struct line temp;		/* Copy of previous line. */
  size_t alloc = 0;
  uintmax_t line_number = 0;
  struct keyfield const *key = keylist;
  bool nonunique = ! unique;
  bool ordered = true;

  initbuf (&buf, sizeof (struct line),
	   MAX (merge_buffer_size, sort_size));
  temp.text = NULL;

  while (fillbuf (&buf, fp, file_name))
    {
      struct line const *line = buffer_linelim (&buf);
      struct line const *linebase = line - buf.nlines;

      /* Make sure the line saved from the old buffer contents is
	 less than or equal to the first line of the new buffer. */
      if (alloc && nonunique <= compare (&temp, line - 1))
	{
	found_disorder:
	  {
	    if (checkonly == 'c')
	      {
		struct line const *disorder_line = line - 1;
		uintmax_t disorder_line_number =
		  buffer_linelim (&buf) - disorder_line + line_number;
		char hr_buf[INT_BUFSIZE_BOUND (uintmax_t)];
		fprintf (stderr, _("%s: %s:%s: disorder: "),
			 program_name, file_name,
			 umaxtostr (disorder_line_number, hr_buf));
		write_bytes (disorder_line->text, disorder_line->length,
			     stderr, _("standard error"));
	      }

	    ordered = false;
	    break;
	  }
	}

      /* Compare each line in the buffer with its successor.  */
      while (linebase < --line)
	if (nonunique <= compare (line, line - 1))
	  goto found_disorder;

      line_number += buf.nlines;

      /* Save the last line of the buffer.  */
      if (alloc < line->length)
	{
	  do
	    {
	      alloc *= 2;
	      if (! alloc)
		{
		  alloc = line->length;
		  break;
		}
	    }
	  while (alloc < line->length);

	  temp.text = xrealloc (temp.text, alloc);
	}
      memcpy (temp.text, line->text, line->length);
      temp.length = line->length;
      if (key)
	{
	  temp.keybeg = temp.text + (line->keybeg - line->text);
	  temp.keylim = temp.text + (line->keylim - line->text);
	}
    }

  xfclose (fp, file_name);
  free (buf.buf);
  free (temp.text);
  return ordered;
}

/* Merge lines from FILES onto OFP.  NTEMPS is the number of temporary
   files (all of which are at the start of the FILES array), and
   NFILES is the number of files; 0 <= NTEMPS <= NFILES <= NMERGE.
   Close input and output files before returning.
   OUTPUT_FILE gives the name of the output file.  If it is NULL,
   the output file is standard output.  If OFP is NULL, the output
   file has not been opened yet (or written to, if standard output).  */

static void
mergefps (struct sortfile *files, size_t ntemps, size_t nfiles,
	  FILE *ofp, char const *output_file)
{
  FILE *fps[NMERGE];		/* Input streams for each file.  */
  struct buffer buffer[NMERGE];	/* Input buffers for each file. */
  struct line saved;		/* Saved line storage for unique check. */
  struct line const *savedline = NULL;
				/* &saved if there is a saved line. */
  size_t savealloc = 0;		/* Size allocated for the saved line. */
  struct line const *cur[NMERGE]; /* Current line in each line table. */
  struct line const *base[NMERGE]; /* Base of each line table.  */
  size_t ord[NMERGE];		/* Table representing a permutation of fps,
				   such that cur[ord[0]] is the smallest line
				   and will be next output. */
  size_t i;
  size_t j;
  size_t t;
  struct keyfield const *key = keylist;
  saved.text = NULL;

  /* Read initial lines from each input file. */
  for (i = 0; i < nfiles; )
    {
      fps[i] = (files[i].pid
		? open_temp (files[i].name, files[i].pid)
		: xfopen (files[i].name, "r"));
      initbuf (&buffer[i], sizeof (struct line),
	       MAX (merge_buffer_size, sort_size / nfiles));
      if (fillbuf (&buffer[i], fps[i], files[i].name))
	{
	  struct line const *linelim = buffer_linelim (&buffer[i]);
	  cur[i] = linelim - 1;
	  base[i] = linelim - buffer[i].nlines;
	  i++;
	}
      else
	{
	  /* fps[i] is empty; eliminate it from future consideration.  */
	  xfclose (fps[i], files[i].name);
	  if (i < ntemps)
	    {
	      ntemps--;
	      zaptemp (files[i].name);
	    }
	  free (buffer[i].buf);
	  --nfiles;
	  for (j = i; j < nfiles; ++j)
	    files[j] = files[j + 1];
	}
    }

  if (! ofp)
    ofp = xfopen (output_file, "w");

  /* Set up the ord table according to comparisons among input lines.
     Since this only reorders two items if one is strictly greater than
     the other, it is stable. */
  for (i = 0; i < nfiles; ++i)
    ord[i] = i;
  for (i = 1; i < nfiles; ++i)
    if (0 < compare (cur[ord[i - 1]], cur[ord[i]]))
      t = ord[i - 1], ord[i - 1] = ord[i], ord[i] = t, i = 0;

  /* Repeatedly output the smallest line until no input remains. */
  while (nfiles)
    {
      struct line const *smallest = cur[ord[0]];

      /* If uniquified output is turned on, output only the first of
	 an identical series of lines. */
      if (unique)
	{
	  if (savedline && compare (savedline, smallest))
	    {
	      savedline = NULL;
	      write_bytes (saved.text, saved.length, ofp, output_file);
	    }
	  if (!savedline)
	    {
	      savedline = &saved;
	      if (savealloc < smallest->length)
		{
		  do
		    if (! savealloc)
		      {
			savealloc = smallest->length;
			break;
		      }
		  while ((savealloc *= 2) < smallest->length);

		  saved.text = xrealloc (saved.text, savealloc);
		}
	      saved.length = smallest->length;
	      memcpy (saved.text, smallest->text, saved.length);
	      if (key)
		{
		  saved.keybeg =
		    saved.text + (smallest->keybeg - smallest->text);
		  saved.keylim =
		    saved.text + (smallest->keylim - smallest->text);
		}
	    }
	}
      else
	write_bytes (smallest->text, smallest->length, ofp, output_file);

      /* Check if we need to read more lines into core. */
      if (base[ord[0]] < smallest)
	cur[ord[0]] = smallest - 1;
      else
	{
	  if (fillbuf (&buffer[ord[0]], fps[ord[0]], files[ord[0]].name))
	    {
	      struct line const *linelim = buffer_linelim (&buffer[ord[0]]);
	      cur[ord[0]] = linelim - 1;
	      base[ord[0]] = linelim - buffer[ord[0]].nlines;
	    }
	  else
	    {
	      /* We reached EOF on fps[ord[0]].  */
	      for (i = 1; i < nfiles; ++i)
		if (ord[i] > ord[0])
		  --ord[i];
	      --nfiles;
	      xfclose (fps[ord[0]], files[ord[0]].name);
	      if (ord[0] < ntemps)
		{
		  ntemps--;
		  zaptemp (files[ord[0]].name);
		}
	      free (buffer[ord[0]].buf);
	      for (i = ord[0]; i < nfiles; ++i)
		{
		  fps[i] = fps[i + 1];
		  files[i] = files[i + 1];
		  buffer[i] = buffer[i + 1];
		  cur[i] = cur[i + 1];
		  base[i] = base[i + 1];
		}
	      for (i = 0; i < nfiles; ++i)
		ord[i] = ord[i + 1];
	      continue;
	    }
	}

      /* The new line just read in may be larger than other lines
	 already in main memory; push it back in the queue until we
	 encounter a line larger than it.  Optimize for the common
	 case where the new line is smallest.  */
      {
	size_t lo = 1;
	size_t hi = nfiles;
	size_t probe = lo;
	size_t ord0 = ord[0];
	size_t count_of_smaller_lines;

	while (lo < hi)
	  {
	    int cmp = compare (cur[ord0], cur[ord[probe]]);
	    if (cmp < 0 || (cmp == 0 && ord0 < ord[probe]))
	      hi = probe;
	    else
	      lo = probe + 1;
	    probe = (lo + hi) / 2;
	  }

	count_of_smaller_lines = lo - 1;
	for (j = 0; j < count_of_smaller_lines; j++)
	  ord[j] = ord[j + 1];
	ord[count_of_smaller_lines] = ord0;
      }

      /* Free up some resources every once in a while.  */
      if (MAX_PROCS_BEFORE_REAP < nprocs)
	reap_some ();
    }

  if (unique && savedline)
    {
      write_bytes (saved.text, saved.length, ofp, output_file);
      free (saved.text);
    }

  xfclose (ofp, output_file);
}

/* Merge into T the two sorted arrays of lines LO (with NLO members)
   and HI (with NHI members).  T, LO, and HI point just past their
   respective arrays, and the arrays are in reverse order.  NLO and
   NHI must be positive, and HI - NHI must equal T - (NLO + NHI).  */

static inline void
mergelines (struct line *t,
	    struct line const *lo, size_t nlo,
	    struct line const *hi, size_t nhi)
{
  for (;;)
    if (compare (lo - 1, hi - 1) <= 0)
      {
	*--t = *--lo;
	if (! --nlo)
	  {
	    /* HI - NHI equalled T - (NLO + NHI) when this function
	       began.  Therefore HI must equal T now, and there is no
	       need to copy from HI to T.  */
	    return;
	  }
      }
    else
      {
	*--t = *--hi;
	if (! --nhi)
	  {
	    do
	      *--t = *--lo;
	    while (--nlo);

	    return;
	  }
      }
}

/* Sort the array LINES with NLINES members, using TEMP for temporary space.
   NLINES must be at least 2.
   The input and output arrays are in reverse order, and LINES and
   TEMP point just past the end of their respective arrays.

   Use a recursive divide-and-conquer algorithm, in the style
   suggested by Knuth volume 3 (2nd edition), exercise 5.2.4-23.  Use
   the optimization suggested by exercise 5.2.4-10; this requires room
   for only 1.5*N lines, rather than the usual 2*N lines.  Knuth
   writes that this memory optimization was originally published by
   D. A. Bell, Comp J. 1 (1958), 75.  */

static void
sortlines (struct line *lines, size_t nlines, struct line *temp)
{
  if (nlines == 2)
    {
      if (0 < compare (&lines[-1], &lines[-2]))
	{
	  struct line tmp = lines[-1];
	  lines[-1] = lines[-2];
	  lines[-2] = tmp;
	}
    }
  else
    {
      size_t nlo = nlines / 2;
      size_t nhi = nlines - nlo;
      struct line *lo = lines;
      struct line *hi = lines - nlo;
      struct line *sorted_lo = temp;

      sortlines (hi, nhi, temp);
      if (1 < nlo)
	sortlines_temp (lo, nlo, sorted_lo);
      else
	sorted_lo[-1] = lo[-1];

      mergelines (lines, sorted_lo, nlo, hi, nhi);
    }
}

/* Like sortlines (LINES, NLINES, TEMP), except output into TEMP
   rather than sorting in place.  */

static void
sortlines_temp (struct line *lines, size_t nlines, struct line *temp)
{
  if (nlines == 2)
    {
      /* Declare `swap' as int, not bool, to work around a bug
	 <http://lists.gnu.org/archive/html/bug-coreutils/2005-10/msg00086.html>
	 in the IBM xlc 6.0.0.0 compiler in 64-bit mode.  */
      int swap = (0 < compare (&lines[-1], &lines[-2]));
      temp[-1] = lines[-1 - swap];
      temp[-2] = lines[-2 + swap];
    }
  else
    {
      size_t nlo = nlines / 2;
      size_t nhi = nlines - nlo;
      struct line *lo = lines;
      struct line *hi = lines - nlo;
      struct line *sorted_hi = temp - nlo;

      sortlines_temp (hi, nhi, sorted_hi);
      if (1 < nlo)
	sortlines (lo, nlo, temp);

      mergelines (temp, lo, nlo, sorted_hi, nhi);
    }
}

/* Scan through FILES[NTEMPS .. NFILES-1] looking for a file that is
   the same as OUTFILE.  If found, merge the found instances (and perhaps
   some other files) into a temporary file so that it can in turn be
   merged into OUTFILE without destroying OUTFILE before it is completely
   read.  Return the new value of NFILES, which differs from the old if
   some merging occurred.

   This test ensures that an otherwise-erroneous use like
   "sort -m -o FILE ... FILE ..." copies FILE before writing to it.
   It's not clear that POSIX requires this nicety.
   Detect common error cases, but don't try to catch obscure cases like
   "cat ... FILE ... | sort -m -o FILE"
   where traditional "sort" doesn't copy the input and where
   people should know that they're getting into trouble anyway.
   Catching these obscure cases would slow down performance in
   common cases.  */

static size_t
avoid_trashing_input (struct sortfile *files, size_t ntemps,
		      size_t nfiles, char const *outfile)
{
  size_t i;
  bool got_outstat = false;
  struct stat outstat;

  for (i = ntemps; i < nfiles; i++)
    {
      bool is_stdin = STREQ (files[i].name, "-");
      bool same;
      struct stat instat;

      if (outfile && STREQ (outfile, files[i].name) && !is_stdin)
	same = true;
      else
	{
	  if (! got_outstat)
	    {
	      if ((outfile
		   ? stat (outfile, &outstat)
		   : fstat (STDOUT_FILENO, &outstat))
		  != 0)
		break;
	      got_outstat = true;
	    }

	  same = (((is_stdin
		    ? fstat (STDIN_FILENO, &instat)
		    : stat (files[i].name, &instat))
		   == 0)
		  && SAME_INODE (instat, outstat));
	}

      if (same)
	{
	  FILE *tftp;
	  pid_t pid;
	  char *temp = create_temp (&tftp, &pid);
	  mergefps (&files[i],0, nfiles - i, tftp, temp);
	  files[i].name = temp;
	  files[i].pid = pid;
	  return i + 1;
	}
    }

  return nfiles;
}

/* Merge the input FILES.  NTEMPS is the number of files at the
   start of FILES that are temporary; it is zero at the top level.
   NFILES is the total number of files.  Put the output in
   OUTPUT_FILE; a null OUTPUT_FILE stands for standard output.  */

static void
merge (struct sortfile *files, size_t ntemps, size_t nfiles,
       char const *output_file)
{
  while (NMERGE < nfiles)
    {
      /* Number of input files processed so far.  */
      size_t in;

      /* Number of output files generated so far.  */
      size_t out;

      /* nfiles % NMERGE; this counts input files that are left over
	 after all full-sized merges have been done.  */
      size_t remainder;

      /* Number of easily-available slots at the next loop iteration.  */
      size_t cheap_slots;

      /* Do as many NMERGE-size merges as possible.  */
      for (out = in = 0; out < nfiles / NMERGE; out++, in += NMERGE)
	{
	  FILE *tfp;
	  pid_t pid;
	  char *temp = create_temp (&tfp, &pid);
	  size_t nt = MIN (ntemps, NMERGE);
	  ntemps -= nt;
	  mergefps (&files[in], nt, NMERGE, tfp, temp);
	  files[out].name = temp;
	  files[out].pid = pid;
	}

      remainder = nfiles - in;
      cheap_slots = NMERGE - out % NMERGE;

      if (cheap_slots < remainder)
	{
	  /* So many files remain that they can't all be put into the last
	     NMERGE-sized output window.  Do one more merge.  Merge as few
	     files as possible, to avoid needless I/O.  */
	  size_t nshortmerge = remainder - cheap_slots + 1;
	  FILE *tfp;
	  pid_t pid;
	  char *temp = create_temp (&tfp, &pid);
	  size_t nt = MIN (ntemps, nshortmerge);
	  ntemps -= nt;
	  mergefps (&files[in], nt, nshortmerge, tfp, temp);
	  files[out].name = temp;
	  files[out++].pid = pid;
	  in += nshortmerge;
	}

      /* Put the remaining input files into the last NMERGE-sized output
	 window, so they will be merged in the next pass.  */
      memmove(&files[out], &files[in], (nfiles - in) * sizeof *files);
      ntemps += out;
      nfiles -= in - out;
    }

  nfiles = avoid_trashing_input (files, ntemps, nfiles, output_file);
  mergefps (files, ntemps, nfiles, NULL, output_file);
}

/* Sort NFILES FILES onto OUTPUT_FILE. */

static void
sort (char * const *files, size_t nfiles, char const *output_file)
{
  struct buffer buf;
  size_t ntemps = 0;
  bool output_file_created = false;

  buf.alloc = 0;

  while (nfiles)
    {
      char const *temp_output;
      char const *file = *files;
      FILE *fp = xfopen (file, "r");
      FILE *tfp;
      size_t bytes_per_line = (2 * sizeof (struct line)
			       - sizeof (struct line) / 2);

      if (! buf.alloc)
	initbuf (&buf, bytes_per_line,
		 sort_buffer_size (&fp, 1, files, nfiles, bytes_per_line));
      buf.eof = false;
      files++;
      nfiles--;

      while (fillbuf (&buf, fp, file))
	{
	  struct line *line;
	  struct line *linebase;

	  if (buf.eof && nfiles
	      && (bytes_per_line + 1
		  < (buf.alloc - buf.used - bytes_per_line * buf.nlines)))
	    {
	      /* End of file, but there is more input and buffer room.
		 Concatenate the next input file; this is faster in
		 the usual case.  */
	      buf.left = buf.used;
	      break;
	    }

	  line = buffer_linelim (&buf);
	  linebase = line - buf.nlines;
	  if (1 < buf.nlines)
	    sortlines (line, buf.nlines, linebase);
	  if (buf.eof && !nfiles && !ntemps && !buf.left)
	    {
	      xfclose (fp, file);
	      tfp = xfopen (output_file, "w");
	      temp_output = output_file;
	      output_file_created = true;
	    }
	  else
	    {
	      ++ntemps;
	      temp_output = create_temp (&tfp, NULL);
	    }

	  do
	    {
	      line--;
	      write_bytes (line->text, line->length, tfp, temp_output);
	      if (unique)
		while (linebase < line && compare (line, line - 1) == 0)
		  line--;
	    }
	  while (linebase < line);

	  xfclose (tfp, temp_output);

	  /* Free up some resources every once in a while.  */
	  if (MAX_PROCS_BEFORE_REAP < nprocs)
	    reap_some ();

	  if (output_file_created)
	    goto finish;
	}
      xfclose (fp, file);
    }

 finish:
  free (buf.buf);

  if (! output_file_created)
    {
      size_t i;
      struct tempnode *node = temphead;
      struct sortfile *tempfiles = xnmalloc (ntemps, sizeof *tempfiles);
      for (i = 0; node; i++)
	{
	  tempfiles[i].name = node->name;
	  tempfiles[i].pid = node->pid;
	  node = node->next;
	}
      merge (tempfiles, ntemps, ntemps, output_file);
      free (tempfiles);
    }
}

/* Insert a malloc'd copy of key KEY_ARG at the end of the key list.  */

static void
insertkey (struct keyfield *key_arg)
{
  struct keyfield **p;
  struct keyfield *key = xmemdup (key_arg, sizeof *key);

  for (p = &keylist; *p; p = &(*p)->next)
    continue;
  *p = key;
  key->next = NULL;
}

/* Report a bad field specification SPEC, with extra info MSGID.  */

static void badfieldspec (char const *, char const *)
     ATTRIBUTE_NORETURN;
static void
badfieldspec (char const *spec, char const *msgid)
{
  error (SORT_FAILURE, 0, _("%s: invalid field specification %s"),
	 _(msgid), quote (spec));
  abort ();
}

/* Report incompatible options.  */

static void incompatible_options (char const *) ATTRIBUTE_NORETURN;
static void
incompatible_options (char const *opts)
{
  error (SORT_FAILURE, 0, _("options `-%s' are incompatible"), opts);
  abort ();
}

/* Check compatibility of ordering options.  */

static void
check_ordering_compatibility (void)
{
  struct keyfield const *key;

  for (key = keylist; key; key = key->next)
    if ((1 < (key->random + key->numeric + key->general_numeric + key->month
	      + !!key->ignore))
	|| (key->random && key->translate))
      {
	char opts[7];
	char *p = opts;
	if (key->ignore == nondictionary)
	  *p++ = 'd';
	if (key->translate)
	  *p++ = 'f';
	if (key->general_numeric)
	  *p++ = 'g';
	if (key->ignore == nonprinting)
	  *p++ = 'i';
	if (key->month)
	  *p++ = 'M';
	if (key->numeric)
	  *p++ = 'n';
	if (key->random)
	  *p++ = 'R';
	*p = '\0';
	incompatible_options (opts);
      }
}

/* Parse the leading integer in STRING and store the resulting value
   (which must fit into size_t) into *VAL.  Return the address of the
   suffix after the integer.  If the value is too large, silently
   substitute SIZE_MAX.  If MSGID is NULL, return NULL after
   failure; otherwise, report MSGID and exit on failure.  */

static char const *
parse_field_count (char const *string, size_t *val, char const *msgid)
{
  char *suffix;
  uintmax_t n;

  switch (xstrtoumax (string, &suffix, 10, &n, ""))
    {
    case LONGINT_OK:
    case LONGINT_INVALID_SUFFIX_CHAR:
      *val = n;
      if (*val == n)
	break;
      /* Fall through.  */
    case LONGINT_OVERFLOW:
    case LONGINT_OVERFLOW | LONGINT_INVALID_SUFFIX_CHAR:
      *val = SIZE_MAX;
      break;

    case LONGINT_INVALID:
      if (msgid)
	error (SORT_FAILURE, 0, _("%s: invalid count at start of %s"),
	       _(msgid), quote (string));
      return NULL;
    }

  return suffix;
}

/* Handle interrupts and hangups. */

static void
sighandler (int sig)
{
  if (! SA_NOCLDSTOP)
    signal (sig, SIG_IGN);

  cleanup ();

  signal (sig, SIG_DFL);
  raise (sig);
}

/* Set the ordering options for KEY specified in S.
   Return the address of the first character in S that
   is not a valid ordering option.
   BLANKTYPE is the kind of blanks that 'b' should skip. */

static char *
set_ordering (const char *s, struct keyfield *key, enum blanktype blanktype)
{
  while (*s)
    {
      switch (*s)
	{
	case 'b':
	  if (blanktype == bl_start || blanktype == bl_both)
	    key->skipsblanks = true;
	  if (blanktype == bl_end || blanktype == bl_both)
	    key->skipeblanks = true;
	  break;
	case 'd':
	  key->ignore = nondictionary;
	  break;
	case 'f':
	  key->translate = fold_toupper;
	  break;
	case 'g':
	  key->general_numeric = true;
	  break;
	case 'i':
	  /* Option order should not matter, so don't let -i override
	     -d.  -d implies -i, but -i does not imply -d.  */
	  if (! key->ignore)
	    key->ignore = nonprinting;
	  break;
	case 'M':
	  key->month = true;
	  break;
	case 'n':
	  key->numeric = true;
	  break;
	case 'R':
	  key->random = true;
	  break;
	case 'r':
	  key->reverse = true;
	  break;
	default:
	  return (char *) s;
	}
      ++s;
    }
  return (char *) s;
}

static struct keyfield *
key_init (struct keyfield *key)
{
  memset (key, 0, sizeof *key);
  key->eword = SIZE_MAX;
  return key;
}

int
main (int argc, char **argv)
{
  struct keyfield *key;
  struct keyfield key_buf;
  struct keyfield gkey;
  char const *s;
  int c = 0;
  char checkonly = 0;
  bool mergeonly = false;
  char *random_source = NULL;
  bool need_random = false;
  size_t nfiles = 0;
  bool posixly_correct = (getenv ("POSIXLY_CORRECT") != NULL);
  bool obsolete_usage = (posix2_version () < 200112);
  char **files;
  char const *outfile = NULL;

  initialize_main (&argc, &argv);
  program_name = argv[0];
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  initialize_exit_failure (SORT_FAILURE);

  hard_LC_COLLATE = hard_locale (LC_COLLATE);
#if HAVE_NL_LANGINFO
  hard_LC_TIME = hard_locale (LC_TIME);
#endif

  /* Get locale's representation of the decimal point.  */
  {
    struct lconv const *locale = localeconv ();

    /* If the locale doesn't define a decimal point, or if the decimal
       point is multibyte, use the C locale's decimal point.  FIXME:
       add support for multibyte decimal points.  */
    decimal_point = to_uchar (locale->decimal_point[0]);
    if (! decimal_point || locale->decimal_point[1])
      decimal_point = '.';

    /* FIXME: add support for multibyte thousands separators.  */
    thousands_sep = to_uchar (*locale->thousands_sep);
    if (! thousands_sep || locale->thousands_sep[1])
      thousands_sep = -1;
  }

  have_read_stdin = false;
  inittables ();

  {
    size_t i;
    static int const sig[] =
      {
	/* The usual suspects.  */
	SIGALRM, SIGHUP, SIGINT, SIGPIPE, SIGQUIT, SIGTERM,
#ifdef SIGPOLL
	SIGPOLL,
#endif
#ifdef SIGPROF
	SIGPROF,
#endif
#ifdef SIGVTALRM
	SIGVTALRM,
#endif
#ifdef SIGXCPU
	SIGXCPU,
#endif
#ifdef SIGXFSZ
	SIGXFSZ,
#endif
      };
    enum { nsigs = sizeof sig / sizeof sig[0] };

#if SA_NOCLDSTOP
    struct sigaction act;

    sigemptyset (&caught_signals);
    for (i = 0; i < nsigs; i++)
      {
	sigaction (sig[i], NULL, &act);
	if (act.sa_handler != SIG_IGN)
	  sigaddset (&caught_signals, sig[i]);
      }

    act.sa_handler = sighandler;
    act.sa_mask = caught_signals;
    act.sa_flags = 0;

    for (i = 0; i < nsigs; i++)
      if (sigismember (&caught_signals, sig[i]))
	sigaction (sig[i], &act, NULL);
#else
    for (i = 0; i < nsigs; i++)
      if (signal (sig[i], SIG_IGN) != SIG_IGN)
	{
	  signal (sig[i], sighandler);
	  siginterrupt (sig[i], 1);
	}
#endif
  }

  /* The signal mask is known, so it is safe to invoke exit_cleanup.  */
  atexit (exit_cleanup);

  gkey.sword = gkey.eword = SIZE_MAX;
  gkey.ignore = NULL;
  gkey.translate = NULL;
  gkey.numeric = gkey.general_numeric = gkey.random = false;
  gkey.month = gkey.reverse = false;
  gkey.skipsblanks = gkey.skipeblanks = false;

  files = xnmalloc (argc, sizeof *files);

  for (;;)
    {
      /* Parse an operand as a file after "--" was seen; or if
	 pedantic and a file was seen, unless the POSIX version
	 predates 1003.1-2001 and -c was not seen and the operand is
	 "-o FILE" or "-oFILE".  */

      if (c == -1
	  || (posixly_correct && nfiles != 0
	      && ! (obsolete_usage
		    && ! checkonly
		    && optind != argc
		    && argv[optind][0] == '-' && argv[optind][1] == 'o'
		    && (argv[optind][2] || optind + 1 != argc)))
	  || ((c = getopt_long (argc, argv, short_options,
				long_options, NULL))
	      == -1))
	{
	  if (argc <= optind)
	    break;
	  files[nfiles++] = argv[optind++];
	}
      else switch (c)
	{
	case 1:
	  key = NULL;
	  if (optarg[0] == '+')
	    {
	      bool minus_pos_usage = (optind != argc && argv[optind][0] == '-'
				      && ISDIGIT (argv[optind][1]));
	      obsolete_usage |= minus_pos_usage & ~posixly_correct;
	      if (obsolete_usage)
		{
		  /* Treat +POS1 [-POS2] as a key if possible; but silently
		     treat an operand as a file if it is not a valid +POS1.  */
		  key = key_init (&key_buf);
		  s = parse_field_count (optarg + 1, &key->sword, NULL);
		  if (s && *s == '.')
		    s = parse_field_count (s + 1, &key->schar, NULL);
		  if (! (key->sword | key->schar))
		    key->sword = SIZE_MAX;
		  if (! s || *set_ordering (s, key, bl_start))
		    {
		      free (key);
		      key = NULL;
		    }
		  else
		    {
		      if (minus_pos_usage)
			{
			  char const *optarg1 = argv[optind++];
			  s = parse_field_count (optarg1 + 1, &key->eword,
					     N_("invalid number after `-'"));
			  if (*s == '.')
			    s = parse_field_count (s + 1, &key->echar,
					       N_("invalid number after `.'"));
			  if (*set_ordering (s, key, bl_end))
			    badfieldspec (optarg1,
				      N_("stray character in field spec"));
			}
		      insertkey (key);
		    }
		}
	    }
	  if (! key)
	    files[nfiles++] = optarg;
	  break;

	case 'b':
	case 'd':
	case 'f':
	case 'g':
	case 'i':
	case 'M':
	case 'n':
	case 'r':
	case 'R':
	  {
	    char str[2];
	    str[0] = c;
	    str[1] = '\0';
	    set_ordering (str, &gkey, bl_both);
	  }
	  break;

	case CHECK_OPTION:
	  c = (optarg
	       ? XARGMATCH ("--check", optarg, check_args, check_types)
	       : 'c');
	  /* Fall through.  */
	case 'c':
	case 'C':
	  if (checkonly && checkonly != c)
	    incompatible_options ("cC");
	  checkonly = c;
	  break;

	case COMPRESS_PROGRAM_OPTION:
	  if (compress_program && strcmp (compress_program, optarg) != 0)
	    error (SORT_FAILURE, 0, _("multiple compress programs specified"));
	  compress_program = optarg;
	  break;

	case 'k':
	  key = key_init (&key_buf);

	  /* Get POS1. */
	  s = parse_field_count (optarg, &key->sword,
				 N_("invalid number at field start"));
	  if (! key->sword--)
	    {
	      /* Provoke with `sort -k0' */
	      badfieldspec (optarg, N_("field number is zero"));
	    }
	  if (*s == '.')
	    {
	      s = parse_field_count (s + 1, &key->schar,
				     N_("invalid number after `.'"));
	      if (! key->schar--)
		{
		  /* Provoke with `sort -k1.0' */
		  badfieldspec (optarg, N_("character offset is zero"));
		}
	    }
	  if (! (key->sword | key->schar))
	    key->sword = SIZE_MAX;
	  s = set_ordering (s, key, bl_start);
	  if (*s != ',')
	    {
	      key->eword = SIZE_MAX;
	      key->echar = 0;
	    }
	  else
	    {
	      /* Get POS2. */
	      s = parse_field_count (s + 1, &key->eword,
				     N_("invalid number after `,'"));
	      if (! key->eword--)
		{
		  /* Provoke with `sort -k1,0' */
		  badfieldspec (optarg, N_("field number is zero"));
		}
	      if (*s == '.')
		s = parse_field_count (s + 1, &key->echar,
				       N_("invalid number after `.'"));
	      else
		{
		  /* `-k 2,3' is equivalent to `+1 -3'.  */
		  key->eword++;
		}
	      s = set_ordering (s, key, bl_end);
	    }
	  if (*s)
	    badfieldspec (optarg, N_("stray character in field spec"));
	  insertkey (key);
	  break;

	case 'm':
	  mergeonly = true;
	  break;

	case 'o':
	  if (outfile && !STREQ (outfile, optarg))
	    error (SORT_FAILURE, 0, _("multiple output files specified"));
	  outfile = optarg;
	  break;

	case RANDOM_SOURCE_OPTION:
	  if (random_source && !STREQ (random_source, optarg))
	    error (SORT_FAILURE, 0, _("multiple random sources specified"));
	  random_source = optarg;
	  break;

	case 's':
	  stable = true;
	  break;

	case 'S':
	  specify_sort_size (optarg);
	  break;

	case 't':
	  {
	    char newtab = optarg[0];
	    if (! newtab)
	      error (SORT_FAILURE, 0, _("empty tab"));
	    if (optarg[1])
	      {
		if (STREQ (optarg, "\\0"))
		  newtab = '\0';
		else
		  {
		    /* Provoke with `sort -txx'.  Complain about
		       "multi-character tab" instead of "multibyte tab", so
		       that the diagnostic's wording does not need to be
		       changed once multibyte characters are supported.  */
		    error (SORT_FAILURE, 0, _("multi-character tab %s"),
			   quote (optarg));
		  }
	      }
	    if (tab != TAB_DEFAULT && tab != newtab)
	      error (SORT_FAILURE, 0, _("incompatible tabs"));
	    tab = newtab;
	  }
	  break;

	case 'T':
	  add_temp_dir (optarg);
	  break;

	case 'u':
	  unique = true;
	  break;

	case 'y':
	  /* Accept and ignore e.g. -y0 for compatibility with Solaris 2.x
	     through Solaris 7.  It is also accepted by many non-Solaris
	     "sort" implementations, e.g., AIX 5.2, HP-UX 11i v2, IRIX 6.5.
	     -y is marked as obsolete starting with Solaris 8 (1999), but is
	     still accepted as of Solaris 10 prerelease (2004).

	     Solaris 2.5.1 "sort -y 100" reads the input file "100", but
	     emulate Solaris 8 and 9 "sort -y 100" which ignores the "100",
	     and which in general ignores the argument after "-y" if it
	     consists entirely of digits (it can even be empty).  */
	  if (optarg == argv[optind - 1])
	    {
	      char const *p;
	      for (p = optarg; ISDIGIT (*p); p++)
		continue;
	      optind -= (*p != '\0');
	    }
	  break;

	case 'z':
	  eolchar = 0;
	  break;

	case_GETOPT_HELP_CHAR;

	case_GETOPT_VERSION_CHAR (PROGRAM_NAME, AUTHORS);

	default:
	  usage (SORT_FAILURE);
	}
    }

  /* Inheritance of global options to individual keys. */
  for (key = keylist; key; key = key->next)
    {
      if (! (key->ignore || key->translate
             || (key->skipsblanks | key->reverse
                 | key->skipeblanks | key->month | key->numeric
                 | key->general_numeric
                 | key->random)))
        {
          key->ignore = gkey.ignore;
          key->translate = gkey.translate;
          key->skipsblanks = gkey.skipsblanks;
          key->skipeblanks = gkey.skipeblanks;
          key->month = gkey.month;
          key->numeric = gkey.numeric;
          key->general_numeric = gkey.general_numeric;
          key->random = gkey.random;
          key->reverse = gkey.reverse;
        }

      need_random |= key->random;
    }

  if (!keylist && (gkey.ignore || gkey.translate
		   || (gkey.skipsblanks | gkey.skipeblanks | gkey.month
		       | gkey.numeric | gkey.general_numeric
                       | gkey.random)))
    {
      insertkey (&gkey);
      need_random |= gkey.random;
    }

  check_ordering_compatibility ();

  reverse = gkey.reverse;

  if (need_random)
    {
      randread_source = randread_new (random_source, MD5_DIGEST_SIZE);
      if (! randread_source)
	die (_("open failed"), random_source);
    }

  if (temp_dir_count == 0)
    {
      char const *tmp_dir = getenv ("TMPDIR");
      add_temp_dir (tmp_dir ? tmp_dir : DEFAULT_TMPDIR);
    }

  if (nfiles == 0)
    {
      static char *minus = "-";
      nfiles = 1;
      free (files);
      files = &minus;
    }

  if (checkonly)
    {
      if (nfiles > 1)
	error (SORT_FAILURE, 0, _("extra operand %s not allowed with -%c"),
	       quote (files[1]), checkonly);

      if (outfile)
	{
	  static char opts[] = {0, 'o', 0};
	  opts[0] = checkonly;
	  incompatible_options (opts);
	}

      /* POSIX requires that sort return 1 IFF invoked with -c or -C and the
	 input is not properly sorted.  */
      exit (check (files[0], checkonly) ? EXIT_SUCCESS : SORT_OUT_OF_ORDER);
    }

  if (mergeonly)
    {
      struct sortfile *sortfiles = xcalloc (nfiles, sizeof *sortfiles);
      size_t i;

      for (i = 0; i < nfiles; ++i)
	sortfiles[i].name = files[i];

      merge (sortfiles, 0, nfiles, outfile);
    }
  else
    sort (files, nfiles, outfile);

  if (have_read_stdin && fclose (stdin) == EOF)
    die (_("close failed"), "-");

  exit (EXIT_SUCCESS);
}
