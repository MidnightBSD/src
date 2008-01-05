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
 * $MidnightBSD: src/lib/libmport/archive.c,v 1.1 2007/12/01 06:21:37 ctriv Exp $
 */


#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>
#include <archive.h>
#include <archive_entry.h>
#include "mport.h"


/* 
 * mport_new_bundle() 
 *
 * allocate a new bundle struct.  Returns null if no
 * memory could be had 
 */
mportBundle* mport_bundle_new() 
{
  return (mportBundle *)malloc(sizeof(mportBundle));
}

/*
 * mport_init_bundle(filename)
 * 
 * set up an bundle for adding files.  Sets the bundle file to
 * filename.
 */
int mport_bundle_init(mportBundle *bundle, const char *filename)
{
  if ((bundle->filename = strdup(filename)) == NULL)
    RETURN_ERROR(MPORT_ERR_NO_MEM, "Couldn't dup filename");
   
  bundle->archive = archive_write_new();
  archive_write_set_compression_bzip2(bundle->archive);
  archive_write_set_format_pax(bundle->archive);
 
  if (archive_write_open_filename(bundle->archive, bundle->filename) != ARCHIVE_OK) {
    RETURN_ERROR(MPORT_ERR_ARCHIVE, archive_error_string(bundle->archive)); 
  }
  
  return MPORT_OK;
}

/* 
 * mport_finish_bundle(bundle)
 *
 * Finish the bundle file, and then free any memory used by the mportBundle struct.
 *
 */
int mport_bundle_finish(mportBundle *bundle)
{
  int ret = MPORT_OK;
  
  if (archive_write_finish(bundle->archive) != MPORT_OK)
    ret = SET_ERROR(MPORT_ERR_ARCHIVE, archive_error_string(bundle->archive));
      
  free(bundle->filename);
  free(bundle);
  
  return ret;
}


/*
 * mport_add_file_to_bundle(bundle, filename, path)
 *
 * Add a single file to the bundle.  filename is the name of the file
 * in the system, while path is where the file should be put in the bundle.
 */
int mport_bundle_add_file(mportBundle *bundle, const char *filename, const char *path) 
{
  struct archive_entry *entry;
  struct stat st;
  int fd, len;
  char buff[1024*64];
  
  if (lstat(filename, &st) != 0) {
    RETURN_ERROR(MPORT_ERR_SYSCALL_FAILED, strerror(errno));
  }

  entry = archive_entry_new();
  archive_entry_set_pathname(entry, path);
 
  
  if (S_ISLNK(st.st_mode)) {
    /* we have us a symlink */
    int linklen;
    char linkdata[PATH_MAX];
    
    linklen = readlink(filename, linkdata, PATH_MAX);
    
    if (linklen < 0) 
      RETURN_ERROR(MPORT_ERR_SYSCALL_FAILED, strerror(errno));
      
    linkdata[linklen] = 0;
    
    archive_entry_set_symlink(entry, linkdata);
  }
  
  
  if (st.st_flags != 0) 
    archive_entry_set_fflags(entry, st.st_flags, 0);
    
  archive_entry_copy_stat(entry, &st);
  
  /* non-regular files get archived with zero size */
  if (!S_ISREG(st.st_mode)) {
    archive_entry_set_size(entry, 0);
  }
  /* make sure we can open the file before its header is put in the archive */
  else if ((fd = open(filename, O_RDONLY)) == -1) {
   RETURN_ERROR(MPORT_ERR_SYSCALL_FAILED, strerror(errno));
  }
    
  if (archive_write_header(bundle->archive, entry) != ARCHIVE_OK)
    RETURN_ERROR(MPORT_ERR_ARCHIVE, archive_error_string(bundle->archive));
  
  /* write the data to the archive if there is data to write */
  if (archive_entry_size(entry) > 0) {
    len = read(fd, buff, sizeof(buff));
    while (len > 0) {
      archive_write_data(bundle->archive, buff, len);
      len = read(fd, buff, sizeof(buff));
    }
  }
    
  archive_entry_free(entry);
  
  if (fd != 0)
    close(fd);

  return MPORT_OK;  
}

