/*-
 * Copyright (c) 2007,2008 Chris Reinhardt
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
 * $MidnightBSD: src/lib/libmport/bundle.c,v 1.1 2008/01/05 22:18:20 ctriv Exp $
 */

/* Portions of this code (the hardlink handling) were inspired by and/or copied 
 * from write.c in bsdtar by Tim Kientzle. Copyright (c) 2003-2007 Tim Kientzle.
 * 
 * The code referenced above was released under the same 2-clause BSD license as
 * libmport.
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

#define	LINK_TABLE_SIZE 512

struct links_table {
  size_t nbuckets;
  size_t nentries;
  struct link_node **buckets;
};

struct link_node {
  int links;
  dev_t dev;
  ino_t ino;
  struct link_node *next;
  struct link_node *previous;
  char *name;
};


static int lookup_hardlink(mportBundle *, struct archive_entry *, const struct stat *);
static void free_linktable(struct links_table *);

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

  bundle->links = NULL; 

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
  
  if (archive_write_finish(bundle->archive) != ARCHIVE_OK)
    ret = SET_ERROR(MPORT_ERR_ARCHIVE, archive_error_string(bundle->archive));

  free_linktable(bundle->links);      
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
 
  if (!S_ISDIR(st.st_mode) && (st.st_nlink > 1))
    if (lookup_hardlink(bundle, entry, &st) != MPORT_OK)
      RETURN_CURRENT_ERROR;
  
  if (S_ISLNK(st.st_mode)) {
    /* we have us a symlink */
    int linklen;
    char linkdata[PATH_MAX + 1];
    
    linklen = readlink(filename, linkdata, PATH_MAX);
    
    if (linklen < 0) 
      RETURN_ERROR(MPORT_ERR_SYSCALL_FAILED, strerror(errno));
      
    linkdata[linklen] = '\0';
    
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

static int lookup_hardlink(mportBundle *bundle, struct archive_entry *entry, const struct stat *st)
{
  struct links_table *links = bundle->links;
  struct link_node *node, **new_buckets;
  int hash;
  size_t i, new_size;

  if (links == NULL) {
    if ((bundle->links = calloc(1, sizeof(struct links_table))) == NULL)
      RETURN_ERROR(MPORT_ERR_NO_MEM, "Couldn't allocate links table");
    
    links = bundle->links;
    links->nbuckets = LINK_TABLE_SIZE;
    links->nentries = 0;
    links->buckets  = calloc(links->nbuckets, sizeof(links->buckets[0]));
    
    if (links->buckets == NULL) {
      RETURN_ERROR(MPORT_ERR_NO_MEM, "Couldn't allocate links table mapping");
    }
  }

  if (links->buckets == NULL) {
    return MPORT_OK; /* just to be safe */ 
  }

  /* if the number of entires is twice the number of buckets, increase the table size */
  if (links->nentries > (links->nbuckets * 2)) {
    new_size = links->nbuckets * 2;
    new_buckets = (struct link_node **)calloc(new_size, sizeof(struct link_node *));
    
    if (new_buckets != NULL) {
      for (i=0; i < links->nbuckets; i++) {
        while (links->buckets[i] != NULL) {
          /* remove old from bucket */
          node = links->buckets[i];
          links->buckets[i] = node;
                
          hash = (node->dev ^ node->ino) % new_size;
          if (new_buckets[hash] != NULL)
            new_buckets[hash]->previous = node;    
          
          node->next = new_buckets[hash];
          node->previous = NULL;
          new_buckets[hash] = node;
        }
      }
      free(links->buckets);
      links->buckets  = new_buckets;
      links->nbuckets = new_size;
    } else {
      RETURN_ERROR(MPORT_ERR_NO_MEM, "Couldn't expand hard links hash table.");
    }
  }
   

  /* ok, we're done with maintence of the hash table, we can now go on to trying
     to find the location of this entry */ 
  hash = (st->st_dev ^ st->st_ino) % links->nbuckets;
   
  for (node = links->buckets[hash]; node != NULL; node = node->next) {
    if (node->dev == st->st_dev && node->ino == st->st_ino) {
      /* we found it!  we're out of here */
      archive_entry_copy_hardlink(entry, node->name);
      
      node->links--;
      
      /* no reason to keep this in the table, we've archived all the links */
      if (node->links <= 0) {
        if (node->previous != NULL)
          node->previous->next = node->next;
        if (node->next != NULL)
          node->next->previous = node->previous;
        if (links->buckets[hash] == node)
          links->buckets[hash] = node->next;
        if (node->name != NULL)
          free(node->name);
        links->nentries--;
        free(node);
      }
          
      return MPORT_OK;    
    }
  }
 
   /* we didn't find any match to this file, so this is the first time we've seen
      it.  Put it in the table */
  node = calloc(1, sizeof(struct link_node));
  if (node != NULL)
    node->name = strdup(archive_entry_pathname(entry));
  if ((node == NULL) || (node->name == NULL))
    RETURN_ERROR(MPORT_ERR_NO_MEM, "Couldn't add file to the links hashtable.");
  
  if (links->buckets[hash] != NULL)
    links->buckets[hash]->previous = node;
  
  links->nentries++;
  node->next = links->buckets[hash];
  node->previous = NULL;
  node->dev   = st->st_dev;
  node->ino   = st->st_ino;
  node->links = st->st_nlink - 1;
  
  links->buckets[hash] = node;
  
  return MPORT_OK;
}


static void free_linktable(struct links_table *links)
{
  size_t i;
  struct link_node *node;
  
  if ((links == NULL) || (links->buckets == NULL))
    return;

  for (i = 0; i < links->nbuckets; i++) {
    while (links->buckets[i] != NULL) {
      node = links->buckets[i];
      links->buckets[i] = node->next;
      
      free(node->name);
      
      if (node->name != NULL)
        free(node->name);
      
      free(node);
    }
  }
  
  free(links->buckets);
  links->buckets = NULL;
}

