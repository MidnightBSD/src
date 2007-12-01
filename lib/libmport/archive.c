/*-
 * Copyright (c) 2007 Chris Reinhardt
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $MidnightBSD: src/lib/libmport/inst_init.c,v 1.1 2007/11/22 08:00:32 ctriv Exp $
 */


#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <archive.h>
#include <archive_entry.h>
#include "mport.h"

__MBSDID("$MidnightBSD: src/lib/libmport/inst_init.c,v 1.1 2007/11/22 08:00:32 ctriv Exp $");


int mport_add_file_to_archive(struct archive *a, const char *filename, const char *path) 
{
  struct archive_entry *entry;
  struct stat st;
  int fd, len;
  char buff[1024*8];
  
  if (lstat(filename, &st) != 0) {
    RETURN_ERROR(MPORT_ERR_SYSCALL_FAILED, strerror(errno));
  }
  
  entry = archive_entry_new();
  archive_entry_copy_stat(entry, &st);
  archive_entry_set_pathname(entry, path);
  archive_write_header(a, entry);
  
  if ((fd = open(filename, O_RDONLY)) == -1) {
    RETURN_ERROR(MPORT_ERR_SYSCALL_FAILED, strerror(errno));
  }
    
  len = read(fd, buff, sizeof(buff));
  while (len > 0) {
    archive_write_data(a, buff, len);
    len = read(fd, buff, sizeof(buff));
  }
    
  archive_entry_free(entry);
  close(fd);

  return MPORT_OK;  
}

