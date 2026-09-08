/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2015, 2025 Lucas Holt
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

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <sqlite3.h>
#include <md5.h>
#include <sha256.h>
#include <stdlib.h>
#include <libgen.h>
#include <fcntl.h>
#include <syslog.h>
#include <stdarg.h>

#include "mport.h"
#include "mport_private.h"
#include "mport_lua.h"

static char **
parse_sample(char *input)
{
	char **ap, **argv;
	argv = calloc(3, sizeof(char *));

	if (argv == NULL)
		return NULL;

	for (ap = argv; (*ap = strsep(&input, " \t")) != NULL;) {
		if (**ap != '\0') {
			if (++ap >= &argv[3])
				break;
		}
	}

	return argv;
}

static int run_unexec(mportInstance *, mportPackageMeta *, mportAssetListEntryType);
static int run_unldconfig(mportInstance *, mportPackageMeta *);
static int run_special_unexec(mportInstance *, mportPackageMeta *);
static int run_pkg_deinstall(mportInstance *, mportPackageMeta *, const char *);
static int delete_pkg_infra(mportInstance *, mportPackageMeta *);
static int check_for_upwards_depends(mportInstance *, mportPackageMeta *);
static void warn_ignored_rmdir_error(/*@notnull@*/ mportInstance *, /*@notnull@*/ const char *);
static bool is_safe_to_delete_dir(mportInstance *, mportPackageMeta *, const char *, const char *);
static int build_info_dir_path(
    /*@notnull@*/ mportPackageMeta *, /*@null@*/ const char *, /*@out@*/ char *, size_t);

static int
unlink_if_unchanged(const char *path, const struct stat *expected)
{
	char parent[FILENAME_MAX];
	char *name;
	int parentfd;
	struct stat current;

	if (path == NULL || expected == NULL)
		return -1;

	strlcpy(parent, path, sizeof(parent));
	name = strrchr(parent, '/');
	if (name == NULL) {
		strlcpy(parent, ".", sizeof(parent));
		name = (char *)path;
	} else {
		*name++ = '\0';
		if (parent[0] == '\0')
			strlcpy(parent, "/", sizeof(parent));
	}

	parentfd = open(parent, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
	if (parentfd == -1)
		return -1;

	if (fstatat(parentfd, name, &current, AT_SYMLINK_NOFOLLOW) != 0 ||
	    current.st_dev != expected->st_dev || current.st_ino != expected->st_ino) {
		close(parentfd);
		errno = EAGAIN;
		return -1;
	}

	int ret = unlinkat(parentfd, name, 0);
	close(parentfd);
	return ret;
}

MPORT_PUBLIC_API int
mport_delete_primative(mportInstance *mport, mportPackageMeta *pack, int force)
{
	sqlite3_stmt *stmt;
	int ret, current, total;
	mportAssetListEntryType type;
	const char *data, *checksum, *cwd;
	struct stat st;
	char hash[65];

	if (force == 0) {
		if (check_for_upwards_depends(mport, pack) != MPORT_OK)
			RETURN_CURRENT_ERROR;
	}

	mport_call_progress_init_cb(mport, "Deleting %s-%s", pack->name, pack->version);

	mport_start_stop_service(mport, pack, SERVICE_STOP);

	mport_lua_script_load(mport, pack);

	/* get the file count for the progress meter */
	if (mport_db_prepare(mport->db, &stmt,
		"SELECT COUNT(*) FROM assets WHERE (type=%i or type=%i or type=%i or type=%i or type=%i or type=%i) AND pkg=%Q",
		ASSET_FILE, ASSET_SAMPLE, ASSET_SAMPLE_OWNER_MODE, ASSET_SHELL,
		ASSET_FILE_OWNER_MODE, ASSET_INFO, pack->name) != MPORT_OK)
		RETURN_CURRENT_ERROR;

	switch (sqlite3_step(stmt)) {
	case SQLITE_ROW:

		total = sqlite3_column_int(stmt, 0) + 1;
		current = 0;
		sqlite3_finalize(stmt);
		break;
	default:
		SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(mport->db));
		sqlite3_finalize(stmt);
		RETURN_CURRENT_ERROR;
	}

	if (mport_lock_islocked(pack) == MPORT_LOCKED) {
		SET_ERROR(MPORT_ERR_FATAL, "Package is locked.");
		RETURN_CURRENT_ERROR;
	}

	if (mport_db_do(mport->db, "UPDATE packages SET status='dirty' WHERE pkg=%Q", pack->name) !=
	    MPORT_OK)
		RETURN_CURRENT_ERROR;

	if (run_unexec(mport, pack, ASSET_PREUNEXEC) != MPORT_OK)
		RETURN_CURRENT_ERROR;

	if (run_unldconfig(mport, pack) != MPORT_OK)
		RETURN_CURRENT_ERROR;

	if (mport_lua_script_run(mport, pack, MPORT_LUA_PRE_DEINSTALL) != MPORT_OK)
		RETURN_CURRENT_ERROR;

	if (run_pkg_deinstall(mport, pack, "DEINSTALL") != MPORT_OK)
		RETURN_CURRENT_ERROR;

	if (mport_db_prepare(mport->db, &stmt,
		"SELECT type,data,checksum FROM assets WHERE pkg=%Q "
		"ORDER BY CASE WHEN type IN (%d, %d, %d, %d, %d) THEN 1 ELSE 0 END, "
		"CASE WHEN type IN (%d, %d, %d, %d, %d) THEN length(data) ELSE 0 END DESC",
		pack->name, ASSET_DIR, ASSET_DIRRM, ASSET_DIRRMTRY, ASSET_DIR_OWNER_MODE,
		ASSET_AUTODIR, ASSET_DIR, ASSET_DIRRM, ASSET_DIRRMTRY, ASSET_DIR_OWNER_MODE,
		ASSET_AUTODIR) != MPORT_OK)
		RETURN_CURRENT_ERROR;

	cwd = pack->prefix;

	while (1) {
		ret = sqlite3_step(stmt);

		if (ret == SQLITE_DONE)
			break;

		if (ret != SQLITE_ROW) {
			/* some error occurred */
			SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(mport->db));
			sqlite3_finalize(stmt);
			RETURN_CURRENT_ERROR;
		}

		type = (mportAssetListEntryType)sqlite3_column_int(stmt, 0);
		data = sqlite3_column_text(stmt, 1);
		checksum = sqlite3_column_text(stmt, 2);

		char file[FILENAME_MAX];
		/* XXX TMP */
		if (data == NULL) {
			/* XXX data is null when ASSET_CHMOD (mode) or similar
			 * commands are in plist */
			snprintf(file, sizeof(file), "%s", mport->root);
		} else if (*data == '/') {
			/* TODO: do we still want to support mport->root here?
			 * seems to fail for /var entries */
			snprintf(file, sizeof(file), "%s%s", mport->root, data);
		} else {
			snprintf(file, sizeof(file), "%s%s/%s", mport->root, pack->prefix, data);
		}

		switch (type) {
		case ASSET_RMEMPTY:
			(mport->progress_step_cb)(++current, total, file);
			if (lstat(file, &st) != 0) {
				mport_call_msg_cb(
				    mport, "Can't stat %s: %s", file, strerror(errno));
				break; /* next asset */
			}

			// remove the file if it is empty
			if (S_ISREG(st.st_mode) && st.st_size == 0) {
				if (unlink_if_unchanged(file, &st) != 0)
					mport_call_msg_cb(mport, "Could not unlink %s: %s", file,
					    strerror(errno));
			}
			break;
		case ASSET_FILE_OWNER_MODE:
		/* falls through */
		case ASSET_FILE:
		/* falls through */
		case ASSET_SHELL:
		/* falls through */
		case ASSET_SAMPLE:
			/* falls through */
		case ASSET_INFO:
			/* falls through */
		case ASSET_SAMPLE_OWNER_MODE:
			(mport->progress_step_cb)(++current, total, file);

			if (lstat(file, &st) != 0) {
				mport_call_msg_cb(
				    mport, "Can't stat %s: %s", file, strerror(errno));
				break; /* next asset */
			}

			if (S_ISREG(st.st_mode)) {
				if (checksum == NULL) {
					mport_call_msg_cb(mport, "Checksum mismatch: %s", file);
				} else if (strlen(checksum) < 34) {
					/* hash is a stack buffer, only written on success;
					   don't strcmp it if MD5File failed. */
					if (MD5File(file, hash) == NULL)
						mport_call_msg_cb(mport, "Can't MD5 %s: %s", file,
						    strerror(errno));
					else if (strcmp(hash, checksum) != 0)
						mport_call_msg_cb(
						    mport, "Checksum mismatch: %s", file);
				} else {
					if (SHA256_File(file, hash) == NULL)
						mport_call_msg_cb(mport, "Can't SHA256 %s: %s",
						    file, strerror(errno));
					else if (strcmp(hash, checksum) != 0)
						mport_call_msg_cb(
						    mport, "Checksum mismatch: %s", file);
				}

				if (type == ASSET_SAMPLE || type == ASSET_SAMPLE_OWNER_MODE) {
					char sample_hash[65];
					char dest_path[FILENAME_MAX];
					char *data_copy;
					char **fileargv;
					bool dest_path_set = false;

					dest_path[0] = '\0';
					data_copy = strdup(data);

					if (data_copy != NULL) {
						fileargv = parse_sample(data_copy);

						if (fileargv != NULL) {
							if (fileargv[1] != NULL) {
								/* Case 2: @sample src dest */
								if (fileargv[1][0] == '/')
									strlcpy(dest_path,
									    fileargv[1],
									    FILENAME_MAX);
								else
									(void)snprintf(dest_path,
									    FILENAME_MAX, "%s%s/%s",
									    mport->root, cwd,
									    fileargv[1]);
								dest_path_set = true;
							} else if (fileargv[0] != NULL) {
								/* Case 1: @sample file.sample */
								char nonSample[FILENAME_MAX];
								strlcpy(
								    nonSample, file, FILENAME_MAX);

								char *sptr = strcasestr(
								    nonSample, ".sample");
								if (sptr != NULL) {
									sptr[0] =
									    '\0'; /* hack off
										     .sample */
									strlcpy(dest_path,
									    nonSample,
									    FILENAME_MAX);
									dest_path_set = true;
								}
							}
							free(fileargv);
						}
						free(data_copy);
					}

					if (dest_path_set && mport_file_exists(dest_path)) {
						bool hashes_match = false;
						/* checksum may be NULL, and hash is only computed
						   above when it is not; without it we cannot
						   verify, so treat the sample as unmatched. */
						if (checksum != NULL && strlen(checksum) < 34) {
							if (MD5File(dest_path, sample_hash) !=
								NULL &&
							    strcmp(sample_hash, hash) == 0) {
								hashes_match = true;
							}
						} else if (checksum != NULL) {
							if (SHA256_File(dest_path, sample_hash) !=
								NULL &&
							    strcmp(sample_hash, hash) == 0) {
								hashes_match = true;
							}
						}

						if (hashes_match) {
							if (unlink(dest_path) != 0)
								mport_call_msg_cb(mport,
								    "Could not unlink %s: %s",
								    dest_path, strerror(errno));
						} else {
							mport_call_msg_cb(mport,
							    "File does not match sample, remove file %s manually.",
							    dest_path);
						}
					}
				}
			}

			if (unlink_if_unchanged(file, &st) != 0)
				mport_call_msg_cb(
				    mport, "Could not unlink %s: %s", file, strerror(errno));

			if (type == ASSET_SHELL) {
				if (mport_shell_unregister(file) != MPORT_OK)
					mport_call_msg_cb(
					    mport, "Could not unregister shell: %s", file);
			}

			break;
		case ASSET_UNEXEC:
			if (mport_run_asset_exec(mport, data, cwd, file) != MPORT_OK) {
				mport_call_msg_cb(
				    mport, "Could not execute %s: %s", data, mport_err_string());
			}
			break;
		case ASSET_LDCONFIG:
			if (mport_xsystem(mport,
				"/usr/sbin/service ldconfig restart > /dev/null") != MPORT_OK) {
				mport_call_msg_cb(
				    mport, "Could not run ldconfig: %s", mport_err_string());
			}
			break;
		case ASSET_DIR:
		case ASSET_DIRRM:
		case ASSET_DIRRMTRY:
		case ASSET_AUTODIR:
		case ASSET_DIR_OWNER_MODE:
			if (is_safe_to_delete_dir(mport, pack, file, data)) {
				mport_removeflags(mport->root, file);
				if (mport_rmdir(file,
					type == ASSET_DIRRMTRY || type == ASSET_AUTODIR ? 1 : 0) !=
				    MPORT_OK) {
					warn_ignored_rmdir_error(mport, file);
				}
			} else if (type != ASSET_DIRRMTRY && type != ASSET_AUTODIR) {
				mport_call_msg_cb(
				    mport, "Directory in use by another package? '%s'", file);
			}

			break;
		default:
			/* do nothing */
			break;
		}
	}

	sqlite3_finalize(stmt);

	if (run_unexec(mport, pack, ASSET_POSTUNEXEC) != MPORT_OK)
		RETURN_CURRENT_ERROR;

	if (run_special_unexec(mport, pack) != MPORT_OK) {
		RETURN_CURRENT_ERROR;
	}

	if (mport_lua_script_run(mport, pack, MPORT_LUA_POST_DEINSTALL) != MPORT_OK)
		RETURN_CURRENT_ERROR;

	if (run_pkg_deinstall(mport, pack, "POST-DEINSTALL") != MPORT_OK)
		RETURN_CURRENT_ERROR;

	if (mport_db_do(mport->db, "BEGIN TRANSACTION") != MPORT_OK)
		RETURN_CURRENT_ERROR;

	if (mport_db_do(mport->db, "DELETE FROM assets WHERE pkg=%Q", pack->name) != MPORT_OK)
		RETURN_CURRENT_ERROR;

	if (mport_db_do(mport->db, "DELETE FROM depends WHERE pkg=%Q", pack->name) != MPORT_OK)
		RETURN_CURRENT_ERROR;

	if (mport_db_do(mport->db, "DELETE FROM packages WHERE pkg=%Q", pack->name) != MPORT_OK)
		RETURN_CURRENT_ERROR;

	if (mport_db_do(mport->db, "DELETE FROM categories WHERE pkg=%Q", pack->name) != MPORT_OK)
		RETURN_CURRENT_ERROR;

	if (mport_db_do(mport->db, "DELETE FROM conflicts WHERE pkg=%Q", pack->name) != MPORT_OK)
		RETURN_CURRENT_ERROR;

	if (mport_db_do(mport->db, "DELETE FROM annotation WHERE pkg=%Q", pack->name) != MPORT_OK)
		RETURN_CURRENT_ERROR;

	if (mport_pkg_message_display(mport, pack) != MPORT_OK)
		RETURN_CURRENT_ERROR;

	if (delete_pkg_infra(mport, pack) != MPORT_OK)
		RETURN_CURRENT_ERROR;

	if (mport_db_do(mport->db, "COMMIT TRANSACTION") != MPORT_OK)
		RETURN_CURRENT_ERROR;

	(mport->progress_step_cb)(++current, total, "DB Updated");

	(mport->progress_free_cb)();

	mport_pkgmeta_logevent(mport, pack, "Package deleted");
	syslog(LOG_NOTICE, "%s-%s deinstalled", pack->name, pack->version);

	return (MPORT_OK);
}

static void
warn_ignored_rmdir_error(/*@notnull@*/ mportInstance *mport, /*@notnull@*/ const char *dir)
{
	const char *err_msg = mport_err_string();

	mport_call_msg_cb(mport, "Could not remove directory '%s': %s", dir,
	    err_msg != NULL ? err_msg : "unknown error");
	mport_set_err(MPORT_OK, NULL);
}

bool
is_safe_to_delete_dir(
    mportInstance *mport, mportPackageMeta *pack, const char *path, const char *asset_path)
{
	sqlite3_stmt *stmt;
	int count;

	if (mport == NULL || pack == NULL || path == NULL || asset_path == NULL) {
		return false;
	}

	/* Don't delete the root or the package prefix directories */
	if (mport->root != NULL && strcmp(mport->root, path) == 0) {
		mport_call_msg_cb(
		    mport, "Skipping removal of root (DESTDIR) directory: '%s'", path);
		return false;
	}
	if (pack->prefix != NULL &&
	    (strcmp(pack->prefix, path) == 0 || strcmp(pack->prefix, asset_path) == 0)) {
		mport_call_msg_cb(
		    mport, "Skipping removal of package prefix directory: '%s'", path);
		return false;
	}

	/* Check if the path is a system directory */
	if (pack->type == MPORT_TYPE_APP && mport_is_system_mtree_dir(asset_path)) {
		mport_call_msg_cb(mport, "Skipping removal of system directory: '%s'", path);
		return false;
	}

	if (mport_db_prepare(mport->db, &stmt,
		"SELECT count(*) from assets where pkg!=%Q and type in (%d, %d, %d, %d, %d) and data=%Q",
		pack->name, ASSET_DIR, ASSET_DIRRM, ASSET_DIRRMTRY, ASSET_DIR_OWNER_MODE,
		ASSET_AUTODIR, asset_path) != MPORT_OK) {
		return false;
	}

	switch (sqlite3_step(stmt)) {
	case SQLITE_ROW:
		count = sqlite3_column_int(stmt, 0);
		break;
	default:
		SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(mport->db));
		sqlite3_finalize(stmt);
		return false;
	}

	sqlite3_finalize(stmt);
	return (count == 0);
}

static int
build_info_dir_path(
    /*@notnull@*/ mportPackageMeta *pkg, /*@null@*/ const char *data, /*@out@*/ char *path,
    size_t path_size)
{
	char info_path[FILENAME_MAX];
	char path_copy[FILENAME_MAX];
	char *dirpath;

	if (pkg == NULL || path == NULL || path_size == 0)
		RETURN_ERROR(MPORT_ERR_FATAL, "Invalid info asset path arguments.");

	if (data == NULL) {
		if (strlcpy(path, "/usr/local/share/info", path_size) >= path_size)
			RETURN_ERROR(MPORT_ERR_FATAL, "Info directory path is too long.");
		return (MPORT_OK);
	}

	if (*data == '/') {
		if (strlcpy(info_path, data, sizeof(info_path)) >= sizeof(info_path))
			RETURN_ERROR(MPORT_ERR_FATAL, "Info asset path is too long.");
	} else {
		if (pkg->prefix == NULL)
			RETURN_ERROR(
			    MPORT_ERR_FATAL, "Package prefix is undefined for info asset.");
		if (snprintf(info_path, sizeof(info_path), "%s/%s", pkg->prefix, data) >=
		    (int)sizeof(info_path)) {
			RETURN_ERROR(MPORT_ERR_FATAL, "Info asset path is too long.");
		}
	}

	if (strlcpy(path_copy, info_path, sizeof(path_copy)) >= sizeof(path_copy))
		RETURN_ERROR(MPORT_ERR_FATAL, "Info asset path is too long.");

	dirpath = dirname(path_copy);
	if (dirpath == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "Unable to determine info directory.");

	if (strlcpy(path, dirpath, path_size) >= path_size)
		RETURN_ERROR(MPORT_ERR_FATAL, "Info directory path is too long.");
	return (MPORT_OK);
}

static int
run_unldconfig(mportInstance *mport, mportPackageMeta *pkg)
{
	int ret;
	char cwd[FILENAME_MAX];
	sqlite3_stmt *assets = NULL;
	sqlite3 *db;
	const char *data;
	mportAssetListEntryType type;

	db = mport->db;

	/* Process @ldconfig steps */
	if (mport_db_prepare(db, &assets,
		"SELECT type, data FROM assets WHERE pkg=%Q and type in (%d, %d)", pkg->name,
		ASSET_LDCONFIG, ASSET_LDCONFIG_LINUX) != MPORT_OK)
		goto UNLDCONFIG_ERROR;

	(void)strlcpy(cwd, pkg->prefix, sizeof(cwd));

	if (mport_chdir(mport, cwd) != MPORT_OK)
		goto UNLDCONFIG_ERROR;

	while (1) {
		ret = sqlite3_step(assets);

		if (ret == SQLITE_DONE)
			break;

		if (ret != SQLITE_ROW) {
			SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
			goto UNLDCONFIG_ERROR;
		}
		type = (mportAssetListEntryType)sqlite3_column_int(assets, 0);
		data = sqlite3_column_text(assets, 1);

		switch (type) {
		case ASSET_LDCONFIG:
			if (mport_xsystem(mport,
				"/usr/sbin/service ldconfig restart > /dev/null") != MPORT_OK) {
				goto UNLDCONFIG_ERROR;
			}
			break;
		case ASSET_LDCONFIG_LINUX:
			if (data == NULL) {
				if (mport_xsystem(mport, "/compat/linux/sbin/ldconfig") !=
				    MPORT_OK) {
					goto UNLDCONFIG_ERROR;
				}
			} else {
				if (mport_exec_linux_ldconfig(mport, (const char *)data) !=
				    MPORT_OK) {
					goto UNLDCONFIG_ERROR;
				}
			}
			break;
		default:
			break;
		}
	}
	sqlite3_finalize(assets);
	mport_pkgmeta_logevent(mport, pkg, type == ASSET_LDCONFIG ? "ldconfig" : "ldconfig-linux");

	return (MPORT_OK);

UNLDCONFIG_ERROR:
	sqlite3_finalize(assets);
	RETURN_CURRENT_ERROR;
}

static int
run_special_unexec(mportInstance *mport, mportPackageMeta *pkg)
{
	int ret;
	char cwd[FILENAME_MAX];
	char info_dir[FILENAME_MAX];
	sqlite3_stmt *assets = NULL;
	sqlite3 *db;
	const char *data;
	mportAssetListEntryType type;

	db = mport->db;

	/* Process @ldconfig steps */
	if (mport_db_prepare(db, &assets,
		"SELECT type, data FROM assets WHERE pkg=%Q and type in (%d,%d,%d,%d)", pkg->name,
		ASSET_DESKTOP_FILE_UTILS, ASSET_GLIB_SCHEMAS, ASSET_INFO, ASSET_KLD) != MPORT_OK)
		goto SPECIAL_ERROR;

	(void)strlcpy(cwd, pkg->prefix, sizeof(cwd));

	if (mport_chdir(mport, cwd) != MPORT_OK)
		goto SPECIAL_ERROR;

	while (1) {
		ret = sqlite3_step(assets);

		if (ret == SQLITE_DONE)
			break;

		if (ret != SQLITE_ROW) {
			SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
			goto SPECIAL_ERROR;
		}
		type = (mportAssetListEntryType)sqlite3_column_int(assets, 0);
		data = sqlite3_column_text(assets, 1);

		switch (type) {
		case ASSET_GLIB_SCHEMAS:
			if (mport_file_exists("/usr/local/bin/glib-compile-schemas") &&
			    mport_exec_glib_compile_schemas(mport,
				data == NULL ? pkg->prefix : (const char *)data) != MPORT_OK) {
				goto SPECIAL_ERROR;
			}
			break;
		case ASSET_INFO:
			if (build_info_dir_path(pkg, data, info_dir, sizeof(info_dir)) != MPORT_OK)
				goto SPECIAL_ERROR;
			if (mport_file_exists("/usr/local/bin/indexinfo") &&
			    mport_exec_indexinfo(mport, info_dir) != MPORT_OK) {
				goto SPECIAL_ERROR;
			}
			break;
		case ASSET_KLD:
			if (mport_exec_kldxref(mport, (const char *)data) != MPORT_OK) {
				goto SPECIAL_ERROR;
			}
			/* attempt to remove the directory containing the kernel
			 * module, if it's not /boot/modules */
			if (strcmp("/boot/modules", data) != 0 &&
			    mport_rmdir(data, 1) != MPORT_OK) {
				warn_ignored_rmdir_error(mport, data);
			}
			break;
		case ASSET_DESKTOP_FILE_UTILS:
			if (mport_file_exists("/usr/local/bin/update-desktop-database") &&
			    mport_xsystem(mport,
				"/usr/local/bin/update-desktop-database -q > /dev/null || true") !=
				MPORT_OK) {
				goto SPECIAL_ERROR;
			}
			break;
		default:
			break;
		}
	}
	sqlite3_finalize(assets);
	return (MPORT_OK);

SPECIAL_ERROR:
	sqlite3_finalize(assets);
	RETURN_CURRENT_ERROR;
}

static int
run_unexec(mportInstance *mport, mportPackageMeta *pkg, mportAssetListEntryType type)
{
	int ret;
	char cwd[FILENAME_MAX];
	sqlite3_stmt *assets = NULL;
	sqlite3 *db;
	const char *data;

	db = mport->db;

	/* Process @postunexec steps */
	if (mport_db_prepare(db, &assets, "SELECT data FROM assets WHERE pkg=%Q and type=%d",
		pkg->name, type) != MPORT_OK)
		goto POSTUN_ERROR;

	(void)strlcpy(cwd, pkg->prefix, sizeof(cwd));

	if (mport_chdir(mport, cwd) != MPORT_OK)
		goto POSTUN_ERROR;

	while (1) {
		ret = sqlite3_step(assets);

		if (ret == SQLITE_DONE)
			break;

		if (ret != SQLITE_ROW) {
			SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
			goto POSTUN_ERROR;
		}
		data = sqlite3_column_text(assets, 0);

		char file[FILENAME_MAX];
		/* XXX TMP */
		if (data == NULL) {
			snprintf(file, sizeof(file), "%s", mport->root);
		} else if (*data == '/') {
			snprintf(file, sizeof(file), "%s%s", mport->root, data);
		} else {
			snprintf(file, sizeof(file), "%s%s/%s", mport->root, pkg->prefix, data);
		}

		if (mport_run_asset_exec(mport, data, cwd, file) != MPORT_OK)
			goto POSTUN_ERROR;
	}
	sqlite3_finalize(assets);
	mport_pkgmeta_logevent(mport, pkg, type == ASSET_POSTUNEXEC ? "postunexec" : "preunexec");

	return (MPORT_OK);

POSTUN_ERROR:
	sqlite3_finalize(assets);
	RETURN_CURRENT_ERROR;
}

static int
run_pkg_deinstall(mportInstance *mport, mportPackageMeta *pack, const char *mode)
{
	char file[FILENAME_MAX];
	char command_file[FILENAME_MAX];
	int ret;

	if (mport_build_infrastructure_path(
		mport, pack, MPORT_DEINSTALL_FILE, true, file, sizeof(file)) != MPORT_OK)
		RETURN_CURRENT_ERROR;

	if (mport_build_infrastructure_path(mport, pack, MPORT_DEINSTALL_FILE, false, command_file,
		sizeof(command_file)) != MPORT_OK)
		RETURN_CURRENT_ERROR;

	if (mport_file_exists(file)) {
		if (chmod(file, 755) != 0)
			RETURN_ERRORX(MPORT_ERR_FATAL, "chmod(%s, 755): %s", file, strerror(errno));

		if ((ret = mport_xsystem(mport, "PKG_PREFIX=%s %s %s %s", pack->prefix,
			 command_file, pack->name, mode)) != 0)
			RETURN_ERRORX(MPORT_ERR_FATAL, "%s %s returned non-zero: %i",
			    MPORT_INSTALL_FILE, mode, ret);
	}

	return (MPORT_OK);
}

/* delete this package's infrastructure dir. */
static int
delete_pkg_infra(mportInstance *mport, mportPackageMeta *pack)
{
	char dir[FILENAME_MAX];
	char file[FILENAME_MAX];

	/* delete mtree file */
	(void)snprintf(file, FILENAME_MAX, "%s%s/%s-%s/%s", mport->root, MPORT_STUB_INFRA_DIR,
	    pack->name, pack->version, MPORT_MTREE_FILE);

	if (mport_file_exists(file) && unlink(file) != 0)
		mport_call_msg_cb(mport, "Could not unlink %s: %s", file, strerror(errno));

	/* delete pkg message file */
	(void)snprintf(file, FILENAME_MAX, "%s%s/%s-%s/%s", mport->root, MPORT_STUB_INFRA_DIR,
	    pack->name, pack->version, MPORT_MESSAGE_FILE);
	if (mport_file_exists(file) && unlink(file) != 0)
		mport_call_msg_cb(mport, "Could not unlink %s: %s", file, strerror(errno));

	/* delete lua files */
	(void)snprintf(file, FILENAME_MAX, "%s%s/%s-%s/%s", mport->root, MPORT_STUB_INFRA_DIR,
	    pack->name, pack->version, MPORT_LUA_POST_INSTALL_FILE);
	if (mport_file_exists(file) && unlink(file) != 0)
		mport_call_msg_cb(mport, "Could not unlink %s: %s", file, strerror(errno));
	(void)snprintf(file, FILENAME_MAX, "%s%s/%s-%s/%s", mport->root, MPORT_STUB_INFRA_DIR,
	    pack->name, pack->version, MPORT_LUA_PRE_INSTALL_FILE);
	if (mport_file_exists(file) && unlink(file) != 0)
		mport_call_msg_cb(mport, "Could not unlink %s: %s", file, strerror(errno));
	(void)snprintf(file, FILENAME_MAX, "%s%s/%s-%s/%s", mport->root, MPORT_STUB_INFRA_DIR,
	    pack->name, pack->version, MPORT_LUA_POST_DEINSTALL_FILE);
	if (mport_file_exists(file) && unlink(file) != 0)
		mport_call_msg_cb(mport, "Could not unlink %s: %s", file, strerror(errno));
	(void)snprintf(file, FILENAME_MAX, "%s%s/%s-%s/%s", mport->root, MPORT_STUB_INFRA_DIR,
	    pack->name, pack->version, MPORT_LUA_PRE_DEINSTALL_FILE);
	if (mport_file_exists(file) && unlink(file) != 0)
		mport_call_msg_cb(mport, "Could not unlink %s: %s", file, strerror(errno));

	(void)snprintf(dir, FILENAME_MAX, "%s%s/%s-%s", mport->root, MPORT_INST_INFRA_DIR,
	    pack->name, pack->version);

	if (mport_file_exists(dir)) {
		if (mport_rmtree(dir) != MPORT_OK) {
			RETURN_ERRORX(MPORT_ERR_FATAL, "mport_rmtree(%s) failed.", dir);
		}
	}

	return (MPORT_OK);
}

static int
check_for_upwards_depends(mportInstance *mport, mportPackageMeta *pack)
{
	sqlite3_stmt *stmt;
	const char *depends;
	char *msg;
	int count;

	if (mport_db_prepare(mport->db, &stmt,
		"SELECT group_concat(packages.pkg),count(packages.pkg) FROM depends JOIN packages ON depends.pkg=packages.pkg WHERE depend_pkgname=%Q",
		pack->name) != MPORT_OK) {
		RETURN_CURRENT_ERROR;
	}

	switch (sqlite3_step(stmt)) {
	case SQLITE_ROW:
		depends = sqlite3_column_text(stmt, 0);
		count = sqlite3_column_int(stmt, 1);

		if (count != 0 && depends != NULL) {
			(void)asprintf(
			    &msg, "%s depend on %s, delete anyway?", depends, pack->name);
			if ((mport->confirm_cb)(msg, "Delete", "Don't delete", 0) != MPORT_OK) {
				sqlite3_finalize(stmt);
				free(msg);
				RETURN_ERRORX(
				    MPORT_ERR_FATAL, "%s depend on %s", depends, pack->name);
			}
			free(msg);
		}

		break;
	default:
		SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(mport->db));
		sqlite3_finalize(stmt);
		RETURN_CURRENT_ERROR;
	}

	sqlite3_finalize(stmt);
	return (MPORT_OK);
}
