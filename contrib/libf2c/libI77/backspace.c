#include "config.h"
#include <sys/types.h>
#include "f2c.h"
#include "fio.h"
integer
f_back (alist * a)
{
  unit *b;
  off_t v, w, x, y, z;
  uiolen n;
  FILE *f;

  f__curunit = b = &f__units[a->aunit];	/* curunit for error messages */
  if (f__init & 2)
    f__fatal (131, "I/O recursion");
  if (a->aunit >= MXUNIT || a->aunit < 0)
    err (a->aerr, 101, "backspace");
  if (b->useek == 0)
    err (a->aerr, 106, "backspace");
  if (b->ufd == NULL)
    {
      fk_open (1, 1, a->aunit);
      return (0);
    }
  if (b->uend == 1)
    {
      b->uend = 0;
      return (0);
    }
  if (b->uwrt)
    {
      t_runc (a);
      if (f__nowreading (b))
	err (a->aerr, errno, "backspace");
    }
  f = b->ufd;			/* may have changed in t_runc() */
  if (b->url > 0)
    {
      x = FTELL (f);
      y = x % b->url;
      if (y == 0)
	x--;
      x /= b->url;
      x *= b->url;
      FSEEK (f, x, SEEK_SET);
      return (0);
    }

  if (b->ufmt == 0)
    {
      FSEEK (f, -(off_t) sizeof (uiolen), SEEK_CUR);
      fread ((char *) &n, sizeof (uiolen), 1, f);
      FSEEK (f, -(off_t) n - 2 * sizeof (uiolen), SEEK_CUR);
      return (0);
    }
  w = x = FTELL (f);
  z = 0;
loop:
  while (x)
    {
      x -= x < 64 ? x : 64;
      FSEEK (f, x, SEEK_SET);
      for (y = x; y < w; y++)
	{
	  if (getc (f) != '\n')
	    continue;
	  v = FTELL (f);
	  if (v == w)
	    {
	      if (z)
		goto break2;
	      goto loop;
	    }
	  z = v;
	}
      err (a->aerr, (EOF), "backspace");
    }
break2:
  FSEEK (f, z, SEEK_SET);
  return 0;
}
