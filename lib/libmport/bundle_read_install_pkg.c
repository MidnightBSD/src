/*-
 * Copyright (c) 2013-2015 Lucas Holt
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

#include "mport.h"
#include "mport_private.h"

#include <sys/stat.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>
#include <stdarg.h>
#include <archive_entry.h>


static int do_pre_install(mportInstance *, mportBundleRead *, mportPackageMeta *);
static int do_actual_install(mportInstance *, mportBundleRead *, mportPackageMeta *);
static int do_post_install(mportInstance *, mportBundleRead *, mportPackageMeta *);
static int run_pkg_install(mportInstance *, mportBundleRead *, mportPackageMeta *, const char *);
static int run_mtree(mportInstance *, mportBundleRead *, mportPackageMeta *);
static int display_pkg_msg(mportInstance *, mportBundleRead *, mportPackageMeta *);


int
mport_bundle_read_install_pkg(mportInstance *mport, mportBundleRead *bundle, mportPackageMeta *pkg)
{
    if (do_pre_install(mport, bundle, pkg) != MPORT_OK)
        RETURN_CURRENT_ERROR;

    if (do_actual_install(mport, bundle, pkg) != MPORT_OK)
        RETURN_CURRENT_ERROR;

    if (do_post_install(mport, bundle, pkg) != MPORT_OK)
        RETURN_CURRENT_ERROR;

    syslog(LOG_NOTICE, "%s-%s installed", pkg->name, pkg->version);

    return MPORT_OK;
}  


/* This does everything that has to happen before we start installing files.
 * We run mtree, pkg-install PRE-INSTALL, etc... 
 */
static int
do_pre_install(mportInstance *mport, mportBundleRead *bundle, mportPackageMeta *pkg)
{
    int ret;
    char cwd[FILENAME_MAX];
    sqlite3_stmt *assets = NULL, *count, *insert = NULL;
    sqlite3 *db;
    mportAssetListEntryType type;
    const char *data, *checksum;

    db = mport->db;

    /* run mtree */
    if (run_mtree(mport, bundle, pkg) != MPORT_OK)
        RETURN_CURRENT_ERROR;

    /* run pkg-install PRE-INSTALL */
    if (run_pkg_install(mport, bundle, pkg, "PRE-INSTALL") != MPORT_OK)
        RETURN_CURRENT_ERROR;

    /* Process @preexec steps */
    if (mport_db_prepare(db, &assets, "SELECT type,data,checksum FROM stub.assets WHERE pkg=%Q", pkg->name) != MPORT_OK)
        goto ERROR;

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

        type = (mportAssetListEntryType) sqlite3_column_int(assets, 0);
        data = sqlite3_column_text(assets, 1);
        checksum = sqlite3_column_text(assets, 2);

        switch (type) {
            case ASSET_CWD:
                (void) strlcpy(cwd, data == NULL ? pkg->prefix : data, sizeof(cwd));
                if (mport_chdir(mport, cwd) != MPORT_OK)
                    goto ERROR;

                break;
            case ASSET_CHMOD:
                printf("asset_chmod %s, %s\n", mode, data);
                if (mode != NULL)
                    free(mode);
                /* TODO: should we reset the mode rather than NULL here */
                if (data == NULL)
                    mode = NULL;
                else
                    mode = strdup(data);
                break;
            case ASSET_CHOWN:
                owner = mport_get_uid(data);
                break;
            case ASSET_CHGRP:
                group = mport_get_gid(data);
                break;
            case ASSET_PREEXEC:
                if (mport_run_asset_exec(mport, data, cwd, file) != MPORT_OK)
                    goto ERROR;
                break;
            default:
                /* do nothing */
                break;
        }
    }
    sqlite3_finalize(assets);
    mport_pkgmeta_logevent(mport, pkg, "preexec");

    return MPORT_OK;

    ERROR:
        sqlite3_finalize(assets);
        RETURN_CURRENT_ERROR;
}

static int
do_actual_install(mportInstance *mport, mportBundleRead *bundle, mportPackageMeta *pkg)
{
    int file_total, ret;
    int file_count = 0;
    mportAssetListEntryType type;
    struct archive_entry *entry;
    const char *data, *checksum;
    char *orig_cwd;
    uid_t owner = 0; /* root */
    gid_t group = 0; /* wheel */
    mode_t *set;
    mode_t newmode;
    char *mode = NULL;
    struct stat sb;
    char file[FILENAME_MAX], cwd[FILENAME_MAX], dir[FILENAME_MAX];
    sqlite3_stmt *assets = NULL, *count, *insert = NULL;
    sqlite3 *db;

    db = mport->db;

    /* sadly, we can't just use abs pathnames, because it will break hardlinks */
    orig_cwd = getcwd(NULL, 0);

    /* get the file count for the progress meter */
    if (mport_db_prepare(db, &count,
                         "SELECT COUNT(*) FROM stub.assets WHERE (type=%i or type=%i or type=%i) AND pkg=%Q",
                         ASSET_FILE, ASSET_SAMPLE, ASSET_SHELL, pkg->name) != MPORT_OK)
        RETURN_CURRENT_ERROR;

    switch (sqlite3_step(count)) {
        case SQLITE_ROW:
            file_total = sqlite3_column_int(count, 0);
            sqlite3_finalize(count);
            break;
        default:
            SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
            sqlite3_finalize(count);
            RETURN_CURRENT_ERROR;
    }

    mport_call_progress_init_cb(mport, "Installing %s-%s", pkg->name, pkg->version);

    /* Insert the package meta row into the packages table (We use pack here because things might have been twiddled) */
    /* Note that this will be marked as dirty by default */
    if (mport_db_do(db,
                    "INSERT INTO packages (pkg, version, origin, prefix, lang, options, comment, os_release, cpe, locked) VALUES (%Q,%Q,%Q,%Q,%Q,%Q,%Q,%Q,%Q,0)",
                    pkg->name, pkg->version, pkg->origin, pkg->prefix, pkg->lang, pkg->options, pkg->comment,
                    pkg->os_release, pkg->cpe) != MPORT_OK)
        goto ERROR;

    /* Insert the assets into the master table (We do this one by one because we want to insert file
     * assets as absolute paths. */
    if (mport_db_prepare(db, &insert, "INSERT INTO assets (pkg, type, data, checksum) values (%Q,?,?,?)", pkg->name) !=
        MPORT_OK)
        goto ERROR;
    /* Insert the depends into the master table */
    if (mport_db_do(db,
                    "INSERT INTO depends (pkg, depend_pkgname, depend_pkgversion, depend_port) SELECT pkg,depend_pkgname,depend_pkgversion,depend_port FROM stub.depends WHERE pkg=%Q",
                    pkg->name) != MPORT_OK)
        goto ERROR;
    /* Insert the categories into the master table */
    if (mport_db_do(db, "INSERT INTO categories (pkg, category) SELECT pkg, category FROM stub.categories WHERE pkg=%Q",
                    pkg->name) != MPORT_OK)
        goto ERROR;

    if (mport_db_prepare(db, &assets, "SELECT type,data,checksum FROM stub.assets WHERE pkg=%Q", pkg->name) != MPORT_OK)
        goto ERROR;

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

        type     = (mportAssetListEntryType) sqlite3_column_int(assets, 0);
        data     = sqlite3_column_text(assets, 1);
        checksum = sqlite3_column_text(assets, 2);

        switch (type) {
            case ASSET_CWD:
                (void) strlcpy(cwd, data == NULL ? pkg->prefix : data, sizeof(cwd));
                if (mport_chdir(mport, cwd) != MPORT_OK)
                    goto ERROR;

                break;
            case ASSET_CHMOD:
                printf("asset_chmod %s, %s\n", mode, data);
                if (mode != NULL)
                    free(mode);
                /* TODO: should we reset the mode rather than NULL here */
                if (data == NULL)
                    mode = NULL;
                else
                    mode = strdup(data);
                break;
            case ASSET_CHOWN:
                owner = mport_get_uid(data);
                break;
            case ASSET_CHGRP:
                group = mport_get_gid(data);
                break;
            case ASSET_DIR:
            case ASSET_DIRRM:
            case ASSET_DIRRMTRY:
                /* TODO: handle mode properly */
                if (stat(data, &sb) == -1)
                    mkdir(data, 0755); /* XXX: we ignore error because it's most likely already there */
                break;
            case ASSET_EXEC:
                if (mport_run_asset_exec(mport, data, cwd, file) != MPORT_OK)
                    goto ERROR;
                break;
            case ASSET_FILE:
                /* FALLS THROUGH */
            case ASSET_SHELL:
                /* FALLS THROUGH */
            case ASSET_SAMPLE:
                if (mport_bundle_read_next_entry(bundle, &entry) != MPORT_OK)
                    goto ERROR;

                (void) snprintf(file, FILENAME_MAX, "%s%s/%s", mport->root, cwd, data);
                archive_entry_set_pathname(entry, file);

                if (mport_bundle_read_extract_next_file(bundle, entry) != MPORT_OK)
                    goto ERROR;

                if (lstat(file, &sb))
                    goto ERROR;

                if (S_ISREG(sb.st_mode)) {
                    /* Set the owner and group */
                    if (chown(file, owner, group) == -1)
                        goto ERROR;

                    /* Set the file permissions, assumes non NFSv4 */
                    if (mode != NULL) {
                        if (stat(file, &sb))
                            goto ERROR;
                        if ((set = setmode(mode)) == NULL)
                            goto ERROR;
                        newmode = getmode(set, sb.st_mode);
                        free(set);
                        if (chmod(file, newmode))
                            goto ERROR;
                    }

                    /* shell registration */
                    if (type == ASSET_SHELL) {
                        if (mport_shell_register(file) != MPORT_OK)
                            goto ERROR;
                    }

                    /* for sample files, if we don't have an existing file
                       make a new one */
                    if (type == ASSET_SAMPLE) {
                        char nonSample[FILENAME_MAX];
                        strlcpy(nonSample, file, FILENAME_MAX);
                        char *sptr = strcasestr(nonSample, ".sample");
                        if (sptr != NULL) {
                            sptr[0] = '\0'; /* hack off .sample */
                            if (!mport_file_exists(nonSample)) {
                                mport_copy_file(file, nonSample);
                            }
                        }
                    }
                }

                (mport->progress_step_cb)(++file_count, file_total, file);

                break;
            default:
                /* do nothing */
                break;
        }

        /* insert this asset into the master database */
        if (sqlite3_bind_int(insert, 1, (int) type) != SQLITE_OK) {
            SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
            goto ERROR;
        }
        if (type == ASSET_FILE || type == ASSET_SAMPLE || type == ASSET_SHELL) {
            /* don't put the root in the database! */
            if (sqlite3_bind_text(insert, 2, file + strlen(mport->root), -1, SQLITE_STATIC) != SQLITE_OK) {
                SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
                goto ERROR;
            }
            if (sqlite3_bind_text(insert, 3, checksum, -1, SQLITE_STATIC) != SQLITE_OK) {
                SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
                goto ERROR;
            }
        } else if (type == ASSET_DIR || type == ASSET_DIRRM || type == ASSET_DIRRMTRY) {
            (void) snprintf(dir, FILENAME_MAX, "%s/%s", cwd, data);
            if (sqlite3_bind_text(insert, 2, dir, -1, SQLITE_STATIC) != SQLITE_OK) {
                SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
                goto ERROR;
            }

            if (sqlite3_bind_null(insert, 3) != SQLITE_OK) {
                SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
                goto ERROR;
            }
        } else {
            if (sqlite3_bind_text(insert, 2, data, -1, SQLITE_STATIC) != SQLITE_OK) {
                SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
                goto ERROR;
            }

            if (sqlite3_bind_null(insert, 3) != SQLITE_OK) {
                SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
                goto ERROR;
            }
        }

        if (sqlite3_step(insert) != SQLITE_DONE) {
            SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(db));
            goto ERROR;
        }

        sqlite3_reset(insert);
    }
    sqlite3_finalize(assets);
    sqlite3_finalize(insert);
    if (mport_db_do(db, "UPDATE packages SET status='clean' WHERE pkg=%Q", pkg->name) != MPORT_OK)
        goto ERROR;
    mport_pkgmeta_logevent(mport, pkg, "Installed");

    (mport->progress_free_cb)();
    (void) mport_chdir(NULL, orig_cwd);
    free(orig_cwd);
    return MPORT_OK;

    ERROR:
        sqlite3_finalize(assets);
        sqlite3_finalize(insert);
        (mport->progress_free_cb)();
        free(orig_cwd);
        //rollback();
        RETURN_CURRENT_ERROR;
}           


#define COPY_METAFILE(TYPE)	(void)snprintf(from, FILENAME_MAX, "%s/%s/%s-%s/%s", bundle->tmpdir, MPORT_STUB_INFRA_DIR, pkg->name, pkg->version, TYPE); \
                                if (mport_file_exists(from)) { \
                                  (void)snprintf(to, FILENAME_MAX, "%s%s/%s-%s/%s", mport->root, MPORT_INST_INFRA_DIR, pkg->name, pkg->version, TYPE); \
                                  if (mport_mkdir(dirname(to)) != MPORT_OK) \
                                    RETURN_CURRENT_ERROR; \
                                  if (mport_copy_file(from, to) != MPORT_OK) \
                                    RETURN_CURRENT_ERROR; \
                                }
                                
static int
do_post_install(mportInstance *mport, mportBundleRead *bundle, mportPackageMeta *pkg)
{
    char to[FILENAME_MAX], from[FILENAME_MAX];

    COPY_METAFILE(MPORT_MTREE_FILE);
    COPY_METAFILE(MPORT_INSTALL_FILE);
    COPY_METAFILE(MPORT_DEINSTALL_FILE);
    COPY_METAFILE(MPORT_MESSAGE_FILE);

    if (run_postexec(mport, bundle, pkg) != MPORT_OK)
        RETURN_CURRENT_ERROR;

    if (display_pkg_msg(mport, bundle, pkg) != MPORT_OK)
        RETURN_CURRENT_ERROR;

    return run_pkg_install(mport, bundle, pkg, "POST-INSTALL");
}

static int run_postexec(mportInstance *mport, mportBundleRead *bundle, mportPackageMeta *pkg)
{
    int ret;
    char cwd[FILENAME_MAX];
    sqlite3_stmt *assets = NULL, *count, *insert = NULL;
    sqlite3 *db;
    mportAssetListEntryType type;
    const char *data, *checksum;
    mode_t *set;
    mode_t newmode;
    char *mode = NULL;

    db = mport->db;

    /* Process @postexec steps */
    if (mport_db_prepare(db, &assets, "SELECT type,data,checksum FROM stub.assets WHERE pkg=%Q", pkg->name) != MPORT_OK)
        goto ERROR;

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

        type = (mportAssetListEntryType) sqlite3_column_int(assets, 0);
        data = sqlite3_column_text(assets, 1);
        checksum = sqlite3_column_text(assets, 2);

        char file[FILENAME_MAX];
        if (data == NULL) {
            snprintf(file, sizeof(file), "%s", mport->root);
        } else if (*data == '/') {
            snprintf(file, sizeof(file), "%s%s", mport->root, data);
        } else {
            snprintf(file, sizeof(file), "%s%s/%s", mport->root, pkg->prefix, data);
        }

        switch (type) {
            case ASSET_CWD:
                (void) strlcpy(cwd, data == NULL ? pkg->prefix : data, sizeof(cwd));
                if (mport_chdir(mport, cwd) != MPORT_OK)
                    goto ERROR;

                break;
            case ASSET_CHMOD:
                printf("asset_chmod %s, %s\n", mode, data);
                if (mode != NULL)
                    free(mode);
                /* TODO: should we reset the mode rather than NULL here */
                if (data == NULL)
                    mode = NULL;
                else
                    mode = strdup(data);
                break;
            case ASSET_CHOWN:
                owner = mport_get_uid(data);
                break;
            case ASSET_CHGRP:
                group = mport_get_gid(data);
                break;
            case ASSET_POSTEXEC:
                if (mport_run_asset_exec(mport, data, cwd, file) != MPORT_OK)
                    goto ERROR;
                break;
            default:
                /* do nothing */
                break;
        }
    }
    sqlite3_finalize(assets);
    mport_pkgmeta_logevent(mport, pkg, "postexec");

    return MPORT_OK;

    ERROR:
    sqlite3_finalize(assets);
    RETURN_CURRENT_ERROR;
}


static int
run_mtree(mportInstance *mport, mportBundleRead *bundle, mportPackageMeta *pkg)
{
    char file[FILENAME_MAX];
    int ret;

    (void) snprintf(file, FILENAME_MAX, "%s/%s/%s-%s/%s", bundle->tmpdir, MPORT_STUB_INFRA_DIR, pkg->name, pkg->version,
                    MPORT_MTREE_FILE);

    if (mport_file_exists(file)) {
        if ((ret = mport_xsystem(mport, "%s -U -f %s -d -e -p %s >/dev/null", MPORT_MTREE_BIN, file, pkg->prefix)) != 0)
            RETURN_ERRORX(MPORT_ERR_FATAL, "%s returned non-zero: %i", MPORT_MTREE_BIN, ret);
    }

    return MPORT_OK;
}


static int
run_pkg_install(mportInstance *mport, mportBundleRead *bundle, mportPackageMeta *pkg, const char *mode)
{
    char file[FILENAME_MAX];
    int ret;

    (void) snprintf(file, FILENAME_MAX, "%s/%s/%s-%s/%s", bundle->tmpdir, MPORT_STUB_INFRA_DIR, pkg->name, pkg->version,
                    MPORT_INSTALL_FILE);

    if (mport_file_exists(file)) {
        if (chmod(file, 755) != 0)
            RETURN_ERRORX(MPORT_ERR_FATAL, "chmod(%s, 755): %s", file, strerror(errno));

        if ((ret = mport_xsystem(mport, "PKG_PREFIX=%s %s %s %s", pkg->prefix, file, pkg->name, mode)) != 0)
            RETURN_ERRORX(MPORT_ERR_FATAL, "%s %s returned non-zero: %i", MPORT_INSTALL_FILE, mode, ret);
    }

    return MPORT_OK;
}
 


static int
display_pkg_msg(mportInstance *mport, mportBundleRead *bundle, mportPackageMeta *pkg)
{
    char filename[FILENAME_MAX];
    char *buf;
    struct stat st;
    FILE *file;

    (void) snprintf(filename, FILENAME_MAX, "%s/%s/%s-%s/%s", bundle->tmpdir, MPORT_STUB_INFRA_DIR, pkg->name,
                    pkg->version, MPORT_MESSAGE_FILE);

    if (stat(filename, &st) == -1)
        /* if we couldn't stat the file, we assume there isn't a pkg-msg */
        return MPORT_OK;

    if ((file = fopen(filename, "r")) == NULL)
        RETURN_ERRORX(MPORT_ERR_FATAL, "Couldn't open %s: %s", filename, strerror(errno));

    if ((buf = (char *) calloc((st.st_size + 1), sizeof(char))) == NULL)
        RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");

    if (fread(buf, 1, st.st_size, file) != (size_t) st.st_size) {
        free(buf);
        RETURN_ERRORX(MPORT_ERR_FATAL, "Read error: %s", strerror(errno));
    }

    buf[st.st_size] = 0;

    mport_call_msg_cb(mport, buf);

    free(buf);

    return MPORT_OK;
}