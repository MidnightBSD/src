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
 * $MidnightBSD: src/lib/libmport/db_schema.c,v 1.3 2007/09/28 03:01:31 ctriv Exp $
 */



#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <sqlite3.h>
#include "mport.h"

__MBSDID("$MidnightBSD: src/lib/libmport/db_schema.c,v 1.3 2007/09/28 03:01:31 ctriv Exp $");


static int create_master_db(sqlite3 **);

/* set up the master database, and related instance infrastructure. */
int mport_inst_init(sqlite3 **db)
{
  if (mkdir(MPORT_INST_DIR, S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH) != 0) {
    if (errno != EEXIST) 
      RETURN_ERROR(MPORT_ERR_SYSCALL_FAILED, strerror(errno));
  }
  
  return create_master_db(db);
}
  
  
static int create_master_db(sqlite3 **db) 
{
  if (sqlite3_open(MPORT_MASTER_DB_FILE, db) != 0) {
    sqlite3_close(*db);
    RETURN_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(*db));
  }
  
  /* create tables */
  return mport_generate_master_schema(*db);
}
