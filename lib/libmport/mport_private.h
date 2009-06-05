/* $MidnightBSD: src/lib/libmport/mport.h,v 1.10 2008/04/26 17:59:26 ctriv Exp $
 *
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

#ifdef DEBUGGING
#include <err.h>
#define DIAG(fmt, ...) warnx(fmt, ## __VA_ARGS__);
#else
#define DIAG(...) 
#endif

#include <osreldate.h>

#define MPORT_PUBLIC_API 

#define MPORT_BUNDLE_VERSION 1
#define MPORT_BUNDLE_VERSION_STR "1"

/* callback syntaxtic sugar */
void mport_call_msg_cb(mportInstance *, const char *, ...);
void mport_call_progress_init_cb(mportInstance *, const char *, ...);


/* precondition checking */
#define MPORT_PRECHECK_INSTALLED   1
#define MPORT_PRECHECK_DEPENDS     2
#define MPORT_PRECHECK_CONFLICTS   4
#define MPORT_PRECHECK_UPGRADEABLE 8
int mport_check_preconditions(mportInstance *, mportPackageMeta *, int);

/* schema */
int mport_generate_master_schema(sqlite3 *);
int mport_generate_stub_schema(sqlite3 *);

/* Various database convience functions */
int mport_attach_stub_db(sqlite3 *, const char *);
int mport_detach_stub_db(sqlite3 *);
int mport_db_do(sqlite3 *, const char *, ...);
int mport_db_prepare(sqlite3 *, sqlite3_stmt **, const char *, ...);

/* pkgmeta */
int mport_pkgmeta_read_stub(mportInstance *, mportPackageMeta ***);
int mport_pkgmeta_get_assetlist(mportInstance *, mportPackageMeta *, mportAssetList **);
int mport_pkgmeta_logevent(mportInstance *, mportPackageMeta *, const char *);


/* Utils */
int mport_copy_file(const char *, const char *);
int mport_rmtree(const char *);
int mport_mkdir(const char *);
int mport_rmdir(const char *, int);
int mport_chdir(mportInstance *, const char *);
int mport_file_exists(const char *);
int mport_xsystem(mportInstance *mport, const char *, ...);
int mport_run_asset_exec(mportInstance *mport, const char *, const char *, const char *);
void mport_free_vec(void *);



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


/* version compare functions */
void mport_version_cmp_sqlite(sqlite3_context *, int, sqlite3_value **);
int mport_version_require_check(char *, char *);

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

/* Instance files */
#define MPORT_INST_DIR 		"/var/db/mport"
#define MPORT_MASTER_DB_FILE	"/var/db/mport/master.db"
#define MPORT_INST_INFRA_DIR	"/var/db/mport/infrastructure"
#define MPORT_INDEX_FILE	"/var/db/mport/index.db"
#define MPORT_FETCH_STAGING_DIR "/var/db/mport/downloads"


#if defined(__i386__)
#define MPORT_ARCH "i386"
#elif defined(__amd64__)
#define MPORT_ARCH "amd64"
#elif defined(__sparc64__)
#define MPORT_ARCH "sparc64"
#else
#error "Unable to detect arch!"
#endif

#if __MidnightBSD_version >= 3000
#define MPORT_OSVERSION "0.3"
#elif __MidnightBSD_version < 3000 && __MidnightBSD_version >= 2000
#define MPORT_OSVERSION "0.2"
#else
#error "libmport only supports MidnightBSD versions 0.2 and greater."
#endif


/* fetch stuff */
#define MPORT_URL_PATH			MPORT_ARCH "/" MPORT_OSVERSION
#define MPORT_INDEX_URL_PATH		MPORT_URL_PATH "/index.db.bz2"
#define MPORT_BOOTSTRAP_INDEX_URL 	"http://index.mport.midnightbsd.org/" MPORT_URL_PATH "/index.db.bz2"

int mport_fetch_index(mportInstance *);
int mport_fetch_bootstrap_index(mportInstance *);
int mport_fetch_bundle(mportInstance *, const char *);

/* a few index things */
int mport_index_get_mirror_list(mportInstance *, char ***);

#define MPORT_CHECK_FOR_INDEX(mport, func) if (!(mport->flags & MPORT_INST_HAVE_INDEX)) RETURN_ERRORX(MPORT_ERR_FATAL, "Attempt to use %s before loading index.", func);
#define MPORT_MAX_INDEX_AGE 3600 * 24 * 7 /* two weeks */

/* Binaries we use */
#define MPORT_MTREE_BIN		"/usr/sbin/mtree"
#define MPORT_SH_BIN		"/bin/sh"
#define MPORT_CHROOT_BIN	"/usr/sbin/chroot"

#define MPORT_URL_MAX		512


#endif /* _MPORT_PRIV_H_ */
