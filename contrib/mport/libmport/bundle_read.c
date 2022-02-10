/*-
 * Copyright (c) 2009 Chris Reinhardt
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

#include "mport.h"
#include "mport_private.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <archive_entry.h>

/*
 * mport_bundle_read_new()
 *
 * allocate a new read bundle struct.  Returns null if no memory could
 * be had.
 */
mportBundleRead *
mport_bundle_read_new(void)
{
	return (mportBundleRead *)calloc(1, sizeof(mportBundleRead));
}

/*
 * mport_bundle_read_init(bundle, filename)
 *
 * connect the bundle struct to the file at filename.
 */
int
mport_bundle_read_init(mportBundleRead *bundle, const char *filename)
{
	if (filename == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "Filename is null");

	if ((bundle->filename = strdup(filename)) == NULL) {
		RETURN_ERROR(MPORT_ERR_FATAL, "Couldn't dup filename");
	}

	if ((bundle->archive = archive_read_new()) == NULL) {
		RETURN_ERROR(MPORT_ERR_FATAL, "Couldn't initialize archive read");
	}

	if (archive_read_support_format_tar(bundle->archive) != ARCHIVE_OK) {
		RETURN_ERROR(MPORT_ERR_FATAL, archive_error_string(bundle->archive));
	}
	if (archive_read_support_filter_xz(bundle->archive) != ARCHIVE_OK) {
		RETURN_ERROR(MPORT_ERR_FATAL, archive_error_string(bundle->archive));
	}

	if (archive_read_open_filename(bundle->archive, bundle->filename, 10240) != ARCHIVE_OK) {
		RETURN_ERROR(MPORT_ERR_FATAL, archive_error_string(bundle->archive));
	}

	return (MPORT_OK);
}

/*
 * mport_bundle_read_finish(bundle)
 *
 * close the file connected to the bundle, and free any memory allocated.
 */
int
mport_bundle_read_finish(mportInstance *mport, mportBundleRead *bundle)
{
	int ret = MPORT_OK;

	if (bundle == NULL) {
		mport_call_msg_cb(mport, "Package is no longer open.");
		RETURN_ERROR(MPORT_ERR_FATAL, "Bundle is null");
	}

	if (bundle->archive != NULL) {
		if (archive_read_close(bundle->archive) != ARCHIVE_OK) {
			mport_call_msg_cb(mport, "Unable to close pacakge.");
			ret = SET_ERROR(MPORT_ERR_FATAL, archive_error_string(bundle->archive));
		}
		
		if (ret != MPORT_ERR_FATAL && archive_read_free(bundle->archive) != ARCHIVE_OK) {
			mport_call_msg_cb(mport, "Unable to free memory used by package archive file.");
			ret = SET_ERROR(MPORT_ERR_FATAL, archive_error_string(bundle->archive));
		}
	}

	if (bundle->stub_attached && (mport != NULL)) {
		if (mport_detach_stub_db(mport->db) != MPORT_OK) {
			mport_call_msg_cb(mport, "Stub database could not be detatched.");
			ret = mport_err_code();
		}
	}

	if (bundle->tmpdir != NULL) {
		if (mport_rmtree(bundle->tmpdir) != MPORT_OK) {
			ret = mport_err_code();
		}
	}

	free(bundle->tmpdir);
	free(bundle->filename);
	free(bundle);

	return (ret);
}

/*
 * mport_bundle_read_extract_metafiles(bundle, &dirnamep)
 *
 * creates a temporary directory containing all the meta files.  It is
 * expected that this will be called before next_entry() or next_file(),
 * terrible things might happen if you don't do this!
 *
 * The calling code should free the memory that dirnamep points to.
 */
int
mport_bundle_read_extract_metafiles(mportBundleRead *bundle, char **dirnamep)
{
	/* extract the meta-files into a temp dir */
	char filepath[FILENAME_MAX];
	const char *file;
	char dirtmpl[] = "/tmp/mport.XXXXXXXX";
	char *tmpdir = mkdtemp(dirtmpl);
	struct archive_entry *entry;

	if (tmpdir == NULL) {
		RETURN_ERROR(MPORT_ERR_FATAL, strerror(errno));
	}

	if ((*dirnamep = strdup(tmpdir)) == NULL) {
		RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");
	}

	while (1) {
		if (mport_bundle_read_next_entry(bundle, &entry) != MPORT_OK) {
			RETURN_CURRENT_ERROR;
		}

		if (entry == NULL)
			break;

		file = archive_entry_pathname(entry);

		if (*file == '+') {
			(void)snprintf(
			    filepath, FILENAME_MAX, "%s/%s", tmpdir, file);
			archive_entry_set_pathname(entry, filepath);

			if (mport_bundle_read_extract_next_file(
				bundle, entry) != MPORT_OK)
				RETURN_CURRENT_ERROR;
		} else {
			/* entry points to the first real file in the bundle, so
			 * we want to hold on to that until next_entry() is
			 * called
			 */
			bundle->firstreal = entry;
			break;
		}
	}

	return (MPORT_OK);
}

/*
 * mport_bundle_read_skip_metafiles(bundle)
 *
 * Skip all the metafiles, leaving the bundle ready for reading datafiles.
 */
int
mport_bundle_read_skip_metafiles(mportBundleRead *bundle)
{
	struct archive_entry *entry;

	while (1) {
		if (mport_bundle_read_next_entry(bundle, &entry) != MPORT_OK)
			RETURN_CURRENT_ERROR;

		if (*(archive_entry_pathname(entry)) != '+') {
			bundle->firstreal = entry;
			break;
		}
	}

	return (MPORT_OK);
}

/*
 * mport_bundle_read_next_entry(bundle, &entry)
 *
 * sets entry to the next file entry in the bundle.
 *
 * If the end the archive is reached, then this function will return MPORT_OK
 * and entry will be set to NULL.
 */
int
mport_bundle_read_next_entry(mportBundleRead *bundle, struct archive_entry **entryp)
{
	int ret;

	if (bundle->firstreal != NULL) {
		/* handle the lookahead issue with extracting metafiles */
		*entryp = bundle->firstreal;
		bundle->firstreal = NULL;
		return MPORT_OK;
	}

	while (1) {
		ret = archive_read_next_header(bundle->archive, entryp);

		if (ret == ARCHIVE_RETRY) {
			continue;
		}

		if (ret == ARCHIVE_FATAL) {
			RETURN_ERROR(MPORT_ERR_FATAL, archive_error_string(bundle->archive));
		}

		/* ret was warn or OK, we're done */
		if (ret == ARCHIVE_EOF) {
			*entryp = NULL;
		}

		break;
	}

	return MPORT_OK;
}

/*
 * mport_bundle_read_extract_next_file(bundle, entry)
 *
 * extract the next file in the bundle, based on the settings in entry.
 * If you need to change things like perms or paths, you can do so by
 * modifing the entry struct before you pass it to this function
 */
/* XXX - should this be implemented as a macro? inline? */
int
mport_bundle_read_extract_next_file(mportBundleRead *bundle, struct archive_entry *entry)
{
	if (archive_read_extract(bundle->archive, entry,
		ARCHIVE_EXTRACT_OWNER | ARCHIVE_EXTRACT_PERM |
		    ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_ACL |
		    ARCHIVE_EXTRACT_FFLAGS) != ARCHIVE_OK) {
		RETURN_ERROR(MPORT_ERR_FATAL, archive_error_string(bundle->archive));
	}

	return (MPORT_OK);
}

/*
 * mport_bundle_read_prep_for_install(mport, bundle)
 *
 * Extract the metafile into a tmpdir that the bundle maintains, attach the stub
 * db for the instance master database.
 */
int
mport_bundle_read_prep_for_install(
    mportInstance *mport, mportBundleRead *bundle)
{
	sqlite3_stmt *stmt;
	int bundle_version;
	int ret;

	if (mport_bundle_read_extract_metafiles(bundle, &(bundle->tmpdir)) != MPORT_OK) {
		RETURN_CURRENT_ERROR;
	}

	if (mport_attach_stub_db(mport->db, bundle->tmpdir) != MPORT_OK) {
		RETURN_CURRENT_ERROR;
	}

	bundle->stub_attached = 1;

	if (mport_db_prepare(mport->db, &stmt,
		"SELECT value FROM stub.meta WHERE field='bundle_format_version'") !=
	    MPORT_OK) {
		sqlite3_finalize(stmt);
		RETURN_CURRENT_ERROR;
	}
	ret = sqlite3_step(stmt);

	switch (ret) {
	case SQLITE_ROW:
		bundle_version = sqlite3_column_int(stmt, 0);
		sqlite3_finalize(stmt);

		if (bundle_version > MPORT_BUNDLE_VERSION) {
			RETURN_ERRORX(MPORT_ERR_FATAL,
			    "%s: bundle is version %i; this version of mport only supports up to version %i",
			    bundle->filename, bundle_version, MPORT_BUNDLE_VERSION);
		}
		break;
	case SQLITE_DONE:
		sqlite3_finalize(stmt);
		RETURN_ERRORX(MPORT_ERR_FATAL, "%s: no stub.meta table, or no bundle_format_version field",
		    bundle->filename);
	default:
		sqlite3_finalize(stmt);
		RETURN_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(mport->db));
	}

	return (MPORT_OK);
}
