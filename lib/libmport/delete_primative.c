/*-
 * Copyright (c) 2015 Lucas Holt
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

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <sqlite3.h>
#include <md5.h>
#include <stdlib.h>
#include <libgen.h>
#include <syslog.h>
#include <stdarg.h>

#include "mport.h"
#include "mport_private.h"

static int run_unexec(mportInstance *, mportPackageMeta *, mportAssetListEntryType);
static int run_pkg_deinstall(mportInstance *, mportPackageMeta *, const char *);
static int delete_pkg_infra(mportInstance *, mportPackageMeta *);
static int check_for_upwards_depends(mportInstance *, mportPackageMeta *);


MPORT_PUBLIC_API int
mport_delete_primative(mportInstance *mport, mportPackageMeta *pack, int force) {
    sqlite3_stmt *stmt;
    int ret, current, total;
    mportAssetListEntryType type;
    const char *data, *checksum, *cwd, *service, *rc_script;
    struct stat st;
    char md5[33];

    if (force == 0) {
        if (check_for_upwards_depends(mport, pack) != MPORT_OK)
            RETURN_CURRENT_ERROR;
    }

    mport_call_progress_init_cb(mport, "Deleteing %s-%s", pack->name, pack->version);

    /* stop any services that might exist; this replaces @stopdaemon */
    if (mport_db_prepare(mport->db, &stmt,
                         "select * from assets where data like '/usr/local/etc/rc.d/%%' and type=%i and pkg=%Q",
                         ASSET_FILE, pack->name) != MPORT_OK)
        RETURN_CURRENT_ERROR;

    while (1) {
        ret = sqlite3_step(stmt);
        if (ret != SQLITE_ROW) {
            break;
        }

        rc_script = sqlite3_column_text(stmt, 0);
        if (rc_script == NULL)
            continue;
        service = basename(rc_script);
        if (mport_xsystem(mport, "/usr/sbin/service %s onestop",
                          service) != 0) {
            mport_call_msg_cb(mport, "Unable to stop service %s\n", service);
        }
    }

    /* get the file count for the progress meter */
    if (mport_db_prepare(mport->db, &stmt,
                         "SELECT COUNT(*) FROM assets WHERE (type=%i or type=%i or type=%i) AND pkg=%Q", ASSET_FILE,
                         ASSET_SAMPLE, ASSET_SHELL, pack->name) != MPORT_OK)
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

    if (mport_db_do(mport->db, "UPDATE packages SET status='dirty' WHERE pkg=%Q", pack->name) != MPORT_OK)
        RETURN_CURRENT_ERROR;

    if (run_unexec(mport, pack, ASSET_PREUNEXEC) != MPORT_OK)
        RETURN_CURRENT_ERROR;

    if (run_pkg_deinstall(mport, pack, "DEINSTALL") != MPORT_OK)
        RETURN_CURRENT_ERROR;

    if (mport_db_prepare(mport->db, &stmt, "SELECT type,data,checksum FROM assets WHERE pkg=%Q", pack->name) !=
        MPORT_OK)
        RETURN_CURRENT_ERROR;

    cwd = pack->prefix;

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

        type = (mportAssetListEntryType) sqlite3_column_int(stmt, 0);
        data = sqlite3_column_text(stmt, 1);
        checksum = sqlite3_column_text(stmt, 2);

        char file[FILENAME_MAX];
        /* XXX TMP */
        if (data == NULL) {
            /* XXX data is null when ASSET_CHMOD (mode) or similar commands are in plist */
            snprintf(file, sizeof(file), "%s", mport->root);
        } else if (*data == '/') {
            snprintf(file, sizeof(file), "%s%s", mport->root, data);
        } else {
            snprintf(file, sizeof(file), "%s%s/%s", mport->root, pack->prefix, data);
        }

        switch (type) {
            case ASSET_FILE:
                /* falls through */
            case ASSET_SHELL:
                /* falls through */
            case ASSET_SAMPLE:
                (mport->progress_step_cb)(++current, total, file);

                if (lstat(file, &st) != 0) {
                    mport_call_msg_cb(mport, "Can't stat %s: %s", file, strerror(errno));
                    break; /* next asset */
                }

                if (S_ISREG(st.st_mode)) {
                    if (MD5File(file, md5) == NULL)
                        mport_call_msg_cb(mport, "Can't md5 %s: %s", file, strerror(errno));

                    if (strcmp(md5, checksum) != 0)
                        mport_call_msg_cb(mport, "Checksum mismatch: %s", file);

                    if (type == ASSET_SAMPLE) {
                        char sample_md5[33];
                        char nonSample[FILENAME_MAX];
                        strlcpy(nonSample, file, FILENAME_MAX);
                        char *sptr = strcasestr(nonSample, ".sample");
                        if (sptr != NULL) {
                            sptr[0] = '\0'; /* hack off .sample */
                            if (MD5File(nonSample, sample_md5) == NULL) {
                                mport_call_msg_cb(mport, "Could not check file %s, review and remove manually.",
                                                  nonSample);
                            } else if (strcmp(sample_md5, md5) == 0) {
                                if (unlink(nonSample) != 0)
                                    mport_call_msg_cb(mport, "Could not unlink %s: %s", file, strerror(errno));
                            } else {
                                mport_call_msg_cb(mport, "File does not match sample, remove file %s manually.",
                                                  nonSample);
                            }
                        }
                    }
                }

                if (unlink(file) != 0)
                    mport_call_msg_cb(mport, "Could not unlink %s: %s", file, strerror(errno));

                if (type == ASSET_SHELL) {
                    if (mport_shell_unregister(file) != MPORT_OK)
                        mport_call_msg_cb(mport, "Could not unregister shell: %s", file);
                }

                break;
            case ASSET_UNEXEC:
                if (mport_run_asset_exec(mport, data, cwd, file) != MPORT_OK) {
                    mport_call_msg_cb(mport, "Could not execute %s: %s", data, mport_err_string());
                }
                break;
            case ASSET_DIR:
            case ASSET_DIRRM:
            case ASSET_DIRRMTRY:
                if (mport_rmdir(file, type == ASSET_DIRRMTRY ? 1 : 0) != MPORT_OK) {
                    mport_call_msg_cb(mport, "Could not remove directory '%s': %s", file, mport_err_string());
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

    if (delete_pkg_infra(mport, pack) != MPORT_OK)
        RETURN_CURRENT_ERROR;

    if (mport_db_do(mport->db, "COMMIT TRANSACTION") != MPORT_OK)
        RETURN_CURRENT_ERROR;

    (mport->progress_step_cb)(++current, total, "DB Updated");


    (mport->progress_free_cb)();

    mport_pkgmeta_logevent(mport, pack, "Package deleted");
    syslog(LOG_NOTICE, "%s-%s deinstalled", pack->name, pack->version);

    return MPORT_OK;
}

static int
run_unexec(mportInstance *mport, mportPackageMeta *pkg, mportAssetListEntryType type) {
    int ret;
    char cwd[FILENAME_MAX];
    sqlite3_stmt *assets = NULL;
    sqlite3 *db;
    const char *data;

    db = mport->db;

    /* Process @postunexec steps */
    if (mport_db_prepare(db, &assets, "SELECT data FROM assets WHERE pkg=%Q and type=%Q", pkg->name,
                         type) != MPORT_OK)
        goto POSTUN_ERROR;

    (void) strlcpy(cwd, pkg->prefix, sizeof(cwd));

    if (mport_chdir(mport, cwd) != MPORT_OK)
        goto ERROR;

    while (1) {
        ret = sqlite3_step(assets);

        if (ret == SQLITE_DONE)
            break;

        if (ret != SQLITE_ROW) {
            SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
            goto ERROR;
        }
         data = sqlite3_column_text(assets, 0);

        char file[FILENAME_MAX];
        /* XXX TMP */
        if (data == NULL) {
            snprintf(file, sizeof(file), "%s", mport->root);
        } else if (*data == '/') {
            snprintf(file, sizeof(file), "%s%s", mport->root, data);
        } else {
            snprintf(file, sizeof(file), "%s%s/%s", mport->root, pack->prefix, data);
        }

        if (mport_run_asset_exec(mport, data, cwd, file) != MPORT_OK)
            goto POSTUN_ERROR;
    }
    sqlite3_finalize(assets);
    mport_pkgmeta_logevent(mport, pkg, type == ASSET_POSTUNEXEC ? "postunexec" : "preunexec");

    return MPORT_OK;

    POSTUN_ERROR:
    sqlite3_finalize(assets);
    RETURN_CURRENT_ERROR;
}


static int
run_pkg_deinstall(mportInstance *mport, mportPackageMeta *pack, const char *mode) {
    char file[FILENAME_MAX];
    int ret;

    (void) snprintf(file, FILENAME_MAX, "%s/%s-%s/%s", MPORT_INST_INFRA_DIR, pack->name, pack->version,
                    MPORT_DEINSTALL_FILE);

    if (mport_file_exists(file)) {
        if (chmod(file, 755) != 0)
            RETURN_ERRORX(MPORT_ERR_FATAL, "chmod(%s, 755): %s", file, strerror(errno));

        if ((ret = mport_xsystem(mport, "PKG_PREFIX=%s %s %s %s", pack->prefix, file, pack->name, mode)) != 0)
            RETURN_ERRORX(MPORT_ERR_FATAL, "%s %s returned non-zero: %i", MPORT_INSTALL_FILE, mode, ret);
    }

    return MPORT_OK;
}


/* delete this package's infrastructure dir. */
static int
delete_pkg_infra(mportInstance *mport, mportPackageMeta *pack) {
    char dir[FILENAME_MAX];

    (void) snprintf(dir, FILENAME_MAX, "%s%s/%s-%s", mport->root, MPORT_INST_INFRA_DIR, pack->name, pack->version);

    if (mport_file_exists(dir)) {
        if (mport_rmtree(dir) != MPORT_OK)
            RETURN_ERRORX(MPORT_ERR_FATAL, "mport_rmtree(%s) failed.", dir);
    }

    return MPORT_OK;
}


static int
check_for_upwards_depends(mportInstance *mport, mportPackageMeta *pack) {
    sqlite3_stmt *stmt;
    const char *depends;
    char *msg;
    int count;

    if (mport_db_prepare(mport->db, &stmt,
                         "SELECT group_concat(packages.pkg),count(packages.pkg) FROM depends JOIN packages ON depends.pkg=packages.pkg WHERE depend_pkgname=%Q",
                         pack->name) != MPORT_OK)
        RETURN_CURRENT_ERROR;

    switch (sqlite3_step(stmt)) {
        case SQLITE_ROW:
            depends = sqlite3_column_text(stmt, 0);
            count = sqlite3_column_int(stmt, 1);

            if (count != 0) {
                (void) asprintf(&msg, "%s depend on %s, delete anyway?", depends, pack->name);
                if ((mport->confirm_cb)(msg, "Delete", "Don't delete", 0) != MPORT_OK) {
                    sqlite3_finalize(stmt);
                    free(msg);
                    RETURN_ERRORX(MPORT_ERR_FATAL, "%s depend on %s", depends, pack->name);
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
    return MPORT_OK;
} 
      
  
