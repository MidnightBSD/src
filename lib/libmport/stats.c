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
	__block sqlite3_stmt *stmt;
	__block sqlite3 *db = mport->db;
	__block mportStats *s;
	__block int result = MPORT_OK;
	__block char *err;

	if ((s = mport_stats_new()) == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");

	*stats = s;

	if (mport_db_prepare(db, &stmt, "SELECT COUNT(*) FROM packages") != MPORT_OK) {
		sqlite3_finalize(stmt);
		RETURN_CURRENT_ERROR;
	}

	dispatch_sync(mportSQLSerial, ^{
		if (sqlite3_step(stmt) != SQLITE_ROW) {
			sqlite3_finalize(stmt);
			err = (char *) sqlite3_errmsg(db);
			result = MPORT_ERR_FATAL;
			return;
		}

		s->pkg_installed = (unsigned int) sqlite3_column_int(stmt, 0);
		sqlite3_finalize(stmt);
	});
	if (result == MPORT_ERR_FATAL) {
		SET_ERRORX(result, "%s", err);
		return result;
	}

    if (mport_db_prepare(db, &stmt, "SELECT COUNT(*) FROM idx.packages") != MPORT_OK) {
		sqlite3_finalize(stmt);
		RETURN_CURRENT_ERROR;
	}

	dispatch_sync(mportSQLSerial, ^{
		if (sqlite3_step(stmt) != SQLITE_ROW) {
			sqlite3_finalize(stmt);
			err = (char *) sqlite3_errmsg(db);
			result = MPORT_ERR_FATAL;
			return;
		}

		s->pkg_available = (unsigned int) sqlite3_column_int(stmt, 0);
		sqlite3_finalize(stmt);
	});

	if (result == MPORT_ERR_FATAL)
		SET_ERRORX(result, "%s", err);
	return result;
}
