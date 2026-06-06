/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2011, 2023 Lucas Holt
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

#include "mport.h"
#include "mport_private.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sha256.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>

static int verify_hash_fd(int, const char *);

MPORT_PUBLIC_API int
mport_clean_database(mportInstance *mport)
{
	int error_code = MPORT_OK;

	if (mport_db_do(mport->db, "vacuum") != MPORT_OK) {
		error_code = mport_err_code();
		mport_call_msg_cb(mport, "Database maintenance failed: %s\n", mport_err_string());
	} else {
		mport_call_msg_cb(mport, "Database maintenance complete.\n");
	}

	return error_code;
}

MPORT_PUBLIC_API int
mport_clean_oldpackages(mportInstance *mport)
{
	int error_code = MPORT_OK;

	int deleted = 0;
	int dfd;
	struct dirent *de;
	DIR *d;

	dfd = open(MPORT_FETCH_STAGING_DIR, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
	if (dfd == -1) {
		error_code = SET_ERRORX(MPORT_ERR_FATAL, "Couldn't open directory %s: %s",
		    MPORT_FETCH_STAGING_DIR, strerror(errno));
		return error_code;
	}

	d = fdopendir(dfd);
	if (d == NULL) {
		close(dfd);
		error_code = SET_ERRORX(MPORT_ERR_FATAL, "Couldn't open directory %s: %s",
		    MPORT_FETCH_STAGING_DIR, strerror(errno));
		return error_code;
	}

	while ((de = readdir(d)) != NULL) {
		mportIndexEntry **indexEntry;
		char *path;
		if (strcmp(".", de->d_name) == 0 || strcmp("..", de->d_name) == 0)
			continue;

		if (mport_index_search(mport, &indexEntry, "bundlefile=%Q", de->d_name) !=
		    MPORT_OK) {
			mport_call_msg_cb(mport, "failed to search index %s: ", mport_err_string());
			continue;
		}

		if (asprintf(&path, "%s/%s", MPORT_FETCH_STAGING_DIR, de->d_name) == -1) {
			if (indexEntry != NULL) {
				mport_index_entry_free_vec(indexEntry);
				indexEntry = NULL;
			}
			continue;
		}

		if (indexEntry == NULL || *indexEntry == NULL) {
			if (unlinkat(dfd, de->d_name, 0) < 0) {
				error_code = SET_ERRORX(MPORT_ERR_FATAL,
				    "Could not delete file %s: %s", path, strerror(errno));
				mport_call_msg_cb(mport, "%s\n", mport_err_string());
			} else {
				deleted++;
			}
		} else {
			int fd;
			struct stat st;
			struct stat current;

			fd = openat(dfd, de->d_name, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
			if (fd == -1) {
				mport_call_msg_cb(
				    mport, "Could not open %s: %s", path, strerror(errno));
			} else if (fstat(fd, &st) == -1) {
				mport_call_msg_cb(
				    mport, "Could not stat %s: %s", path, strerror(errno));
				close(fd);
			} else if (!S_ISREG(st.st_mode)) {
				close(fd);
			} else if (verify_hash_fd(fd, (*indexEntry)->hash) == 0) {
				if (fstatat(dfd, de->d_name, &current, AT_SYMLINK_NOFOLLOW) == -1) {
					error_code = SET_ERRORX(MPORT_ERR_FATAL,
					    "Could not stat file %s: %s", path, strerror(errno));
					mport_call_msg_cb(mport, "%s\n", mport_err_string());
				} else if (current.st_dev == st.st_dev &&
				    current.st_ino == st.st_ino) {
					if (unlinkat(dfd, de->d_name, 0) < 0) {
						error_code = SET_ERRORX(MPORT_ERR_FATAL,
						    "Could not delete file %s: %s", path,
						    strerror(errno));
						mport_call_msg_cb(
						    mport, "%s\n", mport_err_string());
					} else {
						deleted++;
					}
				}
				close(fd);
			} else {
				close(fd);
			}
			mport_index_entry_free_vec(indexEntry);
			indexEntry = NULL;
		}

		free(path);
		path = NULL;
	}

	closedir(d);

	mport_call_msg_cb(mport, "Cleaned up %d packages.\n", deleted);

	return error_code;
}

static int
verify_hash_fd(int fd, const char *hash)
{
	SHA256_CTX ctx;
	unsigned char digest[32];
	char filehash[65];
	unsigned char buffer[8192];
	ssize_t len;

	if (hash == NULL)
		return 1;

	if (lseek(fd, 0, SEEK_SET) == -1)
		return 1;

	SHA256_Init(&ctx);
	while (1) {
		len = read(fd, buffer, sizeof(buffer));
		if (len == -1) {
			if (errno == EINTR)
				continue;
			return 1;
		}
		if (len == 0)
			break;
		SHA256_Update(&ctx, buffer, (size_t)len);
	}
	SHA256_Final(digest, &ctx);

	for (int i = 0; i < 32; i++)
		snprintf(filehash + (i * 2), sizeof(filehash) - (i * 2), "%02x", digest[i]);

	return strncmp(filehash, hash, 64);
}

MPORT_PUBLIC_API int
mport_clean_oldmtree(mportInstance *mport)
{
	int error_code = MPORT_OK;

	int deleted = 0;
	struct dirent *de;
	DIR *d = opendir(MPORT_INST_INFRA_DIR);

	if (d == NULL) {
		error_code = SET_ERRORX(MPORT_ERR_FATAL, "Couldn't open directory %s: %s",
		    MPORT_INST_INFRA_DIR, strerror(errno));
		return error_code;
	}

	while ((de = readdir(d)) != NULL) {
		mportPackageMeta **packs;
		char *path;
		if (strcmp(".", de->d_name) == 0 || strcmp("..", de->d_name) == 0)
			continue;

		char packageName[128];
		strncpy(packageName, de->d_name, 127);
		packageName[127] = '\0';
		char *dash = strrchr(packageName, '-');
		if (dash != NULL) {
			*dash = '\0';
		}

		if (mport_pkgmeta_search_master(mport, &packs, "pkg=%Q", packageName) != MPORT_OK) {
			mport_call_msg_cb(
			    mport, "failed to search master database for %s: ", mport_err_string());
			continue;
		}

		if (asprintf(&path, "%s/%s", MPORT_INST_INFRA_DIR, de->d_name) == -1) {
			if (packs != NULL) {
				mport_pkgmeta_vec_free(packs);
				packs = NULL;
			}
			continue;
		}

		if (packs == NULL || *packs == NULL) {
			if (mport_rmtree(path) != MPORT_OK) {
				error_code = SET_ERRORX(MPORT_ERR_FATAL,
				    "Could not delete file %s: %s", path, strerror(errno));
				mport_call_msg_cb(mport, "%s\n", mport_err_string());
			} else {
				deleted++;
			}
		} else {
			mport_pkgmeta_vec_free(packs);
			packs = NULL;
		}

		free(path);
		path = NULL;
	}

	closedir(d);

	mport_call_msg_cb(mport, "Cleaned up %d mtrees.\n", deleted);

	return error_code;
}

MPORT_PUBLIC_API int
mport_clean_tempfiles(mportInstance *mport)
{
	int error_code = MPORT_OK;

	int deleted = 0;
	struct dirent *de;
	DIR *d = opendir(_PATH_TMP);

	if (d == NULL) {
		error_code = SET_ERRORX(MPORT_ERR_FATAL, "Couldn't open directory %s: %s",
		    MPORT_INST_INFRA_DIR, strerror(errno));
		return error_code;
	}

	while ((de = readdir(d)) != NULL) {
		char *path;
		if (strcmp(".", de->d_name) == 0 || strcmp("..", de->d_name) == 0)
			continue;

		if (!mport_starts_with("mport.", de->d_name))
			continue;

		if (asprintf(&path, "%s%s", _PATH_TMP, de->d_name) == -1) {
			continue;
		}

		int result = unlink(path);

		if (result != 0) {
			error_code = SET_ERRORX(
			    MPORT_ERR_FATAL, "Could not delete file %s: %s", path, strerror(errno));
			mport_call_msg_cb(mport, "%s\n", mport_err_string());
		} else {
			deleted++;
		}

		free(path);
		path = NULL;
	}

	closedir(d);

	mport_call_msg_cb(mport, "Cleaned up %d temporary files.\n", deleted);

	return error_code;
}
