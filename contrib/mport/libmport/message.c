/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2013-2015, 2021, 2023 Lucas Holt
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

#include "mport.h"
#include "mport_private.h"

#include <sys/stat.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>
#include <stdarg.h>
#include <archive_entry.h>
#include <stdbool.h>
#include <ucl.h>

static /*@null@*/ /*@only@*/ mportPackageMessage *message_new(void);
static void message_append(
    /*@notnull@*/ mportPackageMessage **, /*@notnull@*/ /*@only@*/ mportPackageMessage *);
static int message_from_ucl_entry(
    /*@notnull@*/ const ucl_object_t *, /*@notnull@*/ mportPackageMessage **);
static bool message_ucl_valid(/*@notnull@*/ const ucl_object_t *);
static bool message_applies(
    /*@notnull@*/ const mportPackageMessage *, pkg_message_t, /*@null@*/ const char *);

/*
 * Show the parts of pkg's message that apply to an action.  when is
 * PKG_MESSAGE_INSTALL, PKG_MESSAGE_UPGRADE or PKG_MESSAGE_REMOVE.  For an
 * upgrade, prev_version is the version being replaced; the version range of
 * an upgrade entry is checked against it, as pkg(8) does.
 */
int
mport_pkg_message_display(mportInstance *mport, mportPackageMeta *pkg, pkg_message_t when,
    /*@null@*/ const char *prev_version)
{
	mportPackageMessage *messages = NULL;
	mportPackageMessage *msg;

	if (mport_pkg_message_load(mport, pkg, &messages) != MPORT_OK)
		RETURN_CURRENT_ERROR;

	for (msg = messages; msg != NULL; msg = msg->next) {
		if (msg->str == NULL || msg->str[0] == '\0')
			continue;
		if (message_applies(msg, when, prev_version))
			mport_call_msg_cb(mport, "%s", msg->str);
	}

	mport_pkg_message_free(messages);

	return MPORT_OK;
}

static bool
message_applies(/*@notnull@*/ const mportPackageMessage *msg, pkg_message_t when,
    /*@null@*/ const char *prev_version)
{
	if (msg->type == PKG_MESSAGE_ALWAYS)
		return true;

	if (msg->type != when)
		return false;

	if (msg->type != PKG_MESSAGE_UPGRADE ||
	    (msg->minimum_version == NULL && msg->maximum_version == NULL))
		return true;

	/* The range is about the version being replaced; without it, skip. */
	if (prev_version == NULL)
		return false;

	/* Both ends are exclusive, as in pkg(8). */
	if (msg->minimum_version != NULL &&
	    mport_version_cmp(prev_version, msg->minimum_version) <= 0)
		return false;

	if (msg->maximum_version != NULL &&
	    mport_version_cmp(prev_version, msg->maximum_version) >= 0)
		return false;

	return true;
}

/*
 * Load pkg's installed message into a list of entries.  A file that starts
 * with '[' is a UCL array of entries; anything else, or a file that does not
 * parse as such an array, is plain text shown on every action.  *messages is
 * NULL when the package has no message.  The caller frees the list with
 * mport_pkg_message_free().
 */
int
mport_pkg_message_load(
    mportInstance *mport, mportPackageMeta *pkg, /*@out@*/ mportPackageMessage **messages)
{
	char filename[FILENAME_MAX];
	char *buf = NULL;
	struct stat st;
	FILE *file = NULL;
	struct ucl_parser *parser = NULL;
	ucl_object_t *obj = NULL;
	mportPackageMessage *msg = NULL;
	int ret;

	if (messages != NULL)
		*messages = NULL;

	if (mport == NULL || pkg == NULL || messages == NULL) {
		RETURN_ERROR(
		    MPORT_ERR_FATAL, "Invalid argument(s) passed to mport_pkg_message_load()");
	}

	/* Assumes copy_metafile has run on install already */
	(void)snprintf(filename, FILENAME_MAX, "%s%s/%s-%s/%s", mport->root, MPORT_INST_INFRA_DIR,
	    pkg->name, pkg->version, MPORT_MESSAGE_FILE);

	if (stat(filename, &st) == -1) {
		/* if we couldn't stat the file, we assume there isn't a pkg-msg */
		return MPORT_OK;
	}

	if ((file = fopen(filename, "re")) == NULL)
		RETURN_ERRORX(MPORT_ERR_FATAL, "Couldn't open %s: %s", filename, strerror(errno));

	if ((buf = (char *)calloc((size_t)(st.st_size + 1), sizeof(char))) == NULL) {
		fclose(file);
		RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");
	}

	if (fread(buf, sizeof(char), (size_t)st.st_size, file) != (size_t)st.st_size) {
		free(buf);
		fclose(file);
		RETURN_ERRORX(MPORT_ERR_FATAL, "Read error: %s", strerror(errno));
	}

	fclose(file);
	buf[st.st_size] = '\0';

	if (buf[0] == '[') {
		if ((parser = ucl_parser_new(UCL_PARSER_NO_FILEVARS)) == NULL) {
			free(buf);
			RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");
		}
		/* the variables pkg(8) defines for a message */
		if (pkg->prefix != NULL)
			ucl_parser_register_variable(parser, "PREFIX", pkg->prefix);
		if (pkg->name != NULL)
			ucl_parser_register_variable(parser, "PKGNAME", pkg->name);
		if (pkg->origin != NULL)
			ucl_parser_register_variable(parser, "PKGORIGIN", pkg->origin);

		if (ucl_parser_add_chunk(parser, (const unsigned char *)buf, (size_t)st.st_size))
			obj = ucl_parser_get_object(parser);
		ucl_parser_free(parser);

		if (obj != NULL && message_ucl_valid(obj)) {
			free(buf);
			ret = mport_pkg_message_from_ucl(mport, obj, messages);
			ucl_object_unref(obj);
			if (ret != MPORT_OK) {
				mport_pkg_message_free(*messages);
				*messages = NULL;
			}

			return ret;
		}

		/* Not a message array after all: show the file as it is. */
		if (obj != NULL)
			ucl_object_unref(obj);
	}

	if ((msg = message_new()) == NULL) {
		free(buf);
		RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");
	}
	msg->str = buf;
	msg->type = PKG_MESSAGE_ALWAYS;
	*messages = msg;

	return MPORT_OK;
}

/*
 * A UCL pkg-message is an array of objects that each have a "message"
 * string.  Plain text that happens to start with '[' can still parse as UCL
 * (libucl is lenient), so anything else is not treated as one.
 */
static bool
message_ucl_valid(/*@notnull@*/ const ucl_object_t *obj)
{
	const ucl_object_t *cur;
	const ucl_object_t *elt;
	ucl_object_iter_t it = NULL;

	if (ucl_object_type(obj) != UCL_ARRAY)
		return false;

	while ((cur = ucl_object_iterate(obj, &it, true)) != NULL) {
		if (ucl_object_type(cur) != UCL_OBJECT)
			return false;
		elt = ucl_object_find_key(cur, "message");
		if (elt == NULL || ucl_object_type(elt) != UCL_STRING)
			return false;
	}

	return true;
}

/*
 * Append the entries in a parsed message to *messages: one per element of an
 * array, or one for a lone object or string.  Elements without a "message"
 * string are skipped.
 */
int
mport_pkg_message_from_ucl(
    mportInstance *mport, /*@null@*/ const ucl_object_t *obj, mportPackageMessage **messages)
{
	const ucl_object_t *cur;
	ucl_object_iter_t it = NULL;

	(void)mport;

	if (obj == NULL || messages == NULL)
		return MPORT_OK;

	if (ucl_object_type(obj) != UCL_ARRAY)
		return message_from_ucl_entry(obj, messages);

	while ((cur = ucl_object_iterate(obj, &it, true)) != NULL) {
		if (message_from_ucl_entry(cur, messages) != MPORT_OK)
			RETURN_CURRENT_ERROR;
	}

	return MPORT_OK;
}

static int
message_from_ucl_entry(
    /*@notnull@*/ const ucl_object_t *obj, /*@notnull@*/ mportPackageMessage **messages)
{
	const ucl_object_t *elt;
	const char *type;
	mportPackageMessage *msg;

	if (ucl_object_type(obj) == UCL_STRING) {
		elt = obj;
	} else if (ucl_object_type(obj) == UCL_OBJECT) {
		elt = ucl_object_find_key(obj, "message");
		if (elt == NULL || ucl_object_type(elt) != UCL_STRING)
			return MPORT_OK;
	} else {
		return MPORT_OK;
	}

	if ((msg = message_new()) == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");

	if ((msg->str = strdup(ucl_object_tostring(elt))) == NULL) {
		mport_pkg_message_free(msg);
		RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");
	}

	if (ucl_object_type(obj) == UCL_OBJECT) {
		elt = ucl_object_find_key(obj, "type");
		if (elt != NULL && ucl_object_type(elt) == UCL_STRING) {
			type = ucl_object_tostring(elt);
			if (strcasecmp(type, "install") == 0)
				msg->type = PKG_MESSAGE_INSTALL;
			else if (strcasecmp(type, "upgrade") == 0)
				msg->type = PKG_MESSAGE_UPGRADE;
			else if (strcasecmp(type, "remove") == 0)
				msg->type = PKG_MESSAGE_REMOVE;
		}
	}

	/* pkg(8) only reads a version range for upgrade messages */
	if (msg->type == PKG_MESSAGE_UPGRADE) {
		elt = ucl_object_find_key(obj, "minimum_version");
		if (elt != NULL && ucl_object_type(elt) == UCL_STRING &&
		    (msg->minimum_version = strdup(ucl_object_tostring(elt))) == NULL) {
			mport_pkg_message_free(msg);
			RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");
		}

		elt = ucl_object_find_key(obj, "maximum_version");
		if (elt != NULL && ucl_object_type(elt) == UCL_STRING &&
		    (msg->maximum_version = strdup(ucl_object_tostring(elt))) == NULL) {
			mport_pkg_message_free(msg);
			RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");
		}
	}

	message_append(messages, msg);

	return MPORT_OK;
}

static /*@null@*/ /*@only@*/ mportPackageMessage *
message_new(void)
{
	mportPackageMessage *msg;

	if ((msg = (mportPackageMessage *)calloc(1, sizeof(mportPackageMessage))) == NULL)
		return NULL;
	msg->type = PKG_MESSAGE_ALWAYS;

	return msg;
}

static void
message_append(/*@notnull@*/ mportPackageMessage **messages,
    /*@notnull@*/ /*@only@*/ mportPackageMessage *msg)
{
	mportPackageMessage *tail;

	if (*messages == NULL) {
		*messages = msg;
		return;
	}

	for (tail = *messages; tail->next != NULL; tail = tail->next)
		;
	tail->next = msg;
	msg->prev = tail;
}

void
mport_pkg_message_free(/*@null@*/ /*@only@*/ mportPackageMessage *messages)
{
	mportPackageMessage *next;

	while (messages != NULL) {
		next = messages->next;
		free(messages->str);
		free(messages->minimum_version);
		free(messages->maximum_version);
		free(messages);
		messages = next;
	}
}
