/* copypass.c - cpio copy pass sub-function.
   Copyright (C) 1990, 1991, 1992, 2001, 2003, 2004,
   2006 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the Free
   Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301 USA.  */

#include <system.h>

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "filetypes.h"
#include "cpiohdr.h"
#include "dstring.h"
#include "extern.h"
#include "paxlib.h"

#ifndef HAVE_LCHOWN
# define lchown chown
#endif


/* A wrapper around set_perms using another set of arguments */
static void
set_copypass_perms (int fd, const char *name, struct stat *st)
{
  struct cpio_file_stat header;
  header.c_name = name;
  stat_to_cpio (&header, st);
  set_perms (fd, &header);
}

/* Copy files listed on the standard input into directory `directory_name'.
   If `link_flag', link instead of copying.  */

void
process_copy_pass ()
{
  dynamic_string input_name;	/* Name of file from stdin.  */
  dynamic_string output_name;	/* Name of new file.  */
  int dirname_len;		/* Length of `directory_name'.  */
  int res;			/* Result of functions.  */
  char *slash;			/* For moving past slashes in input name.  */
  struct stat in_file_stat;	/* Stat record for input file.  */
  struct stat out_file_stat;	/* Stat record for output file.  */
  int in_file_des;		/* Input file descriptor.  */
  int out_file_des;		/* Output file descriptor.  */
  int existing_dir;		/* True if file is a dir & already exists.  */
#ifdef HPUX_CDF
  int cdf_flag;
  int cdf_char;
#endif

  umask (0);                    /* Reset umask to preserve modes of
				   created files  */

  /* Initialize the copy pass.  */
  dirname_len = strlen (directory_name);
  ds_init (&input_name, 128);
  ds_init (&output_name, dirname_len + 2);
  strcpy (output_name.ds_string, directory_name);
  output_name.ds_string[dirname_len] = '/';
  output_is_seekable = true;

  /* Copy files with names read from stdin.  */
  while (ds_fgetstr (stdin, &input_name, name_end) != NULL)
    {
      int link_res = -1;

      /* Check for blank line and ignore it if found.  */
      if (input_name.ds_string[0] == '\0')
	{
	  error (0, 0, _("blank line ignored"));
	  continue;
	}

      /* Check for current directory and ignore it if found.  */
      if (input_name.ds_string[0] == '.'
	  && (input_name.ds_string[1] == '\0'
	      || (input_name.ds_string[1] == '/'
		  && input_name.ds_string[2] == '\0')))
	continue;

      if ((*xstat) (input_name.ds_string, &in_file_stat) < 0)
	{
	  stat_error (input_name.ds_string);
	  continue;
	}

      /* Make the name of the new file.  */
      for (slash = input_name.ds_string; *slash == '/'; ++slash)
	;
#ifdef HPUX_CDF
      /* For CDF's we add a 2nd `/' after all "hidden" directories.
	 This kind of a kludge, but it's what we do when creating
	 archives, and it's easier to do this than to separately
	 keep track of which directories in a path are "hidden".  */
      slash = add_cdf_double_slashes (slash);
#endif
      ds_resize (&output_name, dirname_len + strlen (slash) + 2);
      strcpy (output_name.ds_string + dirname_len + 1, slash);

      existing_dir = false;
      if (lstat (output_name.ds_string, &out_file_stat) == 0)
	{
	  if (S_ISDIR (out_file_stat.st_mode)
	      && S_ISDIR (in_file_stat.st_mode))
	    {
	      /* If there is already a directory there that
		 we are trying to create, don't complain about it.  */
	      existing_dir = true;
	    }
	  else if (!unconditional_flag
		   && in_file_stat.st_mtime <= out_file_stat.st_mtime)
	    {
	      error (0, 0, _("%s not created: newer or same age version exists"),
		     output_name.ds_string);
	      continue;		/* Go to the next file.  */
	    }
	  else if (S_ISDIR (out_file_stat.st_mode)
			? rmdir (output_name.ds_string)
			: unlink (output_name.ds_string))
	    {
	      error (0, errno, _("cannot remove current %s"),
		     output_name.ds_string);
	      continue;		/* Go to the next file.  */
	    }
	}

      /* Do the real copy or link.  */
      if (S_ISREG (in_file_stat.st_mode))
	{
	  /* Can the current file be linked to a another file?
	     Set link_name to the original file name.  */
	  if (link_flag)
	    /* User said to link it if possible.  Try and link to
	       the original copy.  If that fails we'll still try
	       and link to a copy we've already made.  */
	    link_res = link_to_name (output_name.ds_string, 
				     input_name.ds_string);
	  if ( (link_res < 0) && (in_file_stat.st_nlink > 1) )
	    link_res = link_to_maj_min_ino (output_name.ds_string, 
				major (in_file_stat.st_dev), 
				minor (in_file_stat.st_dev), 
				in_file_stat.st_ino);

	  /* If the file was not linked, copy contents of file.  */
	  if (link_res < 0)
	    {
	      in_file_des = open (input_name.ds_string,
				  O_RDONLY | O_BINARY, 0);
	      if (in_file_des < 0)
		{
		  open_error (input_name.ds_string);
		  continue;
		}
	      out_file_des = open (output_name.ds_string,
				   O_CREAT | O_WRONLY | O_BINARY, 0600);
	      if (out_file_des < 0 && create_dir_flag)
		{
		  create_all_directories (output_name.ds_string);
		  out_file_des = open (output_name.ds_string,
				       O_CREAT | O_WRONLY | O_BINARY, 0600);
		}
	      if (out_file_des < 0)
		{
		  open_error (output_name.ds_string);
		  close (in_file_des);
		  continue;
		}

	      copy_files_disk_to_disk (in_file_des, out_file_des, in_file_stat.st_size, input_name.ds_string);
	      disk_empty_output_buffer (out_file_des);
	      /* Debian hack to fix a bug in the --sparse option.
                 This bug has been reported to
                 "bug-gnu-utils@prep.ai.mit.edu".  (96/7/10) -BEM */
	      if (delayed_seek_count > 0)
		{
		  lseek (out_file_des, delayed_seek_count-1, SEEK_CUR);
		  write (out_file_des, "", 1);
		  delayed_seek_count = 0;
		}

	      set_copypass_perms (out_file_des,
				  output_name.ds_string, &in_file_stat);

	      if (reset_time_flag)
                {
                  set_file_times (in_file_des,
				  input_name.ds_string,
                                  in_file_stat.st_atime,
                                  in_file_stat.st_mtime);
                  set_file_times (out_file_des,
				  output_name.ds_string,
                                  in_file_stat.st_atime,
                                  in_file_stat.st_mtime);
	        } 

	      if (close (in_file_des) < 0)
		close_error (input_name.ds_string);

	      if (close (out_file_des) < 0)
		close_error (output_name.ds_string);

	      warn_if_file_changed(input_name.ds_string, in_file_stat.st_size,
                                   in_file_stat.st_mtime);
	    }
	}
      else if (S_ISDIR (in_file_stat.st_mode))
	{
#ifdef HPUX_CDF
	  cdf_flag = 0;
#endif
	  if (!existing_dir)
	    {
#ifdef HPUX_CDF
	      /* If the directory name ends in a + and is SUID,
		 then it is a CDF.  Strip the trailing + from the name
		 before creating it.  */
	      cdf_char = strlen (output_name.ds_string) - 1;
	      if ( (cdf_char > 0) &&
		   (in_file_stat.st_mode & 04000) &&
		   (output_name.ds_string [cdf_char] == '+') )
		{
		  output_name.ds_string [cdf_char] = '\0';
		  cdf_flag = 1;
		}
#endif
	      res = mkdir (output_name.ds_string, in_file_stat.st_mode);

	    }
	  else
	    res = 0;
	  if (res < 0 && create_dir_flag)
	    {
	      create_all_directories (output_name.ds_string);
	      res = mkdir (output_name.ds_string, in_file_stat.st_mode);
	    }
	  if (res < 0)
	    {
	      /* In some odd cases where the output_name includes `.',
	         the directory may have actually been created by
	         create_all_directories(), so the mkdir will fail
	         because the directory exists.  If that's the case,
	         don't complain about it.  */
	      if ( (errno != EEXIST) ||
      		   (lstat (output_name.ds_string, &out_file_stat) != 0) ||
	           !(S_ISDIR (out_file_stat.st_mode) ) )
		{
		  stat_error (output_name.ds_string);
		  continue;
		}
	    }
	  set_copypass_perms (-1, output_name.ds_string, &in_file_stat);
	}
      else if (S_ISCHR (in_file_stat.st_mode) ||
	       S_ISBLK (in_file_stat.st_mode) ||
#ifdef S_ISFIFO
	       S_ISFIFO (in_file_stat.st_mode) ||
#endif
#ifdef S_ISSOCK
	       S_ISSOCK (in_file_stat.st_mode) ||
#endif
	       0)
	{
	  /* Can the current file be linked to a another file?
	     Set link_name to the original file name.  */
	  if (link_flag)
	    /* User said to link it if possible.  */
	    link_res = link_to_name (output_name.ds_string, 
				     input_name.ds_string);
	  if ( (link_res < 0) && (in_file_stat.st_nlink > 1) )
	    link_res = link_to_maj_min_ino (output_name.ds_string, 
			major (in_file_stat.st_dev),
			minor (in_file_stat.st_dev),
			in_file_stat.st_ino);

	  if (link_res < 0)
	    {
	      res = mknod (output_name.ds_string, in_file_stat.st_mode,
			   in_file_stat.st_rdev);
	      if (res < 0 && create_dir_flag)
		{
		  create_all_directories (output_name.ds_string);
		  res = mknod (output_name.ds_string, in_file_stat.st_mode,
			       in_file_stat.st_rdev);
		}
	      if (res < 0)
		{
		  mknod_error (output_name.ds_string);
		  continue;
		}
	      set_copypass_perms (-1, output_name.ds_string, &in_file_stat);
	    }
	}

#ifdef S_ISLNK
      else if (S_ISLNK (in_file_stat.st_mode))
	{
	  char *link_name;
	  int link_size;
	  link_name = (char *) xmalloc ((unsigned int) in_file_stat.st_size + 1);

	  link_size = readlink (input_name.ds_string, link_name,
			        in_file_stat.st_size);
	  if (link_size < 0)
	    {
	      readlink_error (input_name.ds_string);
	      free (link_name);
	      continue;
	    }
	  link_name[link_size] = '\0';

	  res = UMASKED_SYMLINK (link_name, output_name.ds_string,
				 in_file_stat.st_mode);
	  if (res < 0 && create_dir_flag)
	    {
	      create_all_directories (output_name.ds_string);
	      res = UMASKED_SYMLINK (link_name, output_name.ds_string,
				     in_file_stat.st_mode);
	    }
	  if (res < 0)
	    {
	      symlink_error (output_name.ds_string, link_name);
	      free (link_name);
	      continue;
	    }

	  /* Set the attributes of the new link.  */
	  if (!no_chown_flag)
            {
              uid_t uid = set_owner_flag ? set_owner : in_file_stat.st_uid;
              gid_t gid = set_group_flag ? set_group : in_file_stat.st_gid;
	      if ((lchown (output_name.ds_string, uid, gid) < 0)
		  && errno != EPERM)
	        chown_error_details (output_name.ds_string, uid, gid);
            }
	  free (link_name);
	}
#endif
      else
	{
	  error (0, 0, _("%s: unknown file type"), input_name.ds_string);
	}

      if (verbose_flag)
	fprintf (stderr, "%s\n", output_name.ds_string);
      if (dot_flag)
	fputc ('.', stderr);
    }

  if (dot_flag)
    fputc ('\n', stderr);
  if (!quiet_flag)
    {
      res = (output_bytes + io_block_size - 1) / io_block_size;
      fprintf (stderr, ngettext ("%d block\n", "%d blocks\n", res), res);
    }
}

/* Try and create a hard link from FILE_NAME to another file 
   with the given major/minor device number and inode.  If no other
   file with the same major/minor/inode numbers is known, add this file
   to the list of known files and associated major/minor/inode numbers
   and return -1.  If another file with the same major/minor/inode
   numbers is found, try and create another link to it using
   link_to_name, and return 0 for success and -1 for failure.  */

int
link_to_maj_min_ino (char *file_name, int st_dev_maj, int st_dev_min,
		     int st_ino)
{
  int	link_res;
  char *link_name;
  link_res = -1;
  /* Is the file a link to a previously copied file?  */
  link_name = find_inode_file (st_ino,
			       st_dev_maj,
			       st_dev_min);
  if (link_name == NULL)
    add_inode (st_ino, file_name,
	       st_dev_maj,
	       st_dev_min);
  else
    link_res = link_to_name (file_name, link_name);
  return link_res;
}

/* Try and create a hard link from LINK_NAME to LINK_TARGET.  If
   `create_dir_flag' is set, any non-existent (parent) directories 
   needed by LINK_NAME will be created.  If the link is successfully
   created and `verbose_flag' is set, print "LINK_TARGET linked to LINK_NAME\n".
   If the link can not be created and `link_flag' is set, print
   "cannot link LINK_TARGET to LINK_NAME\n".  Return 0 if the link
   is created, -1 otherwise.  */

int
link_to_name (char *link_name, char *link_target)
{
  int res = link (link_target, link_name);
  if (res < 0 && create_dir_flag)
    {
      create_all_directories (link_name);
      res = link (link_target, link_name);
    }
  if (res == 0)
    {
      if (verbose_flag)
	error (0, 0, _("%s linked to %s"),
	       link_target, link_name);
    }
  else if (link_flag)
    {
      error (0, errno, _("cannot link %s to %s"),
	     link_target, link_name);
    }
  return res;
}
