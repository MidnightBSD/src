/* $MidnightBSD$
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
__MBSDID("$MidnightBSD: src/usr.sbin/pkg_install/lib/plist.c,v 1.50.2.1 2006/01/10 22:15:06 krion Exp $");



/* plist stuff */

#include <sys/queue.h>
#include <stdio.h>


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

mportPlist* mport_new_plist(void);
void mport_free_plist(mportPlist *);
int mport_parse_plist_file(FILE *, mportPlist *);

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
  char *req_script;
} mportPackageMeta;  

mportPackageMeta * mport_new_packagemeta(void);
void mport_free_packagemeta(mportPackageMeta *);

/* Package creation */
int mport_create_pkg(mportPlist *, mportPackageMeta *);

#include <sqlite3.h>

/* schema */
void mport_generate_plist_schema(sqlite3 *);
void mport_generate_package_schema(sqlite3 *);

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

#define RETURN_ERROR(code, msg) return mport_set_errx((code), "Error at %s:(%d): %s", __FILE__, __LINE__, (msg))

/* Utils */

int mport_copy_file(const char *, const char *);
int mport_file_exists(const char *);

#endif
