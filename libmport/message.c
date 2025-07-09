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
#include <errno.h>
#include <unistd.h>
#include <syslog.h>
#include <stdarg.h>
#include <archive_entry.h>
#include <ucl.h>

int
mport_pkg_message_display(mportInstance *mport, mportPackageMeta *pkg)
{
	mportPackageMessage packageMessage;
	pkg_message_t expectedType;

	packageMessage.minimum_version = NULL;
	packageMessage.maximum_version = NULL;
	packageMessage.str = NULL;
	packageMessage.prev = NULL;
	packageMessage.next = NULL;
	packageMessage.type = PKG_MESSAGE_ALWAYS; // default type

	if (mport_pkg_message_load(mport, pkg, &packageMessage) != MPORT_OK) {
		RETURN_CURRENT_ERROR;
	}

	switch (pkg->action) {
	case MPORT_ACTION_INSTALL:
		expectedType = PKG_MESSAGE_INSTALL;
		break;
	case MPORT_ACTION_UPDATE:
	case MPORT_ACTION_UPGRADE:
		expectedType = PKG_MESSAGE_UPGRADE;
		break;
	case MPORT_ACTION_DELETE:
		expectedType = PKG_MESSAGE_REMOVE;
		break;
	default:
		expectedType = PKG_MESSAGE_INSTALL;
	}

	/* Limit message display based on version if provided. */
	if (packageMessage.minimum_version != NULL && 
		mport_version_cmp(packageMessage.minimum_version, pkg->version) == 1) {
			free(packageMessage.str);
			packageMessage.str = NULL;
			return MPORT_OK;
	}

	if (packageMessage.maximum_version != NULL && 
		mport_version_cmp(packageMessage.maximum_version, pkg->version) == -1) {
			free(packageMessage.str);
			packageMessage.str = NULL;
			return MPORT_OK;
	}


	if (packageMessage.type == expectedType || packageMessage.type == PKG_MESSAGE_ALWAYS) {
		if (packageMessage.str != NULL && packageMessage.str[0] != '\0')
			mport_call_msg_cb(mport, "%s", packageMessage.str);
	}

	free(packageMessage.str);
	packageMessage.str = NULL;

	return MPORT_OK;
}

int
mport_pkg_message_load(
    mportInstance *mport, mportPackageMeta *pkg, mportPackageMessage *packageMessage)
{
	char filename[FILENAME_MAX];
	char *buf;
	struct stat st;
	FILE *file;
	struct ucl_parser *parser;
	ucl_object_t *obj;

	/* Assumes copy_metafile has run on install already */
	(void)snprintf(filename, FILENAME_MAX, "%s%s/%s-%s/%s", mport->root, MPORT_INST_INFRA_DIR,
	    pkg->name, pkg->version, MPORT_MESSAGE_FILE);

	if (stat(filename, &st) == -1) {
		/* if we couldn't stat the file, we assume there isn't a pkg-msg */
		return MPORT_OK;
	}

	if ((file = fopen(filename, "re")) == NULL)
		RETURN_ERRORX(MPORT_ERR_FATAL, "Couldn't open %s: %s", filename, strerror(errno));

	if ((buf = (char *)calloc((size_t)(st.st_size + 1), sizeof(char))) == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");

	if (fread(buf, sizeof(char), (size_t)st.st_size, file) != (size_t)st.st_size) {
		free(buf);
		RETURN_ERRORX(MPORT_ERR_FATAL, "Read error: %s", strerror(errno));
	}

	buf[st.st_size] = '\0';

	if (buf[0] == '[') {
		parser = ucl_parser_new(0);
		// remove leading/trailing array entries
		buf[0] = ' ';
		buf[st.st_size - 1] = '\0';

		if (ucl_parser_add_chunk(parser, (const unsigned char *)buf, st.st_size)) {
			obj = ucl_parser_get_object(parser);
			ucl_parser_free(parser);
			free(buf);

			packageMessage = mport_pkg_message_from_ucl(mport, obj, packageMessage);
			ucl_object_unref(obj);

			return packageMessage == NULL ? MPORT_ERR_FATAL : MPORT_OK;
		}

		ucl_parser_free(parser);
	} else {
		/*obj = ucl_object_fromlstring(buf, st.st_size);
		packageMessage = mport_pkg_message_from_ucl(mport, obj, packageMessage);
		ucl_object_unref(obj); */
		packageMessage->str = strdup(buf);
		packageMessage->type = PKG_MESSAGE_ALWAYS;
		free(buf);
	}

	return MPORT_OK;
}

mportPackageMessage *
mport_pkg_message_from_ucl(mportInstance *mport, const ucl_object_t *obj, mportPackageMessage *msg)
{
	const ucl_object_t *enhanced;

	if (ucl_object_type(obj) == UCL_STRING) {
		msg->str = strdup(ucl_object_tostring(obj));
	} else if (ucl_object_type(obj) == UCL_OBJECT) {
		/* New format of pkg message */
		enhanced = ucl_object_find_key(obj, "message");
		if (enhanced == NULL || ucl_object_type(enhanced) != UCL_STRING) {
			return NULL;
		}
		msg->str = strdup(ucl_object_tostring(enhanced));

		enhanced = ucl_object_find_key(obj, "minimum_version");
		if (enhanced != NULL && ucl_object_type(enhanced) == UCL_STRING) {
			msg->minimum_version = strdup(ucl_object_tostring(enhanced));
		}

		enhanced = ucl_object_find_key(obj, "maximum_version");
		if (enhanced != NULL && ucl_object_type(enhanced) == UCL_STRING) {
			msg->maximum_version = strdup(ucl_object_tostring(enhanced));
		}

		enhanced = ucl_object_find_key(obj, "type");
		if (enhanced != NULL && ucl_object_type(enhanced) == UCL_STRING) {
			const char *type = ucl_object_tostring(enhanced);
			if (type != NULL) {
				if (strcmp(type, "install") == 0) {
					msg->type = PKG_MESSAGE_INSTALL;
				} else if (strcmp(type, "upgrade") == 0) {
					msg->type = PKG_MESSAGE_UPGRADE;
				} else if (strcmp(type, "remove") == 0) {
					msg->type = PKG_MESSAGE_REMOVE;
				} else {
					msg->type = PKG_MESSAGE_ALWAYS;
				}
			}
		}
	}

	return msg;
}
