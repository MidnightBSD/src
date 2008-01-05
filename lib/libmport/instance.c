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
 * $MidnightBSD: src/lib/libmport/inst_init.c,v 1.3 2007/12/05 17:02:15 ctriv Exp $
 */



#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "mport.h"

__MBSDID("$MidnightBSD: src/lib/libmport/inst_init.c,v 1.3 2007/12/05 17:02:15 ctriv Exp $");


/* allocate mem for a mportInstance */
mportInstance * mport_instance_new() 
{
 return (mportInstance *)malloc(sizeof(mportInstance)); 
}
 

/* set up the master database, and related instance infrastructure. */
int mport_instance_init(mportInstance *mport, const char *root)
{
  char dir[FILENAME_MAX];
  
  if (root != NULL) {
    mport->root = strdup(root);
  } else {
    mport->root = strdup("");
  }

  (void)snprintf(dir, FILENAME_MAX, "%s/%s", mport->root, MPORT_INST_DIR);

  if (mport_mkdir(dir) != MPORT_OK)
    RETURN_CURRENT_ERROR;
  
  (void)snprintf(dir, FILENAME_MAX, "%s/%s", mport->root, MPORT_INST_INFRA_DIR);
  
  if (mport_mkdir(dir) != MPORT_OK)
    RETURN_CURRENT_ERROR;

  /* dir is a file here, just trying to save memory */
  (void)snprintf(dir, FILENAME_MAX, "%s/%s", mport->root, MPORT_MASTER_DB_FILE);
  if (sqlite3_open(dir, &(mport->db)) != 0) {
    sqlite3_close(mport->db);
    RETURN_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(mport->db));
  }
  
  
  /* set the default UI callbacks */
  mport->msg_cb           = &mport_default_msg_cb;
  mport->progress_init_cb = &mport_default_progress_init_cb;
  mport->progress_step_cb = &mport_default_progress_step_cb;
  mport->progress_free_cb = &mport_default_progress_free_cb;
  mport->confirm_cb       = &mport_default_confirm_cb;
  

  /* create tables */
  return mport_generate_master_schema(mport->db);
}


/* Setters for the variable UI callbacks. */
void mport_set_msg_cb(mportInstance *mport, mport_msg_cb cb) 
{
  mport->msg_cb = cb;
}

void mport_set_progress_init_cb(mportInstance *mport, mport_progress_init_cb cb)
{
  mport->progress_init_cb = cb;
}

void mport_set_progress_step_cb(mportInstance *mport, mport_progress_step_cb cb)
{
  mport->progress_step_cb = cb;
}

void mport_set_progress_free_cb(mportInstance *mport, mport_progress_free_cb cb)
{
  mport->progress_free_cb = cb;
}


void mport_set_confirm_cb(mportInstance *mport, mport_confirm_cb cb) 
{
  mport->confirm_cb = cb;
}


int mport_instance_free(mportInstance *mport) 
{
  if (sqlite3_close(mport->db) != SQLITE_OK) {
    RETURN_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(mport->db));
  }
  
  free(mport->root);  
  free(mport);
  return MPORT_OK;
}

