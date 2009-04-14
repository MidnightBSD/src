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

#ifndef _MPORT_H_
#define _MPORT_H_


#include <sys/cdefs.h>
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

/* For now this is just the FreeBSD list, this will change soon. */
enum _AssetListEntryType { 
  ASSET_INVALID, ASSET_FILE, ASSET_CWD, ASSET_CHMOD, ASSET_CHOWN, ASSET_CHGRP,
  ASSET_COMMENT, ASSET_IGNORE, ASSET_NAME, ASSET_EXEC, ASSET_UNEXEC,
  ASSET_SRC, ASSET_DISPLY, ASSET_PKGDEP, ASSET_CONFLICTS, ASSET_MTREE,
  ASSET_DIRRM, ASSET_DIRRMTRY, ASSET_IGNORE_INST, ASSET_OPTION, ASSET_ORIGIN,
  ASSET_DEPORIGIN, ASSET_NOINST, ASSET_DISPLAY
};

typedef enum _AssetListEntryType mportAssetListEntryType;

struct _AssetListEntry {
  mportAssetListEntryType type;
  char *data;
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
} mportPackageMeta;  


mportPackageMeta * mport_pkgmeta_new(void);
void mport_pkgmeta_free(mportPackageMeta *);
void mport_pkgmeta_vec_free(mportPackageMeta **);
int mport_pkgmeta_search_master(mportInstance *, mportPackageMeta ***, const char *, ...);
int mport_pkgmeta_get_downdepends(mportInstance *, mportPackageMeta *, mportPackageMeta ***);
int mport_pkgmeta_get_updepends(mportInstance *, mportPackageMeta *, mportPackageMeta ***);



typedef struct {
  char *pkg_filename;
  char *sourcedir;
  char **depends;
  char *mtree;
  char **conflicts;
  char *pkginstall;
  char *pkgdeinstall;
  char *pkgmessage;
} mportCreateExtras;  

mportCreateExtras * mport_createextras_new(void);
void mport_createextras_free(mportCreateExtras *);

/* Package creation */
int mport_create_primative(mportAssetList *, mportPackageMeta *, mportCreateExtras *);

/* Merge primative */
int mport_merge_primative(const char **, const char *);

/* Package installation */
int mport_install_primative(mportInstance *, const char *, const char *);

/* package updating */
int mport_update_primative(mportInstance *, const char *);

/* Package deletion */
int mport_delete_primative(mportInstance *, mportPackageMeta *, int);


/* version comparing */
int mport_version_cmp(const char *, const char *);

/* Errors */
int mport_err_code(void);
const char * mport_err_string(void);


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
#define MPORT_ERR_MALFORMED_BUNDLE	17
#define MPORT_ERR_NOT_UPGRADABLE	18


/* Utils */
void mport_parselist(char *, char ***);


#endif /* ! defined _MPORT_H */

