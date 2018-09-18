/*-
 * Copyright (c) 2010 Lucas Holt
 * Copyright (c) 2007-2009 Chris Reinhardt
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
 */

#include <sys/cdefs.h>
__MBSDID("$MidnightBSD$");

#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <errno.h>
#include "mport.h"
#include "mport_private.h"

static int populate_meta_from_stmt(mportPackageMeta *, sqlite3 *, sqlite3_stmt *);
static int populate_vec_from_stmt(mportPackageMeta ***, int, sqlite3 *, sqlite3_stmt *);


/* Package meta-data creation and destruction */
MPORT_PUBLIC_API mportPackageMeta* 
mport_pkgmeta_new(void) 
{
  /* we use calloc so any pointers that aren't set are NULL.
     (calloc zero's out the memory region. */
    return (mportPackageMeta *) calloc(1, sizeof(mportPackageMeta));
}

MPORT_PUBLIC_API void
mport_pkgmeta_free(mportPackageMeta *pack)
{
	int i;

	free(pack->name);
	pack->name = NULL;
	free(pack->version);
	pack->version = NULL;
	free(pack->lang);
	pack->lang = NULL;
	free(pack->comment);
	pack->comment = NULL;
	free(pack->desc);
	pack->desc = NULL;
	free(pack->prefix);
	pack->prefix = NULL;
	free(pack->origin);
	pack->origin = NULL;
	free(pack->os_release);
	pack->os_release = NULL;
	free(pack->cpe);
	pack->cpe = NULL;
	free(pack->deprecated);
	pack->deprecated = NULL;
	free(pack->flavor);
	pack->flavor = NULL;

	i = 0;
	if (pack->categories != NULL) {
		while (pack->categories[i] != NULL) {
			free(pack->categories[i]);
			pack->categories[i] = NULL;
			i++;
		}
	}

	free(pack->categories);
	free(pack);
}

/* free a vector of mportPackageMeta pointers */
MPORT_PUBLIC_API void
mport_pkgmeta_vec_free(mportPackageMeta **vec)
{
    int i;
    for (i = 0; *(vec + i) != NULL; i++) {
        mport_pkgmeta_free(*(vec + i));
    }

    free(vec);
}


/* mport_pkgmeta_read_stub(mportInstance *mport, mportPackageMeta ***pack)
 *
 * Allocates and populates a vector of mportPackageMeta structs from the stub database
 * connected to db. These structs represent all the packages in the stub database.
 * This does not populate the conflicts and depends fields.
 */
int
mport_pkgmeta_read_stub(mportInstance *mport, mportPackageMeta ***ref)
{
    sqlite3_stmt *stmt;
    sqlite3 *db = mport->db;
    int len, ret;

    if (mport_db_prepare(db, &stmt, "SELECT COUNT(*) FROM stub.packages") != MPORT_OK)
        RETURN_CURRENT_ERROR;

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
    }

    len = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    if (len == 0) {
        /* a stub should have packages! */
        RETURN_ERROR(MPORT_ERR_FATAL, "stub database contains no packages.");
    }

    if (mport_db_prepare(db, &stmt,
                         "SELECT pkg, version, origin, lang, prefix, comment, os_release, cpe, deprecated, expiration_date, no_provide_shlib, flavor FROM stub.packages") !=
        MPORT_OK) {
        sqlite3_finalize(stmt);
        RETURN_CURRENT_ERROR;
    }

    ret = populate_vec_from_stmt(ref, len, db, stmt);

    sqlite3_finalize(stmt);

    return ret;
}


 
 


/* mport_pkgmeta_search_master(mportInstance *mport, mportPacakgeMeta ***pack, const char *where, ...)
 *
 * Allocate and populate the package meta for the given package from the
 * master database.
 * 
 * 'where' and the vargs are used to be build a where clause.  For example to search by
 * name:
 * 
 * mport_pkgmeta_search_master(mport, &packvec, "pkg=%Q", name);
 *
 * or by origin
 *
 * mport_pkgmeta_search_master(mport, &packvec, "origin=%Q", origin);
 *
 * pack is set to NULL and MPORT_OK is returned if no packages where found.
 */
MPORT_PUBLIC_API int
mport_pkgmeta_search_master(mportInstance *mport, mportPackageMeta ***ref, const char *fmt, ...)
{
    va_list args;
    sqlite3_stmt *stmt;
    int ret, len;
    char *where;
    sqlite3 *db = mport->db;

    va_start(args, fmt);
    where = sqlite3_vmprintf(fmt, args);
    va_end(args);

    if (where == NULL)
        RETURN_ERROR(MPORT_ERR_FATAL, "Could not build where clause");

    if (mport_db_prepare(db, &stmt, "SELECT count(*) FROM packages WHERE %s", where) != MPORT_OK) {
        sqlite3_finalize(stmt);
        RETURN_CURRENT_ERROR;
    }

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
    }


    len = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    if (len == 0) {
        sqlite3_free(where);
        *ref = NULL;
        return MPORT_OK;
    }

    if (mport_db_prepare(db, &stmt,
                         "SELECT pkg, version, origin, lang, prefix, comment, os_release, cpe, locked, deprecated, expiration_date, no_provide_shlib, flavor FROM packages WHERE %s",
                         where) != MPORT_OK) {
        sqlite3_finalize(stmt);
        RETURN_CURRENT_ERROR;
    }

    ret = populate_vec_from_stmt(ref, len, db, stmt);

    sqlite3_free(where);
    sqlite3_finalize(stmt);

    return ret;
}


/* int mport_pkgmeta_list(mportInstance *mport, mportPackageMeta ***ref)
 *
 * List all packages currently installed
 * 
 * pack is set to NULL and MPORT_OK is returned if no packages where found.
 */
MPORT_PUBLIC_API int
mport_pkgmeta_list(mportInstance *mport, mportPackageMeta ***ref)
{
    sqlite3_stmt *stmt;
    int ret, len;
    sqlite3 *db = mport->db;

    if (mport_db_prepare(db, &stmt, "SELECT count(*) FROM packages") != MPORT_OK) {
        sqlite3_finalize(stmt);
        RETURN_CURRENT_ERROR;
    }

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
    }

    len = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    if (len == 0) {
        *ref = NULL;
        return MPORT_OK;
    }

    if (mport_db_prepare(db, &stmt,
                         "SELECT pkg, version, origin, lang, prefix, comment, os_release, cpe, locked, deprecated, expiration_date, no_provide_shlib, flavor FROM packages ORDER BY pkg, version") !=
        MPORT_OK) {
        sqlite3_finalize(stmt);
        RETURN_CURRENT_ERROR;
    }

    ret = populate_vec_from_stmt(ref, len, db, stmt);

    sqlite3_finalize(stmt);

    return ret;
}



/* mport_pkgmeta_get_downdepends(mportInstance *mport, mportPackageMeta *pkg, mportPackageMeta ***pkg_vec)
 * 
 * Populate the depends of a pkg using the data in the master database.  
 */
MPORT_PUBLIC_API int
mport_pkgmeta_get_downdepends(mportInstance *mport, mportPackageMeta *pkg, mportPackageMeta ***pkg_vec_p)
{
  int count = 0;
  int ret;
  sqlite3_stmt *stmt;
  
  /* if the depends are set, there's nothing for us to do */
  if (mport_db_prepare(mport->db, &stmt, "SELECT COUNT(*) FROM depends WHERE pkg=%Q", pkg->name) != MPORT_OK) {
    sqlite3_finalize(stmt);
    RETURN_CURRENT_ERROR;
  }
    
  if (sqlite3_step(stmt) != SQLITE_ROW) {
    sqlite3_finalize(stmt);
    RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(mport->db));
  }
  
  count = sqlite3_column_int(stmt, 0);
  sqlite3_finalize(stmt);
  
  if (count == 0) {
    *pkg_vec_p = NULL;
    return MPORT_OK;  
  }

  if (mport_db_prepare(mport->db, &stmt, 
      "SELECT packages.pkg, packages.version, packages.origin, packages.lang, packages.prefix, packages.comment, packages.os_release, packages.cpe, packages.locked, packages.deprecated, packages.expiration_date, packages.no_provide_shlib, packages.flavor FROM packages,depends WHERE packages.pkg=depends.depend_pkgname AND depends.pkg=%Q", 
      pkg->name) != MPORT_OK) {
    sqlite3_finalize(stmt);
    RETURN_CURRENT_ERROR;
  }

  ret = populate_vec_from_stmt(pkg_vec_p, count, mport->db, stmt);
 
  sqlite3_finalize(stmt);
  return ret; 
}  



/* mport_pkgmeta_get_updepends(mportInstance *mport, mportPackageMeta *pkg, mportPackageMeta ***pkg_vec)
 * 
 * Populate the upwards depends of a pkg using the data in the master database.  
 */
MPORT_PUBLIC_API int mport_pkgmeta_get_updepends(mportInstance *mport, mportPackageMeta *pkg, mportPackageMeta ***pkg_vec_p)
{
  int count = 0;
  int ret;
  sqlite3_stmt *stmt;
  
  /* if the depends are set, there's nothing for us to do */
  if (mport_db_prepare(mport->db, &stmt, "SELECT COUNT(*) FROM depends WHERE depend_pkgname=%Q", pkg->name) != MPORT_OK) {
    sqlite3_finalize(stmt);
    RETURN_CURRENT_ERROR;
  }
    
  if (sqlite3_step(stmt) != SQLITE_ROW) {
    sqlite3_finalize(stmt);
    RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(mport->db));
  }
  
  count = sqlite3_column_int(stmt, 0);
  sqlite3_finalize(stmt);
  
  if (count == 0) {
    *pkg_vec_p = NULL;
    return MPORT_OK;  
  }

  if (mport_db_prepare(mport->db, &stmt, 
      "SELECT packages.pkg, packages.version, packages.origin, packages.lang, packages.prefix, packages.comment, packages.locked, packages.deprecated, packages.expiration_date, packages.no_provide_shlib, packages.flavor FROM packages,depends WHERE packages.pkg=depends.pkg AND depends.depend_pkgname=%Q", 
                       pkg->name) != MPORT_OK) {
    sqlite3_finalize(stmt);
    RETURN_CURRENT_ERROR;
  }

  ret = populate_vec_from_stmt(pkg_vec_p, count, mport->db, stmt);
 
  sqlite3_finalize(stmt);
  return ret; 
}


/* mport_pkgmeta_logevent(mport, pkg, "Hi there!");
 *
 * Create an entry in the log table for this pkg (and version), using the given message.
 */
int
mport_pkgmeta_logevent(mportInstance *mport, mportPackageMeta *pkg, const char *msg) 
{
	struct timespec now;
  
	if (clock_gettime(CLOCK_REALTIME, &now) != 0) {
		RETURN_ERROR(MPORT_ERR_FATAL, strerror(errno));
	}

	if (pkg == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "pkg is null");

	if (pkg->name == NULL || pkg->version == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "pkg is not initialized");

	if (msg == NULL)
		RETURN_ERROR(MPORT_ERR_WARN, "null message to log");
          
	return mport_db_do(mport->db, 
	  "INSERT INTO log (pkg, version, date, msg) VALUES (%s,%s,%i,%s)", 
		pkg->name, pkg->version, now.tv_sec, msg);
}


static int populate_vec_from_stmt(mportPackageMeta ***ref, int len, sqlite3 *db, sqlite3_stmt *stmt)
{
	mportPackageMeta **vec;
	int done = 0;
	vec = (mportPackageMeta **) calloc((1 + len), sizeof(mportPackageMeta *));
	*ref = vec;

	while (!done) {
		switch (sqlite3_step(stmt)) {
			case SQLITE_ROW:
				*vec = mport_pkgmeta_new();
				if (*vec == NULL)
					RETURN_ERROR(MPORT_ERR_FATAL, "Couldn't allocate meta.");
				if (populate_meta_from_stmt(*vec, db, stmt) != MPORT_OK)
					RETURN_CURRENT_ERROR;
				vec++;
				break;
			case SQLITE_DONE:
				/* set the last cell in the array to null */
				*vec = NULL;
				done++;
				break;
			default:
				RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
				break; /* not reached */
		}
	}

	/* not reached */
	return MPORT_OK;
}


static int populate_meta_from_stmt(mportPackageMeta *pack, sqlite3 *db, sqlite3_stmt *stmt)
{
	const char *tmp = 0;

	/* Copy pkg to pack->name */
	if ((tmp = sqlite3_column_text(stmt, 0)) == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));

	if ((pack->name = strdup(tmp)) == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");

	/* Copy version to pack->version */
	if ((tmp = sqlite3_column_text(stmt, 1)) == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));

	if ((pack->version = strdup(tmp)) == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");

	/* Copy origin to pack->origin */
	if ((tmp = sqlite3_column_text(stmt, 2)) == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));

	if ((pack->origin = strdup(tmp)) == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");

	/* Copy lang to pack->lang */
	if ((tmp = sqlite3_column_text(stmt, 3)) == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));

	if ((pack->lang = strdup(tmp)) == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");

	/* Copy prefix to pack->prefix */
	if ((tmp = sqlite3_column_text(stmt, 4)) == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));

	if ((pack->prefix = strdup(tmp)) == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");


	/* Copy comment to pack->comment */
	if ((tmp = sqlite3_column_text(stmt, 5)) == NULL) {
		if ((pack->comment = strdup("")) == NULL)
			RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");
	} else {
		if ((pack->comment = strdup(tmp)) == NULL)
			RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");
	}

	/* os_release */
	if ((tmp = sqlite3_column_text(stmt, 6)) == NULL)
		return MPORT_OK;

	if ((pack->os_release = strdup(tmp)) == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");

	pack->locked = sqlite3_column_int(stmt, 8);

	/* CPE */
	if ((tmp = sqlite3_column_text(stmt, 7)) == NULL) {
		return MPORT_OK;
	}
	if ((pack->cpe = strdup(tmp)) == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");

	/* deprecated */
	if ((tmp = sqlite3_column_text(stmt, 9)) == NULL) {
		return MPORT_OK;
	}
	if ((pack->deprecated = strdup(tmp)) == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");

	pack->expiration_date = sqlite3_column_int64(stmt, 10);

	pack->no_provide_shlib =  sqlite3_column_int(stmt, 11);

	/* flavor */
	if ((tmp = sqlite3_column_text(stmt, 12)) == NULL) {
		return MPORT_OK;
	}
	if ((pack->flavor = strdup(tmp)) == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");

	return MPORT_OK;
}
