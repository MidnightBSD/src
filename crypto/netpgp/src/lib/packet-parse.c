/*-
 * Copyright (c) 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Alistair Crooks (agc@NetBSD.org)
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Copyright (c) 2005-2008 Nominet UK (www.nic.uk)
 * All rights reserved.
 * Contributors: Ben Laurie, Rachel Willmer. The Contributors have asserted
 * their moral rights under the UK Copyright Design and Patents Act 1988 to
 * be recorded as the authors of this copyright work.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License.
 *
 * You may obtain a copy of the License at
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/** \file
 * \brief Parser for OpenPGP packets
 */
#include "config.h"

#ifdef HAVE_SYS_CDEFS_H
#include <sys/cdefs.h>
#endif

#if defined(__NetBSD__)
__COPYRIGHT("@(#) Copyright (c) 2009 The NetBSD Foundation, Inc. All rights reserved.");
__RCSID("$NetBSD: packet-parse.c,v 1.18 2009/05/31 23:26:20 agc Exp $");
#endif

#ifdef HAVE_OPENSSL_CAST_H
#include <openssl/cast.h>
#endif

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <errno.h>

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#include "packet.h"
#include "packet-parse.h"
#include "keyring.h"
#include "errors.h"
#include "packet-show.h"
#include "create.h"
#include "readerwriter.h"
#include "netpgpdefs.h"
#include "crypto.h"
#include "netpgpdigest.h"

#define ERRP(cbinfo, cont, err)	do {					\
	cont.u.error.error = err;					\
	CALLBACK(cbinfo, OPS_PARSER_ERROR, &cont);			\
	return 0;							\
	/*NOTREACHED*/							\
} while(/*CONSTCOND*/0)

/**
 * limread_data reads the specified amount of the subregion's data
 * into a data_t structure
 *
 * \param data	Empty structure which will be filled with data
 * \param len	Number of octets to read
 * \param subregion
 * \param pinfo	How to parse
 *
 * \return 1 on success, 0 on failure
 */
static int 
limread_data(__ops_data_t *data, unsigned int len,
		  __ops_region_t *subregion, __ops_parseinfo_t *pinfo)
{
	data->len = len;

	if (subregion->length - subregion->readc < len) {
		(void) fprintf(stderr, "limread_data: bad length\n");
		return 0;
	}

	data->contents = calloc(1, data->len);
	if (!data->contents) {
		return 0;
	}

	return __ops_limited_read(data->contents, data->len, subregion,
			&pinfo->errors, &pinfo->readinfo, &pinfo->cbinfo);
}

/**
 * read_data reads the remainder of the subregion's data
 * into a data_t structure
 *
 * \param data
 * \param subregion
 * \param pinfo
 *
 * \return 1 on success, 0 on failure
 */
static int 
read_data(__ops_data_t *data, __ops_region_t *region, __ops_parseinfo_t *pinfo)
{
	int	cc;

	cc = region->length - region->readc;
	return (cc >= 0) ? limread_data(data, (unsigned)cc, region, pinfo) : 0;
}

/**
 * Reads the remainder of the subregion as a string.
 * It is the user's responsibility to free the memory allocated here.
 */

static int 
read_unsig_str(unsigned char **str, __ops_region_t *subregion,
		     __ops_parseinfo_t *pinfo)
{
	size_t	len = 0;

	len = subregion->length - subregion->readc;
	if ((*str = calloc(1, len + 1)) == NULL) {
		return 0;
	}
	if (len &&
	    !__ops_limited_read(*str, len, subregion, &pinfo->errors,
				     &pinfo->readinfo, &pinfo->cbinfo)) {
		return 0;
	}
	/* ! ensure the string is NULL-terminated */
	(*str)[len] = '\0';
	return 1;
}

static int 
read_string(char **str, __ops_region_t *subregion, __ops_parseinfo_t *pinfo)
{
	return read_unsig_str((unsigned char **) str, subregion, pinfo);
}

void 
__ops_init_subregion(__ops_region_t *subregion, __ops_region_t *region)
{
	(void) memset(subregion, 0x0, sizeof(*subregion));
	subregion->parent = region;
}

/*
 * XXX: replace __ops_ptag_t with something more appropriate for limiting reads
 */

/**
 * low-level function to read data from reader function
 *
 * Use this function, rather than calling the reader directly.
 *
 * If the accumulate flag is set in *pinfo, the function
 * adds the read data to the accumulated data, and updates
 * the accumulated length. This is useful if, for example,
 * the application wants access to the raw data as well as the
 * parsed data.
 *
 * This function will also try to read the entire amount asked for, but not
 * if it is over INT_MAX. Obviously many callers will know that they
 * never ask for that much and so can avoid the extra complexity of
 * dealing with return codes and filled-in lengths.
 *
 * \param *dest
 * \param *plength
 * \param flags
 * \param *pinfo
 *
 * \return OPS_R_OK
 * \return OPS_R_PARTIAL_READ
 * \return OPS_R_EOF
 * \return OPS_R_EARLY_EOF
 *
 * \sa #__ops_reader_ret_t for details of return codes
 */

static int 
sub_base_read(void *dest, size_t length, __ops_error_t **errors,
	      __ops_reader_t *readinfo, __ops_cbdata_t *cbinfo)
{
	size_t          n;

	/* reading more than this would look like an error */
	if (length > INT_MAX)
		length = INT_MAX;

	for (n = 0; n < length;) {
		int	r;

		r = readinfo->reader((char *) dest + n, length - n, errors,
				readinfo, cbinfo);
		if (r > (int)(length - n)) {
			(void) fprintf(stderr, "sub_base_read: bad read\n");
			return 0;
		}
		if (r < 0) {
			return r;
		}
		if (r == 0) {
			break;
		}
		n += r;
	}

	if (n == 0) {
		return 0;
	}
	if (readinfo->accumulate) {
		if (readinfo->asize < readinfo->alength) {
			(void) fprintf(stderr, "sub_base_read: bad size\n");
			return 0;
		}
		if (readinfo->alength + n > readinfo->asize) {
			readinfo->asize = readinfo->asize * 2 + n;
			readinfo->accumulated = realloc(readinfo->accumulated,
							readinfo->asize);
		}
		if (readinfo->asize < readinfo->alength + n) {
			(void) fprintf(stderr, "sub_base_read: bad realloc\n");
			return 0;
		}
		(void) memcpy(readinfo->accumulated + readinfo->alength, dest,
				n);
	}
	/* we track length anyway, because it is used for packet offsets */
	readinfo->alength += n;
	/* and also the position */
	readinfo->position += n;

	return n;
}

int 
__ops_stacked_read(void *dest, size_t length, __ops_error_t **errors,
		 __ops_reader_t *readinfo, __ops_cbdata_t *cbinfo)
{
	return sub_base_read(dest, length, errors, readinfo->next, cbinfo);
}

/* This will do a full read so long as length < MAX_INT */
static int 
base_read(unsigned char *dest, size_t length,
	  __ops_parseinfo_t *pinfo)
{
	return sub_base_read(dest, length, &pinfo->errors, &pinfo->readinfo,
			     &pinfo->cbinfo);
}

/*
 * Read a full size_t's worth. If the return is < than length, then
 * *last_read tells you why - < 0 for an error, == 0 for EOF
 */

static size_t 
full_read(unsigned char *dest,
		size_t length,
		int *last_read,
		__ops_error_t **errors,
		__ops_reader_t *readinfo,
		__ops_cbdata_t *cbinfo)
{
	size_t          t;
	int             r = 0;	/* preset in case some loon calls with length
				 * == 0 */

	for (t = 0; t < length;) {
		r = sub_base_read(dest + t, length - t, errors, readinfo,
				cbinfo);
		if (r <= 0) {
			*last_read = r;
			return t;
		}
		t += r;
	}

	*last_read = r;

	return t;
}



/** Read a scalar value of selected length from reader.
 *
 * Read an unsigned scalar value from reader in Big Endian representation.
 *
 * This function does not know or care about packet boundaries. It
 * also assumes that an EOF is an error.
 *
 * \param *result	The scalar value is stored here
 * \param *reader	Our reader
 * \param length	How many bytes to read
 * \return		1 on success, 0 on failure
 */
static unsigned 
_read_scalar(unsigned *result, unsigned length,
	     __ops_parseinfo_t *pinfo)
{
	unsigned        t = 0;

	if (length > sizeof(*result)) {
		(void) fprintf(stderr, "_read_scalar: bad length\n");
		return 0;
	}

	while (length--) {
		unsigned char   c;
		int             r;

		r = base_read(&c, 1, pinfo);
		if (r != 1)
			return 0;
		t = (t << 8) + c;
	}

	*result = t;
	return 1;
}

/**
 * \ingroup Core_ReadPackets
 * \brief Read bytes from a region within the packet.
 *
 * Read length bytes into the buffer pointed to by *dest.
 * Make sure we do not read over the packet boundary.
 * Updates the Packet Tag's __ops_ptag_t::readc.
 *
 * If length would make us read over the packet boundary, or if
 * reading fails, we call the callback with an error.
 *
 * Note that if the region is indeterminate, this can return a short
 * read - check region->last_read for the length. EOF is indicated by
 * a success return and region->last_read == 0 in this case (for a
 * region of known length, EOF is an error).
 *
 * This function makes sure to respect packet boundaries.
 *
 * \param dest		The destination buffer
 * \param length	How many bytes to read
 * \param region	Pointer to packet region
 * \param errors    Error stack
 * \param readinfo		Reader info
 * \param cbinfo	Callback info
 * \return		1 on success, 0 on error
 */
unsigned 
__ops_limited_read(unsigned char *dest,
			size_t length,
			__ops_region_t *region,
			__ops_error_t **errors,
			__ops_reader_t *readinfo,
			__ops_cbdata_t *cbinfo)
{
	size_t	r;
	int	lr;

	if (!region->indeterminate &&
	    region->readc + length > region->length) {
		OPS_ERROR(errors, OPS_E_P_NOT_ENOUGH_DATA, "Not enough data");
		return 0;
	}
	r = full_read(dest, length, &lr, errors, readinfo, cbinfo);
	if (lr < 0) {
		OPS_ERROR(errors, OPS_E_R_READ_FAILED, "Read failed");
		return 0;
	}
	if (!region->indeterminate && r != length) {
		OPS_ERROR(errors, OPS_E_R_READ_FAILED, "Read failed");
		return 0;
	}
	region->last_read = r;
	do {
		region->readc += r;
		if (region->parent && region->length > region->parent->length) {
			(void) fprintf(stderr,
				"ops_limited_read: bad length\n");
			return 0;
		}
	} while ((region = region->parent) != NULL);
	return 1;
}

/**
   \ingroup Core_ReadPackets
   \brief Call __ops_limited_read on next in stack
*/
unsigned 
__ops_stacked_limited_read(unsigned char *dest, unsigned length,
			 __ops_region_t *region,
			 __ops_error_t **errors,
			 __ops_reader_t *readinfo,
			 __ops_cbdata_t *cbinfo)
{
	return __ops_limited_read(dest, length, region, errors,
				readinfo->next, cbinfo);
}

static unsigned 
limread(unsigned char *dest, unsigned length,
	     __ops_region_t *region, __ops_parseinfo_t *info)
{
	return __ops_limited_read(dest, length, region, &info->errors,
				&info->readinfo, &info->cbinfo);
}

static unsigned 
exact_limread(unsigned char *dest, unsigned len,
		   __ops_region_t *region,
		   __ops_parseinfo_t *pinfo)
{
	unsigned   ret;

	pinfo->exact_read = 1;
	ret = limread(dest, len, region, pinfo);
	pinfo->exact_read = 0;
	return ret;
}

/** Skip over length bytes of this packet.
 *
 * Calls limread() to skip over some data.
 *
 * This function makes sure to respect packet boundaries.
 *
 * \param length	How many bytes to skip
 * \param *region	Pointer to packet region
 * \param *pinfo	How to parse
 * \return		1 on success, 0 on error (calls the cb with OPS_PARSER_ERROR in limread()).
 */
static int 
limskip(unsigned length, __ops_region_t *region, __ops_parseinfo_t *pinfo)
{
	unsigned char   buf[NETPGP_BUFSIZ];

	while (length > 0) {
		unsigned	n = length % NETPGP_BUFSIZ;

		if (!limread(buf, n, region, pinfo)) {
			return 0;
		}
		length -= n;
	}
	return 1;
}

/** Read a scalar.
 *
 * Read a big-endian scalar of length bytes, respecting packet
 * boundaries (by calling limread() to read the raw data).
 *
 * This function makes sure to respect packet boundaries.
 *
 * \param *dest		The scalar value is stored here
 * \param length	How many bytes make up this scalar (at most 4)
 * \param *region	Pointer to current packet region
 * \param *pinfo	How to parse
 * \param *cb		The callback
 * \return		1 on success, 0 on error (calls the cb with OPS_PARSER_ERROR in limread()).
 *
 * \see RFC4880 3.1
 */
static int 
limread_scalar(unsigned *dest,
			unsigned len,
			__ops_region_t *region,
			__ops_parseinfo_t *pinfo)
{
	unsigned char   c[4] = "";
	unsigned        t;
	unsigned        n;

	if (len > 4) {
		(void) fprintf(stderr, "limread_scalar: bad length\n");
		return 0;
	}
	/*LINTED*/
	if (/*CONSTCOND*/sizeof(*dest) < 4) {
		(void) fprintf(stderr, "limread_scalar: bad dest\n");
		return 0;
	}
	if (!limread(c, len, region, pinfo)) {
		return 0;
	}
	for (t = 0, n = 0; n < len; ++n) {
		t = (t << 8) + c[n];
	}
	*dest = t;
	return 1;
}

/** Read a scalar.
 *
 * Read a big-endian scalar of length bytes, respecting packet
 * boundaries (by calling limread() to read the raw data).
 *
 * The value read is stored in a size_t, which is a different size
 * from an unsigned on some platforms.
 *
 * This function makes sure to respect packet boundaries.
 *
 * \param *dest		The scalar value is stored here
 * \param length	How many bytes make up this scalar (at most 4)
 * \param *region	Pointer to current packet region
 * \param *pinfo	How to parse
 * \param *cb		The callback
 * \return		1 on success, 0 on error (calls the cb with OPS_PARSER_ERROR in limread()).
 *
 * \see RFC4880 3.1
 */
static int 
limread_size_t(size_t *dest,
				unsigned length,
				__ops_region_t *region,
				__ops_parseinfo_t *pinfo)
{
	unsigned        tmp;

	/*
	 * Note that because the scalar is at most 4 bytes, we don't care if
	 * size_t is bigger than usigned
	 */
	if (!limread_scalar(&tmp, length, region, pinfo))
		return 0;

	*dest = tmp;
	return 1;
}

/** Read a timestamp.
 *
 * Timestamps in OpenPGP are unix time, i.e. seconds since The Epoch (1.1.1970).  They are stored in an unsigned scalar
 * of 4 bytes.
 *
 * This function reads the timestamp using limread_scalar().
 *
 * This function makes sure to respect packet boundaries.
 *
 * \param *dest		The timestamp is stored here
 * \param *ptag		Pointer to current packet's Packet Tag.
 * \param *reader	Our reader
 * \param *cb		The callback
 * \return		see limread_scalar()
 *
 * \see RFC4880 3.5
 */
static int 
limited_read_time(time_t *dest, __ops_region_t *region,
		  __ops_parseinfo_t *pinfo)
{
	unsigned char   c;
	time_t          mytime = 0;
	int             i = 0;

	/*
         * Cannot assume that time_t is 4 octets long -
	 * SunOS 5.10 and NetBSD both have 64-bit time_ts.
         */
	if (/* CONSTCOND */sizeof(time_t) == 4) {
		return limread_scalar((unsigned *)(void *)dest, 4, region, pinfo);
	}
	for (i = 0; i < 4; i++) {
		if (!limread(&c, 1, region, pinfo)) {
			return 0;
		}
		mytime = (mytime << 8) + c;
	}
	*dest = mytime;
	return 1;
}

/**
 * \ingroup Core_MPI
 * Read a multiprecision integer.
 *
 * Large numbers (multiprecision integers, MPI) are stored in OpenPGP in two parts.  First there is a 2 byte scalar
 * indicating the length of the following MPI in Bits.  Then follow the bits that make up the actual number, most
 * significant bits first (Big Endian).  The most significant bit in the MPI is supposed to be 1 (unless the MPI is
 * encrypted - then it may be different as the bit count refers to the plain text but the bits are encrypted).
 *
 * Unused bits (i.e. those filling up the most significant byte from the left to the first bits that counts) are
 * supposed to be cleared - I guess. XXX - does anything actually say so?
 *
 * This function makes sure to respect packet boundaries.
 *
 * \param **pgn		return the integer there - the BIGNUM is created by BN_bin2bn() and probably needs to be freed
 * 				by the caller XXX right ben?
 * \param *ptag		Pointer to current packet's Packet Tag.
 * \param *reader	Our reader
 * \param *cb		The callback
 * \return		1 on success, 0 on error (by limread_scalar() or limread() or if the MPI is not properly formed (XXX
 * 				 see comment below - the callback is called with a OPS_PARSER_ERROR in case of an error)
 *
 * \see RFC4880 3.2
 */
static int 
limread_mpi(BIGNUM **pbn, __ops_region_t *region, __ops_parseinfo_t *pinfo)
{
	unsigned char   buf[NETPGP_BUFSIZ] = "";
					/* an MPI has a 2 byte length part.
					 * Length is given in bits, so the
					 * largest we should ever need for
					 * the buffer is NETPGP_BUFSIZ bytes. */
	unsigned        length;
	unsigned        nonzero;
	unsigned   		ret;

	pinfo->reading_mpi_len = 1;
	ret = limread_scalar(&length, 2, region, pinfo);

	pinfo->reading_mpi_len = 0;
	if (!ret)
		return 0;

	nonzero = length & 7;	/* there should be this many zero bits in the
				 * MS byte */
	if (!nonzero)
		nonzero = 8;
	length = (length + 7) / 8;

	if (length == 0) {
		/* if we try to read a length of 0, then fail */
		if (__ops_get_debug_level(__FILE__)) {
			(void) fprintf(stderr, "limread_mpi: 0 length\n");
		}
		return 0;
	}
	if (length > NETPGP_BUFSIZ) {
		(void) fprintf(stderr, "limread_mpi: bad length\n");
		return 0;
	}
	if (!limread(buf, length, region, pinfo)) {
		return 0;
	}
	if (((unsigned)buf[0] >> nonzero) != 0 ||
	    !((unsigned)buf[0] & (1U << (nonzero - 1U)))) {
		OPS_ERROR(&pinfo->errors, OPS_E_P_MPI_FORMAT_ERROR, "MPI Format error");
		/* XXX: Ben, one part of
		 * this constraint does
		 * not apply to
		 * encrypted MPIs the
		 * draft says. -- peter */
		return 0;
	}
	*pbn = BN_bin2bn(buf, (int)length, NULL);
	return 1;
}

/** Read some data with a New-Format length from reader.
 *
 * \sa Internet-Draft RFC4880.txt Section 4.2.2
 *
 * \param *length	Where the decoded length will be put
 * \param *pinfo	How to parse
 * \return		1 if OK, else 0
 *
 */

static unsigned 
read_new_length(unsigned *length, __ops_parseinfo_t *pinfo)
{
	unsigned char   c;

	if (base_read(&c, 1, pinfo) != 1)
		return 0;
	if (c < 192) {
		/* 1. One-octet packet */
		*length = c;
		return 1;
	} else if (c >= 192 && c <= 223) {
		/* 2. Two-octet packet */
		unsigned        t = (c - 192) << 8;

		if (base_read(&c, 1, pinfo) != 1)
			return 0;
		*length = t + c + 192;
		return 1;
	} else if (c == 255) {
		/* 3. Five-Octet packet */
		return _read_scalar(length, 4, pinfo);
	} else if (c >= 224 && c < 255) {
		/* 4. Partial Body Length */
		/* XXX - agc - gpg multi-recipient encryption uses this */
		OPS_ERROR(&pinfo->errors, OPS_E_UNIMPLEMENTED,
		"New format Partial Body Length fields not yet implemented");
		return 0;
	}
	return 0;
}

/** Read the length information for a new format Packet Tag.
 *
 * New style Packet Tags encode the length in one to five octets.  This function reads the right amount of bytes and
 * decodes it to the proper length information.
 *
 * This function makes sure to respect packet boundaries.
 *
 * \param *length	return the length here
 * \param *ptag		Pointer to current packet's Packet Tag.
 * \param *reader	Our reader
 * \param *cb		The callback
 * \return		1 on success, 0 on error (by limread_scalar() or limread() or if the MPI is not properly formed (XXX
 * 				 see comment below)
 *
 * \see RFC4880 4.2.2
 * \see __ops_ptag_t
 */
static int 
limited_read_new_length(unsigned *length, __ops_region_t *region,
			__ops_parseinfo_t *pinfo)
{
	unsigned char   c = 0x0;

	if (!limread(&c, 1, region, pinfo)) {
		return 0;
	}
	if (c < 192) {
		*length = c;
		return 1;
	}
	if (c < 255) {
		unsigned        t = (c - 192) << 8;

		if (!limread(&c, 1, region, pinfo)) {
			return 0;
		}
		*length = t + c + 192;
		return 1;
	}
	return limread_scalar(length, 4, region, pinfo);
}

/**
\ingroup Core_Create
\brief Free allocated memory
*/
static void 
data_free(__ops_data_t *data)
{
	(void) free(data->contents);
	data->contents = NULL;
	data->len = 0;
}

/**
\ingroup Core_Create
\brief Free allocated memory
*/
static void 
string_free(char **str)
{
	(void) free(*str);
	*str = NULL;
}

/**
\ingroup Core_Create
\brief Free allocated memory
*/
/* ! Free packet memory, set pointer to NULL */
void 
__ops_subpacket_free(__ops_subpacket_t *packet)
{
	(void) free(packet->raw);
	packet->raw = NULL;
}

/**
\ingroup Core_Create
\brief Free allocated memory
*/
static void 
__ops_headers_free(__ops_headers_t *headers)
{
	unsigned        n;

	for (n = 0; n < headers->headerc; ++n) {
		(void) free(headers->headers[n].key);
		(void) free(headers->headers[n].value);
	}
	(void) free(headers->headers);
	headers->headers = NULL;
}

/**
\ingroup Core_Create
\brief Free allocated memory
*/
static void 
cleartext_trailer_free(__ops_cleartext_trailer_t *trailer)
{
	(void) free(trailer->hash);
	trailer->hash = NULL;
}

/**
\ingroup Core_Create
\brief Free allocated memory
*/
static void 
__ops_cmd_get_passphrase_free(__ops_seckey_passphrase_t *skp)
{
	if (skp->passphrase && *skp->passphrase) {
		(void) free(*skp->passphrase);
		*skp->passphrase = NULL;
	}
}

/**
   \ingroup Core_Create
   \brief Free the memory used when parsing this signature sub-packet type
*/
static void 
ss_userdef_free(__ops_ss_userdef_t *ss_userdef)
{
	data_free(&ss_userdef->data);
}

/**
   \ingroup Core_Create
   \brief Free the memory used when parsing this signature sub-packet type
*/
static void 
ss_reserved_free(__ops_ss_unknown_t *ss_unknown)
{
	data_free(&ss_unknown->data);
}

/**
   \ingroup Core_Create
   \brief Free the memory used when parsing this packet type
*/
static void 
trust_free(__ops_trust_t *trust)
{
	data_free(&trust->data);
}

/**
 * \ingroup Core_Create
 * \brief Free the memory used when parsing a private/experimental PKA signature
 * \param unknown_sig
 */
static void 
free_unknown_sig_pka(__ops_unknown_sig_t *unknown_sig)
{
	data_free(&unknown_sig->data);
}

/**
\ingroup Core_Create
\brief Free allocated memory
*/
static void 
free_BN(BIGNUM **pp)
{
	BN_free(*pp);
	*pp = NULL;
}

/**
 * \ingroup Core_Create
 * \brief Free the memory used when parsing a signature
 * \param sig
 */
static void 
sig_free(__ops_sig_t *sig)
{
	switch (sig->info.key_alg) {
	case OPS_PKA_RSA:
	case OPS_PKA_RSA_SIGN_ONLY:
		free_BN(&sig->info.sig.rsa.sig);
		break;

	case OPS_PKA_DSA:
		free_BN(&sig->info.sig.dsa.r);
		free_BN(&sig->info.sig.dsa.s);
		break;

	case OPS_PKA_ELGAMAL_ENCRYPT_OR_SIGN:
		free_BN(&sig->info.sig.elgamal.r);
		free_BN(&sig->info.sig.elgamal.s);
		break;

	case OPS_PKA_PRIVATE00:
	case OPS_PKA_PRIVATE01:
	case OPS_PKA_PRIVATE02:
	case OPS_PKA_PRIVATE03:
	case OPS_PKA_PRIVATE04:
	case OPS_PKA_PRIVATE05:
	case OPS_PKA_PRIVATE06:
	case OPS_PKA_PRIVATE07:
	case OPS_PKA_PRIVATE08:
	case OPS_PKA_PRIVATE09:
	case OPS_PKA_PRIVATE10:
		free_unknown_sig_pka(&sig->info.sig.unknown);
		break;

	default:
		(void) fprintf(stderr, "sig_free: bad sig type\n");
	}
}

/**
 \ingroup Core_Create
 \brief Free the memory used when parsing this signature sub-packet type
 \param ss_skapref
*/
static void 
ss_skapref_free(__ops_ss_skapref_t *ss_skapref)
{
	data_free(&ss_skapref->data);
}

/**
   \ingroup Core_Create
   \brief Free the memory used when parsing this signature sub-packet type
   \param ss_hashpref
*/
static void 
ss_hashpref_free(__ops_ss_hashpref_t *ss_hashpref)
{
	data_free(&ss_hashpref->data);
}

/**
   \ingroup Core_Create
   \brief Free the memory used when parsing this signature sub-packet type
*/
static void 
ss_zpref_free(__ops_ss_zpref_t *ss_zpref)
{
	data_free(&ss_zpref->data);
}

/**
   \ingroup Core_Create
   \brief Free the memory used when parsing this signature sub-packet type
*/
static void 
ss_key_flags_free(__ops_ss_key_flags_t *ss_key_flags)
{
	data_free(&ss_key_flags->data);
}

/**
   \ingroup Core_Create
   \brief Free the memory used when parsing this signature sub-packet type
*/
static void 
ss_key_server_prefs_free(__ops_ss_key_server_prefs_t *ss_key_server_prefs)
{
	data_free(&ss_key_server_prefs->data);
}

/**
   \ingroup Core_Create
   \brief Free the memory used when parsing this signature sub-packet type
*/
static void 
ss_features_free(__ops_ss_features_t *ss_features)
{
	data_free(&ss_features->data);
}

/**
   \ingroup Core_Create
   \brief Free the memory used when parsing this signature sub-packet type
*/
static void 
ss_notation_free(__ops_ss_notation_t *ss_notation)
{
	data_free(&ss_notation->name);
	data_free(&ss_notation->value);
}

/**
\ingroup Core_Create
\brief Free allocated memory
*/
/* ! Free the memory used when parsing this signature sub-packet type */
static void 
ss_regexp_free(__ops_ss_regexp_t *regexp)
{
	string_free(&regexp->regexp);
}

/**
\ingroup Core_Create
\brief Free allocated memory
*/
/* ! Free the memory used when parsing this signature sub-packet type */
static void 
ss_policy_free(__ops_ss_policy_t *policy)
{
	string_free(&policy->url);
}

/**
\ingroup Core_Create
\brief Free allocated memory
*/
/* ! Free the memory used when parsing this signature sub-packet type */
static void 
ss_keyserv_free(__ops_ss_keyserv_t *preferred_key_server)
{
	string_free(&preferred_key_server->name);
}

/**
   \ingroup Core_Create
   \brief Free the memory used when parsing this signature sub-packet type
*/
static void 
ss_revocation_free(__ops_ss_revocation_t *ss_revocation)
{
	string_free(&ss_revocation->reason);
}

static void 
ss_embedded_sig_free(__ops_ss_embedded_sig_t *ss_embedded_sig)
{
	data_free(&ss_embedded_sig->sig);
}

/**
\ingroup Core_Create
\brief Free allocated memory
*/
/* ! Free any memory allocated when parsing the packet content */
void 
__ops_parser_content_free(__ops_packet_t *c)
{
	switch (c->tag) {
	case OPS_PARSER_PTAG:
	case OPS_PTAG_CT_COMPRESSED:
	case OPS_PTAG_SS_CREATION_TIME:
	case OPS_PTAG_SS_EXPIRATION_TIME:
	case OPS_PTAG_SS_KEY_EXPIRY:
	case OPS_PTAG_SS_TRUST:
	case OPS_PTAG_SS_ISSUER_KEY_ID:
	case OPS_PTAG_CT_1_PASS_SIG:
	case OPS_PTAG_SS_PRIMARY_USER_ID:
	case OPS_PTAG_SS_REVOCABLE:
	case OPS_PTAG_SS_REVOCATION_KEY:
	case OPS_PTAG_CT_LITERAL_DATA_HEADER:
	case OPS_PTAG_CT_LITERAL_DATA_BODY:
	case OPS_PTAG_CT_SIGNED_CLEARTEXT_BODY:
	case OPS_PTAG_CT_UNARMOURED_TEXT:
	case OPS_PTAG_CT_ARMOUR_TRAILER:
	case OPS_PTAG_CT_SIGNATURE_HEADER:
	case OPS_PTAG_CT_SE_DATA_HEADER:
	case OPS_PTAG_CT_SE_IP_DATA_HEADER:
	case OPS_PTAG_CT_SE_IP_DATA_BODY:
	case OPS_PTAG_CT_MDC:
	case OPS_GET_SECKEY:
		break;

	case OPS_PTAG_CT_SIGNED_CLEARTEXT_HEADER:
		__ops_headers_free(&c->u.cleartext_head.headers);
		break;

	case OPS_PTAG_CT_ARMOUR_HEADER:
		__ops_headers_free(&c->u.armour_header.headers);
		break;

	case OPS_PTAG_CT_SIGNED_CLEARTEXT_TRAILER:
		cleartext_trailer_free(&c->u.cleartext_trailer);
		break;

	case OPS_PTAG_CT_TRUST:
		trust_free(&c->u.trust);
		break;

	case OPS_PTAG_CT_SIGNATURE:
	case OPS_PTAG_CT_SIGNATURE_FOOTER:
		sig_free(&c->u.sig);
		break;

	case OPS_PTAG_CT_PUBLIC_KEY:
	case OPS_PTAG_CT_PUBLIC_SUBKEY:
		__ops_pubkey_free(&c->u.pubkey);
		break;

	case OPS_PTAG_CT_USER_ID:
		__ops_userid_free(&c->u.userid);
		break;

	case OPS_PTAG_SS_SIGNERS_USER_ID:
		__ops_userid_free(&c->u.ss_signer);
		break;

	case OPS_PTAG_CT_USER_ATTR:
		__ops_userattr_free(&c->u.userattr);
		break;

	case OPS_PTAG_SS_PREFERRED_SKA:
		ss_skapref_free(&c->u.ss_skapref);
		break;

	case OPS_PTAG_SS_PREFERRED_HASH:
		ss_hashpref_free(&c->u.ss_hashpref);
		break;

	case OPS_PTAG_SS_PREF_COMPRESS:
		ss_zpref_free(&c->u.ss_zpref);
		break;

	case OPS_PTAG_SS_KEY_FLAGS:
		ss_key_flags_free(&c->u.ss_key_flags);
		break;

	case OPS_PTAG_SS_KEYSERV_PREFS:
		ss_key_server_prefs_free(&c->u.ss_key_server_prefs);
		break;

	case OPS_PTAG_SS_FEATURES:
		ss_features_free(&c->u.ss_features);
		break;

	case OPS_PTAG_SS_NOTATION_DATA:
		ss_notation_free(&c->u.ss_notation);
		break;

	case OPS_PTAG_SS_REGEXP:
		ss_regexp_free(&c->u.ss_regexp);
		break;

	case OPS_PTAG_SS_POLICY_URI:
		ss_policy_free(&c->u.ss_policy);
		break;

	case OPS_PTAG_SS_PREF_KEYSERV:
		ss_keyserv_free(&c->u.ss_keyserv);
		break;

	case OPS_PTAG_SS_USERDEFINED00:
	case OPS_PTAG_SS_USERDEFINED01:
	case OPS_PTAG_SS_USERDEFINED02:
	case OPS_PTAG_SS_USERDEFINED03:
	case OPS_PTAG_SS_USERDEFINED04:
	case OPS_PTAG_SS_USERDEFINED05:
	case OPS_PTAG_SS_USERDEFINED06:
	case OPS_PTAG_SS_USERDEFINED07:
	case OPS_PTAG_SS_USERDEFINED08:
	case OPS_PTAG_SS_USERDEFINED09:
	case OPS_PTAG_SS_USERDEFINED10:
		ss_userdef_free(&c->u.ss_userdef);
		break;

	case OPS_PTAG_SS_RESERVED:
		ss_reserved_free(&c->u.ss_unknown);
		break;

	case OPS_PTAG_SS_REVOCATION_REASON:
		ss_revocation_free(&c->u.ss_revocation);
		break;

	case OPS_PTAG_SS_EMBEDDED_SIGNATURE:
		ss_embedded_sig_free(&c->u.ss_embedded_sig);
		break;

	case OPS_PARSER_PACKET_END:
		__ops_subpacket_free(&c->u.packet);
		break;

	case OPS_PARSER_ERROR:
	case OPS_PARSER_ERRCODE:
		break;

	case OPS_PTAG_CT_SECRET_KEY:
	case OPS_PTAG_CT_ENCRYPTED_SECRET_KEY:
		__ops_seckey_free(&c->u.seckey);
		break;

	case OPS_PTAG_CT_PK_SESSION_KEY:
	case OPS_PTAG_CT_ENCRYPTED_PK_SESSION_KEY:
		__ops_pk_sesskey_free(&c->u.pk_sesskey);
		break;

	case OPS_GET_PASSPHRASE:
		__ops_cmd_get_passphrase_free(&c->u.skey_passphrase);
		break;

	default:
		fprintf(stderr, "Can't free %d (0x%x)\n", c->tag, c->tag);
	}
}

/**
\ingroup Core_Create
\brief Free allocated memory
*/
void 
__ops_pk_sesskey_free(__ops_pk_sesskey_t *sk)
{
	switch (sk->alg) {
	case OPS_PKA_RSA:
		free_BN(&sk->parameters.rsa.encrypted_m);
		break;

	case OPS_PKA_ELGAMAL:
		free_BN(&sk->parameters.elgamal.g_to_k);
		free_BN(&sk->parameters.elgamal.encrypted_m);
		break;

	default:
		(void) fprintf(stderr, "__ops_pk_sesskey_free: bad alg\n");
		break;
	}
}

/**
\ingroup Core_Create
\brief Free allocated memory
*/
/* ! Free the memory used when parsing a public key */
void 
__ops_pubkey_free(__ops_pubkey_t *p)
{
	switch (p->alg) {
	case OPS_PKA_RSA:
	case OPS_PKA_RSA_ENCRYPT_ONLY:
	case OPS_PKA_RSA_SIGN_ONLY:
		free_BN(&p->key.rsa.n);
		free_BN(&p->key.rsa.e);
		break;

	case OPS_PKA_DSA:
		free_BN(&p->key.dsa.p);
		free_BN(&p->key.dsa.q);
		free_BN(&p->key.dsa.g);
		free_BN(&p->key.dsa.y);
		break;

	case OPS_PKA_ELGAMAL:
	case OPS_PKA_ELGAMAL_ENCRYPT_OR_SIGN:
		free_BN(&p->key.elgamal.p);
		free_BN(&p->key.elgamal.g);
		free_BN(&p->key.elgamal.y);
		break;

	case 0:
		/* nothing to free */
		break;

	default:
		(void) fprintf(stderr, "__ops_pubkey_free: bad alg\n");
	}
}

/**
   \ingroup Core_ReadPackets
*/
static int 
parse_pubkey_data(__ops_pubkey_t *key, __ops_region_t *region,
		      __ops_parseinfo_t *pinfo)
{
	unsigned char   c = 0x0;

	if (region->readc != 0) {
		/* We should not have read anything so far */
		(void) fprintf(stderr, "parse_pubkey_data: bad length\n");
		return 0;
	}

	if (!limread(&c, 1, region, pinfo)) {
		return 0;
	}
	key->version = c;
	if (key->version < 2 || key->version > 4) {
		OPS_ERROR_1(&pinfo->errors, OPS_E_PROTO_BAD_PUBLIC_KEY_VRSN,
			    "Bad public key version (0x%02x)", key->version);
		return 0;
	}
	if (!limited_read_time(&key->birthtime, region, pinfo)) {
		return 0;
	}

	key->days_valid = 0;
	if ((key->version == 2 || key->version == 3) &&
	    !limread_scalar(&key->days_valid, 2, region, pinfo)) {
		return 0;
	}

	if (!limread(&c, 1, region, pinfo)) {
		return 0;
	}
	key->alg = c;

	switch (key->alg) {
	case OPS_PKA_DSA:
		if (!limread_mpi(&key->key.dsa.p, region, pinfo) ||
		    !limread_mpi(&key->key.dsa.q, region, pinfo) ||
		    !limread_mpi(&key->key.dsa.g, region, pinfo) ||
		    !limread_mpi(&key->key.dsa.y, region, pinfo)) {
			return 0;
		}
		break;

	case OPS_PKA_RSA:
	case OPS_PKA_RSA_ENCRYPT_ONLY:
	case OPS_PKA_RSA_SIGN_ONLY:
		if (!limread_mpi(&key->key.rsa.n, region, pinfo) ||
		    !limread_mpi(&key->key.rsa.e, region, pinfo)) {
			return 0;
		}
		break;

	case OPS_PKA_ELGAMAL:
	case OPS_PKA_ELGAMAL_ENCRYPT_OR_SIGN:
		if (!limread_mpi(&key->key.elgamal.p, region, pinfo) ||
		    !limread_mpi(&key->key.elgamal.g, region, pinfo) ||
		    !limread_mpi(&key->key.elgamal.y, region, pinfo)) {
			return 0;
		}
		break;

	default:
		OPS_ERROR_1(&pinfo->errors,
			OPS_E_ALG_UNSUPPORTED_PUBLIC_KEY_ALG,
			"Unsupported Public Key algorithm (%s)",
			__ops_show_pka(key->alg));
		return 0;
	}

	return 1;
}


/**
 * \ingroup Core_ReadPackets
 * \brief Parse a public key packet.
 *
 * This function parses an entire v3 (== v2) or v4 public key packet for RSA, ElGamal, and DSA keys.
 *
 * Once the key has been parsed successfully, it is passed to the callback.
 *
 * \param *ptag		Pointer to the current Packet Tag.  This function should consume the entire packet.
 * \param *reader	Our reader
 * \param *cb		The callback
 * \return		1 on success, 0 on error
 *
 * \see RFC4880 5.5.2
 */
static int 
parse_pubkey(__ops_content_tag_t tag, __ops_region_t *region,
		 __ops_parseinfo_t *pinfo)
{
	__ops_packet_t pkt;

	if (!parse_pubkey_data(&pkt.u.pubkey, region, pinfo))
		return 0;

	/* XXX: this test should be done for all packets, surely? */
	if (region->readc != region->length) {
		OPS_ERROR_1(&pinfo->errors, OPS_E_R_UNCONSUMED_DATA,
			    "Unconsumed data (%d)", region->length - region->readc);
		return 0;
	}
	CALLBACK(&pinfo->cbinfo, tag, &pkt);

	return 1;
}


/**
\ingroup Core_Create
\brief Free allocated memory
*/
/* ! Free the memory used when parsing this packet type */
void 
__ops_userattr_free(__ops_userattr_t *user_att)
{
	data_free(&user_att->data);
}

/**
 * \ingroup Core_ReadPackets
 * \brief Parse one user attribute packet.
 *
 * User attribute packets contain one or more attribute subpackets.
 * For now, handle the whole packet as raw data.
 */

static int 
parse_userattr(__ops_region_t *region, __ops_parseinfo_t *pinfo)
{

	__ops_packet_t pkt;

	/*
	 * xxx- treat as raw data for now. Could break down further into
	 * attribute sub-packets later - rachel
	 */

	if (region->readc != 0) {
		/* We should not have read anything so far */
		(void) fprintf(stderr, "parse_userattr: bad length\n");
		return 0;
	}

	if (!read_data(&pkt.u.userattr.data, region, pinfo))
		return 0;

	CALLBACK(&pinfo->cbinfo, OPS_PTAG_CT_USER_ATTR, &pkt);

	return 1;
}

/**
\ingroup Core_Create
\brief Free allocated memory
*/
/* ! Free the memory used when parsing this packet type */
void 
__ops_userid_free(__ops_userid_t *id)
{
	(void) free(id->userid);
	id->userid = NULL;
}

/**
 * \ingroup Core_ReadPackets
 * \brief Parse a user id.
 *
 * This function parses an user id packet, which is basically just a char array the size of the packet.
 *
 * The char array is to be treated as an UTF-8 string.
 *
 * The userid gets null terminated by this function.  Freeing it is the responsibility of the caller.
 *
 * Once the userid has been parsed successfully, it is passed to the callback.
 *
 * \param *ptag		Pointer to the Packet Tag.  This function should consume the entire packet.
 * \param *reader	Our reader
 * \param *cb		The callback
 * \return		1 on success, 0 on error
 *
 * \see RFC4880 5.11
 */
static int 
parse_userid(__ops_region_t *region, __ops_parseinfo_t *pinfo)
{
	__ops_packet_t pkt;

	 if (region->readc != 0) {
		/* We should not have read anything so far */
		(void) fprintf(stderr, "parse_userid: bad length\n");
		return 0;
	}

	/* XXX should we not like check malloc's return value? */
	pkt.u.userid.userid = calloc(1, region->length + 1);

	if (region->length &&
	    !limread(pkt.u.userid.userid, region->length, region,
			pinfo)) {
		return 0;
	}
	pkt.u.userid.userid[region->length] = '\0';
	CALLBACK(&pinfo->cbinfo, OPS_PTAG_CT_USER_ID, &pkt);
	return 1;
}

static __ops_hash_t     *
parse_hash_find(__ops_parseinfo_t *pinfo,
		    const unsigned char keyid[OPS_KEY_ID_SIZE])
{
	size_t          n;

	for (n = 0; n < pinfo->nhashes; ++n) {
		if (memcmp(pinfo->hashes[n].keyid, keyid,
					OPS_KEY_ID_SIZE) == 0) {
			return &pinfo->hashes[n].hash;
		}
	}
	return NULL;
}

/**
 * \ingroup Core_Parse
 * \brief Parse a version 3 signature.
 *
 * This function parses an version 3 signature packet, handling RSA and DSA signatures.
 *
 * Once the signature has been parsed successfully, it is passed to the callback.
 *
 * \param *ptag		Pointer to the Packet Tag.  This function should consume the entire packet.
 * \param *reader	Our reader
 * \param *cb		The callback
 * \return		1 on success, 0 on error
 *
 * \see RFC4880 5.2.2
 */
static int 
parse_v3_sig(__ops_region_t *region,
		   __ops_parseinfo_t *pinfo)
{
	__ops_packet_t	pkt;
	unsigned char		c = 0x0;

	/* clear signature */
	(void) memset(&pkt.u.sig, 0x0, sizeof(pkt.u.sig));

	pkt.u.sig.info.version = OPS_V3;

	/* hash info length */
	if (!limread(&c, 1, region, pinfo)) {
		return 0;
	}
	if (c != 5) {
		ERRP(&pinfo->cbinfo, pkt, "bad hash info length");
	}

	if (!limread(&c, 1, region, pinfo)) {
		return 0;
	}
	pkt.u.sig.info.type = c;
	/* XXX: check signature type */

	if (!limited_read_time(&pkt.u.sig.info.birthtime, region, pinfo)) {
		return 0;
	}
	pkt.u.sig.info.birthtime_set = 1;

	if (!limread(pkt.u.sig.info.signer_id, OPS_KEY_ID_SIZE, region,
			pinfo)) {
		return 0;
	}
	pkt.u.sig.info.signer_id_set = 1;

	if (!limread(&c, 1, region, pinfo)) {
		return 0;
	}
	pkt.u.sig.info.key_alg = c;
	/* XXX: check algorithm */

	if (!limread(&c, 1, region, pinfo)) {
		return 0;
	}
	pkt.u.sig.info.hash_alg = c;
	/* XXX: check algorithm */

	if (!limread(pkt.u.sig.hash2, 2, region, pinfo)) {
		return 0;
	}

	switch (pkt.u.sig.info.key_alg) {
	case OPS_PKA_RSA:
	case OPS_PKA_RSA_SIGN_ONLY:
		if (!limread_mpi(&pkt.u.sig.info.sig.rsa.sig, region, pinfo)) {
			return 0;
		}
		break;

	case OPS_PKA_DSA:
		if (!limread_mpi(&pkt.u.sig.info.sig.dsa.r, region, pinfo) ||
		    !limread_mpi(&pkt.u.sig.info.sig.dsa.s, region, pinfo)) {
			return 0;
		}
		break;

	case OPS_PKA_ELGAMAL_ENCRYPT_OR_SIGN:
		if (!limread_mpi(&pkt.u.sig.info.sig.elgamal.r, region,
				pinfo) ||
		    !limread_mpi(&pkt.u.sig.info.sig.elgamal.s, region,
		    		pinfo)) {
			return 0;
		}
		break;

	default:
		OPS_ERROR_1(&pinfo->errors,
			OPS_E_ALG_UNSUPPORTED_SIGNATURE_ALG,
			"Unsupported signature key algorithm (%s)",
			__ops_show_pka(pkt.u.sig.info.key_alg));
		return 0;
	}

	if (region->readc != region->length) {
		OPS_ERROR_1(&pinfo->errors, OPS_E_R_UNCONSUMED_DATA,
			"Unconsumed data (%d)",
			region->length - region->readc);
		return 0;
	}
	if (pkt.u.sig.info.signer_id_set) {
		pkt.u.sig.hash = parse_hash_find(pinfo,
				pkt.u.sig.info.signer_id);
	}
	CALLBACK(&pinfo->cbinfo, OPS_PTAG_CT_SIGNATURE, &pkt);
	return 1;
}

/**
 * \ingroup Core_ReadPackets
 * \brief Parse one signature sub-packet.
 *
 * Version 4 signatures can have an arbitrary amount of (hashed and unhashed) subpackets.  Subpackets are used to hold
 * optional attributes of subpackets.
 *
 * This function parses one such signature subpacket.
 *
 * Once the subpacket has been parsed successfully, it is passed to the callback.
 *
 * \param *ptag		Pointer to the Packet Tag.  This function should consume the entire subpacket.
 * \param *reader	Our reader
 * \param *cb		The callback
 * \return		1 on success, 0 on error
 *
 * \see RFC4880 5.2.3
 */
static int 
parse_one_sig_subpacket(__ops_sig_t *sig,
			      __ops_region_t *region,
			      __ops_parseinfo_t *pinfo)
{
	__ops_region_t	subregion;
	__ops_packet_t	pkt;
	unsigned char   bools = 0x0;
	unsigned char	c = 0x0;
	unsigned	doread = 1;
	unsigned        t8;
	unsigned        t7;

	__ops_init_subregion(&subregion, region);
	if (!limited_read_new_length(&subregion.length, region, pinfo)) {
		return 0;
	}

	if (subregion.length > region->length) {
		ERRP(&pinfo->cbinfo, pkt, "Subpacket too long");
	}

	if (!limread(&c, 1, &subregion, pinfo)) {
		return 0;
	}

	t8 = (c & 0x7f) / 8;
	t7 = 1 << (c & 7);

	pkt.critical = (unsigned)c >> 7;
	pkt.tag = OPS_PTAG_SIG_SUBPKT_BASE + (c & 0x7f);

	/* Application wants it delivered raw */
	if (pinfo->ss_raw[t8] & t7) {
		pkt.u.ss_raw.tag = pkt.tag;
		pkt.u.ss_raw.length = subregion.length - 1;
		pkt.u.ss_raw.raw = calloc(1, pkt.u.ss_raw.length);
		if (!limread(pkt.u.ss_raw.raw, pkt.u.ss_raw.length,
				&subregion, pinfo)) {
			return 0;
		}
		CALLBACK(&pinfo->cbinfo, OPS_PTAG_RAW_SS, &pkt);
		return 1;
	}
	switch (pkt.tag) {
	case OPS_PTAG_SS_CREATION_TIME:
	case OPS_PTAG_SS_EXPIRATION_TIME:
	case OPS_PTAG_SS_KEY_EXPIRY:
		if (!limited_read_time(&pkt.u.ss_time.time, &subregion, pinfo))
			return 0;
		if (pkt.tag == OPS_PTAG_SS_CREATION_TIME) {
			sig->info.birthtime = pkt.u.ss_time.time;
			sig->info.birthtime_set = 1;
		}
		break;

	case OPS_PTAG_SS_TRUST:
		if (!limread(&pkt.u.ss_trust.level, 1, &subregion, pinfo) ||
		    !limread(&pkt.u.ss_trust.amount, 1, &subregion, pinfo)) {
			return 0;
		}
		break;

	case OPS_PTAG_SS_REVOCABLE:
		if (!limread(&bools, 1, &subregion, pinfo)) {
			return 0;
		}
		pkt.u.ss_revocable.revocable = !!bools;
		break;

	case OPS_PTAG_SS_ISSUER_KEY_ID:
		if (!limread(pkt.u.ss_issuer.key_id, OPS_KEY_ID_SIZE,
				&subregion, pinfo)) {
			return 0;
		}
		(void) memcpy(sig->info.signer_id,
			pkt.u.ss_issuer.key_id, OPS_KEY_ID_SIZE);
		sig->info.signer_id_set = 1;
		break;

	case OPS_PTAG_SS_PREFERRED_SKA:
		if (!read_data(&pkt.u.ss_skapref.data, &subregion, pinfo)) {
			return 0;
		}
		break;

	case OPS_PTAG_SS_PREFERRED_HASH:
		if (!read_data(&pkt.u.ss_hashpref.data, &subregion, pinfo)) {
			return 0;
		}
		break;

	case OPS_PTAG_SS_PREF_COMPRESS:
		if (!read_data(&pkt.u.ss_zpref.data,
				&subregion, pinfo)) {
			return 0;
		}
		break;

	case OPS_PTAG_SS_PRIMARY_USER_ID:
		if (!limread(&bools, 1, &subregion, pinfo)) {
			return 0;
		}
		pkt.u.ss_primary_userid.primary_userid = !!bools;
		break;

	case OPS_PTAG_SS_KEY_FLAGS:
		if (!read_data(&pkt.u.ss_key_flags.data, &subregion, pinfo)) {
			return 0;
		}
		break;

	case OPS_PTAG_SS_KEYSERV_PREFS:
		if (!read_data(&pkt.u.ss_key_server_prefs.data, &subregion,
				pinfo)) {
			return 0;
		}
		break;

	case OPS_PTAG_SS_FEATURES:
		if (!read_data(&pkt.u.ss_features.data, &subregion, pinfo)) {
			return 0;
		}
		break;

	case OPS_PTAG_SS_SIGNERS_USER_ID:
		if (!read_unsig_str(&pkt.u.ss_signer.userid, &subregion,
				pinfo)) {
			return 0;
		}
		break;

	case OPS_PTAG_SS_EMBEDDED_SIGNATURE:
		/* \todo should do something with this sig? */
		if (!read_data(&pkt.u.ss_embedded_sig.sig, &subregion, pinfo)) {
			return 0;
		}
		break;

	case OPS_PTAG_SS_NOTATION_DATA:
		if (!limread_data(&pkt.u.ss_notation.flags, 4,
				&subregion, pinfo)) {
			return 0;
		}
		if (!limread_size_t(&pkt.u.ss_notation.name.len, 2,
				&subregion, pinfo)) {
			return 0;
		}
		if (!limread_size_t(&pkt.u.ss_notation.value.len, 2,
				&subregion, pinfo)) {
			return 0;
		}
		if (!limread_data(&pkt.u.ss_notation.name,
				pkt.u.ss_notation.name.len,
				&subregion, pinfo)) {
			return 0;
		}
		if (!limread_data(&pkt.u.ss_notation.value,
			   pkt.u.ss_notation.value.len,
			   &subregion, pinfo)) {
			return 0;
		}
		break;

	case OPS_PTAG_SS_POLICY_URI:
		if (!read_string(&pkt.u.ss_policy.url, &subregion, pinfo)) {
			return 0;
		}
		break;

	case OPS_PTAG_SS_REGEXP:
		if (!read_string(&pkt.u.ss_regexp.regexp, &subregion, pinfo)) {
			return 0;
		}
		break;

	case OPS_PTAG_SS_PREF_KEYSERV:
		if (!read_string(&pkt.u.ss_keyserv.name, &subregion, pinfo)) {
			return 0;
		}
		break;

	case OPS_PTAG_SS_USERDEFINED00:
	case OPS_PTAG_SS_USERDEFINED01:
	case OPS_PTAG_SS_USERDEFINED02:
	case OPS_PTAG_SS_USERDEFINED03:
	case OPS_PTAG_SS_USERDEFINED04:
	case OPS_PTAG_SS_USERDEFINED05:
	case OPS_PTAG_SS_USERDEFINED06:
	case OPS_PTAG_SS_USERDEFINED07:
	case OPS_PTAG_SS_USERDEFINED08:
	case OPS_PTAG_SS_USERDEFINED09:
	case OPS_PTAG_SS_USERDEFINED10:
		if (!read_data(&pkt.u.ss_userdef.data, &subregion, pinfo)) {
			return 0;
		}
		break;

	case OPS_PTAG_SS_RESERVED:
		if (!read_data(&pkt.u.ss_unknown.data, &subregion, pinfo)) {
			return 0;
		}
		break;

	case OPS_PTAG_SS_REVOCATION_REASON:
		/* first byte is the machine-readable code */
		if (!limread(&pkt.u.ss_revocation.code, 1, &subregion, pinfo)) {
			return 0;
		}
		/* the rest is a human-readable UTF-8 string */
		if (!read_string(&pkt.u.ss_revocation.reason, &subregion,
				pinfo)) {
			return 0;
		}
		break;

	case OPS_PTAG_SS_REVOCATION_KEY:
		/* octet 0 = class. Bit 0x80 must be set */
		if (!limread(&pkt.u.ss_revocation_key.class, 1,
				&subregion, pinfo)) {
			return 0;
		}
		if (!(pkt.u.ss_revocation_key.class & 0x80)) {
			printf("Warning: OPS_PTAG_SS_REVOCATION_KEY class: "
			       "Bit 0x80 should be set\n");
			return 0;
		}
		/* octet 1 = algid */
		if (!limread(&pkt.u.ss_revocation_key.algid, 1,
				&subregion, pinfo)) {
			return 0;
		}
		/* octets 2-21 = fingerprint */
		if (!limread(&pkt.u.ss_revocation_key.fingerprint[0],
				OPS_FINGERPRINT_SIZE, &subregion, pinfo)) {
			return 0;
		}
		break;

	default:
		if (pinfo->ss_parsed[t8] & t7) {
			OPS_ERROR_1(&pinfo->errors, OPS_E_PROTO_UNKNOWN_SS,
				    "Unknown signature subpacket type (%d)",
				    c & 0x7f);
		}
		doread = 0;
		break;
	}

	/* Application doesn't want it delivered parsed */
	if (!(pinfo->ss_parsed[t8] & t7)) {
		if (pkt.critical) {
			OPS_ERROR_1(&pinfo->errors,
				OPS_E_PROTO_CRITICAL_SS_IGNORED,
				"Critical signature subpacket ignored (%d)",
				c & 0x7f);
		}
		if (!doread &&
		    !limskip(subregion.length - 1, &subregion, pinfo)) {
			return 0;
		}
		if (doread) {
			__ops_parser_content_free(&pkt);
		}
		return 1;
	}
	if (doread && subregion.readc != subregion.length) {
		OPS_ERROR_1(&pinfo->errors, OPS_E_R_UNCONSUMED_DATA,
			    "Unconsumed data (%d)",
			    subregion.length - subregion.readc);
		return 0;
	}
	CALLBACK(&pinfo->cbinfo, pkt.tag, &pkt);
	return 1;
}

/**
 * \ingroup Core_ReadPackets
 * \brief Parse several signature subpackets.
 *
 * Hashed and unhashed subpacket sets are preceded by an octet count that specifies the length of the complete set.
 * This function parses this length and then calls parse_one_sig_subpacket() for each subpacket until the
 * entire set is consumed.
 *
 * This function does not call the callback directly, parse_one_sig_subpacket() does for each subpacket.
 *
 * \param *ptag		Pointer to the Packet Tag.
 * \param *reader	Our reader
 * \param *cb		The callback
 * \return		1 on success, 0 on error
 *
 * \see RFC4880 5.2.3
 */
static int 
parse_sig_subpkts(__ops_sig_t *sig,
			   __ops_region_t *region,
			   __ops_parseinfo_t *pinfo)
{
	__ops_region_t	subregion;
	__ops_packet_t	pkt;

	__ops_init_subregion(&subregion, region);
	if (!limread_scalar(&subregion.length, 2, region, pinfo)) {
		return 0;
	}

	if (subregion.length > region->length) {
		ERRP(&pinfo->cbinfo, pkt, "Subpacket set too long");
	}

	while (subregion.readc < subregion.length) {
		if (!parse_one_sig_subpacket(sig, &subregion, pinfo)) {
			return 0;
		}
	}

	if (subregion.readc != subregion.length) {
		if (!limskip(subregion.length - subregion.readc,
				&subregion, pinfo)) {
			ERRP(&pinfo->cbinfo, pkt,
"parse_sig_subpkts: subpacket length read mismatch");
		}
		ERRP(&pinfo->cbinfo, pkt, "Subpacket length mismatch");
	}
	return 1;
}

/**
 * \ingroup Core_ReadPackets
 * \brief Parse a version 4 signature.
 *
 * This function parses a version 4 signature including all its hashed and unhashed subpackets.
 *
 * Once the signature packet has been parsed successfully, it is passed to the callback.
 *
 * \param *ptag		Pointer to the Packet Tag.
 * \param *reader	Our reader
 * \param *cb		The callback
 * \return		1 on success, 0 on error
 *
 * \see RFC4880 5.2.3
 */
static int 
parse_v4_sig(__ops_region_t *region, __ops_parseinfo_t *pinfo)
{
	unsigned char   c = 0x0;
	__ops_packet_t pkt;

	/* debug=1; */
	if (__ops_get_debug_level(__FILE__)) {
		fprintf(stderr, "\nparse_v4_sig\n");
	}
	/* clear signature */
	(void) memset(&pkt.u.sig, 0x0, sizeof(pkt.u.sig));

	/*
	 * We need to hash the packet data from version through the hashed
	 * subpacket data
	 */

	pkt.u.sig.v4_hashstart = pinfo->readinfo.alength - 1;

	/* Set version,type,algorithms */

	pkt.u.sig.info.version = OPS_V4;

	if (!limread(&c, 1, region, pinfo)) {
		return 0;
	}
	pkt.u.sig.info.type = c;
	if (__ops_get_debug_level(__FILE__)) {
		fprintf(stderr, "signature type=%d (%s)\n",
			pkt.u.sig.info.type,
			__ops_show_sig_type(pkt.u.sig.info.type));
	}
	/* XXX: check signature type */

	if (!limread(&c, 1, region, pinfo)) {
		return 0;
	}
	pkt.u.sig.info.key_alg = c;
	/* XXX: check algorithm */
	if (__ops_get_debug_level(__FILE__)) {
		(void) fprintf(stderr, "key_alg=%d (%s)\n",
			pkt.u.sig.info.key_alg,
			__ops_show_pka(pkt.u.sig.info.key_alg));
	}
	if (!limread(&c, 1, region, pinfo)) {
		return 0;
	}
	pkt.u.sig.info.hash_alg = c;
	/* XXX: check algorithm */
	if (__ops_get_debug_level(__FILE__)) {
		fprintf(stderr, "hash_alg=%d %s\n",
			pkt.u.sig.info.hash_alg,
		  __ops_show_hash_alg(pkt.u.sig.info.hash_alg));
	}
	CALLBACK(&pinfo->cbinfo, OPS_PTAG_CT_SIGNATURE_HEADER, &pkt);

	if (!parse_sig_subpkts(&pkt.u.sig, region, pinfo)) {
		return 0;
	}

	pkt.u.sig.info.v4_hashlen = pinfo->readinfo.alength
					- pkt.u.sig.v4_hashstart;

	/* copy hashed subpackets */
	if (pkt.u.sig.info.v4_hashed) {
		(void) free(pkt.u.sig.info.v4_hashed);
	}
	pkt.u.sig.info.v4_hashed = calloc(1, pkt.u.sig.info.v4_hashlen);

	if (!pinfo->readinfo.accumulate) {
		/* We must accumulate, else we can't check the signature */
		fprintf(stderr, "*** ERROR: must set accumulate to 1\n");
		return 0;
	}
	(void) memcpy(pkt.u.sig.info.v4_hashed,
	       pinfo->readinfo.accumulated + pkt.u.sig.v4_hashstart,
	       pkt.u.sig.info.v4_hashlen);

	if (!parse_sig_subpkts(&pkt.u.sig, region, pinfo)) {
		return 0;
	}

	if (!limread(pkt.u.sig.hash2, 2, region, pinfo)) {
		return 0;
	}

	switch (pkt.u.sig.info.key_alg) {
	case OPS_PKA_RSA:
		if (!limread_mpi(&pkt.u.sig.info.sig.rsa.sig, region, pinfo)) {
			return 0;
		}
		break;

	case OPS_PKA_DSA:
		if (!limread_mpi(&pkt.u.sig.info.sig.dsa.r, region, pinfo)) {
			/*
			 * usually if this fails, it just means we've reached
			 * the end of the keyring
			 */
			if (__ops_get_debug_level(__FILE__)) {
				(void) fprintf(stderr,
				"Error reading DSA r field in signature");
			}
			return 0;
		}
		if (!limread_mpi(&pkt.u.sig.info.sig.dsa.s, region, pinfo)) {
			ERRP(&pinfo->cbinfo, pkt,
			"Error reading DSA s field in signature");
		}
		break;

	case OPS_PKA_ELGAMAL_ENCRYPT_OR_SIGN:
		if (!limread_mpi(&pkt.u.sig.info.sig.elgamal.r, region,
				pinfo) ||
		    !limread_mpi(&pkt.u.sig.info.sig.elgamal.s, region,
		    		pinfo)) {
			return 0;
		}
		break;

	case OPS_PKA_PRIVATE00:
	case OPS_PKA_PRIVATE01:
	case OPS_PKA_PRIVATE02:
	case OPS_PKA_PRIVATE03:
	case OPS_PKA_PRIVATE04:
	case OPS_PKA_PRIVATE05:
	case OPS_PKA_PRIVATE06:
	case OPS_PKA_PRIVATE07:
	case OPS_PKA_PRIVATE08:
	case OPS_PKA_PRIVATE09:
	case OPS_PKA_PRIVATE10:
		if (!read_data(&pkt.u.sig.info.sig.unknown.data, region,
				pinfo)) {
			return 0;
		}
		break;

	default:
		OPS_ERROR_1(&pinfo->errors, OPS_E_ALG_UNSUPPORTED_SIGNATURE_ALG,
			    "Bad v4 signature key algorithm (%s)",
			    __ops_show_pka(pkt.u.sig.info.key_alg));
		return 0;
	}
	if (region->readc != region->length) {
		OPS_ERROR_1(&pinfo->errors, OPS_E_R_UNCONSUMED_DATA,
			    "Unconsumed data (%d)",
			    region->length - region->readc);
		return 0;
	}
	CALLBACK(&pinfo->cbinfo, OPS_PTAG_CT_SIGNATURE_FOOTER, &pkt);
	return 1;
}

/**
 * \ingroup Core_ReadPackets
 * \brief Parse a signature subpacket.
 *
 * This function calls the appropriate function to handle v3 or v4 signatures.
 *
 * Once the signature packet has been parsed successfully, it is passed to the callback.
 *
 * \param *ptag		Pointer to the Packet Tag.
 * \param *reader	Our reader
 * \param *cb		The callback
 * \return		1 on success, 0 on error
 */
static int 
parse_sig(__ops_region_t *region, __ops_parseinfo_t *pinfo)
{
	unsigned char   c = 0x0;
	__ops_packet_t pkt;

	if (region->readc != 0) {
		/* We should not have read anything so far */
		(void) fprintf(stderr, "parse_sig: bad length\n");
		return 0;
	}

	(void) memset(&pkt, 0x0, sizeof(pkt));
	if (!limread(&c, 1, region, pinfo)) {
		return 0;
	}
	if (c == 2 || c == 3) {
		return parse_v3_sig(region, pinfo);
	}
	if (c == 4) {
		return parse_v4_sig(region, pinfo);
	}
	OPS_ERROR_1(&pinfo->errors, OPS_E_PROTO_BAD_SIGNATURE_VRSN,
		    "Bad signature version (%d)", c);
	return 0;
}

/**
 \ingroup Core_ReadPackets
 \brief Parse Compressed packet
*/
static int 
parse_compressed(__ops_region_t *region, __ops_parseinfo_t *pinfo)
{
	__ops_packet_t	pkt;
	unsigned char		c = 0x0;

	if (!limread(&c, 1, region, pinfo)) {
		return 0;
	}

	pkt.u.compressed.type = c;

	CALLBACK(&pinfo->cbinfo, OPS_PTAG_CT_COMPRESSED, &pkt);

	/*
	 * The content of a compressed data packet is more OpenPGP packets
	 * once decompressed, so recursively handle them
	 */

	return __ops_decompress(region, pinfo, pkt.u.compressed.type);
}

/* XXX: this could be improved by sharing all hashes that are the */
/* same, then duping them just before checking the signature. */
static void 
parse_hash_init(__ops_parseinfo_t *pinfo, __ops_hash_alg_t type,
		    const unsigned char *keyid)
{
	__ops_hashtype_t *hash;

	pinfo->hashes = realloc(pinfo->hashes,
			      (pinfo->nhashes + 1) * sizeof(*pinfo->hashes));
	hash = &pinfo->hashes[pinfo->nhashes++];

	__ops_hash_any(&hash->hash, type);
	hash->hash.init(&hash->hash);
	(void) memcpy(hash->keyid, keyid, sizeof(hash->keyid));
}

/**
   \ingroup Core_ReadPackets
   \brief Parse a One Pass Signature packet
*/
static int 
parse_one_pass(__ops_region_t * region, __ops_parseinfo_t * pinfo)
{
	unsigned char   c = 0x0;
	__ops_packet_t pkt;

	if (!limread(&pkt.u.one_pass_sig.version, 1, region, pinfo)) {
		return 0;
	}
	if (pkt.u.one_pass_sig.version != 3) {
		OPS_ERROR_1(&pinfo->errors, OPS_E_PROTO_BAD_ONE_PASS_SIG_VRSN,
			    "Bad one-pass signature version (%d)",
			    pkt.u.one_pass_sig.version);
		return 0;
	}
	if (!limread(&c, 1, region, pinfo)) {
		return 0;
	}
	pkt.u.one_pass_sig.sig_type = c;

	if (!limread(&c, 1, region, pinfo)) {
		return 0;
	}
	pkt.u.one_pass_sig.hash_alg = c;

	if (!limread(&c, 1, region, pinfo)) {
		return 0;
	}
	pkt.u.one_pass_sig.key_alg = c;

	if (!limread(pkt.u.one_pass_sig.keyid,
			  sizeof(pkt.u.one_pass_sig.keyid), region, pinfo)) {
		return 0;
	}

	if (!limread(&c, 1, region, pinfo)) {
		return 0;
	}
	pkt.u.one_pass_sig.nested = !!c;
	CALLBACK(&pinfo->cbinfo, OPS_PTAG_CT_1_PASS_SIG, &pkt);
	/* XXX: we should, perhaps, let the app choose whether to hash or not */
	parse_hash_init(pinfo, pkt.u.one_pass_sig.hash_alg,
			    pkt.u.one_pass_sig.keyid);
	return 1;
}

/**
 \ingroup Core_ReadPackets
 \brief Parse a Trust packet
*/
static int
parse_trust(__ops_region_t *region, __ops_parseinfo_t *pinfo)
{
	__ops_packet_t pkt;

	if (!read_data(&pkt.u.trust.data, region, pinfo)) {
		return 0;
	}
	CALLBACK(&pinfo->cbinfo, OPS_PTAG_CT_TRUST, &pkt);
	return 1;
}

static void 
parse_hash_data(__ops_parseinfo_t *pinfo, const void *data,
		    size_t length)
{
	size_t          n;

	for (n = 0; n < pinfo->nhashes; ++n) {
		pinfo->hashes[n].hash.add(&pinfo->hashes[n].hash, data, length);
	}
}

/**
   \ingroup Core_ReadPackets
   \brief Parse a Literal Data packet
*/
static int 
parse_litdata(__ops_region_t *region, __ops_parseinfo_t *pinfo)
{
	__ops_memory_t	*mem;
	__ops_packet_t	 pkt;
	unsigned char	 c = 0x0;

	if (!limread(&c, 1, region, pinfo)) {
		return 0;
	}
	pkt.u.litdata_header.format = c;
	if (!limread(&c, 1, region, pinfo)) {
		return 0;
	}
	if (!limread((unsigned char *)pkt.u.litdata_header.filename,
			(unsigned)c, region, pinfo)) {
		return 0;
	}
	pkt.u.litdata_header.filename[c] = '\0';
	if (!limited_read_time(&pkt.u.litdata_header.mtime, region, pinfo)) {
		return 0;
	}
	CALLBACK(&pinfo->cbinfo, OPS_PTAG_CT_LITERAL_DATA_HEADER, &pkt);
	mem = pkt.u.litdata_body.mem = __ops_memory_new();
	__ops_memory_init(pkt.u.litdata_body.mem,
			(unsigned)(region->length * 1.01) + 12);
	pkt.u.litdata_body.data = mem->buf;

	while (region->readc < region->length) {
		unsigned        readc = region->length - region->readc;

		if (!limread(mem->buf, readc, region, pinfo)) {
			return 0;
		}
		pkt.u.litdata_body.length = readc;
		parse_hash_data(pinfo, pkt.u.litdata_body.data, region->length);
		CALLBACK(&pinfo->cbinfo, OPS_PTAG_CT_LITERAL_DATA_BODY, &pkt);
	}

	/* XXX - get rid of mem here? */

	return 1;
}

/**
 * \ingroup Core_Create
 *
 * __ops_seckey_free() frees the memory associated with "key". Note that
 * the key itself is not freed.
 *
 * \param key
 */

void 
__ops_seckey_free(__ops_seckey_t *key)
{
	switch (key->pubkey.alg) {
	case OPS_PKA_RSA:
	case OPS_PKA_RSA_ENCRYPT_ONLY:
	case OPS_PKA_RSA_SIGN_ONLY:
		free_BN(&key->key.rsa.d);
		free_BN(&key->key.rsa.p);
		free_BN(&key->key.rsa.q);
		free_BN(&key->key.rsa.u);
		break;

	case OPS_PKA_DSA:
		free_BN(&key->key.dsa.x);
		break;

	default:
		(void) fprintf(stderr,
			"__ops_seckey_free: Unknown algorithm: %d (%s)\n",
			key->pubkey.alg,
			__ops_show_pka(key->pubkey.alg));
	}
	(void) free(key->checkhash);
	__ops_pubkey_free(&key->pubkey);
}

static int 
consume_packet(__ops_region_t *region, __ops_parseinfo_t *pinfo, unsigned warn)
{
	__ops_packet_t	pkt;
	__ops_data_t	remainder;

	if (region->indeterminate) {
		ERRP(&pinfo->cbinfo, pkt,
			"Can't consume indeterminate packets");
	}

	if (read_data(&remainder, region, pinfo)) {
		/* now throw it away */
		data_free(&remainder);
		if (warn) {
			OPS_ERROR(&pinfo->errors, OPS_E_P_PACKET_CONSUMED,
				"Warning: packet consumer");
		}
		return 1;
	}
	OPS_ERROR(&pinfo->errors, OPS_E_P_PACKET_NOT_CONSUMED,
			(warn) ? "Warning: Packet was not consumed" :
				"Packet was not consumed");
	return warn;
}

/**
 * \ingroup Core_ReadPackets
 * \brief Parse a secret key
 */
static int 
parse_seckey(__ops_region_t *region, __ops_parseinfo_t *pinfo)
{
	__ops_packet_t		pkt;
	__ops_region_t		encregion;
	__ops_region_t	       *saved_region = NULL;
	unsigned char		c = 0x0;
	__ops_crypt_t		decrypt;
	__ops_hash_t		checkhash;
	unsigned		blocksize;
	unsigned		crypted;
	int			ret = 1;

	if (__ops_get_debug_level(__FILE__)) {
		fprintf(stderr, "\n---------\nparse_seckey:\n");
		fprintf(stderr,
			"region length=%d, readc=%d, remainder=%d\n",
			region->length, region->readc,
			region->length - region->readc);
	}
	(void) memset(&pkt, 0x0, sizeof(pkt));
	if (!parse_pubkey_data(&pkt.u.seckey.pubkey, region, pinfo)) {
		return 0;
	}
	if (__ops_get_debug_level(__FILE__)) {
		fprintf(stderr, "parse_seckey: public key parsed\n");
		__ops_print_pubkey(&pkt.u.seckey.pubkey);
	}
	pinfo->reading_v3_secret = pkt.u.seckey.pubkey.version != OPS_V4;

	if (!limread(&c, 1, region, pinfo)) {
		return 0;
	}
	pkt.u.seckey.s2k_usage = c;

	if (pkt.u.seckey.s2k_usage == OPS_S2KU_ENCRYPTED ||
	    pkt.u.seckey.s2k_usage == OPS_S2KU_ENCRYPTED_AND_HASHED) {
		if (!limread(&c, 1, region, pinfo)) {
			return 0;
		}
		pkt.u.seckey.alg = c;
		if (!limread(&c, 1, region, pinfo)) {
			return 0;
		}
		pkt.u.seckey.s2k_specifier = c;
		switch (pkt.u.seckey.s2k_specifier) {
		case OPS_S2KS_SIMPLE:
		case OPS_S2KS_SALTED:
		case OPS_S2KS_ITERATED_AND_SALTED:
			break;
		default:
			(void) fprintf(stderr,
				"parse_seckey: bad seckey\n");
			return 0;
		}
		if (!limread(&c, 1, region, pinfo)) {
			return 0;
		}
		pkt.u.seckey.hash_alg = c;
		if (pkt.u.seckey.s2k_specifier != OPS_S2KS_SIMPLE &&
		    !limread(pkt.u.seckey.salt, 8, region, pinfo)) {
			return 0;
		}
		if (pkt.u.seckey.s2k_specifier ==
					OPS_S2KS_ITERATED_AND_SALTED) {
			if (!limread(&c, 1, region, pinfo)) {
				return 0;
			}
			pkt.u.seckey.octetc =
				(16 + ((unsigned)c & 15)) <<
						(((unsigned)c >> 4) + 6);
		}
	} else if (pkt.u.seckey.s2k_usage != OPS_S2KU_NONE) {
		/* this is V3 style, looks just like a V4 simple hash */
		pkt.u.seckey.alg = c;
		pkt.u.seckey.s2k_usage = OPS_S2KU_ENCRYPTED;
		pkt.u.seckey.s2k_specifier = OPS_S2KS_SIMPLE;
		pkt.u.seckey.hash_alg = OPS_HASH_MD5;
	}
	crypted = pkt.u.seckey.s2k_usage == OPS_S2KU_ENCRYPTED ||
		pkt.u.seckey.s2k_usage == OPS_S2KU_ENCRYPTED_AND_HASHED;

	if (crypted) {
		__ops_packet_t	seckey;
		unsigned char   key[OPS_MAX_KEY_SIZE + OPS_MAX_HASH_SIZE];
		__ops_hash_t	hashes[(OPS_MAX_KEY_SIZE + OPS_MIN_HASH_SIZE - 1) / OPS_MIN_HASH_SIZE];
		size_t          passlen;
		char           *passphrase;
		int             hashsize;
		int             keysize;
		int             n;

		blocksize = __ops_block_size(pkt.u.seckey.alg);
		if (blocksize == 0 || blocksize > OPS_MAX_BLOCK_SIZE) {
			(void) fprintf(stderr,
				"parse_seckey: bad blocksize\n");
			return 0;
		}

		if (!limread(pkt.u.seckey.iv, blocksize, region, pinfo)) {
			return 0;
		}
		(void) memset(&seckey, 0x0, sizeof(seckey));
		passphrase = NULL;
		seckey.u.skey_passphrase.passphrase = &passphrase;
		seckey.u.skey_passphrase.seckey = &pkt.u.seckey;
		CALLBACK(&pinfo->cbinfo, OPS_GET_PASSPHRASE, &seckey);
		if (!passphrase) {
			if (__ops_get_debug_level(__FILE__)) {
				/* \todo make into proper error */
				(void) fprintf(stderr,
				"parse_seckey: can't get passphrase\n");
			}
			if (!consume_packet(region, pinfo, 0)) {
				return 0;
			}

			CALLBACK(&pinfo->cbinfo,
				OPS_PTAG_CT_ENCRYPTED_SECRET_KEY, &pkt);

			return 1;
		}
		keysize = __ops_key_size(pkt.u.seckey.alg);
		if (keysize == 0 || keysize > OPS_MAX_KEY_SIZE) {
			(void) fprintf(stderr,
				"parse_seckey: bad keysize\n");
			return 0;
		}

		hashsize = __ops_hash_size(pkt.u.seckey.hash_alg);
		if (hashsize == 0 || hashsize > OPS_MAX_HASH_SIZE) {
			(void) fprintf(stderr,
				"parse_seckey: bad hashsize\n");
			return 0;
		}

		for (n = 0; n * hashsize < keysize; ++n) {
			int             i;

			__ops_hash_any(&hashes[n],
				pkt.u.seckey.hash_alg);
			hashes[n].init(&hashes[n]);
			/* preload hashes with zeroes... */
			for (i = 0; i < n; ++i) {
				hashes[n].add(&hashes[n],
					(const unsigned char *) "", 1);
			}
		}
		passlen = strlen(passphrase);
		for (n = 0; n * hashsize < keysize; ++n) {
			unsigned        i;

			switch (pkt.u.seckey.s2k_specifier) {
			case OPS_S2KS_SALTED:
				hashes[n].add(&hashes[n],
					pkt.u.seckey.salt,
					OPS_SALT_SIZE);
				/* FALLTHROUGH */
			case OPS_S2KS_SIMPLE:
				hashes[n].add(&hashes[n],
					(unsigned char *) passphrase, passlen);
				break;

			case OPS_S2KS_ITERATED_AND_SALTED:
				for (i = 0; i < pkt.u.seckey.octetc;
						i += passlen + OPS_SALT_SIZE) {
					unsigned	j;

					j = passlen + OPS_SALT_SIZE;
					if (i + j > pkt.u.seckey.octetc && i != 0) {
						j = pkt.u.seckey.octetc - i;
					}
					hashes[n].add(&hashes[n],
						pkt.u.seckey.salt,
						(unsigned)(j > OPS_SALT_SIZE) ?
							OPS_SALT_SIZE : j);
					if (j > OPS_SALT_SIZE) {
						hashes[n].add(&hashes[n],
						(unsigned char *) passphrase,
						j - OPS_SALT_SIZE);
					}
				}

			}
		}

		for (n = 0; n * hashsize < keysize; ++n) {
			int	r;

			r = hashes[n].finish(&hashes[n], key + n * hashsize);
			if (r != hashsize) {
				(void) fprintf(stderr,
					"parse_seckey: bad r\n");
				return 0;
			}
		}

		__ops_forget(passphrase, passlen);
		(void) free(passphrase);

		__ops_crypt_any(&decrypt, pkt.u.seckey.alg);
		if (__ops_get_debug_level(__FILE__)) {
			unsigned int    i = 0;
			fprintf(stderr, "\nREADING:\niv=");
			for (i = 0;
			     i < __ops_block_size(pkt.u.seckey.alg);
			     i++) {
				fprintf(stderr, "%02x ", pkt.u.seckey.iv[i]);
			}
			fprintf(stderr, "\n");
			fprintf(stderr, "key=");
			for (i = 0; i < CAST_KEY_LENGTH; i++) {
				fprintf(stderr, "%02x ", key[i]);
			}
			fprintf(stderr, "\n");
		}
		decrypt.set_iv(&decrypt, pkt.u.seckey.iv);
		decrypt.set_key(&decrypt, key);

		/* now read encrypted data */

		__ops_reader_push_decrypt(pinfo, &decrypt, region);

		/*
		 * Since all known encryption for PGP doesn't compress, we
		 * can limit to the same length as the current region (for
		 * now).
		 */
		__ops_init_subregion(&encregion, NULL);
		encregion.length = region->length - region->readc;
		if (pkt.u.seckey.pubkey.version != OPS_V4) {
			encregion.length -= 2;
		}
		saved_region = region;
		region = &encregion;
	}
	if (pkt.u.seckey.s2k_usage == OPS_S2KU_ENCRYPTED_AND_HASHED) {
		pkt.u.seckey.checkhash = calloc(1, OPS_CHECKHASH_SIZE);
		__ops_hash_sha1(&checkhash);
		__ops_reader_push_hash(pinfo, &checkhash);
	} else {
		__ops_reader_push_sum16(pinfo);
	}

	switch (pkt.u.seckey.pubkey.alg) {
	case OPS_PKA_RSA:
	case OPS_PKA_RSA_ENCRYPT_ONLY:
	case OPS_PKA_RSA_SIGN_ONLY:
		if (!limread_mpi(&pkt.u.seckey.key.rsa.d, region, pinfo) ||
		    !limread_mpi(&pkt.u.seckey.key.rsa.p, region, pinfo) ||
		    !limread_mpi(&pkt.u.seckey.key.rsa.q, region, pinfo) ||
		    !limread_mpi(&pkt.u.seckey.key.rsa.u, region, pinfo)) {
			ret = 0;
		}
		break;

	case OPS_PKA_DSA:
		if (!limread_mpi(&pkt.u.seckey.key.dsa.x, region, pinfo)) {
			ret = 0;
		}
		break;

	default:
		OPS_ERROR_2(&pinfo->errors,
			OPS_E_ALG_UNSUPPORTED_PUBLIC_KEY_ALG,
			"Unsupported Public Key algorithm %d (%s)",
			pkt.u.seckey.pubkey.alg,
			__ops_show_pka(pkt.u.seckey.pubkey.alg));
		ret = 0;
	}

	if (__ops_get_debug_level(__FILE__)) {
		(void) fprintf(stderr, "4 MPIs read\n");
	}
	pinfo->reading_v3_secret = 0;

	if (pkt.u.seckey.s2k_usage == OPS_S2KU_ENCRYPTED_AND_HASHED) {
		unsigned char   hash[OPS_CHECKHASH_SIZE];

		__ops_reader_pop_hash(pinfo);
		checkhash.finish(&checkhash, hash);

		if (crypted &&
		    pkt.u.seckey.pubkey.version != OPS_V4) {
			__ops_reader_pop_decrypt(pinfo);
			region = saved_region;
		}
		if (ret) {
			if (!limread(pkt.u.seckey.checkhash,
				OPS_CHECKHASH_SIZE, region, pinfo)) {
				return 0;
			}

			if (memcmp(hash, pkt.u.seckey.checkhash,
					OPS_CHECKHASH_SIZE) != 0) {
				ERRP(&pinfo->cbinfo, pkt,
					"Hash mismatch in secret key");
			}
		}
	} else {
		unsigned short  sum;

		sum = __ops_reader_pop_sum16(pinfo);
		if (crypted &&
		    pkt.u.seckey.pubkey.version != OPS_V4) {
			__ops_reader_pop_decrypt(pinfo);
			region = saved_region;
		}
		if (ret) {
			if (!limread_scalar(&pkt.u.seckey.checksum, 2,
					region, pinfo))
				return 0;

			if (sum != pkt.u.seckey.checksum) {
				ERRP(&pinfo->cbinfo, pkt,
					"Checksum mismatch in secret key");
			}
		}
	}

	if (crypted && pkt.u.seckey.pubkey.version == OPS_V4) {
		__ops_reader_pop_decrypt(pinfo);
	}
	if (ret && region->readc != region->length) {
		(void) fprintf(stderr, "parse_seckey: bad length\n");
		return 0;
	}
	if (!ret) {
		return 0;
	}
	CALLBACK(&pinfo->cbinfo, OPS_PTAG_CT_SECRET_KEY, &pkt);
	if (__ops_get_debug_level(__FILE__)) {
		(void) fprintf(stderr, "--- end of parse_seckey\n\n");
	}
	return 1;
}

/**
   \ingroup Core_ReadPackets
   \brief Parse a Public Key Session Key packet
*/
static int 
parse_pk_sesskey(__ops_region_t *region,
		     __ops_parseinfo_t *pinfo)
{
	const __ops_seckey_t	*secret;
	__ops_packet_t		 sesskey;
	__ops_packet_t		 pkt;
	unsigned char		*iv;
	unsigned char	   	 c = 0x0;
	unsigned char		 cs[2];
	unsigned		 k;
	BIGNUM			*enc_m;
	int			 n;

	/* Can't rely on it being CAST5 */
	/* \todo FIXME RW */
	/* const size_t sz_unencoded_m_buf=CAST_KEY_LENGTH+1+2; */
	unsigned char		 unencoded_m_buf[1024];

	if (!limread(&c, 1, region, pinfo)) {
		return 0;
	}
	pkt.u.pk_sesskey.version = c;
	if (pkt.u.pk_sesskey.version != OPS_PKSK_V3) {
		OPS_ERROR_1(&pinfo->errors, OPS_E_PROTO_BAD_PKSK_VRSN,
			"Bad public-key encrypted session key version (%d)",
			    pkt.u.pk_sesskey.version);
		return 0;
	}
	if (!limread(pkt.u.pk_sesskey.key_id,
			  sizeof(pkt.u.pk_sesskey.key_id), region, pinfo)) {
		return 0;
	}
	if (__ops_get_debug_level(__FILE__)) {
		int             i;
		int             x = sizeof(pkt.u.pk_sesskey.key_id);

		printf("session key: public key id: x=%d\n", x);
		for (i = 0; i < x; i++) {
			printf("%2x ", pkt.u.pk_sesskey.key_id[i]);
		}
		printf("\n");
	}
	if (!limread(&c, 1, region, pinfo)) {
		return 0;
	}
	pkt.u.pk_sesskey.alg = c;
	switch (pkt.u.pk_sesskey.alg) {
	case OPS_PKA_RSA:
		if (!limread_mpi(&pkt.u.pk_sesskey.parameters.rsa.encrypted_m,
				      region, pinfo)) {
			return 0;
		}
		enc_m = pkt.u.pk_sesskey.parameters.rsa.encrypted_m;
		break;

	case OPS_PKA_ELGAMAL:
		if (!limread_mpi(&pkt.u.pk_sesskey.parameters.elgamal.g_to_k,
				      region, pinfo) ||
		    !limread_mpi(
			&pkt.u.pk_sesskey.parameters.elgamal.encrypted_m,
					 region, pinfo)) {
			return 0;
		}
		enc_m = pkt.u.pk_sesskey.parameters.elgamal.encrypted_m;
		break;

	default:
		OPS_ERROR_1(&pinfo->errors,
			OPS_E_ALG_UNSUPPORTED_PUBLIC_KEY_ALG,
			"Unknown public key algorithm in session key (%s)",
			__ops_show_pka(pkt.u.pk_sesskey.alg));
		return 0;
	}

	(void) memset(&sesskey, 0x0, sizeof(sesskey));
	secret = NULL;
	sesskey.u.get_seckey.seckey = &secret;
	sesskey.u.get_seckey.pk_sesskey = &pkt.u.pk_sesskey;

	CALLBACK(&pinfo->cbinfo, OPS_GET_SECKEY, &sesskey);

	if (!secret) {
		CALLBACK(&pinfo->cbinfo, OPS_PTAG_CT_ENCRYPTED_PK_SESSION_KEY,
			&pkt);
		return 1;
	}
	n = __ops_decrypt_decode_mpi(unencoded_m_buf, sizeof(unencoded_m_buf),
			enc_m, secret);
	if (n < 1) {
		ERRP(&pinfo->cbinfo, pkt, "decrypted message too short");
		return 0;
	}

	/* PKA */
	pkt.u.pk_sesskey.symm_alg = unencoded_m_buf[0];

	if (!__ops_is_sa_supported(pkt.u.pk_sesskey.symm_alg)) {
		/* ERR1P */
		OPS_ERROR_1(&pinfo->errors, OPS_E_ALG_UNSUPPORTED_SYMMETRIC_ALG,
			    "Symmetric algorithm %s not supported",
			    __ops_show_symm_alg(
				pkt.u.pk_sesskey.symm_alg));
		return 0;
	}
	k = __ops_key_size(pkt.u.pk_sesskey.symm_alg);

	if ((unsigned) n != k + 3) {
		OPS_ERROR_2(&pinfo->errors, OPS_E_PROTO_DECRYPTED_MSG_WRONG_LEN,
		      "decrypted message wrong length (got %d expected %d)",
			    n, k + 3);
		return 0;
	}
	if (k > sizeof(pkt.u.pk_sesskey.key)) {
		(void) fprintf(stderr, "parse_pk_sesskey: bad keylength\n");
		return 0;
	}

	(void) memcpy(pkt.u.pk_sesskey.key, unencoded_m_buf + 1, k);

	if (__ops_get_debug_level(__FILE__)) {
		unsigned int    j;
		printf("session key recovered (len=%d):\n", k);
		for (j = 0; j < k; j++)
			printf("%2x ", pkt.u.pk_sesskey.key[j]);
		printf("\n");
	}
	pkt.u.pk_sesskey.checksum = unencoded_m_buf[k + 1] +
			(unencoded_m_buf[k + 2] << 8);
	if (__ops_get_debug_level(__FILE__)) {
		printf("session key checksum: %2x %2x\n",
			unencoded_m_buf[k + 1], unencoded_m_buf[k + 2]);
	}

	/* Check checksum */
	__ops_calc_sesskey_checksum(&pkt.u.pk_sesskey, &cs[0]);
	if (unencoded_m_buf[k + 1] != cs[0] ||
	    unencoded_m_buf[k + 2] != cs[1]) {
		OPS_ERROR_4(&pinfo->errors, OPS_E_PROTO_BAD_SK_CHECKSUM,
		"Session key checksum wrong: expected %2x %2x, got %2x %2x",
		cs[0], cs[1], unencoded_m_buf[k + 1],
		unencoded_m_buf[k + 2]);
		return 0;
	}
	/* all is well */
	CALLBACK(&pinfo->cbinfo, OPS_PTAG_CT_PK_SESSION_KEY, &pkt);

	__ops_crypt_any(&pinfo->decrypt, pkt.u.pk_sesskey.symm_alg);
	iv = calloc(1, pinfo->decrypt.blocksize);
	pinfo->decrypt.set_iv(&pinfo->decrypt, iv);
	pinfo->decrypt.set_key(&pinfo->decrypt, pkt.u.pk_sesskey.key);
	__ops_encrypt_init(&pinfo->decrypt);
	(void) free(iv);
	return 1;
}

static int 
__ops_decrypt_se_data(__ops_content_tag_t tag, __ops_region_t *region,
		    __ops_parseinfo_t *pinfo)
{
	__ops_crypt_t	*decrypt;
	const int	 printerrors = 1;
	int		 r = 1;

	decrypt = __ops_get_decrypt(pinfo);
	if (decrypt) {
		unsigned char   buf[OPS_MAX_BLOCK_SIZE + 2] = "";
		size_t          b = decrypt->blocksize;
		/* __ops_packet_t pkt; */
		__ops_region_t    encregion;


		__ops_reader_push_decrypt(pinfo, decrypt, region);

		__ops_init_subregion(&encregion, NULL);
		encregion.length = b + 2;

		if (!exact_limread(buf, b + 2, &encregion, pinfo)) {
			return 0;
		}
		if (buf[b - 2] != buf[b] || buf[b - 1] != buf[b + 1]) {
			__ops_reader_pop_decrypt(pinfo);
			OPS_ERROR_4(&pinfo->errors,
				OPS_E_PROTO_BAD_SYMMETRIC_DECRYPT,
				"Bad symmetric decrypt (%02x%02x vs %02x%02x)",
				buf[b - 2], buf[b - 1], buf[b], buf[b + 1]);
			return 0;
		}
		if (tag == OPS_PTAG_CT_SE_DATA_BODY) {
			decrypt->decrypt_resync(decrypt);
			decrypt->block_encrypt(decrypt, decrypt->civ,
					decrypt->civ);
		}
		r = __ops_parse(pinfo, !printerrors);

		__ops_reader_pop_decrypt(pinfo);
	} else {
		__ops_packet_t pkt;

		while (region->readc < region->length) {
			unsigned        len;

			len = region->length - region->readc;
			if (len > sizeof(pkt.u.se_data_body.data))
				len = sizeof(pkt.u.se_data_body.data);

			if (!limread(pkt.u.se_data_body.data, len,
					region, pinfo)) {
				return 0;
			}
			pkt.u.se_data_body.length = len;
			CALLBACK(&pinfo->cbinfo, tag, &pkt);
		}
	}

	return r;
}

static int 
__ops_decrypt_se_ip_data(__ops_content_tag_t tag, __ops_region_t *region,
		       __ops_parseinfo_t *pinfo)
{
	__ops_crypt_t	*decrypt;
	const int	 printerrors = 1;
	int		 r = 1;

	decrypt = __ops_get_decrypt(pinfo);
	if (decrypt) {
		__ops_reader_push_decrypt(pinfo, decrypt, region);
		__ops_reader_push_se_ip_data(pinfo, decrypt, region);

		r = __ops_parse(pinfo, !printerrors);

		__ops_reader_pop_se_ip_data(pinfo);
		__ops_reader_pop_decrypt(pinfo);
	} else {
		__ops_packet_t pkt;

		while (region->readc < region->length) {
			unsigned        len;

			len = region->length - region->readc;
			if (len > sizeof(pkt.u.se_data_body.data)) {
				len = sizeof(pkt.u.se_data_body.data);
			}

			if (!limread(pkt.u.se_data_body.data,
					len, region, pinfo)) {
				return 0;
			}

			pkt.u.se_data_body.length = len;

			CALLBACK(&pinfo->cbinfo, tag, &pkt);
		}
	}

	return r;
}

/**
   \ingroup Core_ReadPackets
   \brief Read a Symmetrically Encrypted packet
*/
static int 
parse_se_data(__ops_region_t *region, __ops_parseinfo_t *pinfo)
{
	__ops_packet_t pkt;

	/* there's no info to go with this, so just announce it */
	CALLBACK(&pinfo->cbinfo, OPS_PTAG_CT_SE_DATA_HEADER, &pkt);

	/*
	 * The content of an encrypted data packet is more OpenPGP packets
	 * once decrypted, so recursively handle them
	 */
	return __ops_decrypt_se_data(OPS_PTAG_CT_SE_DATA_BODY, region, pinfo);
}

/**
   \ingroup Core_ReadPackets
   \brief Read a Symmetrically Encrypted Integrity Protected packet
*/
static int 
parse_se_ip_data(__ops_region_t *region, __ops_parseinfo_t *pinfo)
{
	__ops_packet_t	pkt;
	unsigned char   c = 0x0;

	if (!limread(&c, 1, region, pinfo)) {
		return 0;
	}
	pkt.u.se_ip_data_header.version = c;

	if (pkt.u.se_ip_data_header.version != OPS_SE_IP_V1) {
		(void) fprintf(stderr, "parse_se_ip_data: bad version\n");
		return 0;
	}

	/*
	 * The content of an encrypted data packet is more OpenPGP packets
	 * once decrypted, so recursively handle them
	 */
	return __ops_decrypt_se_ip_data(OPS_PTAG_CT_SE_IP_DATA_BODY, region,
			pinfo);
}

/**
   \ingroup Core_ReadPackets
   \brief Read a MDC packet
*/
static int 
parse_mdc(__ops_region_t *region, __ops_parseinfo_t *pinfo)
{
	__ops_packet_t pkt;

	pkt.u.mdc.length = OPS_SHA1_HASH_SIZE;
	pkt.u.mdc.data = calloc(1, OPS_SHA1_HASH_SIZE);
	if (!limread(pkt.u.mdc.data, OPS_SHA1_HASH_SIZE, region, pinfo)) {
		return 0;
	}
	CALLBACK(&pinfo->cbinfo, OPS_PTAG_CT_MDC, &pkt);
	(void) free(pkt.u.mdc.data);
	return 1;
}

/**
 * \ingroup Core_ReadPackets
 * \brief Parse one packet.
 *
 * This function parses the packet tag.  It computes the value of the
 * content tag and then calls the appropriate function to handle the
 * content.
 *
 * \param *pinfo	How to parse
 * \param *pktlen	On return, will contain number of bytes in packet
 * \return 1 on success, 0 on error, -1 on EOF */
static int 
__ops_parse_packet(__ops_parseinfo_t *pinfo, unsigned long *pktlen)
{
	__ops_packet_t	pkt;
	__ops_region_t	region;
	unsigned char	ptag;
	unsigned	indeterminate = 0;
	int		ret;

	pkt.u.ptag.position = pinfo->readinfo.position;

	ret = base_read(&ptag, 1, pinfo);

	if (__ops_get_debug_level(__FILE__)) {
		(void) fprintf(stderr,
			"__ops_parse_packet: base_read returned %d\n",
			ret);
	}

	/* errors in the base read are effectively EOF. */
	if (ret <= 0) {
		return -1;
	}

	*pktlen = 0;

	if (!(ptag & OPS_PTAG_ALWAYS_SET)) {
		pkt.u.error.error = "Format error (ptag bit not set)";
		CALLBACK(&pinfo->cbinfo, OPS_PARSER_ERROR, &pkt);
		return 0;
	}
	pkt.u.ptag.new_format = !!(ptag & OPS_PTAG_NEW_FORMAT);
	if (pkt.u.ptag.new_format) {
		pkt.u.ptag.type = ptag & OPS_PTAG_NF_CONTENT_TAG_MASK;
		pkt.u.ptag.length_type = 0;
		if (!read_new_length(&pkt.u.ptag.length, pinfo)) {
			return 0;
		}
	} else {
		unsigned   rb;

		rb = 0;
		pkt.u.ptag.type = ((unsigned)ptag &
				OPS_PTAG_OF_CONTENT_TAG_MASK)
			>> OPS_PTAG_OF_CONTENT_TAG_SHIFT;
		pkt.u.ptag.length_type = ptag & OPS_PTAG_OF_LENGTH_TYPE_MASK;
		switch (pkt.u.ptag.length_type) {
		case OPS_PTAG_OLD_LEN_1:
			rb = _read_scalar(&pkt.u.ptag.length, 1, pinfo);
			break;

		case OPS_PTAG_OLD_LEN_2:
			rb = _read_scalar(&pkt.u.ptag.length, 2, pinfo);
			break;

		case OPS_PTAG_OLD_LEN_4:
			rb = _read_scalar(&pkt.u.ptag.length, 4, pinfo);
			break;

		case OPS_PTAG_OLD_LEN_INDETERMINATE:
			pkt.u.ptag.length = 0;
			indeterminate = 1;
			rb = 1;
			break;
		}
		if (!rb) {
			return 0;
		}
	}

	CALLBACK(&pinfo->cbinfo, OPS_PARSER_PTAG, &pkt);

	__ops_init_subregion(&region, NULL);
	region.length = pkt.u.ptag.length;
	region.indeterminate = indeterminate;
	if (__ops_get_debug_level(__FILE__)) {
		(void) fprintf(stderr, "__ops_parse_packet: type %d\n",
			       pkt.u.ptag.type);
	}
	switch (pkt.u.ptag.type) {
	case OPS_PTAG_CT_SIGNATURE:
		ret = parse_sig(&region, pinfo);
		break;

	case OPS_PTAG_CT_PUBLIC_KEY:
	case OPS_PTAG_CT_PUBLIC_SUBKEY:
		ret = parse_pubkey(pkt.u.ptag.type, &region, pinfo);
		break;

	case OPS_PTAG_CT_TRUST:
		ret = parse_trust(&region, pinfo);
		break;

	case OPS_PTAG_CT_USER_ID:
		ret = parse_userid(&region, pinfo);
		break;

	case OPS_PTAG_CT_COMPRESSED:
		ret = parse_compressed(&region, pinfo);
		break;

	case OPS_PTAG_CT_1_PASS_SIG:
		ret = parse_one_pass(&region, pinfo);
		break;

	case OPS_PTAG_CT_LITERAL_DATA:
		ret = parse_litdata(&region, pinfo);
		break;

	case OPS_PTAG_CT_USER_ATTR:
		ret = parse_userattr(&region, pinfo);
		break;

	case OPS_PTAG_CT_SECRET_KEY:
		ret = parse_seckey(&region, pinfo);
		break;

	case OPS_PTAG_CT_SECRET_SUBKEY:
		ret = parse_seckey(&region, pinfo);
		break;

	case OPS_PTAG_CT_PK_SESSION_KEY:
		ret = parse_pk_sesskey(&region, pinfo);
		break;

	case OPS_PTAG_CT_SE_DATA:
		ret = parse_se_data(&region, pinfo);
		break;

	case OPS_PTAG_CT_SE_IP_DATA:
		ret = parse_se_ip_data(&region, pinfo);
		break;

	case OPS_PTAG_CT_MDC:
		ret = parse_mdc(&region, pinfo);
		break;

	default:
		OPS_ERROR_1(&pinfo->errors, OPS_E_P_UNKNOWN_TAG,
			    "Unknown content tag 0x%x",
			    pkt.u.ptag.type);
		ret = 0;
	}

	/* Ensure that the entire packet has been consumed */

	if (region.length != region.readc && !region.indeterminate) {
		if (!consume_packet(&region, pinfo, 0)) {
			ret = -1;
		}
	}

	/* also consume it if there's been an error? */
	/* \todo decide what to do about an error on an */
	/* indeterminate packet */
	if (ret == 0) {
		if (!consume_packet(&region, pinfo, 0)) {
			ret = -1;
		}
	}
	/* set pktlen */

	*pktlen = pinfo->readinfo.alength;

	/* do callback on entire packet, if desired and there was no error */

	if (ret > 0 && pinfo->readinfo.accumulate) {
		pkt.u.packet.length = pinfo->readinfo.alength;
		pkt.u.packet.raw = pinfo->readinfo.accumulated;
		pinfo->readinfo.accumulated = NULL;
		pinfo->readinfo.asize = 0;
		CALLBACK(&pinfo->cbinfo, OPS_PARSER_PACKET_END, &pkt);
	}
	pinfo->readinfo.alength = 0;

	return (ret < 0) ? -1 : (ret) ? 1 : 0;
}

/**
 * \ingroup Core_ReadPackets
 *
 * \brief Parse packets from an input stream until EOF or error.
 *
 * \details Setup the necessary parsing configuration in "pinfo"
 * before calling __ops_parse().
 *
 * That information includes :
 *
 * - a "reader" function to be used to get the data to be parsed
 *
 * - a "callback" function to be called when this library has identified
 * a parseable object within the data
 *
 * - whether the calling function wants the signature subpackets
 * returned raw, parsed or not at all.
 *
 * After returning, pinfo->errors holds any errors encountered while parsing.
 *
 * \param pinfo	Parsing configuration
 * \return		1 on success in all packets, 0 on error in any packet
 *
 * \sa CoreAPI Overview
 *
 * \sa __ops_print_errors()
 *
 */

int 
__ops_parse(__ops_parseinfo_t *pinfo, int perrors)
{
	unsigned long   pktlen;
	int             r;

	do {
		r = __ops_parse_packet(pinfo, &pktlen);
	} while (r != -1);
	if (perrors) {
		__ops_print_errors(pinfo->errors);
	}
	return (pinfo->errors == NULL);
}

/**
 * \ingroup Core_ReadPackets
 *
 * \brief Specifies whether one or more signature
 * subpacket types should be returned parsed; or raw; or ignored.
 *
 * \param	pinfo	Pointer to previously allocated structure
 * \param	tag	Packet tag. OPS_PTAG_SS_ALL for all SS tags; or one individual signature subpacket tag
 * \param	type	Parse type
 * \todo Make all packet types optional, not just subpackets */
void 
__ops_parse_options(__ops_parseinfo_t *pinfo,
		  __ops_content_tag_t tag,
		  __ops_parse_type_t type)
{
	int             t8, t7;

	if (tag == OPS_PTAG_SS_ALL) {
		int             n;

		for (n = 0; n < 256; ++n) {
			__ops_parse_options(pinfo,
				OPS_PTAG_SIG_SUBPKT_BASE + n,
				type);
		}
		return;
	}
	if (tag < OPS_PTAG_SIG_SUBPKT_BASE ||
	    tag > OPS_PTAG_SIG_SUBPKT_BASE + NTAGS - 1) {
		(void) fprintf(stderr, "__ops_parse_options: bad tag\n");
		return;
	}
	t8 = (tag - OPS_PTAG_SIG_SUBPKT_BASE) / 8;
	t7 = 1 << ((tag - OPS_PTAG_SIG_SUBPKT_BASE) & 7);
	switch (type) {
	case OPS_PARSE_RAW:
		pinfo->ss_raw[t8] |= t7;
		pinfo->ss_parsed[t8] &= ~t7;
		break;

	case OPS_PARSE_PARSED:
		pinfo->ss_raw[t8] &= ~t7;
		pinfo->ss_parsed[t8] |= t7;
		break;

	case OPS_PARSE_IGNORE:
		pinfo->ss_raw[t8] &= ~t7;
		pinfo->ss_parsed[t8] &= ~t7;
		break;
	}
}

/**
\ingroup Core_ReadPackets
\brief Creates a new zero-ed __ops_parseinfo_t struct
\sa __ops_parseinfo_delete()
*/
__ops_parseinfo_t *
__ops_parseinfo_new(void)
{
	return calloc(1, sizeof(__ops_parseinfo_t));
}

/**
\ingroup Core_ReadPackets
\brief Free __ops_parseinfo_t struct and its contents
\sa __ops_parseinfo_new()
*/
void 
__ops_parseinfo_delete(__ops_parseinfo_t *pinfo)
{
	__ops_cbdata_t	*cbinfo;
	__ops_cbdata_t	*next;

	for (cbinfo = pinfo->cbinfo.next; cbinfo; cbinfo = next) {
		next = cbinfo->next;
		(void) free(cbinfo);
	}
	if (pinfo->readinfo.destroyer) {
		pinfo->readinfo.destroyer(&pinfo->readinfo);
	}
	__ops_free_errors(pinfo->errors);
	if (pinfo->readinfo.accumulated) {
		(void) free(pinfo->readinfo.accumulated);
	}
	(void) free(pinfo);
}

/**
\ingroup Core_ReadPackets
\brief Returns the parse_info's reader_info
\return Pointer to the reader_info inside the parse_info
*/
__ops_reader_t *
__ops_readinfo(__ops_parseinfo_t *pinfo)
{
	return &pinfo->readinfo;
}

/**
\ingroup Core_ReadPackets
\brief Sets the parse_info's callback
This is used when adding the first callback in a stack of callbacks.
\sa __ops_callback_push()
*/

void 
__ops_set_callback(__ops_parseinfo_t *pinfo, __ops_cbfunc_t *cb, void *arg)
{
	pinfo->cbinfo.cbfunc = cb;
	pinfo->cbinfo.arg = arg;
	pinfo->cbinfo.errors = &pinfo->errors;
}

/**
\ingroup Core_ReadPackets
\brief Adds a further callback to a stack of callbacks
\sa __ops_set_callback()
*/
void 
__ops_callback_push(__ops_parseinfo_t *pinfo, __ops_cbfunc_t *cb, void *arg)
{
	__ops_cbdata_t *cbinfo = calloc(1, sizeof(*cbinfo));

	(void) memcpy(cbinfo, &pinfo->cbinfo, sizeof(*cbinfo));
	cbinfo->io = pinfo->io;
	pinfo->cbinfo.next = cbinfo;
	__ops_set_callback(pinfo, cb, arg);
}

/**
\ingroup Core_ReadPackets
\brief Returns callback's arg
*/
void *
__ops_callback_arg(__ops_cbdata_t *cbinfo)
{
	return cbinfo->arg;
}

/**
\ingroup Core_ReadPackets
\brief Returns callback's errors
*/
void *
__ops_callback_errors(__ops_cbdata_t *cbinfo)
{
	return cbinfo->errors;
}

/**
\ingroup Core_ReadPackets
\brief Calls the parse_cb_info's callback if present
\return Return value from callback, if present; else OPS_FINISHED
*/
__ops_cb_ret_t 
__ops_callback(const __ops_packet_t *pkt, __ops_cbdata_t *cbinfo)
{
	return (cbinfo->cbfunc) ? cbinfo->cbfunc(pkt, cbinfo) : OPS_FINISHED;
}

/**
\ingroup Core_ReadPackets
\brief Calls the next callback  in the stack
\return Return value from callback
*/
__ops_cb_ret_t 
__ops_stacked_callback(const __ops_packet_t *pkt, __ops_cbdata_t *cbinfo)
{
	return __ops_callback(pkt, cbinfo->next);
}

/**
\ingroup Core_ReadPackets
\brief Returns the parse_info's errors
\return parse_info's errors
*/
__ops_error_t    *
__ops_parseinfo_get_errors(__ops_parseinfo_t *pinfo)
{
	return pinfo->errors;
}

__ops_crypt_t    *
__ops_get_decrypt(__ops_parseinfo_t *pinfo)
{
	return (pinfo->decrypt.alg) ? &pinfo->decrypt : NULL;
}
