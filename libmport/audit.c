/*-
 * SPDX-License-Identifier: BSD-2-Clause
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
#include <strings.h>
#include <time.h>
#include <err.h>
#include <unistd.h>

#include <ucl.h>

static char *readJsonFile(char *jsonFile);
static bool cve_affects_version(const ucl_object_t *cve, const char *version,
    /*@null@*/ const char *vendor, /*@null@*/ const char *product);
static bool cpe_vendor_product(
    /*@null@*/ const char *cpe, char *vendor, size_t vendorLen, char *product, size_t productLen)
    /*@requires maxSet(vendor) >= (vendorLen - 1) /\ maxSet(product) >= (productLen - 1)@*/;

MPORT_PUBLIC_API char *
mport_audit(mportInstance *mport, const char *packageName, bool dependOn)
{
	mportPackageMeta **packs = NULL;
	char *pkgAudit = NULL;

	if (mport == NULL) {
		SET_ERROR(MPORT_ERR_FATAL, "mport not initialized");
		return (NULL);
	}

	if (packageName == NULL) {
		SET_ERROR(MPORT_ERR_FATAL, "Package name not found.");
		return (NULL);
	}

	if (mport_pkgmeta_search_master(mport, &packs, "pkg=%Q", packageName) != MPORT_OK) {
		mport_pkgmeta_vec_free(packs);
		packs = NULL;

		return (NULL);
	}

	if (packs != NULL && (*packs)->cpe != NULL && (*packs)->cpe[0] != '\0') {
		char *path = mport_fetch_cves(mport, (*packs)->cpe);
		char *jsonData = readJsonFile(path);

		if (path != NULL) {
			unlink(path);
			free(path);
			path = NULL;
		}

		if (jsonData == NULL) {
			SET_ERROR(MPORT_ERR_FATAL, "Error opening CVE file");
		} else {
			(void)mport_audit_report(mport, *packs, jsonData, dependOn, &pkgAudit);
			free(jsonData);
			jsonData = NULL;
		}
	}

	mport_pkgmeta_vec_free(packs);
	packs = NULL;

	return pkgAudit;
}

/*
 * Format the audit report for an installed package from the CVE feed's JSON
 * response.  When the index is loaded and offers a newer version of the
 * package, each vulnerability also says whether that version fixes it, so the
 * user can tell whether upgrading helps.  *out is NULL on failure.
 */
int
mport_audit_report(/*@notnull@*/ mportInstance *mport, /*@notnull@*/ mportPackageMeta *pack,
    /*@notnull@*/ const char *jsonData, bool dependOn, /*@out@*/ /*@null@*/ char **out)
{
	mportPackageMeta **depends = NULL;
	mportPackageMeta **depends_orig = NULL;
	struct ucl_parser *parser = NULL;
	ucl_object_t *root = NULL;
	const ucl_object_t *cur;
	ucl_object_iter_t it = NULL;
	FILE *bufferFp = NULL;
	size_t size;
	char *indexName = NULL;
	char *indexVersion = NULL;
	char *pkgAudit = NULL;
	bool haveIndex = false;
	int vulnerable = 0;
	int stillAffected = 0;
	char cpeVendor[256];
	char cpeProduct[256];
	const char *vendor = NULL;
	const char *product = NULL;

	if (out == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "Invalid audit report arguments");
	*out = NULL;

	if (mport == NULL || pack == NULL || jsonData == NULL || pack->version == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "Invalid audit report arguments");

	/*
	 * The feed lists every product in each CVE's configurations, including
	 * other products and the platforms they run on.  Only compare versions
	 * of the product the installed package's CPE names.
	 */
	if (cpe_vendor_product(
		pack->cpe, cpeVendor, sizeof(cpeVendor), cpeProduct, sizeof(cpeProduct))) {
		vendor = cpeVendor;
		product = cpeProduct;
	}

	// Create a UCL parser and parse the JSON string
	parser = ucl_parser_new(0);
	if (parser == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "Failed to create JSON parser");
	if (!ucl_parser_add_chunk(parser, (const unsigned char *)jsonData, strlen(jsonData))) {
		SET_ERRORX(
		    MPORT_ERR_FATAL, "Failed to parse JSON: %s\n", ucl_parser_get_error(parser));
		ucl_parser_free(parser);
		return (MPORT_ERR_FATAL);
	}

	root = ucl_parser_get_object(parser);
	ucl_parser_free(parser);

	/*
	 * Only a version newer than the installed one can fix anything.  An index
	 * lookup failure leaves the report without fix status rather than failing
	 * the audit.
	 */
	if ((mport->flags & MPORT_INST_HAVE_INDEX) != 0 &&
	    mport_index_version_get(mport, pack, &indexName, &indexVersion) == MPORT_OK) {
		haveIndex = true;
		if (indexVersion != NULL && mport_version_cmp(pack->version, indexVersion) >= 0) {
			free(indexName);
			free(indexVersion);
			indexName = NULL;
			indexVersion = NULL;
		}
	}

	bufferFp = open_memstream(&pkgAudit, &size);
	if (bufferFp == NULL) {
		free(indexName);
		free(indexVersion);
		ucl_object_unref(root);
		RETURN_ERROR(MPORT_ERR_FATAL, "Error allocating memory for audit entries");
	}

	while ((cur = ucl_object_iterate(root, &it, true))) {
		const ucl_object_t *cveId;
		const ucl_object_t *desc;
		const ucl_object_t *severity;
		bool fixed = true;

		if (ucl_object_type(cur) != UCL_OBJECT) {
			SET_ERROR(MPORT_ERR_FATAL, "Expected an object in the array");
			continue;
		}

		/*
		 * An entry without an id cannot be listed, so skip it entirely
		 * rather than count it in the header and summary.
		 */
		cveId = ucl_object_find_key(cur, "cveId");
		if (cveId == NULL || ucl_object_type(cveId) != UCL_STRING) {
			continue;
		}

		if (!cve_affects_version(cur, pack->version, vendor, product)) {
			continue;
		}

		if (mport->verbosity == MPORT_VQUIET) {
			fprintf(bufferFp, "%s-%s\n", pack->name, pack->version);
			break;
		}

		if (vulnerable == 0) {
			fprintf(bufferFp, "%s-%s is vulnerable:\n\n", pack->name, pack->version);
		}
		vulnerable++;

		if (indexVersion != NULL &&
		    cve_affects_version(cur, indexVersion, vendor, product)) {
			fixed = false;
			stillAffected++;
		}

		desc = ucl_object_find_key(cur, "description");
		severity = ucl_object_find_key(cur, "severity");

		fprintf(bufferFp, "%s\n", ucl_object_tostring(cveId));
		if (desc != NULL && ucl_object_type(desc) == UCL_STRING) {
			fprintf(bufferFp, "Description: %s\n", ucl_object_tostring(desc));
		}
		if (severity != NULL && ucl_object_type(severity) == UCL_STRING) {
			fprintf(bufferFp, "Severity: %s\n", ucl_object_tostring(severity));
		}
		if (indexVersion != NULL) {
			fprintf(bufferFp, "%s in: %s-%s\n", fixed ? "Fixed" : "Not fixed",
			    indexName, indexVersion);
		}
		fprintf(bufferFp, "\n");
	}

	if (vulnerable > 0 && haveIndex) {
		if (indexVersion == NULL) {
			fprintf(bufferFp, "No newer version of %s is available in the index.\n",
			    pack->name);
		} else if (stillAffected == 0) {
			fprintf(bufferFp, "Upgrade to %s-%s to fix %s.\n", indexName, indexVersion,
			    vulnerable == 1 ? "this vulnerability" : "these vulnerabilities");
		} else {
			fprintf(bufferFp,
			    "%s-%s is available in the index but is still affected by %d of %d vulnerabilities.\n",
			    indexName, indexVersion, stillAffected, vulnerable);
		}
	}

	if (dependOn) {
		if (mport_pkgmeta_get_downdepends(mport, pack, &depends_orig) == MPORT_OK) {
			if (depends_orig != NULL) {
				depends = depends_orig;
				fprintf(bufferFp, "Packages that depend on %s:", pack->name);
				while (*depends != NULL) {
					fprintf(bufferFp, " %s", (*depends)->name);
					depends++;
				}
				fprintf(bufferFp, "\n");

				mport_pkgmeta_vec_free(depends_orig);
				depends_orig = NULL;
				depends = NULL;
			}
		}
	}

	fclose(bufferFp);
	bufferFp = NULL;

	free(indexName);
	free(indexVersion);
	ucl_object_unref(root);

	*out = pkgAudit;
	return (MPORT_OK);
}

/*
 * Copy field index of a CPE 2.3 formatted string into buf, dropping the
 * backslash that quotes a character.  False when the field is missing or does
 * not fit.
 */
static bool
cpe_field(const char *cpe, int index, char *buf, size_t len)
/*@requires maxSet(buf) >= (len - 1)@*/
{
	const char *c;
	int field = 0;
	size_t i = 0;

	if (len == 0)
		return (false);

	for (c = cpe; *c != '\0'; c++) {
		if (*c == ':') {
			if (field == index)
				break;
			field++;
			continue;
		}
		if (*c == '\\' && c[1] != '\0')
			c++;
		if (field == index) {
			if (i + 1 >= len)
				return (false);
			buf[i++] = *c;
		}
	}

	if (field != index)
		return (false);

	buf[i] = '\0';
	return (true);
}

/*
 * Extract the vendor and product of a CPE 2.3 formatted string such as
 * cpe:2.3:a:apache:http_server:2.4.69:...  False when the CPE is not in that
 * form or either field is empty or a wildcard, in which case nothing can be
 * filtered on.
 */
static bool
cpe_vendor_product(
    /*@null@*/ const char *cpe, char *vendor, size_t vendorLen, char *product, size_t productLen)
{
	if (vendorLen == 0 || productLen == 0)
		return (false);

	vendor[0] = '\0';
	product[0] = '\0';

	if (cpe == NULL || strncmp(cpe, "cpe:2.3:", 8) != 0)
		return (false);

	if (!cpe_field(cpe, 3, vendor, vendorLen) || !cpe_field(cpe, 4, product, productLen))
		return (false);

	if (vendor[0] == '\0' || strcmp(vendor, "*") == 0 || product[0] == '\0' ||
	    strcmp(product, "*") == 0)
		return (false);

	return (true);
}

/*
 * Whether a CVE product entry is the installed package's product.  An entry
 * without names cannot be ruled out, so it still counts.
 */
static bool
cve_product_matches(
    const ucl_object_t *entry, /*@null@*/ const char *vendor, /*@null@*/ const char *product)
{
	const ucl_object_t *entryVendor;
	const ucl_object_t *entryName;

	if (vendor == NULL || product == NULL)
		return (true);

	entryVendor = ucl_object_find_key(entry, "vendorName");
	entryName = ucl_object_find_key(entry, "name");
	if (entryVendor == NULL || ucl_object_type(entryVendor) != UCL_STRING ||
	    entryName == NULL || ucl_object_type(entryName) != UCL_STRING)
		return (true);

	return (strcasecmp(ucl_object_tostring(entryVendor), vendor) == 0 &&
	    strcasecmp(ucl_object_tostring(entryName), product) == 0);
}

/*
 * A CVE affects version when one of its entries for the installed product
 * (vendor and product, when known) matches every version ("*") or lists a
 * version at or above it.
 */
static bool
cve_affects_version(const ucl_object_t *cve, const char *version, /*@null@*/ const char *vendor,
    /*@null@*/ const char *product)
{
	const ucl_object_t *products = ucl_object_find_key(cve, "products");
	ucl_object_iter_t pit = NULL;
	const ucl_object_t *entry;

	if (products == NULL || ucl_object_type(products) != UCL_ARRAY) {
		return (false);
	}

	for (entry = ucl_object_iterate(products, &pit, true); entry != NULL;
	    entry = ucl_object_iterate(products, &pit, true)) {
		const ucl_object_t *productVersion = ucl_object_find_key(entry, "version");
		const char *version_str;

		if (productVersion == NULL || ucl_object_type(productVersion) != UCL_STRING) {
			continue;
		}

		if (!cve_product_matches(entry, vendor, product)) {
			continue;
		}

		version_str = ucl_object_tostring(productVersion);
		if (version_str == NULL)
			continue;

		if ('*' == version_str[0] || mport_version_cmp(version, version_str) <= 0) {
			return (true);
		}
	}

	return (false);
}

static char *
readJsonFile(char *jsonFile)
{
	FILE *fp;
	long file_size;
	size_t size;
	char *buffer;

	if (jsonFile == NULL) {
		return (NULL);
	}

	// Open the JSON file
	fp = fopen(jsonFile, "rb");
	if (!fp) {
		SET_ERROR(MPORT_ERR_WARN, "could not open file");
		return NULL;
	}

	// Get the file size
	if (fseek(fp, 0, SEEK_END) != 0) {
		SET_ERROR(MPORT_ERR_WARN, "could not determine file size");
		fclose(fp);
		return NULL;
	}

	file_size = ftell(fp);
	if (file_size < 0) {
		SET_ERROR(MPORT_ERR_WARN, "could not determine file size");
		fclose(fp);
		return NULL;
	}

	if (fseek(fp, 0, SEEK_SET) != 0) {
		SET_ERROR(MPORT_ERR_WARN, "could not rewind file");
		fclose(fp);
		return NULL;
	}

	size = (size_t)file_size;

	// Allocate memory for the file contents
	buffer = (char *)malloc(size + 1);
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
	buffer[size] = '\0';

	// Close the file
	fclose(fp);

	return buffer;
}
