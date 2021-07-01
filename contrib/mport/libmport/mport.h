/* 
 * Copyright (c) 2013, 2014, 2021 Lucas Holt
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

#ifndef _MPORT_H_
#define _MPORT_H_


#include <sys/cdefs.h>
#include <archive.h>
#include <sqlite3.h>
#include <sys/queue.h>
#include <stdio.h>
#include <stdbool.h>

typedef void (*mport_msg_cb)(const char *);
typedef void (*mport_progress_init_cb)(const char *);
typedef void (*mport_progress_step_cb)(int, int, const char *);
typedef void (*mport_progress_free_cb)(void);
typedef int (*mport_confirm_cb)(const char *, const char *, const char *, int);

/* Mport Instance (an installed copy of the mport system) */
#define MPORT_INST_HAVE_INDEX 1
#define MPORT_LOCAL_PKG_PATH "/var/db/mport/downloads"

typedef struct {
  int flags;
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
void mport_default_progress_init_cb(const char *);
void mport_default_progress_step_cb(int, int, const char *);
void mport_default_progress_free_cb(void);

enum _AssetListEntryType {
    ASSET_INVALID, ASSET_FILE, ASSET_CWD, ASSET_CHMOD, ASSET_CHOWN, ASSET_CHGRP,
    ASSET_COMMENT, ASSET_IGNORE, ASSET_NAME, ASSET_EXEC, ASSET_UNEXEC,
    ASSET_SRC, ASSET_DISPLY, ASSET_PKGDEP, ASSET_CONFLICTS, ASSET_MTREE,
    ASSET_DIRRM, ASSET_DIRRMTRY, ASSET_IGNORE_INST, ASSET_OPTION, ASSET_ORIGIN,
    ASSET_DEPORIGIN, ASSET_NOINST, ASSET_DISPLAY, ASSET_DIR,
    ASSET_SAMPLE, ASSET_SHELL,
    ASSET_PREEXEC, ASSET_PREUNEXEC, ASSET_POSTEXEC, ASSET_POSTUNEXEC,
    ASSET_FILE_OWNER_MODE, ASSET_DIR_OWNER_MODE, 
    ASSET_SAMPLE_OWNER_MODE, ASSET_LDCONFIG, ASSET_LDCONFIG_LINUX,
    ASSET_RMEMPTY
};

typedef enum _AssetListEntryType mportAssetListEntryType;

struct _AssetListEntry {
	mportAssetListEntryType type;
	char *data;
	char *checksum;
	char *owner;
	char *group;
	char *mode;

	STAILQ_ENTRY(_AssetListEntry) next;
};

STAILQ_HEAD(_AssetList, _AssetListEntry);

typedef struct _AssetList mportAssetList;
typedef struct _AssetListEntry mportAssetListEntry;

mportAssetList* mport_assetlist_new(void);
void mport_assetlist_free(mportAssetList *);
int mport_parse_plistfile(FILE *, mportAssetList *);

/* Package Meta-data structure */
typedef struct {
    char *name;
    char *version;
    char *lang;
    char *options;
    char *comment;
    char *desc;
    char *prefix;
    char *origin;
    char **categories;
    char *os_release;
    char *cpe;
    int locked;
    char *deprecated;
    time_t expiration_date;
    int no_provide_shlib;
    char *flavor;
} mportPackageMeta;

int mport_asset_get_assetlist(mportInstance *, mportPackageMeta *, mportAssetList **);
int mport_asset_get_package_from_file_path(mportInstance *, const char *, mportPackageMeta **);

mportPackageMeta * mport_pkgmeta_new(void);
void mport_pkgmeta_free(mportPackageMeta *);
void mport_pkgmeta_vec_free(mportPackageMeta **);
int mport_pkgmeta_search_master(mportInstance *, mportPackageMeta ***, const char *, ...);
int mport_pkgmeta_list(mportInstance *mport, mportPackageMeta ***ref);
int mport_pkgmeta_get_downdepends(mportInstance *, mportPackageMeta *, mportPackageMeta ***);
int mport_pkgmeta_get_updepends(mportInstance *, mportPackageMeta *, mportPackageMeta ***);


/* index */
typedef struct {
  char *pkgname;
  char *version;
  char *comment;
  char *bundlefile;
  char *license;
  char *hash;
} mportIndexEntry;

int mport_index_load(mportInstance *);
int mport_index_get(mportInstance *);
int mport_index_check(mportInstance *, mportPackageMeta *);
int mport_index_list(mportInstance *, mportIndexEntry ***);
int mport_index_lookup_pkgname(mportInstance *, const char *, mportIndexEntry ***);
int mport_index_search(mportInstance *, mportIndexEntry ***, const char *, ...);
void mport_index_entry_free_vec(mportIndexEntry **);
void mport_index_entry_free(mportIndexEntry *);

/* Index Depends */

typedef struct {
  char *pkgname;
  char *version;
  char *d_pkgname;
  char *d_version;
} mportDependsEntry;

int mport_index_depends_list(mportInstance *, const char *, const char *, mportDependsEntry ***);
void mport_index_depends_free_vec(mportDependsEntry **);
void mport_index_depends_free(mportDependsEntry *);


/* Info */
char * mport_info(mportInstance *mport, const char *packageName);

/* Package creation */

typedef struct {
  char *pkg_filename;
  char *sourcedir;
  char **depends;
  char *mtree;
  char **conflicts;
  char *pkginstall;
  char *pkgdeinstall;
  char *pkgmessage;
  bool is_backup;
} mportCreateExtras;  

mportCreateExtras * mport_createextras_new(void);
void mport_createextras_free(mportCreateExtras *);

int mport_create_primative(mportAssetList *, mportPackageMeta *, mportCreateExtras *);

/* Merge primative */
int mport_merge_primative(const char **, const char *);

/* Package installation */
int mport_install(mportInstance *, const char *, const char *, const char *);
int mport_install_primative(mportInstance *, const char *, const char *);

/* package updating */
int mport_update(mportInstance *, const char *);
int mport_update_primative(mportInstance *, const char *);

/* package upgrade */
int mport_upgrade(mportInstance *);

/* Package deletion */
int mport_delete_primative(mportInstance *, mportPackageMeta *, int);

/* package verify */
int mport_verify_package(mportInstance *, mportPackageMeta *);

/* version comparing */
int mport_version_cmp(const char *, const char *);

/* fetch XXX: This should become private */
int mport_fetch_bundle(mportInstance *, const char *);
int mport_download(mportInstance *, const char *, char **);

/* Errors */
int mport_err_code(void);
const char * mport_err_string(void);


#define MPORT_OK			    0
#define MPORT_ERR_FATAL 		1
#define MPORT_ERR_WARN			2

/* Clean */
int mport_clean_database(mportInstance *);
int mport_clean_oldpackages(mportInstance *);

/* Setting */
char * mport_setting_get(mportInstance *, const char *);
int mport_setting_set(mportInstance *, const char *, const char *);

/* Utils */
void mport_parselist(char *, char ***);
int mport_verify_hash(const char *, const char *);
int mport_file_exists(const char *);
char * mport_version(void);
char * mport_get_osrelease(void);

/* Locks */
enum _LockState {
	MPORT_UNLOCKED, MPORT_LOCKED
};

typedef enum _LockState mportLockState;

int mport_lock_lock(mportInstance *, mportPackageMeta *);
int mport_lock_unlock(mportInstance *, mportPackageMeta *);
int mport_lock_islocked(mportPackageMeta *);

/* Statistics */
typedef struct {
    unsigned int pkg_installed;
    unsigned int pkg_available;
    /* off_t pkg_installed_size;
       off_t pkg_available_size; */
} mportStats;

int mport_stats(mportInstance *, mportStats **);
int mport_stats_free(mportStats *);
mportStats * mport_stats_new(void);

/* Import/Export */
int mport_import(mportInstance*,  char *);
int mport_export(mportInstance*, char *);

#endif /* ! defined _MPORT_H */
