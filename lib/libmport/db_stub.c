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
 * $MidnightBSD: src/lib/libmport/db_stub.c,v 1.1 2007/11/22 08:00:32 ctriv Exp $
 */



#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>
#include "mport.h"

__MBSDID("$MidnightBSD: src/lib/libmport/db_stub.c,v 1.1 2007/11/22 08:00:32 ctriv Exp $");

int mport_attach_stub_db(sqlite3 *db, const char *dir)
{
  char *file;
  asprintf(&file, "%s/%s", dir, MPORT_STUB_DB_FILE);
  
  if (mport_db_do(db, "ATTACH %Q AS stub", file) != MPORT_OK) { 
    RETURN_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
  }
  
  free(file);
  
  return MPORT_OK;
}

int mport_get_meta_from_db(sqlite3 *db, mportPackageMeta **ref)
{
  sqlite3_stmt *stmt;
  const char *tmp = 0;
  mportPackageMeta *pack;
  
  *ref = mport_new_packagemeta();
  
  pack = *ref;
  
  if (pack == NULL) 
    return MPORT_ERR_NO_MEM;
    
  if (sqlite3_prepare_v2(db, "SELECT pkg, version, origin, lang, prefix FROM stub.package", -1, &stmt, &tmp) != SQLITE_OK)
    RETURN_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
    
  if (sqlite3_step(stmt) != SQLITE_ROW) 
    RETURN_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
  
  /* Copy pkg to pack->name */
  if ((tmp = sqlite3_column_text(stmt, 0)) == NULL) 
    RETURN_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));

  if ((pack->name = strdup(tmp)) == NULL)
    return MPORT_ERR_NO_MEM;

  /* Copy version to pack->version */
  if ((tmp = sqlite3_column_text(stmt, 1)) == NULL) 
    RETURN_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
  
  if ((pack->version = strdup(tmp)) == NULL)
    return MPORT_ERR_NO_MEM;
  
  /* Copy origin to pack->origin */
  if ((tmp = sqlite3_column_text(stmt, 2)) == NULL) 
    RETURN_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
  
  if ((pack->origin = strdup(tmp)) == NULL)
    return MPORT_ERR_NO_MEM;

  /* Copy lang to pack->lang */
  if ((tmp = sqlite3_column_text(stmt, 3)) == NULL) 
    RETURN_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
  
  if ((pack->lang = strdup(tmp)) == NULL)
    return MPORT_ERR_NO_MEM;

  /* Copy prefix to pack->prefix */
  if ((tmp = sqlite3_column_text(stmt, 4)) == NULL) 
    RETURN_ERROR(MPORT_ERR_SQLITE, sqlite3_errmsg(db));
  
  if ((pack->prefix = strdup(tmp)) == NULL)
    return MPORT_ERR_NO_MEM;

  sqlite3_finalize(stmt);
  
  return MPORT_OK;
}

