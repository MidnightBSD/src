#include "config.h"
#include "f2c.h"
#include "fio.h"
#include <string.h>
#ifndef NON_POSIX_STDIO
#ifdef MSDOS
#include "io.h"
#else
#include "unistd.h"		/* for access */
#endif
#endif

#undef abs
#undef min
#undef max
#include <stdlib.h>
extern int f__canseek (FILE *);
extern integer f_clos (cllist *);

#ifdef NON_ANSI_RW_MODES
char *f__r_mode[2] = { "r", "r" };
char *f__w_mode[4] = { "w", "w", "r+w", "r+w" };
#else
char *f__r_mode[2] = { "rb", "r" };
char *f__w_mode[4] = { "wb", "w", "r+b", "r+" };
#endif

static char f__buf0[400], *f__buf = f__buf0;
int f__buflen = (int) sizeof (f__buf0);

static void
f__bufadj (int n, int c)
{
  unsigned int len;
  char *nbuf, *s, *t, *te;

  if (f__buf == f__buf0)
    f__buflen = 1024;
  while (f__buflen <= n)
    f__buflen <<= 1;
  len = (unsigned int) f__buflen;
  if (len != f__buflen || !(nbuf = (char *) malloc (len)))
    f__fatal (113, "malloc failure");
  s = nbuf;
  t = f__buf;
  te = t + c;
  while (t < te)
    *s++ = *t++;
  if (f__buf != f__buf0)
    free (f__buf);
  f__buf = nbuf;
}

int
f__putbuf (int c)
{
  char *s, *se;
  int n;

  if (f__hiwater > f__recpos)
    f__recpos = f__hiwater;
  n = f__recpos + 1;
  if (n >= f__buflen)
    f__bufadj (n, f__recpos);
  s = f__buf;
  se = s + f__recpos;
  if (c)
    *se++ = c;
  *se = 0;
  for (;;)
    {
      fputs (s, f__cf);
      s += strlen (s);
      if (s >= se)
	break;			/* normally happens the first time */
      putc (*s++, f__cf);
    }
  return 0;
}

void
x_putc (int c)
{
  if (f__recpos >= f__buflen)
    f__bufadj (f__recpos, f__buflen);
  f__buf[f__recpos++] = c;
}

#define opnerr(f,m,s) \
  do {if(f) {f__init &= ~2; errno= m;} else opn_err(m,s,a); return(m);} while(0)

static void
opn_err (int m, char *s, olist * a)
{
  if (a->ofnm)
    {
      /* supply file name to error message */
      if (a->ofnmlen >= f__buflen)
	f__bufadj ((int) a->ofnmlen, 0);
      g_char (a->ofnm, a->ofnmlen, f__curunit->ufnm = f__buf);
    }
  f__fatal (m, s);
}

integer
f_open (olist * a)
{
  unit *b;
  integer rv;
  char buf[256], *s, *env;
  cllist x;
  int ufmt;
  FILE *tf;
  int fd, len;
#ifndef NON_UNIX_STDIO
  int n;
#endif
  if (f__init != 1)
    f_init ();
  f__external = 1;
  if (a->ounit >= MXUNIT || a->ounit < 0)
    err (a->oerr, 101, "open");
  f__curunit = b = &f__units[a->ounit];
  if (b->ufd)
    {
      if (a->ofnm == 0)
	{
	same:if (a->oblnk)
	    b->ublnk = *a->oblnk == 'z' || *a->oblnk == 'Z';
	  return (0);
	}
#ifdef NON_UNIX_STDIO
      if (b->ufnm
	  && strlen (b->ufnm) == a->ofnmlen
	  && !strncmp (b->ufnm, a->ofnm, (unsigned) a->ofnmlen))
	goto same;
#else
      g_char (a->ofnm, a->ofnmlen, buf);
      if (f__inode (buf, &n) == b->uinode && n == b->udev)
	goto same;
#endif
      x.cunit = a->ounit;
      x.csta = 0;
      x.cerr = a->oerr;
      if ((rv = f_clos (&x)) != 0)
	return rv;
    }
  b->url = (int) a->orl;
  b->ublnk = a->oblnk && (*a->oblnk == 'z' || *a->oblnk == 'Z');
  if (a->ofm == 0)
    if ((a->oacc) && (*a->oacc == 'D' || *a->oacc == 'd'))
      b->ufmt = 0;
    else
      b->ufmt = 1;
  else if (*a->ofm == 'f' || *a->ofm == 'F')
    b->ufmt = 1;
  else
    b->ufmt = 0;
  ufmt = b->ufmt;
#ifdef url_Adjust
  if (b->url && !ufmt)
    url_Adjust (b->url);
#endif
  if (a->ofnm)
    {
      g_char (a->ofnm, a->ofnmlen, buf);
      if (!buf[0])
	opnerr (a->oerr, 107, "open");
    }
  else
    sprintf (buf, "fort.%ld", (long) a->ounit);
  b->uscrtch = 0;
  b->uend = 0;
  b->uwrt = 0;
  b->ufd = 0;
  b->urw = 3;
  switch (a->osta ? *a->osta : 'u')
    {
    case 'o':
    case 'O':
#ifdef NON_POSIX_STDIO
      if (!(tf = fopen (buf, "r")))
	opnerr (a->oerr, errno, "open");
      fclose (tf);
#else
      if (access (buf, 0))
	opnerr (a->oerr, errno, "open");
#endif
      break;
    case 's':
    case 'S':
      b->uscrtch = 1;
#ifdef HAVE_MKSTEMP		/* Allow use of TMPDIR preferentially. */
      env = getenv ("TMPDIR");
      if (!env)
	env = getenv ("TEMP");
      if (!env)
	env = "/tmp";
      len = strlen (env);
      if (len > 256 - (int) sizeof ("/tmp.FXXXXXX"))
	err (a->oerr, 132, "open");
      strcpy (buf, env);
      strcat (buf, "/tmp.FXXXXXX");
      fd = mkstemp (buf);
      if (fd == -1 || close (fd))
	err (a->oerr, 132, "open");
#else /* ! defined (HAVE_MKSTEMP) */
#ifdef HAVE_TEMPNAM		/* Allow use of TMPDIR preferentially. */
      s = tempnam (0, buf);
      if (strlen (s) >= sizeof (buf))
	err (a->oerr, 132, "open");
      (void) strcpy (buf, s);
      free (s);
#else /* ! defined (HAVE_TEMPNAM) */
#ifdef HAVE_TMPNAM
      tmpnam (buf);
#else
      (void) strcpy (buf, "tmp.FXXXXXX");
      (void) mktemp (buf);
#endif
#endif /* ! defined (HAVE_TEMPNAM) */
#endif /* ! defined (HAVE_MKSTEMP) */
      goto replace;
    case 'n':
    case 'N':
#ifdef NON_POSIX_STDIO
      if ((tf = fopen (buf, "r")) || (tf = fopen (buf, "a")))
	{
	  fclose (tf);
	  opnerr (a->oerr, 128, "open");
	}
#else
      if (!access (buf, 0))
	opnerr (a->oerr, 128, "open");
#endif
      /* no break */
    case 'r':			/* Fortran 90 replace option */
    case 'R':
    replace:
      if ((tf = fopen (buf, f__w_mode[0])))
	fclose (tf);
    }

  b->ufnm = (char *) malloc ((unsigned int) (strlen (buf) + 1));
  if (b->ufnm == NULL)
    opnerr (a->oerr, 113, "no space");
  (void) strcpy (b->ufnm, buf);
  if ((s = a->oacc) && b->url)
    ufmt = 0;
  if (!(tf = fopen (buf, f__w_mode[ufmt | 2])))
    {
      if ((tf = fopen (buf, f__r_mode[ufmt])))
	b->urw = 1;
      else if ((tf = fopen (buf, f__w_mode[ufmt])))
	{
	  b->uwrt = 1;
	  b->urw = 2;
	}
      else
	err (a->oerr, errno, "open");
    }
  b->useek = f__canseek (b->ufd = tf);
#ifndef NON_UNIX_STDIO
  if ((b->uinode = f__inode (buf, &b->udev)) == -1)
    opnerr (a->oerr, 108, "open");
#endif
  if (b->useek)
    {
      if (a->orl)
	FSEEK (b->ufd, 0, SEEK_SET);
      else if ((s = a->oacc) && (*s == 'a' || *s == 'A')
	       && FSEEK (b->ufd, 0, SEEK_END))
	opnerr (a->oerr, 129, "open");
    }
  return (0);
}

int
fk_open (int seq, int fmt, ftnint n)
{
  char nbuf[10];
  olist a;
  int rtn;
  int save_init;

  (void) sprintf (nbuf, "fort.%ld", (long) n);
  a.oerr = 1;
  a.ounit = n;
  a.ofnm = nbuf;
  a.ofnmlen = strlen (nbuf);
  a.osta = NULL;
  a.oacc = seq == SEQ ? "s" : "d";
  a.ofm = fmt == FMT ? "f" : "u";
  a.orl = seq == DIR ? 1 : 0;
  a.oblnk = NULL;
  save_init = f__init;
  f__init &= ~2;
  rtn = f_open (&a);
  f__init = save_init | 1;
  return rtn;
}
