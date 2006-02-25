/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2001-2003  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: builtin.c,v 1.1.1.2 2006-02-25 02:32:03 laffer1 Exp $ */

/*
 * The built-in "version", "hostname", "id" and "authors" databases.
 */

#include <config.h>

#include <string.h>
#include <stdio.h>

#include <isc/print.h>
#include <isc/result.h>
#include <isc/util.h>

#include <dns/sdb.h>
#include <dns/result.h>

#include <named/builtin.h>
#include <named/globals.h>
#include <named/server.h>
#include <named/os.h>

typedef struct builtin builtin_t;

static isc_result_t do_version_lookup(dns_sdblookup_t *lookup);
static isc_result_t do_hostname_lookup(dns_sdblookup_t *lookup);
static isc_result_t do_authors_lookup(dns_sdblookup_t *lookup);
static isc_result_t do_id_lookup(dns_sdblookup_t *lookup);

/*
 * We can't use function pointers as the db_data directly
 * because ANSI C does not guarantee that function pointers
 * can safely be cast to void pointers and back.
 */

struct builtin {
	isc_result_t (*do_lookup)(dns_sdblookup_t *lookup);
};

static builtin_t version_builtin = { do_version_lookup };
static builtin_t hostname_builtin = { do_hostname_lookup };
static builtin_t authors_builtin = { do_authors_lookup };
static builtin_t id_builtin = { do_id_lookup };

static dns_sdbimplementation_t *builtin_impl;

static isc_result_t
builtin_lookup(const char *zone, const char *name, void *dbdata,
	       dns_sdblookup_t *lookup)
{
	builtin_t *b = (builtin_t *) dbdata;

	UNUSED(zone);

	if (strcmp(name, "@") == 0)
		return (b->do_lookup(lookup));
	else
		return (ISC_R_NOTFOUND);
}

static isc_result_t
put_txt(dns_sdblookup_t *lookup, const char *text) {
	unsigned char buf[256];
	unsigned int len = strlen(text);
	if (len > 255)
		len = 255; /* Silently truncate */
	buf[0] = len;
	memcpy(&buf[1], text, len);
	return (dns_sdb_putrdata(lookup, dns_rdatatype_txt, 0, buf, len + 1));
}

static isc_result_t
do_version_lookup(dns_sdblookup_t *lookup) {
	if (ns_g_server->version_set) {	
		if (ns_g_server->version == NULL)
			return (ISC_R_SUCCESS);
		else
			return (put_txt(lookup, ns_g_server->version));
	} else {
		return (put_txt(lookup, ns_g_version));
	}
}

static isc_result_t
do_hostname_lookup(dns_sdblookup_t *lookup) {
	if (ns_g_server->hostname_set) {
		if (ns_g_server->hostname == NULL)
			return (ISC_R_SUCCESS);
		else
			return (put_txt(lookup, ns_g_server->hostname));
	} else {
		char buf[256];
		isc_result_t result = ns_os_gethostname(buf, sizeof(buf));
		if (result != ISC_R_SUCCESS)
			return (result);
		return (put_txt(lookup, buf));
	}
}

static isc_result_t
do_authors_lookup(dns_sdblookup_t *lookup) {
	isc_result_t result;
	const char **p;
	static const char *authors[] = {
		"Mark Andrews",
		"James Brister",
		"Ben Cottrell",
		"Michael Graff",
		"Andreas Gustafsson",
		"Bob Halley",
		"David Lawrence",
		"Danny Mayer",
		"Damien Neil",
		"Matt Nelson",
		"Michael Sawyer",
		"Brian Wellington",
		NULL
	};

	/*
	 * If a version string is specified, disable the authors.bind zone.
	 */
	if (ns_g_server->version_set)
		return (ISC_R_SUCCESS);

	for (p = authors; *p != NULL; p++) {
		result = put_txt(lookup, *p);
		if (result != ISC_R_SUCCESS)
			return (result);
	}
	return (ISC_R_SUCCESS);
}

static isc_result_t
do_id_lookup(dns_sdblookup_t *lookup) {

	if (ns_g_server->server_usehostname) {
		char buf[256];
		isc_result_t result = ns_os_gethostname(buf, sizeof(buf));
		if (result != ISC_R_SUCCESS)
			return (result);
		return (put_txt(lookup, buf));
	}

	if (ns_g_server->server_id == NULL)
		return (ISC_R_SUCCESS);
	else
		return (put_txt(lookup, ns_g_server->server_id));
}

static isc_result_t
builtin_authority(const char *zone, void *dbdata, dns_sdblookup_t *lookup) {
	isc_result_t result;

	UNUSED(zone);
	UNUSED(dbdata);

	result = dns_sdb_putsoa(lookup, "@", "hostmaster", 0);
	if (result != ISC_R_SUCCESS)
		return (ISC_R_FAILURE);
	result = dns_sdb_putrr(lookup, "ns", 0, "@");
	if (result != ISC_R_SUCCESS)
		return (ISC_R_FAILURE);

	return (ISC_R_SUCCESS);
}

static isc_result_t
builtin_create(const char *zone, int argc, char **argv,
	       void *driverdata, void **dbdata)
{
	UNUSED(zone);
	UNUSED(driverdata);
	if (argc != 1)
		return (DNS_R_SYNTAX);
	if (strcmp(argv[0], "version") == 0)
		*dbdata = &version_builtin;
	else if (strcmp(argv[0], "hostname") == 0)
		*dbdata = &hostname_builtin;
	else if (strcmp(argv[0], "authors") == 0)
		*dbdata = &authors_builtin;
	else if (strcmp(argv[0], "id") == 0)
		*dbdata = &id_builtin;
	else
		return (ISC_R_NOTIMPLEMENTED);
	return (ISC_R_SUCCESS);
}

static dns_sdbmethods_t builtin_methods = {
	builtin_lookup,
	builtin_authority,
	NULL,		/* allnodes */
	builtin_create,
	NULL		/* destroy */
};

isc_result_t
ns_builtin_init(void) {
	RUNTIME_CHECK(dns_sdb_register("_builtin", &builtin_methods, NULL,
				       DNS_SDBFLAG_RELATIVEOWNER |
				       DNS_SDBFLAG_RELATIVERDATA,
				       ns_g_mctx, &builtin_impl)
		      == ISC_R_SUCCESS);
	return (ISC_R_SUCCESS);
}

void
ns_builtin_deinit(void) {
	dns_sdb_unregister(&builtin_impl);
}
