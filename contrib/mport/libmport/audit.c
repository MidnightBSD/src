/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2023 Lucas Holt
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <err.h>
#include <unistd.h>

#include <ucl.h>

static char *readJsonFile(char *jsonFile);

MPORT_PUBLIC_API char *
mport_audit(mportInstance *mport, const char *packageName, bool dependOn)
{
	mportPackageMeta **packs = NULL;
	mportPackageMeta **depends, **depends_orig = NULL;
	char *pkgAudit = NULL;
	struct ucl_parser *parser = NULL;

	if (mport == NULL) {
		SET_ERROR(MPORT_ERR_FATAL, "mport not initialized");
		return (NULL);
	}

	if (packageName == NULL) {
		SET_ERROR(MPORT_ERR_FATAL, "Package name not found.");
		return (NULL);
	}

	if (mport_pkgmeta_search_master(mport, &packs, "pkg=%Q", packageName) != MPORT_OK) {
		return (NULL);
	}

	if (packs != NULL) {
		if ((*packs)->cpe != NULL && (*packs)->cpe[0] != '\0') {
			char *path = mport_fetch_cves(mport, (*packs)->cpe);
			char *jsonData = readJsonFile(path);
			if (jsonData == NULL) {
				SET_ERROR(MPORT_ERR_FATAL, "Error opening CVE file");
				unlink(path);
				return (NULL);
			}

			size_t len = strlen(jsonData);

			// Create a UCL parser and parse the JSON string
			parser = ucl_parser_new(0);
			if (!ucl_parser_add_chunk(parser, (const unsigned char *)jsonData, len)) {
				SET_ERRORX(MPORT_ERR_FATAL, "Failed to parse JSON: %s\n",
				    ucl_parser_get_error(parser));
				ucl_parser_free(parser);

				free(jsonData);
				unlink(path);

				return (NULL);
			}

			ucl_object_t *root = ucl_parser_get_object(parser);
			ucl_parser_free(parser);

			const ucl_object_t *cur;
			ucl_object_iter_t it = NULL;

			size_t size;
			FILE *bufferFp = open_memstream(&pkgAudit, &size);
			if (bufferFp == NULL) {
				SET_ERROR(MPORT_ERR_FATAL, "Error allocating memory for audit entries");
				free(jsonData);
				unlink(path);
				free(path);
				return (NULL);
			}

			bool first = true;
			while ((cur = ucl_object_iterate(root, &it, true))) {
				if (ucl_object_type(cur) != UCL_OBJECT) {
					SET_ERROR(MPORT_ERR_FATAL, "Expected an object in the array");
					continue;
				}

				if (mport->quiet) {
					fprintf(bufferFp, "%s-%s\n", (*packs)->name, (*packs)->version);
					break;
				}

				if (first) {
					fprintf(bufferFp, "%s-%s is vulnerable:\n\n",
					    (*packs)->name, (*packs)->version);
					first = false;
				}

				const ucl_object_t *cveId = ucl_object_find_key(cur, "cveId");
				if (cveId != NULL && ucl_object_type(cveId) == UCL_STRING) {
					fprintf(bufferFp, "%s\n", ucl_object_tostring(cveId));

					const ucl_object_t *desc = ucl_object_find_key(cur, "description");
					if (desc != NULL && ucl_object_type(desc) == UCL_STRING) {
						fprintf(bufferFp, "Description: %s\n", ucl_object_tostring(desc));
					}
					const ucl_object_t *severity = ucl_object_find_key(cur, "severity");
					if (severity != NULL && ucl_object_type(severity) == UCL_STRING) {
						fprintf(bufferFp, "Severity: %s\n", ucl_object_tostring(severity));
					}
					fprintf(bufferFp, "\n");
				}
			}

			if (dependOn) {
				if (mport_pkgmeta_get_downdepends(mport, (*packs), &depends_orig) == MPORT_OK) {
					if (depends_orig != NULL) {
						depends = depends_orig;
						fprintf(bufferFp, "Packages that depend on %s:", (*packs)->name);
						while (*depends != NULL) {
							fprintf(bufferFp, " %s", (*depends)->name);
							depends++;
						}
						fprintf(bufferFp, "\n");

						mport_pkgmeta_vec_free(depends_orig);
					}
				}
			}

			free(jsonData);
			fclose(bufferFp);
			unlink(path);
			free(path);
			ucl_object_unref(root);

			mport_pkgmeta_vec_free(packs);
			packs = NULL;
		}

	}
	return pkgAudit;
}

static char *readJsonFile(char *jsonFile)
{
		FILE *fp;
		size_t size;
		char *buffer;

		// Open the JSON file
		fp = fopen(jsonFile, "rb");
		if (!fp) {
			SET_ERROR(MPORT_ERR_WARN, "could not open file");
			return NULL;
		}

		// Get the file size
		fseek(fp, 0, SEEK_END);
		size = ftell(fp);
		fseek(fp, 0, SEEK_SET);

		// Allocate memory for the file contents
		buffer = (char *)malloc(size);
		if (!buffer) {
			SET_ERROR(MPORT_ERR_WARN, "could not allocate memory");
			fclose(fp);
			return NULL;
		}

		// Read the file into the buffer
		if (fread(buffer, 1, size, fp) != size) {
			SET_ERROR(MPORT_ERR_WARN, "could not read file");
			fclose(fp);
			free(buffer);
			return NULL;
		}

		// Close the file
		fclose(fp);

		return buffer;
	}
