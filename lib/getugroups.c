/* getugroups.c -- return a list of the groups a user is in

   Copyright (C) 1990, 1991, 1998, 1999, 2000, 2003, 2004, 2005, 2006
   Free Software Foundation.

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
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

/* Written by David MacKenzie. */

#include <config.h>

#include <sys/types.h>
#include <stdio.h> /* grp.h on alpha OSF1 V2.0 uses "FILE *". */
#include <grp.h>

#include <unistd.h>

#include <errno.h>
#ifndef EOVERFLOW
# define EOVERFLOW EINVAL
#endif

/* Some old header files might not declare setgrent, getgrent, and endgrent.
   If you don't have them at all, we can't implement this function.
   You lose!  */
struct group *getgrent ();

#include <string.h>

#define STREQ(s1, s2) ((strcmp (s1, s2) == 0))

/* Like `getgroups', but for user USERNAME instead of for the current
   process.  Store at most MAXCOUNT group IDs in the GROUPLIST array.
   If GID is not -1, store it first (if possible).  GID should be the
   group ID (pw_gid) obtained from getpwuid, in case USERNAME is not
   listed in /etc/groups.
   Always return the number of groups of which USERNAME is a member.  */

int
getugroups (int maxcount, GETGROUPS_T *grouplist, char *username, gid_t gid)
{
  struct group *grp;
  register char **cp;
  register int count = 0;

  if (gid != (gid_t) -1)
    {
      if (maxcount != 0)
	grouplist[count] = gid;
      ++count;
    }

  setgrent ();
  while ((grp = getgrent ()) != 0)
    {
      for (cp = grp->gr_mem; *cp; ++cp)
	{
	  int n;

	  if ( ! STREQ (username, *cp))
	    continue;

	  /* See if this group number is already on the list.  */
	  for (n = 0; n < count; ++n)
	    if (grouplist && grouplist[n] == grp->gr_gid)
	      break;

	  /* If it's a new group number, then try to add it to the list.  */
	  if (n == count)
	    {
	      if (maxcount != 0)
		{
		  if (count >= maxcount)
		    {
		      endgrent ();
		      return count;
		    }
		  grouplist[count] = grp->gr_gid;
		}
	      count++;
	      if (count < 0)
		{
		  errno = EOVERFLOW;
		  return -1;
		}
	    }
	}
    }
  endgrent ();

  return count;
}
