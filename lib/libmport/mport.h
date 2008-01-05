/* $MidnightBSD: src/lib/libmport/mport.h,v 1.8 2007/12/05 17:02:15 ctriv Exp $
 *
 * Copyright (c) 2007 Chris Reinhardt
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

#ifndef _MPORT_H_
#define _MPORT_H_


#include <sys/cdefs.h>
__MBSDID("$MidnightBSD: src/lib/libmport/mport.h,v 1.8 2007/12/05 17:02:15 ctriv Exp $");


#include <archive.h>
#include <sqlite3.h>
#include <sys/queue.h>
#include <stdio.h>

typedef void (*mport_msg_cb)(const char *);
typedef void (*mport_progress_init_cb)(void);
typedef void (*mport_progress_step_cb)(int, int, const char *);
typedef void (*mport_progress_free_cb)(void);
typedef int (*mport_confirm_cb)(const char *, const char *, const char *, int);

/* Mport Instance (an installed copy of the mport system) */
typedef struct {
  sqlite3 *db;
  char *root;
  mport_msg_cb msg_cb;
  mport_progress_init_cb progress_init_cb;
  mport_progress_step_cb progress_step_cb;
  mport_progress_free_cb progress_free_cb;
  mport_confirm_cb confirm_cb;
} mportInstance;

mportInstance * mport_instance_new(void);
int mport_instance_init(mportInstance *, const char *);
int mport_instance_free(mportInstance *);

void mport_set_msg_cb(mportInstance *, mport_msg_cb);
void mport_set_progress_init_cb(mportInstance *, mport_progress_init_cb);
void mport_set_progress_step_cb(mportInstance *, mport_progress_step_cb);
void mport_set_progress_free_cb(mportInstance *, mport_progress_free_cb);
void mport_set_confirm_cb(mportInstance *, mport_confirm_cb);

void mport_default_msg_cb(const char *);
int mport_default_confirm_cb(const char *, const char *, const char *, int);
void mport_default_progress_init_cb(void);
void mport_default_progress_step_cb(int, int, const char *);
void mport_default_progress_free_cb(void);


/* Mport Bundle (a file containing packages) */
typedef struct {
  struct archive *archive;
  char *filename;
} mportBundle;

mportBundle* mport_bundle_new(void);
int mport_bundle_init(mportBundle *, const char *);
int mport_bundle_finish(mportBundle *);
int mport_bundle_add_file(mportBundle *, const char *, const char *);


/* For now this is just the FreeBSD list, this will change soon. */
enum _PlistEntryType { 
  PLIST_INVALID, PLIST_FILE, PLIST_CWD, PLIST_CHMOD, PLIST_CHOWN, PLIST_CHGRP,
  PLIST_COMMENT, PLIST_IGNORE, PLIST_NAME, PLIST_EXEC, PLIST_UNEXEC,
  PLIST_SRC, PLIST_DISPLY, PLIST_PKGDEP, PLIST_CONFLICTS, PLIST_MTREE,
  PLIST_DIRRM, PLIST_DIRRMTRY, PLIST_IGNORE_INST, PLIST_OPTION, PLIST_ORIGIN,
  PLIST_DEPORIGIN, PLIST_NOINST, PLIST_DISPLAY
};

typedef enum _PlistEntryType mportPlistEntryType;

struct _PlistEntry {
  mportPlistEntryType type;
  char *data;
  STAILQ_ENTRY(_PlistEntry) next;
};

STAILQ_HEAD(_Plist, _PlistEntry);

typedef struct _Plist mportPlist;
typedef struct _PlistEntry mportPlistEntry;

mportPlist* mport_plist_new(void);
void mport_plist_free(mportPlist *);
int mport_plist_parsefile(FILE *, mportPlist *);

/* Package Meta-data structure */

typedef struct {
  char *pkg_filename;
  char *name;
  char *version;
  char *lang;
  char *options;
  char *comment;
  char *sourcedir;
  char *desc;
  char *prefix;
  char **depends;
  char *mtree;
  char *origin;
  char **conflicts;
  char *pkginstall;
  char *pkgdeinstall;
  char *pkgmessage;
} mportPackageMeta;  

mportPackageMeta * mport_packagemeta_new(void);
void mport_packagemeta_free(mportPackageMeta *);
void mport_packagemeta_vec_free(mportPackageMeta **);


/* Package creation */
int mport_create_primative(mportPlist *, mportPackageMeta *);

/* Package installation */
int mport_install_primative(mportInstance *, const char *, const char *);

/* Package deletion */
int mport_delete_primative(mportInstance *, mportPackageMeta *, int);

/* precondition checking */
int mport_check_update_preconditions(mportInstance *, mportPackageMeta *);
int mport_check_install_preconditions(mportInstance *, mportPackageMeta *);

/* schema */
int mport_generate_master_schema(sqlite3 *);
int mport_generate_stub_schema(sqlite3 *);

/* Various database convience functions */
int mport_attach_stub_db(sqlite3 *, const char *);
int mport_detach_stub_db(sqlite3 *);
int mport_get_meta_from_stub(sqlite3 *, mportPackageMeta ***);
int mport_get_meta_from_master(mportInstance *, mportPackageMeta ***, const char *, ...);
int mport_db_do(sqlite3 *, const char *, ...);
int mport_db_prepare(sqlite3 *, sqlite3_stmt **, const char *, ...);


/* version comparing */
int mport_version_cmp(const char *, const char *);


/* Errors */
int mport_err_code(void);
char * mport_err_string(void);

int mport_set_err(int, const char *);
int mport_set_errx(int , const char *, ...);

#define MPORT_OK			0
#define MPORT_ERR_NO_MEM 		1
#define MPORT_ERR_FILEIO 		2
#define MPORT_ERR_MALFORMED_PLIST 	3
#define MPORT_ERR_SQLITE		4
#define MPORT_ERR_FILE_NOT_FOUND	5
#define MPORT_ERR_SYSCALL_FAILED	6
#define MPORT_ERR_ARCHIVE		7
#define MPORT_ERR_INTERNAL		8
#define MPORT_ERR_ALREADY_INSTALLED	9
#define MPORT_ERR_CONFLICTS		10
#define MPORT_ERR_MISSING_DEPEND	11
#define MPORT_ERR_MALFORMED_VERSION	12
#define MPORT_ERR_MALFORMED_DEPEND	13
#define MPORT_ERR_NO_SUCH_PKG		14
#define MPORT_ERR_CHECKSUM_MISMATCH	15
#define MPORT_ERR_UPWARDS_DEPENDS	16

#define RETURN_CURRENT_ERROR return mport_err_code()
#define RETURN_ERROR(code, msg) return mport_set_errx((code), "Error at %s:(%d): %s", __FILE__, __LINE__, (msg))
#define SET_ERROR(code, msg) mport_set_errx((code), "Error at %s:(%d): %s", __FILE__, __LINE__, (msg))
#define RETURN_ERRORX(code, fmt, ...) return mport_set_errx((code), "Error at %s:(%d): " fmt, __FILE__, __LINE__, __VA_ARGS__)
#define SET_ERRORX(code, fmt, ...) mport_set_errx((code), "Error at %s:(%d): " fmt, __FILE__, __LINE__, __VA_ARGS__)


/* Utils */
int mport_copy_file(const char *, const char *);
int mport_rmtree(const char *);
int mport_mkdir(const char *);
int mport_rmdir(const char *, int);
int mport_file_exists(const char *);
int mport_xsystem(mportInstance *mport, const char *, ...);
void mport_parselist(char *, char ***);
int mport_run_plist_exec(mportInstance *mport, const char *, const char *, const char *);



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

/* Binaries we use */
#define MPORT_MTREE_BIN		"/usr/sbin/mtree"
#define MPORT_SH_BIN		"/bin/sh"
#define MPORT_CHROOT_BIN	"/usr/sbin/chroot"

#endif /* ! defined _MPORT_H */

