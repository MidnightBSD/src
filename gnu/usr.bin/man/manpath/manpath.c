/*
 * manpath.c
 *
 * Copyright (c) 1990, 1991, John W. Eaton.
 *
 * You may distribute under the terms of the GNU General Public
 * License as specified in the file COPYING that comes with the man
 * distribution.
 *
 * John W. Eaton
 * jwe@che.utexas.edu
 * Department of Chemical Engineering
 * The University of Texas at Austin
 * Austin, Texas  78712
 *
 * $FreeBSD: src/gnu/usr.bin/man/manpath/manpath.c,v 1.13 2002/10/27 02:12:52 kris Exp $
 */

#define MANPATH_MAIN

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "config.h"
#include "manpath.h"
#include "gripes.h"

#ifdef STDC_HEADERS
#include <stdlib.h>
#else
extern int fprintf ();
extern int strcmp ();
extern int strncmp ();
extern char *memcpy ();
extern char *getenv();
extern char *malloc();
extern void free ();
extern int exit ();
#endif

extern char *strdup ();
extern int is_directory ();

#ifndef MAIN
extern int debug;
#endif

#ifdef MAIN

#ifndef STDC_HEADERS
extern char *strcpy ();
extern int fflush ();
#endif

char *prognam;
int debug;
int locale;
char *man_locales;

/*
 * Examine user's PATH and print a reasonable MANPATH.
 */
int
main(argc, argv)
     int argc;
     char **argv;
{
  int c;
  int quiet;
  char *mp;
  extern int getopt ();
  extern char *mkprogname ();
  void usage ();
  char *manpath ();

  quiet = 1;

  prognam = mkprogname (argv[0]);

  while ((c = getopt (argc, argv, "dhLq?")) != EOF)
    {
      switch (c)
	{
	case 'd':
	  debug++;
	  break;
	case 'L':
	  locale++;
	  break;
	case 'q':
	  quiet = 0;
	  break;
	case '?':
	case 'h':
	default:
	  usage();
          break;
	}
    }

  mp = manpath (quiet);

  fprintf (stdout, "%s\n", mp);
  fflush (stdout);

  return 0;
}

void
usage ()
{
  fprintf (stderr, "usage: %s [-dLq]\n", prognam);
  exit (1);
}
#endif /* MAIN */

/*
 * If the environment variable MANPATH is set, return it.
 * If the environment variable PATH is set and has a nonzero length,
 * try to determine the corresponding manpath, otherwise, return the
 * default manpath.
 *
 * The manpath.config file is used to map system wide /bin directories
 * to top level man page directories.
 *
 * For directories which are in the user's path but not in the
 * manpath.config file, see if there is a subdirectory `man' or `MAN'.
 * If so, add that directory to the path.  Example:  user has
 * $HOME/bin in his path and the directory $HOME/bin/man exists -- the
 * directory $HOME/bin/man will be added to the manpath.
 *
 * Also search for a `man' directory next to the directory on the path.
 * Example: $HOME/bin will look for $HOME/man
 */
char *
manpath (perrs)
     register int perrs;
{
  register int len;
  register char *manpathlist;
  register char *path;
  int  get_dirlist ();
  char *def_path ();
  char *get_manpath ();

  if (get_dirlist ())
      gripe_reading_mp_config (config_file);

#ifdef MAIN
  if (locale)
    {
      if ((manpathlist = getenv ("MANLOCALES")) != NULL)
	/*
	 * This must be it.
	 */
	{
	  if (perrs)
	    fprintf (stderr, "(Warning: MANLOCALES environment variable set)\n");
	  return strdup (manpathlist);
	}
      return (man_locales ? man_locales : "");
    }
#endif /* MAIN */

  if ((manpathlist = getenv ("MANPATH")) != NULL)
    /*
     * This must be it.
     */
    {
      if (perrs)
	fprintf (stderr, "(Warning: MANPATH environment variable set)\n");
      return strdup (manpathlist);
    }
  else if ((path = getenv ("PATH")) == NULL)
    /*
     * Things aren't going to work well, but hey...
     */
    {
      if (perrs)
	fprintf (stderr, "Warning: path not set\n");
      return def_path (perrs);
    }
  else
    {
      if ((len = strlen (path)) == 0)
	/*
	 * Things aren't going to work well here either...
	 */
	{
	  if (perrs)
	    fprintf (stderr, "Warning: path set but has zero length\n");
	  return def_path (perrs);
	}
      return get_manpath (perrs, path);
    }
}

/*
 * Get the list of bin directories and the corresponding man
 * directories from the manpath.config file.
 *
 * This is ugly.
 */
int
get_dirlist ()
{
  int i;
  char *bp;
  char *p;
  char buf[BUFSIZ];
  DIRLIST *dlp = list;
  FILE *config;

  if ((config = fopen (config_file, "r")) == NULL)
    gripe_getting_mp_config (config_file);

  while ((bp = fgets (buf, BUFSIZ, config)) != NULL)
    {
      while (*bp && (*bp == ' ' || *bp == '\t'))
	bp++;

      if (*bp == '#' || *bp == '\n')
	continue;

      if (!strncmp ("MANDATORY_MANPATH", bp, 17) ||
	  !strncmp ("OPTIONAL_MANPATH", bp, 16))
	{
	  if ((p = strchr (bp, ' ')) == NULL &&
	      (p = strchr (bp, '\t')) == NULL) {
	    fclose(config);
	    return -1;
	  }

	  dlp->type = *bp == 'M'? MANPATH_MANDATORY: MANPATH_OPTIONAL;

	  bp = p;

	  while (*bp && *bp != '\n' && (*bp == ' ' || *bp == '\t'))
	    bp++;

	  i = 0;
	  while (*bp && *bp != '\n' && *bp != ' ' && *bp != '\t')
	    dlp->mandir[i++] = *bp++;
	  dlp->mandir[i] = '\0';

	  if (debug)
	    fprintf (stderr, "found %s man directory %s\n",
		     dlp->type == MANPATH_MANDATORY? "mandatory": "optional",
		     dlp->mandir);
	}
      else if (!strncmp ("MANPATH_MAP", bp, 11))
	{
	  if ((p = strchr (bp, ' ')) == NULL &&
	      (p = strchr (bp, '\t')) == NULL) {
	    fclose(config);
	    return -1;
	  }

	  bp = p;

	  dlp->type = MANPATH_MAP;

	  while (*bp && *bp != '\n' && (*bp == ' ' || *bp == '\t'))
	    bp++;

	  i = 0;
	  while (*bp && *bp != '\n' && *bp != ' ' && *bp != '\t')
	    dlp->bin[i++] = *bp++;
	  dlp->bin[i] = '\0';

	  while (*bp && *bp != '\n' && (*bp == ' ' || *bp == '\t'))
	    bp++;

	  i = 0;
	  while (*bp && *bp != '\n' && *bp != ' ' && *bp != '\t')
	    dlp->mandir[i++] = *bp++;
	  dlp->mandir[i] = '\0';

	  if (debug)
	    fprintf (stderr, "found manpath map %s --> %s\n",
		     dlp->bin, dlp->mandir);
	}
      else if (!strncmp ("MANLOCALES", bp, 10))
	{
	  if ((p = strchr (bp, ' ')) == NULL &&
	      (p = strchr (bp, '\t')) == NULL) {
	    fclose(config);
	    return -1;
	  }

	  bp = p;

	  while (*bp && *bp != '\n' && (*bp == ' ' || *bp == '\t'))
	    bp++;

	  for (p = bp; *p && *p != '\n'; p++)
		;
	  do {
		*p-- = '\0';
	  } while (p >= bp && (*p == ' ' || *p == '\t'));

#ifdef MAIN
	  if (man_locales != NULL)
		free (man_locales);

	  if ((man_locales = strdup (bp)) == NULL) {
		fclose(config);
		return -1;
	  }
#endif  /* MAIN */

	  if (debug)
	    fprintf (stderr, "found man locales: %s\n", bp);
	}
      else
	{
	  gripe_reading_mp_config (config_file);
	}
      dlp++;
    }

  fclose(config);
  dlp->bin[0] = '\0';
  dlp->mandir[0] = '\0';
  dlp->type = MANPATH_NONE;

  return 0;
}

/*
 * Construct the default manpath.  This picks up mandatory
 * and optional (if they exist) manpaths only.
 */
char *
def_path (perrs)
     int perrs;
{
  register int len;
  register char *manpathlist, *p;
  register DIRLIST *dlp;

  len = 0;
  dlp = list;
  while (dlp->type != MANPATH_NONE) {
    if (dlp->type == MANPATH_MANDATORY || dlp->type == MANPATH_OPTIONAL)
      len += strlen (dlp->mandir) + 1;
    dlp++;
  }

  manpathlist = (char *) malloc (len);
  if (manpathlist == NULL)
    gripe_alloc (len, "manpathlist");

  *manpathlist = '\0';

  dlp = list;
  p = manpathlist;
  while (dlp->type != MANPATH_NONE) {
    if (dlp->type == MANPATH_MANDATORY || dlp->type == MANPATH_OPTIONAL) {
      int status;
      char *path = dlp->mandir;

      status = is_directory(path);

      if (status < 0 && perrs && dlp->type == MANPATH_MANDATORY)
	{
	  fprintf (stderr, "Warning: couldn't stat file %s!\n", path);
	}
      else if (status == 0 && perrs)
	{
	  fprintf (stderr, "Warning: %s isn't a directory!\n", path);
	}
      else if (status == 1)
	{
	  len = strlen (path);
	  memcpy (p, path, len);
	  p += len;
	  *p++ = ':';
	}
    }
    dlp++;
  }

  p[-1] = '\0';

  return manpathlist;
}

/*
 * For each directory in the user's path, see if it is one of the
 * directories listed in the manpath.config file.  If so, and it is
 * not already in the manpath, add it.  If the directory is not listed
 * in the manpath.config file, see if there is a subdirectory `man' or
 * `MAN'.  If so, and it is not already in the manpath, add it.
 * Example:  user has $HOME/bin in his path and the directory
 * $HOME/bin/man exists -- the directory $HOME/bin/man will be added
 * to the manpath.
 */
char *
get_manpath (perrs, path)
     register int perrs;
     register char *path;
{
  register int len;
  register char *tmppath;
  register char *t;
  register char *p;
  register char **lp;
  register char *end;
  register char *manpathlist;
  register DIRLIST *dlp;
  void add_dir_to_list ();
  char *has_subdirs ();

  tmppath = strdup (path);

  for (p = tmppath; ; p = end+1)
    {
      if ((end = strchr(p, ':')) != NULL)
	*end = '\0';

      if (debug)
	fprintf (stderr, "\npath directory %s ", p);

      if (*p != '/')
	{
	  if (debug)
	    fprintf (stderr, "is not an absolute pathname\n");

	  goto found;	/* skip. */
	}

      /*
       * The directory we're working on is in the config file.
       * If we haven't added it to the list yet, do.
       */
      for (dlp = list; dlp->mandir[0] != '\0'; dlp++)
	if (dlp->bin[0] != '\0' && !strcmp (p, dlp->bin))
	  {
	    if (debug)
	      fprintf (stderr, "is in the config file\n");

	    add_dir_to_list (tmplist, dlp->mandir, perrs);
	    goto found;
	  }

      /*
       * The directory we're working on isn't in the config file.  See
       * if it has man or MAN subdirectories.  If so, and it hasn't
       * been added to the list, do.
       */
      if (debug)
	fprintf (stderr, "is not in the config file\n");

      t = has_subdirs (p);
      if (t != NULL)
	{
	  if (debug)
	    fprintf (stderr, "but it does have a man or MAN subdirectory\n");

	  add_dir_to_list (tmplist, t, perrs);
	  free (t);
	}
      else
	{
	  if (debug)
	    fprintf (stderr, "and doesn't have man or MAN subdirectories\n");
	}

    found:

      if (!end)
	break;
    }

  if (debug)
    fprintf (stderr, "\nadding mandatory man directories\n\n");

  dlp = list;
  while (dlp->type != MANPATH_NONE) {
    if (dlp->type == MANPATH_MANDATORY || dlp->type == MANPATH_OPTIONAL)
      add_dir_to_list (tmplist, dlp->mandir,
	dlp->type == MANPATH_MANDATORY? perrs: 0);
    dlp++;
  }

  len = 0;
  lp = tmplist;
  while (*lp != NULL)
    {
      len += strlen (*lp) + 1;
      lp++;
    }

  if (!len)
    return strdup("");

  manpathlist = (char *) malloc (len);
  if (manpathlist == NULL)
    gripe_alloc (len, "manpathlist");

  *manpathlist = '\0';

  lp = tmplist;
  p = manpathlist;
  while (*lp != NULL)
    {
      len = strlen (*lp);
      memcpy (p, *lp, len);
      p += len;
      *p++ = ':';
      lp++;
    }

  p[-1] = '\0';

  return manpathlist;
}

/*
 * Add a directory to the manpath list if it isn't already there.
 */
void
add_dir_to_list (lp, dir, perrs)
     char **lp;
     char *dir;
     int perrs;
{
  extern char *strdup ();
  int status;

  while (*lp != NULL)
    {
      if (!strcmp (*lp, dir))
	{
	  if (debug)
	    fprintf (stderr, "%s is already in the manpath\n", dir);
	  return;
	}
      lp++;
    }
  /*
   * Not found -- add it.
   */
  status = is_directory(dir);

  if (status < 0 && perrs)
    {
      fprintf (stderr, "Warning: couldn't stat file %s!\n", dir);
    }
  else if (status == 0 && perrs)
    {
      fprintf (stderr, "Warning: %s isn't a directory!\n", dir);
    }
  else if (status == 1)
    {
      if (debug)
	fprintf (stderr, "adding %s to manpath\n", dir);

      *lp = strdup (dir);
    }
}

/*
 * Check to see if the current directory has man or MAN
 * subdirectories.
 */
char *
has_subdirs (p)
     register char *p;
{
  int len;
  register char *t;

  len = strlen (p);

  t = (char *) malloc ((unsigned) len + 5);
  if (t == NULL)
    gripe_alloc (len+5, "p\n");

  memcpy (t, p, len);
  strcpy (t + len, "/man");

  if (is_directory (t) == 1)
    return t;

  strcpy (t + len, "/MAN");

  if (is_directory (t) == 1)
    return t;

  /* If the path ends in `bin' then replace with `man' and see if that works. */
  if (len > 3 && strncmp(t+len-4, "/bin", 4) == 0) {
    strcpy(t+len-4, "/man");

    if (is_directory(t) == 1) 
       return t;
  }

  return NULL;
}
