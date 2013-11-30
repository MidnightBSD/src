/* This function serves as replacement for a missing fchownat function,
   as well as a work around for the fchownat bug in glibc-2.4:
    <http://lists.ubuntu.com/archives/ubuntu-users/2006-September/093218.html>
   when the buggy fchownat-with-AT_SYMLINK_NOFOLLOW operates on a symlink, it
   mistakenly affects the symlink referent, rather than the symlink itself.

   Copyright (C) 2006-2007 Free Software Foundation, Inc.

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

/* written by Jim Meyering */

#include <config.h>

#include "openat.h"

#include <unistd.h>

#include "dirname.h" /* solely for definition of IS_ABSOLUTE_FILE_NAME */
#include "lchown.h"
#include "save-cwd.h"
#include "openat-priv.h"

/* Replacement for Solaris' function by the same name.
   Invoke chown or lchown on file, FILE, using OWNER and GROUP, in the
   directory open on descriptor FD.  If FLAG is AT_SYMLINK_NOFOLLOW, then
   use lchown, otherwise, use chown.  If possible, do it without changing
   the working directory.  Otherwise, resort to using save_cwd/fchdir,
   then mkdir/restore_cwd.  If either the save_cwd or the restore_cwd
   fails, then give a diagnostic and exit nonzero.  */

#define AT_FUNC_NAME fchownat
#define AT_FUNC_F1 lchown
#define AT_FUNC_F2 chown
#define AT_FUNC_USE_F1_COND flag == AT_SYMLINK_NOFOLLOW
#define AT_FUNC_POST_FILE_PARAM_DECLS , uid_t owner, gid_t group, int flag
#define AT_FUNC_POST_FILE_ARGS        , owner, group
#include "at-func.c"
