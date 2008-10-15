/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef	_SYS_ZAP_H
#define	_SYS_ZAP_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

/*
 * ZAP - ZFS Attribute Processor
 *
 * The ZAP is a module which sits on top of the DMU (Data Managemnt
 * Unit) and implements a higher-level storage primitive using DMU
 * objects.  Its primary consumer is the ZPL (ZFS Posix Layer).
 *
 * A "zapobj" is a DMU object which the ZAP uses to stores attributes.
 * Users should use only zap routines to access a zapobj - they should
 * not access the DMU object directly using DMU routines.
 *
 * The attributes stored in a zapobj are name-value pairs.  The name is
 * a zero-terminated string of up to ZAP_MAXNAMELEN bytes (including
 * terminating NULL).  The value is an array of integers, which may be
 * 1, 2, 4, or 8 bytes long.  The total space used by the array (number
 * of integers * integer length) can be up to ZAP_MAXVALUELEN bytes.
 * Note that an 8-byte integer value can be used to store the location
 * (object number) of another dmu object (which may be itself a zapobj).
 * Note that you can use a zero-length attribute to store a single bit
 * of information - the attribute is present or not.
 *
 * The ZAP routines are thread-safe.  However, you must observe the
 * DMU's restriction that a transaction may not be operated on
 * concurrently.
 *
 * Any of the routines that return an int may return an I/O error (EIO
 * or ECHECKSUM).
 *
 *
 * Implementation / Performance Notes:
 *
 * The ZAP is intended to operate most efficiently on attributes with
 * short (49 bytes or less) names and single 8-byte values, for which
 * the microzap will be used.  The ZAP should be efficient enough so
 * that the user does not need to cache these attributes.
 *
 * The ZAP's locking scheme makes its routines thread-safe.  Operations
 * on different zapobjs will be processed concurrently.  Operations on
 * the same zapobj which only read data will be processed concurrently.
 * Operations on the same zapobj which modify data will be processed
 * concurrently when there are many attributes in the zapobj (because
 * the ZAP uses per-block locking - more than 128 * (number of cpus)
 * small attributes will suffice).
 */

/*
 * We're using zero-terminated byte strings (ie. ASCII or UTF-8 C
 * strings) for the names of attributes, rather than a byte string
 * bounded by an explicit length.  If some day we want to support names
 * in character sets which have embedded zeros (eg. UTF-16, UTF-32),
 * we'll have to add routines for using length-bounded strings.
 */

#include <sys/dmu.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	ZAP_MAXNAMELEN 256
#define	ZAP_MAXVALUELEN 1024

/*
 * Create a new zapobj with no attributes and return its object number.
 */
uint64_t zap_create(objset_t *ds, dmu_object_type_t ot,
    dmu_object_type_t bonustype, int bonuslen, dmu_tx_t *tx);

/*
 * Create a new zapobj with no attributes from the given (unallocated)
 * object number.
 */
int zap_create_claim(objset_t *ds, uint64_t obj, dmu_object_type_t ot,
    dmu_object_type_t bonustype, int bonuslen, dmu_tx_t *tx);

/*
 * The zapobj passed in must be a valid ZAP object for all of the
 * following routines.
 */

/*
 * Destroy this zapobj and all its attributes.
 *
 * Frees the object number using dmu_object_free.
 */
int zap_destroy(objset_t *ds, uint64_t zapobj, dmu_tx_t *tx);

/*
 * Manipulate attributes.
 *
 * 'integer_size' is in bytes, and must be 1, 2, 4, or 8.
 */

/*
 * Retrieve the contents of the attribute with the given name.
 *
 * If the requested attribute does not exist, the call will fail and
 * return ENOENT.
 *
 * If 'integer_size' is smaller than the attribute's integer size, the
 * call will fail and return EINVAL.
 *
 * If 'integer_size' is equal to or larger than the attribute's integer
 * size, the call will succeed and return 0.  * When converting to a
 * larger integer size, the integers will be treated as unsigned (ie. no
 * sign-extension will be performed).
 *
 * 'num_integers' is the length (in integers) of 'buf'.
 *
 * If the attribute is longer than the buffer, as many integers as will
 * fit will be transferred to 'buf'.  If the entire attribute was not
 * transferred, the call will return EOVERFLOW.
 */
int zap_lookup(objset_t *ds, uint64_t zapobj, const char *name,
    uint64_t integer_size, uint64_t num_integers, void *buf);

/*
 * Create an attribute with the given name and value.
 *
 * If an attribute with the given name already exists, the call will
 * fail and return EEXIST.
 */
int zap_add(objset_t *ds, uint64_t zapobj, const char *name,
    int integer_size, uint64_t num_integers,
    const void *val, dmu_tx_t *tx);

/*
 * Set the attribute with the given name to the given value.  If an
 * attribute with the given name does not exist, it will be created.  If
 * an attribute with the given name already exists, the previous value
 * will be overwritten.  The integer_size may be different from the
 * existing attribute's integer size, in which case the attribute's
 * integer size will be updated to the new value.
 */
int zap_update(objset_t *ds, uint64_t zapobj, const char *name,
    int integer_size, uint64_t num_integers, const void *val, dmu_tx_t *tx);

/*
 * Get the length (in integers) and the integer size of the specified
 * attribute.
 *
 * If the requested attribute does not exist, the call will fail and
 * return ENOENT.
 */
int zap_length(objset_t *ds, uint64_t zapobj, const char *name,
    uint64_t *integer_size, uint64_t *num_integers);

/*
 * Remove the specified attribute.
 *
 * If the specified attribute does not exist, the call will fail and
 * return ENOENT.
 */
int zap_remove(objset_t *ds, uint64_t zapobj, const char *name, dmu_tx_t *tx);

/*
 * Returns (in *count) the number of attributes in the specified zap
 * object.
 */
int zap_count(objset_t *ds, uint64_t zapobj, uint64_t *count);


/*
 * Returns (in name) the name of the entry whose value
 * (za_first_integer) is value, or ENOENT if not found.  The string
 * pointed to by name must be at least 256 bytes long.
 */
int zap_value_search(objset_t *os, uint64_t zapobj, uint64_t value, char *name);

struct zap;
struct zap_leaf;
typedef struct zap_cursor {
	/* This structure is opaque! */
	objset_t *zc_objset;
	struct zap *zc_zap;
	struct zap_leaf *zc_leaf;
	uint64_t zc_zapobj;
	uint64_t zc_hash;
	uint32_t zc_cd;
} zap_cursor_t;

typedef struct {
	int za_integer_length;
	uint64_t za_num_integers;
	uint64_t za_first_integer;	/* no sign extension for <8byte ints */
	char za_name[MAXNAMELEN];
} zap_attribute_t;

/*
 * The interface for listing all the attributes of a zapobj can be
 * thought of as cursor moving down a list of the attributes one by
 * one.  The cookie returned by the zap_cursor_serialize routine is
 * persistent across system calls (and across reboot, even).
 */

/*
 * Initialize a zap cursor, pointing to the "first" attribute of the
 * zapobj.  You must _fini the cursor when you are done with it.
 */
void zap_cursor_init(zap_cursor_t *zc, objset_t *ds, uint64_t zapobj);
void zap_cursor_fini(zap_cursor_t *zc);

/*
 * Get the attribute currently pointed to by the cursor.  Returns
 * ENOENT if at the end of the attributes.
 */
int zap_cursor_retrieve(zap_cursor_t *zc, zap_attribute_t *za);

/*
 * Advance the cursor to the next attribute.
 */
void zap_cursor_advance(zap_cursor_t *zc);

/*
 * Get a persistent cookie pointing to the current position of the zap
 * cursor.  The low 4 bits in the cookie are always zero, and thus can
 * be used as to differentiate a serialized cookie from a different type
 * of value.  The cookie will be less than 2^32 as long as there are
 * fewer than 2^22 (4.2 million) entries in the zap object.
 */
uint64_t zap_cursor_serialize(zap_cursor_t *zc);

/*
 * Initialize a zap cursor pointing to the position recorded by
 * zap_cursor_serialize (in the "serialized" argument).  You can also
 * use a "serialized" argument of 0 to start at the beginning of the
 * zapobj (ie.  zap_cursor_init_serialized(..., 0) is equivalent to
 * zap_cursor_init(...).)
 */
void zap_cursor_init_serialized(zap_cursor_t *zc, objset_t *ds,
    uint64_t zapobj, uint64_t serialized);


#define	ZAP_HISTOGRAM_SIZE 10

typedef struct zap_stats {
	/*
	 * Size of the pointer table (in number of entries).
	 * This is always a power of 2, or zero if it's a microzap.
	 * In general, it should be considerably greater than zs_num_leafs.
	 */
	uint64_t zs_ptrtbl_len;

	uint64_t zs_blocksize;		/* size of zap blocks */

	/*
	 * The number of blocks used.  Note that some blocks may be
	 * wasted because old ptrtbl's and large name/value blocks are
	 * not reused.  (Although their space is reclaimed, we don't
	 * reuse those offsets in the object.)
	 */
	uint64_t zs_num_blocks;

	/*
	 * Pointer table values from zap_ptrtbl in the zap_phys_t
	 */
	uint64_t zs_ptrtbl_nextblk;	  /* next (larger) copy start block */
	uint64_t zs_ptrtbl_blks_copied;   /* number source blocks copied */
	uint64_t zs_ptrtbl_zt_blk;	  /* starting block number */
	uint64_t zs_ptrtbl_zt_numblks;    /* number of blocks */
	uint64_t zs_ptrtbl_zt_shift;	  /* bits to index it */

	/*
	 * Values of the other members of the zap_phys_t
	 */
	uint64_t zs_block_type;		/* ZBT_HEADER */
	uint64_t zs_magic;		/* ZAP_MAGIC */
	uint64_t zs_num_leafs;		/* The number of leaf blocks */
	uint64_t zs_num_entries;	/* The number of zap entries */
	uint64_t zs_salt;		/* salt to stir into hash function */

	/*
	 * Histograms.  For all histograms, the last index
	 * (ZAP_HISTOGRAM_SIZE-1) includes any values which are greater
	 * than what can be represented.  For example
	 * zs_leafs_with_n5_entries[ZAP_HISTOGRAM_SIZE-1] is the number
	 * of leafs with more than 45 entries.
	 */

	/*
	 * zs_leafs_with_n_pointers[n] is the number of leafs with
	 * 2^n pointers to it.
	 */
	uint64_t zs_leafs_with_2n_pointers[ZAP_HISTOGRAM_SIZE];

	/*
	 * zs_leafs_with_n_entries[n] is the number of leafs with
	 * [n*5, (n+1)*5) entries.  In the current implementation, there
	 * can be at most 55 entries in any block, but there may be
	 * fewer if the name or value is large, or the block is not
	 * completely full.
	 */
	uint64_t zs_blocks_with_n5_entries[ZAP_HISTOGRAM_SIZE];

	/*
	 * zs_leafs_n_tenths_full[n] is the number of leafs whose
	 * fullness is in the range [n/10, (n+1)/10).
	 */
	uint64_t zs_blocks_n_tenths_full[ZAP_HISTOGRAM_SIZE];

	/*
	 * zs_entries_using_n_chunks[n] is the number of entries which
	 * consume n 24-byte chunks.  (Note, large names/values only use
	 * one chunk, but contribute to zs_num_blocks_large.)
	 */
	uint64_t zs_entries_using_n_chunks[ZAP_HISTOGRAM_SIZE];

	/*
	 * zs_buckets_with_n_entries[n] is the number of buckets (each
	 * leaf has 64 buckets) with n entries.
	 * zs_buckets_with_n_entries[1] should be very close to
	 * zs_num_entries.
	 */
	uint64_t zs_buckets_with_n_entries[ZAP_HISTOGRAM_SIZE];
} zap_stats_t;

/*
 * Get statistics about a ZAP object.  Note: you need to be aware of the
 * internal implementation of the ZAP to correctly interpret some of the
 * statistics.  This interface shouldn't be relied on unless you really
 * know what you're doing.
 */
int zap_get_stats(objset_t *ds, uint64_t zapobj, zap_stats_t *zs);

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_ZAP_H */
