/*-
 * Copyright (c) 2013, 2014, 2018 Lucas Holt
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

#include <sqlite3.h>
#include <stdlib.h>
#include <unistd.h>
#include "mport.h"
#include "mport_private.h"

static int mport_upgrade_master_schema_0to2(sqlite3 *);
static int mport_upgrade_master_schema_2to3(sqlite3 *);
static int mport_upgrade_master_schema_3to4(sqlite3 *);
static int mport_upgrade_master_schema_4to6(sqlite3 *);
static int mport_upgrade_master_schema_6to7(sqlite3 *);


/* mport_db_do(sqlite3 *db, const char *sql, ...)
 * 
 * A wrapper for executing a single sql query.  Takes a sqlite3 struct
 * pointer, a format string and a list of args.  See the documentation for 
 * sqlite3_vmprintf() for format information.
 */
int
mport_db_do(sqlite3 *db, const char *fmt, ...)
{
	va_list args;
	char *sql;
	__block int result = MPORT_OK;
	__block char *err;

	va_start(args, fmt);

	sql = sqlite3_vmprintf(fmt, args);

	va_end(args);

	if (sql == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "Couldn't allocate memory for sql statement");

	dispatch_sync(mportSQLSerial, ^{
		int sqlcode = sqlite3_exec(db, sql, 0, 0, 0);
		/* if we get an error code, we want to run it again in some cases */
		if (sqlcode == SQLITE_BUSY || sqlcode == SQLITE_LOCKED) {
			sleep(1);
			if (sqlite3_exec(db, sql, 0, 0, 0) != SQLITE_OK) {
				err = (char *) sqlite3_errmsg(db);
				result = MPORT_ERR_FATAL;
			}
		} else if (sqlcode != SQLITE_OK) {
			err = (char *) sqlite3_errmsg(db);
			result = MPORT_ERR_FATAL;
		}

		sqlite3_free(sql);
	});

	if (result == MPORT_ERR_FATAL)
		SET_ERRORX(result, "sql error preparing '%s' : %s", sql, err);

	return result;
}


/* mport_db_prepare(sqlite3 *, sqlite3_stmt **, const char *, ...)
 * 
 * A wrapper for preparing sqlite statements into statement structs.
 * This function returns MPORT_OK on success.  The sqlite3_stmt pointer 
 * may be null if this function does not return MPORT_OK.
 */
int
mport_db_prepare(sqlite3 *db, sqlite3_stmt **stmt, const char *fmt, ...)
{
	va_list args;
	char *sql;
	__block int result = MPORT_OK;
	__block char *err;

	va_start(args, fmt);
	sql = sqlite3_vmprintf(fmt, args);
	va_end(args);

	if (sql == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "Couldn't allocate memory for sql statement");

	dispatch_sync(mportSQLSerial, ^{
		int sqlcode = sqlite3_prepare_v2(db, sql, -1, stmt, NULL);
		if (sqlcode == SQLITE_BUSY || sqlcode == SQLITE_LOCKED) {
			sleep(1);
			if (sqlite3_prepare_v2(db, sql, -1, stmt, NULL) != SQLITE_OK) {
				err = (char *) sqlite3_errmsg(db);
				result = MPORT_ERR_FATAL;
			}
		} else if (sqlcode != SQLITE_OK) {
			err = (char *) sqlite3_errmsg(db);
			result = MPORT_ERR_FATAL;
		}

		sqlite3_free(sql);
	});

	if (result == MPORT_ERR_FATAL)
		SET_ERRORX(result, "sql error preparing '%s' : %s", sql, err);
	return result;
}

int
mport_db_count(sqlite3 *db, int *count, const char *fmt, ...) {
	va_list args;
	char *sql;
	__block int result = MPORT_OK;
	__block char *err;
	__block int realCount = 0;

	va_start(args, fmt);
	sql = sqlite3_vmprintf(fmt, args);
	va_end(args);

	if (sql == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "Couldn't allocate memory for sql statement");

	dispatch_sync(mportSQLSerial, ^{
		sqlite3_stmt *stmt;
		int sqlcode = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
		if (sqlcode == SQLITE_BUSY || sqlcode == SQLITE_LOCKED) {
			sleep(1);
			if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
				err = (char *) sqlite3_errmsg(db);
				result = MPORT_ERR_FATAL;
				return;
			}
		} else if (sqlcode != SQLITE_OK) {
			err = (char *) sqlite3_errmsg(db);
			result = MPORT_ERR_FATAL;
			return;
		}

		sqlite3_free(sql);

		if (sqlite3_step(stmt) != SQLITE_ROW) {
			sqlite3_finalize(stmt);
			return;
		}

		realCount = sqlite3_column_int(stmt, 0);
		sqlite3_finalize(stmt);
	});
	*count = realCount;

	if (result == MPORT_ERR_FATAL)
		SET_ERRORX(result, "sql error preparing '%s' : %s", sql, err);
	return result;
}

  

/* mport_attach_stub_db(sqlite *db, const char *tmpdir) 
 *
 * Attaches tmpdir/MPORT_STUB_DB_FILE to the given database handle as 
 * 'stub'.  (stub.table to access a table in the stub db)
 *
 * Returns MPORT_OK on success.
 */
int
mport_attach_stub_db(sqlite3 *db, const char *dir)
{
	char *file;

	asprintf(&file, "%s/%s", dir, MPORT_STUB_DB_FILE);
  
	if (mport_db_do(db, "ATTACH %Q AS stub", file) != MPORT_OK) { 
		free(file);
		RETURN_CURRENT_ERROR;
	}
  
	free(file);
  
	return (MPORT_OK);
}


/* mport_detach_stub_db(sqlite *db) 
 *
 * The inverse of mport_attach_stub_db().
 *
 * Returns MPORT_OK on success.
 */
int
mport_detach_stub_db(sqlite3 *db)
{
	if (mport_db_do(db, "DETACH stub") != MPORT_OK) 
		RETURN_CURRENT_ERROR;
  
	return (MPORT_OK);
}





#define RUN_SQL(db, sql) \
  if (mport_db_do(db, sql) != MPORT_OK) \
    RETURN_CURRENT_ERROR

int
mport_generate_stub_schema(sqlite3 *db) 
{
	char *ptr;
	char *sql;

	ptr = mport_get_osrelease();
	if (ptr == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "OS Release could not be determined");
	asprintf(&sql, "INSERT INTO meta VALUES (\"os_release\", \"%s\")", ptr);
	
	RUN_SQL(db, "CREATE TABLE meta (field text NOT NULL, value text NOT NULL)");
	RUN_SQL(db, "INSERT INTO meta VALUES (\"bundle_format_version\", " MPORT_BUNDLE_VERSION_STR ")");
	RUN_SQL(db, sql);
	RUN_SQL(db, "CREATE TABLE assets (pkg text not NULL, type int NOT NULL, data text, checksum text, owner text, grp text, mode text)");
	RUN_SQL(db, "CREATE TABLE packages (pkg text NOT NULL, version text NOT NULL, origin text NOT NULL, lang text, options text, prefix text NOT NULL, comment text, os_release text NOT NULL, cpe text NOT NULL, deprecated text, expiration_date int64, no_provide_shlib int NOT NULL, flavor text)");
	RUN_SQL(db, "CREATE TABLE conflicts (pkg text NOT NULL, conflict_pkg text NOT NULL, conflict_version text NOT NULL)");
	RUN_SQL(db, "CREATE TABLE depends (pkg text NOT NULL, depend_pkgname text NOT NULL, depend_pkgversion text, depend_port text NOT NULL)");
	RUN_SQL(db, "CREATE TABLE categories (pkg text NOT NULL, category text NOT NULL)");

	return (MPORT_OK);
}

int
mport_upgrade_master_schema(sqlite3 *db, int databaseVersion) 
{
	if (databaseVersion == MPORT_MASTER_VERSION)
		return MPORT_OK;

	switch (databaseVersion) {
		case 0:
		case 1:
			mport_upgrade_master_schema_0to2(db);
			mport_upgrade_master_schema_2to3(db);
			mport_upgrade_master_schema_4to6(db);
			mport_upgrade_master_schema_6to7(db);
			mport_set_database_version(db);
			break;
		case 2:
			mport_upgrade_master_schema_2to3(db);
			/* falls through */
		case 3:
			mport_upgrade_master_schema_3to4(db);
			/* falls through */
		case 4:
			/* falls through */
		case 5:
			mport_upgrade_master_schema_4to6(db);
			/* falls through */
		case 6:
			mport_upgrade_master_schema_6to7(db);
			mport_set_database_version(db);
			break;
		case 7:
			break;
		default:
			RETURN_ERROR(MPORT_ERR_FATAL, "Invalid master database version");
	}

	return (MPORT_OK);
}

static int
mport_upgrade_master_schema_0to2(sqlite3 *db)
{

	RUN_SQL(db, "ALTER TABLE packages ADD COLUMN os_release text;");
	RUN_SQL(db, "update packages set os_release='0.4';");

	return (MPORT_OK);
}

static int
mport_upgrade_master_schema_2to3(sqlite3 *db)
{

        RUN_SQL(db, "ALTER TABLE packages ADD COLUMN cpe text;");

        return (MPORT_OK);
}

static int
mport_upgrade_master_schema_3to4(sqlite3 *db)
{
	RUN_SQL(db, "ALTER TABLE Packages ADD COLUMN locked int;");
	RUN_SQL(db, "update packages set locked=0;");

	return (MPORT_OK);
}

static int
mport_upgrade_master_schema_4to6(sqlite3 *db)
{
	RUN_SQL(db, "ALTER TABLE assets ADD COLUMN owner text;");
	RUN_SQL(db, "ALTER TABLE assets ADD COLUMN grp text;");
	RUN_SQL(db, "ALTER TABLE assets ADD COLUMN mode text;");

	return (MPORT_OK);
}

static int
mport_upgrade_master_schema_6to7(sqlite3 *db) {
	RUN_SQL(db, "ALTER TABLE packages ADD COLUMN deprecated text");
	RUN_SQL(db, "ALTER TABLE packages ADD COLUMN expiration_date int64");
	RUN_SQL(db, "ALTER TABLE packages ADD COLUMN no_provide_shlib int");
	RUN_SQL(db, "ALTER TABLE packages ADD COLUMN flavor text");

	RUN_SQL(db, "update packages set no_provide_shlib = 0");
	RUN_SQL(db, "update packages set expiration_date = 0");
	RUN_SQL(db, "update packages set deprecated = ''");
	RUN_SQL(db, "update packages set flavor = ''");

	return (MPORT_OK);
}

int
mport_generate_master_schema(sqlite3 *db) 
{

	RUN_SQL(db, "CREATE TABLE IF NOT EXISTS packages (pkg text NOT NULL, version text NOT NULL, origin text NOT NULL, prefix text NOT NULL, lang text, options text, status text default 'dirty', comment text, os_release text NOT NULL default '1.0', cpe text, locked int NOT NULL default '0', deprecated text default '', expiration_date int64 NOT NULL default '0', no_provide_shlib int default '0', flavor text default '')");
	RUN_SQL(db, "CREATE UNIQUE INDEX IF NOT EXISTS packages_pkg ON packages (pkg)");
	RUN_SQL(db, "CREATE INDEX IF NOT EXISTS packages_origin ON packages (origin)");

	RUN_SQL(db, "CREATE TABLE IF NOT EXISTS depends (pkg text NOT NULL, depend_pkgname text NOT NULL, depend_pkgversion text, depend_port text NOT NULL)");
	RUN_SQL(db, "CREATE INDEX IF NOT EXISTS depends_pkg ON depends (pkg)");
	RUN_SQL(db, "CREATE INDEX IF NOT EXISTS depends_dependpkgname ON depends (depend_pkgname)");

	RUN_SQL(db, "CREATE TABLE IF NOT EXISTS log (pkg text NOT NULL, version text NOT NULL, date int NOT NULL, msg text NOT NULL)");
	RUN_SQL(db, "CREATE INDEX IF NOT EXISTS log_pkg ON log (pkg, version)");

	RUN_SQL(db, "CREATE TABLE IF NOT EXISTS assets (pkg text NOT NULL, type int NOT NULL, data text, checksum text, owner text, grp text, mode text)");
	RUN_SQL(db, "CREATE INDEX IF NOT EXISTS assets_pkg ON assets (pkg)");
  
	RUN_SQL(db, "CREATE TABLE IF NOT EXISTS categories (pkg text NOT NULL, category text NOT NULL)");
	RUN_SQL(db, "CREATE INDEX IF NOT EXISTS categories_pkg ON categories (pkg, category)");

	RUN_SQL(db, "CREATE TABLE IF NOT EXISTS settings (name text NOT NULL, val text NOT NULL)");
	RUN_SQL(db, "CREATE INDEX IF NOT EXISTS settings_name ON settings (name)");

	mport_set_database_version(db);

	return (MPORT_OK);
}
