/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2012 Lucas Holt
 * Copyright (c) 2007-2009 Chris Reinhardt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *  notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *  notice, this list of conditions and the following disclaimer in the
 *  documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.	IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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
#include <sys/wait.h>
#include <errno.h>
#include <string.h>
#include <sqlite3.h>
#include <md5.h>
#include <sha256.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <spawn.h>
#include "mport.h"
#include "mport_private.h"

extern char **environ;

static void verify_mtree(mportInstance *, mportPackageMeta *);

MPORT_PUBLIC_API int
mport_verify_package(mportInstance *mport, mportPackageMeta *pack)
{
	sqlite3_stmt *stmt;
	mportAssetListEntryType type;
	int ret;
	const char *data, *checksum;
	struct stat st;
	char hash[65];

	mport_call_msg_cb(mport, "Verifying %s-%s", pack->name, pack->version);
	if (mport_db_prepare(mport->db, &stmt, "SELECT type,data,checksum FROM assets WHERE pkg=%Q",
		pack->name) != MPORT_OK) {
		sqlite3_finalize(stmt);
		RETURN_CURRENT_ERROR;
	}

	while (1) {
		ret = sqlite3_step(stmt);

		if (ret == SQLITE_DONE)
			break;

		if (ret != SQLITE_ROW) {
			/* some error occured */
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
			/* XXX data is null when ASSET_CHMOD (mode) or similar commands are in plist
			 */
			snprintf(file, sizeof(file), "%s", mport->root);
		} else if (*data == '/') {
			/* we don't use mport->root because it's an absolute path like /var */
			snprintf(file, sizeof(file), "%s", data);
		} else {
			snprintf(file, sizeof(file), "%s%s/%s", mport->root, pack->prefix, data);
		}

		switch (type) {
		case ASSET_FILE_OWNER_MODE:
			/* FALLS THROUGH */
		case ASSET_FILE:
			/* FALLS THROUGH */
		case ASSET_SHELL:
			/* FALLS THROUGH */
		case ASSET_SAMPLE:
			/* FALLS THROUGH */
		case ASSET_INFO:
			/* FALLS THROUGH */
		case ASSET_SAMPLE_OWNER_MODE:

			if (lstat(file, &st) != 0) {
				mport_call_msg_cb(
				    mport, "Can't stat %s: %s", file, strerror(errno));
				break; /* next asset */
			}

			if (S_ISREG(st.st_mode)) {
				if (checksum == NULL || *checksum == '\0') {
					mport_call_msg_cb(
					    mport, "Source checksum missing %s", file);
				} else {
					size_t len = strlen(checksum);
					if (len < 34) {
						if (MD5File(file, hash) == NULL) {
							mport_call_msg_cb(mport, "Can't MD5 %s: %s",
							    file, strerror(errno));
							continue;
						}
					} else {
						if (SHA256_File(file, hash) == NULL) {
							mport_call_msg_cb(mport,
							    "Can't SHA256 %s: %s", file,
							    strerror(errno));
							continue;
						}
					}

					if (strcmp(hash, checksum) != 0)
						mport_call_msg_cb(mport,
						    "Checksum mismatch: %s %s %s", file, hash,
						    checksum);
				}
			}

			break;
		default:
			/* do nothing */
			break;
		}
	}

	sqlite3_finalize(stmt);

	verify_mtree(mport, pack);

	return (MPORT_OK);
}

static void
verify_mtree(mportInstance *mport, mportPackageMeta *pack)
{
	char mtree_path[FILENAME_MAX];
	char prefix[FILENAME_MAX];
	int pipefd[2];
	posix_spawn_file_actions_t action;
	pid_t pid;
	int error, pstat;

	(void)snprintf(mtree_path, sizeof(mtree_path), "%s%s/%s-%s/%s", mport->root,
	    MPORT_INST_INFRA_DIR, pack->name, pack->version, MPORT_MTREE_FILE);

	if (!mport_file_exists(mtree_path))
		return;

	(void)snprintf(prefix, sizeof(prefix), "%s%s", mport->root, pack->prefix);

	if (pipe(pipefd) == -1) {
		mport_call_msg_cb(mport, "mtree verify: pipe: %s", strerror(errno));
		return;
	}

	if ((error = posix_spawn_file_actions_init(&action)) != 0) {
		close(pipefd[0]);
		close(pipefd[1]);
		mport_call_msg_cb(
		    mport, "mtree verify: posix_spawn_file_actions_init: %s", strerror(error));
		return;
	}
	if ((error = posix_spawn_file_actions_adddup2(&action, pipefd[1], STDOUT_FILENO)) != 0 ||
	    (error = posix_spawn_file_actions_addclose(&action, pipefd[0])) != 0) {
		posix_spawn_file_actions_destroy(&action);
		close(pipefd[0]);
		close(pipefd[1]);
		mport_call_msg_cb(mport, "mtree verify: file actions setup: %s", strerror(error));
		return;
	}

	char *const args[] = { MPORT_MTREE_BIN, "-f", mtree_path, "-d", "-e", "-p", prefix, NULL };

	error = posix_spawn(&pid, MPORT_MTREE_BIN, &action, NULL, args, environ);
	posix_spawn_file_actions_destroy(&action);
	close(pipefd[1]);

	if (error != 0) {
		close(pipefd[0]);
		mport_call_msg_cb(mport, "mtree verify: posix_spawn: %s", strerror(error));
		return;
	}

	FILE *fp = fdopen(pipefd[0], "r");
	if (fp != NULL) {
		char line[FILENAME_MAX * 2];
		while (fgets(line, sizeof(line), fp) != NULL) {
			size_t len = strlen(line);
			if (len > 0 && line[len - 1] == '\n')
				line[len - 1] = '\0';
			if (line[0] != '\0')
				mport_call_msg_cb(
				    mport, "mtree %s-%s: %s", pack->name, pack->version, line);
		}
		fclose(fp);
	} else {
		close(pipefd[0]);
	}

	int wres;
	do {
		wres = waitpid(pid, &pstat, 0);
	} while (wres == -1 && errno == EINTR);

	if (wres == -1) {
		mport_call_msg_cb(mport, "mtree %s-%s: waitpid failed: %s", pack->name,
		    pack->version, strerror(errno));
	} else if (WIFSIGNALED(pstat)) {
		mport_call_msg_cb(mport, "mtree %s-%s: terminated by signal %d", pack->name,
		    pack->version, WTERMSIG(pstat));
	} else if (WIFEXITED(pstat) && WEXITSTATUS(pstat) > 1) {
		/* exit 1 means discrepancies found (normal); >1 indicates a tool error */
		mport_call_msg_cb(mport, "mtree %s-%s: exited with status %d", pack->name,
		    pack->version, WEXITSTATUS(pstat));
	}
}

MPORT_PUBLIC_API int
mport_recompute_checksums(mportInstance *mport, mportPackageMeta *pack)
{
	sqlite3_stmt *stmt, *update_stmt;
	mportAssetListEntryType type;
	int ret;
	const char *data, *checksum;
	struct stat st;
	char hash[65];

	mport_call_msg_cb(mport, "Recomputing checksums for %s-%s", pack->name, pack->version);

	if (mport_db_prepare(mport->db, &stmt, "SELECT type,data,checksum FROM assets WHERE pkg=%Q",
		pack->name) != MPORT_OK) {
		sqlite3_finalize(stmt);
		RETURN_CURRENT_ERROR;
	}

	if (mport_db_prepare(mport->db, &update_stmt,
		"UPDATE assets SET checksum=? WHERE pkg=? AND data=?") != MPORT_OK) {
		sqlite3_finalize(stmt);
		sqlite3_finalize(update_stmt);
		RETURN_CURRENT_ERROR;
	}

	if (mport_db_do(mport->db, "BEGIN TRANSACTION") != MPORT_OK) {
		sqlite3_finalize(stmt);
		sqlite3_finalize(update_stmt);
		RETURN_CURRENT_ERROR;
	}

	while (1) {
		ret = sqlite3_step(stmt);

		if (ret == SQLITE_DONE)
			break;

		if (ret != SQLITE_ROW) {
			/* some error occured */
			SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(mport->db));
			mport_db_do(mport->db, "ROLLBACK");
			sqlite3_finalize(stmt);
			sqlite3_finalize(update_stmt);
			RETURN_CURRENT_ERROR;
		}

		type = (mportAssetListEntryType)sqlite3_column_int(stmt, 0);
		data = sqlite3_column_text(stmt, 1);
		checksum = sqlite3_column_text(stmt, 2);

		char file[FILENAME_MAX];
		/* XXX TMP */
		if (data == NULL) {
			/* XXX data is null when ASSET_CHMOD (mode) or similar commands are in plist
			 */
			snprintf(file, sizeof(file), "%s", mport->root);
		} else if (*data == '/') {
			/* we don't use mport->root because it's an absolute path like /var */
			snprintf(file, sizeof(file), "%s", data);
		} else {
			snprintf(file, sizeof(file), "%s%s/%s", mport->root, pack->prefix, data);
		}

		switch (type) {
		case ASSET_FILE_OWNER_MODE:
			/* FALLS THROUGH */
		case ASSET_FILE:
			/* FALLS THROUGH */
		case ASSET_SHELL:
			/* FALLS THROUGH */
		case ASSET_SAMPLE:
			/* FALLS THROUGH */
		case ASSET_INFO:
			/* FALLS THROUGH */
		case ASSET_SAMPLE_OWNER_MODE:

			if (lstat(file, &st) != 0) {
				mport_call_msg_cb(
				    mport, "Can't stat %s: %s", file, strerror(errno));
				break; /* next asset */
			}

			if (S_ISREG(st.st_mode)) {
				if (checksum == NULL || *checksum == '\0') {
					mport_call_msg_cb(
					    mport, "Source checksum missing %s", file);
				} else {
					size_t len = strlen(checksum);
					if (len < 34) {
						if (MD5File(file, hash) == NULL) {
							mport_call_msg_cb(mport, "Can't MD5 %s: %s",
							    file, strerror(errno));
							break;
						}
					} else {
						if (SHA256_File(file, hash) == NULL) {
							mport_call_msg_cb(mport,
							    "Can't SHA256 %s: %s", file,
							    strerror(errno));
							break;
						}
					}

					if (strcmp(hash, checksum) != 0) {
						mport_call_msg_cb(mport,
						    "Updating checksum for %s: %s -> %s", file,
						    checksum, hash);

						sqlite3_reset(update_stmt);
						sqlite3_bind_text(
						    update_stmt, 1, hash, -1, SQLITE_STATIC);
						sqlite3_bind_text(
						    update_stmt, 2, pack->name, -1, SQLITE_STATIC);
						sqlite3_bind_text(
						    update_stmt, 3, data, -1, SQLITE_STATIC);

						if (sqlite3_step(update_stmt) != SQLITE_DONE) {
							mport_call_msg_cb(mport,
							    "Failed to update checksum in database: %s",
							    sqlite3_errmsg(mport->db));
						}
					}
				}
			}

			break;
		default:
			/* do nothing */
			break;
		}
	}

	sqlite3_finalize(stmt);
	sqlite3_finalize(update_stmt);

	if (mport_db_do(mport->db, "COMMIT") != MPORT_OK)
		RETURN_CURRENT_ERROR;

	return (MPORT_OK);
}

/*
 * mport_check_missing_depends(mport)
 *
 * Scan all installed packages and report any whose recorded dependencies are
 * not currently installed.  Returns the number of missing dependency
 * relationships found, or -1 on a fatal error.
 */
MPORT_PUBLIC_API int
mport_check_missing_depends(mportInstance *mport)
{
	sqlite3_stmt *stmt;
	int missing = 0;
	int ret;

	if (mport_db_prepare(mport->db, &stmt,
		"SELECT d.pkg, d.depend_pkgname, d.depend_pkgversion "
		"FROM depends d "
		"WHERE EXISTS ("
		"  SELECT 1 FROM packages p WHERE p.pkg = d.pkg"
		") "
		"AND NOT EXISTS ("
		"  SELECT 1 FROM packages p2 WHERE p2.pkg = d.depend_pkgname"
		") "
		"ORDER BY d.pkg, d.depend_pkgname") != MPORT_OK) {
		SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(mport->db));
		return (-1);
	}

	while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
		const char *pkg = (const char *)sqlite3_column_text(stmt, 0);
		const char *dep = (const char *)sqlite3_column_text(stmt, 1);
		const char *ver = (const char *)sqlite3_column_text(stmt, 2);

		if (ver != NULL && *ver != '\0') {
			mport_call_msg_cb(mport,
			    "Missing dependency: %s requires %s-%s (not installed)", pkg, dep, ver);
		} else {
			mport_call_msg_cb(
			    mport, "Missing dependency: %s requires %s (not installed)", pkg, dep);
		}
		missing++;
	}

	sqlite3_finalize(stmt);

	if (ret != SQLITE_DONE) {
		SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(mport->db));
		return (-1);
	}

	return (missing);
}