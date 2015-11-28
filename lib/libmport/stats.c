#include <sys/cdefs.h>
__MBSDID("$MidnightBSD$");

#include <sys/cdefs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "mport.h"
#include "mport_private.h"

/* allocate mem for a mportInstance */
MPORT_PUBLIC_API mportStats *
mport_stats_new(void)
{
    return (mportStats *)calloc(1, sizeof(mportStats));
}

MPORT_PUBLIC_API int
mport_stats_free(mportStats *stats)
{
    free(stats);
    return MPORT_OK;
}

MPORT_PUBLIC_API int
mport_stats(mportInstance *mport, mportStats **stats)
{
    sqlite3_stmt *stmt;
    sqlite3 *db = mport->db;
    mportStats *s;

    if ((s = mport_stats_new()) == NULL)
        RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");

    *stats = s;

    if (mport_db_prepare(db, &stmt, "SELECT COUNT(*) FROM packages") != MPORT_OK)
        RETURN_CURRENT_ERROR;

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
    }

    s->pkg_installed = (unsigned int) sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    if (mport_db_prepare(db, &stmt, "SELECT COUNT(*) FROM idx.packages") != MPORT_OK)
        RETURN_CURRENT_ERROR;

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
    }

    s->pkg_available = (unsigned int) sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    return (MPORT_OK);
}
