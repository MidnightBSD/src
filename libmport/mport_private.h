/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2011, 2013, 2015, 2021-2024 Lucas Holt
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
 *
 */

#ifndef _MPORT_PRIV_H_
#define _MPORT_PRIV_H_

#ifdef DEBUG
#include <err.h>
#define DIAG(fmt, ...) warnx(fmt, ## __VA_ARGS__);
#else
#define DIAG(...) 
#endif

#if defined(__MidnightBSD__)
#include <osreldate.h>
#include <ohash.h>
#else
struct ohash_info {

};

struct ohash {

};
#endif
#include <sqlite3.h>
#include <ucl.h>
#include <zstd.h>

#include <tllist.h>

#define MPORT_PUBLIC_API 

#define MPORT_MASTER_VERSION 13
#define MPORT_BUNDLE_VERSION 6
#define MPORT_BUNDLE_VERSION_STR "6"
#define MPORT_VERSION "2.7.2"

#define MPORT_SETTING_MIRROR_REGION "mirror_region"
#define MPORT_SETTING_TARGET_OS "target_os"

/* precondition checking */
#define MPORT_PRECHECK_INSTALLED   1
#define MPORT_PRECHECK_DEPENDS     2
#define MPORT_PRECHECK_CONFLICTS   4
#define MPORT_PRECHECK_UPGRADEABLE 8
#define MPORT_PRECHECK_OS          16
#define MPORT_PRECHECK_MOVED       32
#define MPORT_PRECHECK_DEPRECATED  64
int mport_check_preconditions(mportInstance *, mportPackageMeta *, long);

/* schema */
int mport_generate_master_schema(sqlite3 *);
int mport_generate_stub_schema(mportInstance *, sqlite3 *);
int mport_upgrade_master_schema(sqlite3 *, int);

/* instance */
int mport_get_database_version(sqlite3 *);
int mport_set_database_version(sqlite3 *);

/* Various database convenience functions */
int mport_attach_stub_db(sqlite3 *, const char *);
int mport_detach_stub_db(sqlite3 *);
int mport_db_do(sqlite3 *, const char *, ...);
int mport_db_prepare(sqlite3 *, sqlite3_stmt **, const char *, ...);
int mport_db_count(sqlite3 *, int *, const char *, ...);

/* pkgmeta */
int mport_pkgmeta_read_stub(mportInstance *, mportPackageMeta ***);
int mport_pkgmeta_logevent(mportInstance *, mportPackageMeta *, const char *);

/* Service */
typedef enum {
    SERVICE_START,
    SERVICE_STOP
} service_action_t;
int mport_start_stop_service(mportInstance *mport, mportPackageMeta *pack, service_action_t action);

/* Utils */
bool mport_starts_with(const char *, const char *);
char* mport_hash_file(const char *);
char* mport_extract_hash_from_file(const char *);
int mport_copy_file(const char *, const char *);
int mport_copy_fd(int, int);
uid_t mport_get_uid(const char *);
gid_t mport_get_gid(const char *);
char* mport_directory(const char *path);
int mport_rmtree(const char *);
int mport_mkdir(const char *);
int mport_mkdirp(char *, mode_t);
int mport_removeflags(const char *, const char *);
int mport_rmdir(const char *, int);
int mport_chdir(mportInstance *, const char *);
int mport_xsystem(mportInstance *, const char *, ...);
int mport_run_asset_exec(mportInstance *, const char *, const char *, const char *);
void mport_free_vec(void *);
int mport_decompress_zstd(const char *, const char *);
int mport_shell_register(const char *);
int mport_shell_unregister(const char *);
char * mport_str_remove(const char *str, const char ch);
time_t mport_get_time(void);
bool mport_check_answer_bool(char *answer);
int mport_count_spaces(const char *str);
char * mport_tokenize(char **args);

enum parse_states {
	START,
	ORDINARY_TEXT,
	OPEN_SINGLE_QUOTES,
	IN_SINGLE_QUOTES,
	OPEN_DOUBLE_QUOTES,
	IN_DOUBLE_QUOTES,
};

/* Mport Bundle (a file containing packages) */
typedef struct {
  struct archive *archive;
  char *filename;
  struct links_table *links;
} mportBundleWrite;


typedef struct {
  struct archive *archive;
  char *filename;
  char *tmpdir;
  struct archive_entry *firstreal;
  short stub_attached;
} mportBundleRead;


mportBundleWrite* mport_bundle_write_new(void);
int mport_bundle_write_init(mportBundleWrite *, const char *);
int mport_bundle_write_finish(mportBundleWrite *);
int mport_bundle_write_add_file(mportBundleWrite *, const char *, const char *);
int mport_bundle_write_add_entry(mportBundleWrite *, mportBundleRead *, struct archive_entry *);


mportBundleRead* mport_bundle_read_new(void);
int mport_bundle_read_init(mportBundleRead *, const char *);
int mport_bundle_read_finish(mportInstance *, mportBundleRead *);
int mport_bundle_read_prep_for_install(mportInstance *, mportBundleRead *);
int mport_bundle_read_extract_metafiles(mportBundleRead *, char **);
int mport_bundle_read_skip_metafiles(mportBundleRead *);
int mport_bundle_read_next_entry(mportBundleRead *, struct archive_entry **);
int mport_bundle_read_extract_next_file(mportBundleRead *, struct archive_entry *);
int mport_bundle_read_install_pkg(mportInstance *, mportBundleRead *, mportPackageMeta *);
int mport_bundle_read_update_pkg(mportInstance *, mportBundleRead *, mportPackageMeta *);

int mport_install_depends(mportInstance *, const char *, const char *, mportAutomatic);
int mport_update_down(mportInstance *, mportPackageMeta *, struct ohash_info *, struct ohash *);

/* version compare functions */
void mport_version_cmp_sqlite(sqlite3_context *, int, sqlite3_value **);
int mport_version_require_check(const char *, const char *);

int mport_pkg_message_display(mportInstance *, mportPackageMeta *);
int mport_pkg_message_load(mportInstance *, mportPackageMeta *, mportPackageMessage *);
mportPackageMessage* mport_pkg_message_from_ucl(mportInstance *, const ucl_object_t *, mportPackageMessage *);

#define RETURN_CURRENT_ERROR return mport_err_code()
#define RETURN_ERROR(code, msg) return mport_set_errx((code), "Error at %s:(%d): %s", __FILE__, __LINE__, (msg))
#define SET_ERROR(code, msg) mport_set_errx((code), "Error at %s:(%d): %s", __FILE__, __LINE__, (msg))
#define RETURN_ERRORX(code, fmt, ...) return mport_set_errx((code), "Error at %s:(%d): " fmt, __FILE__, __LINE__, __VA_ARGS__)
#define SET_ERRORX(code, fmt, ...) mport_set_errx((code), "Error at %s:(%d): " fmt, __FILE__, __LINE__, __VA_ARGS__)
int mport_set_err(int, const char *);
int mport_set_errx(int , const char *, ...);


/* Infrastructure files */
#define MPORT_STUB_DB_FILE 	"+CONTENTS.db"
#define MPORT_STUB_INFRA_DIR	"+INFRASTRUCTURE"
#define MPORT_MTREE_FILE   	"mtree"
#define MPORT_INSTALL_FILE 	"pkg-install"
#define MPORT_DEINSTALL_FILE	"pkg-deinstall"
#define MPORT_MESSAGE_FILE	"pkg-message"
#define MPORT_LUA_PRE_INSTALL_FILE "pkg-pre-install.lua"
#define MPORT_LUA_POST_INSTALL_FILE "pkg-post-install.lua"
#define MPORT_LUA_PRE_DEINSTALL_FILE "pkg-pre-deinstall.lua"
#define MPORT_LUA_POST_DEINSTALL_FILE "pkg-post-deinstall.lua"

/* Instance files */
#define MPORT_INST_DIR 		"/var/db/mport"
#define MPORT_MASTER_DB_FILE        MPORT_INST_DIR "/master.db"
#define MPORT_INST_INFRA_DIR        MPORT_INST_DIR "/infrastructure"
#define MPORT_INDEX_COMPRESS_EXT    ".zst"
#define MPORT_INDEX_FILE_NAME      "index.db"
#define MPORT_INDEX_FILE_SOURCE     MPORT_INDEX_FILE_NAME MPORT_INDEX_COMPRESS_EXT
#define MPORT_INDEX_FILE            MPORT_INST_DIR "/" MPORT_INDEX_FILE_NAME
#define MPORT_INDEX_FILE_COMPRESSED        MPORT_INST_DIR "/" MPORT_INDEX_FILE_NAME MPORT_INDEX_COMPRESS_EXT
#define MPORT_INDEX_FILE_HASH       MPORT_INST_DIR "/" MPORT_INDEX_FILE_NAME MPORT_INDEX_COMPRESS_EXT ".sha256"
#define MPORT_FETCH_STAGING_DIR     MPORT_INST_DIR "/downloads"

#define MPORT_INSTALL_MEDIA_DIR     "/packages"
#define MPORT_INSTALL_MEDIA_INDEX_FILE  MPORT_INSTALL_MEDIA_DIR "/" MPORT_INDEX_FILE_NAME

#if defined(__i386__)
#define MPORT_ARCH "i386"
#elif defined(__amd64__)
#define MPORT_ARCH "amd64"
#else
#error "Unable to detect arch!"
#endif

#if __MidnightBSD_version >= 400000
#define MPORT_OSVERSION "4.0"
#elif __MidnightBSD_version >= 302000
#define MPORT_OSVERSION "3.2"
#elif __MidnightBSD_version >= 301000
#define MPORT_OSVERSION "3.1"
#elif __MidnightBSD_version >= 300000
#define MPORT_OSVERSION "3.0"
#elif defined(__linux__)
#define MPORT_OSVERSION "linux"
#else
#error "libmport only supports MidnightBSD versions 3.0 and above."
#endif

/* fetch stuff */
#define MPORT_URL_PATH			MPORT_ARCH "/" MPORT_OSVERSION
#define MPORT_INDEX_URL_PATH		MPORT_URL_PATH "/" MPORT_INDEX_FILE_NAME MPORT_INDEX_COMPRESS_EXT
#define MPORT_BOOTSTRAP_INDEX_URL 	"https://index.mport.midnightbsd.org/"
#define MPORT_SECURITY_URL  "https://sec.midnightbsd.org"

int mport_fetch_index(mportInstance *);
int mport_fetch_bootstrap_index(mportInstance *);
char * mport_fetch_cves(mportInstance *mport, char *cpe);

/* a few index things */
int mport_index_get_mirror_list(mportInstance *, char ***, int *);
char * mport_index_file_path(void);

/* script things */
int get_socketpair(int *);
int mport_script_run_child(mportInstance *, int, int *, int, const char*);

#define MPORT_CHECK_FOR_INDEX(mport, func) if (!(mport->flags & MPORT_INST_HAVE_INDEX)) RETURN_ERRORX(MPORT_ERR_FATAL, "Attempt to use %s before loading index.", (func));
#define MPORT_DAY (3600 * 24)
#define MPORT_MAX_INDEX_AGE (MPORT_DAY * 7) /* one week */
#define MPORT_SETTING_INDEX_LAST_CHECKED "index_last_check"
#define MPORT_SETTING_REPO_AUTOUPDATE "index_autoupdate"
#define MPORT_SETTING_HANDLE_RC_SCRIPTS "handle_rc_scripts"

/* Binaries we use */
#define MPORT_MTREE_BIN		"/usr/sbin/mtree"
#define MPORT_CHROOT_BIN	"/usr/sbin/chroot"

#define MPORT_URL_MAX		512

#endif /* _MPORT_PRIV_H_ */
