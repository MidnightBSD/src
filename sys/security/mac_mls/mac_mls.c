/*-
 * Copyright (c) 1999-2002 Robert N. M. Watson
 * Copyright (c) 2001-2005 McAfee, Inc.
 * All rights reserved.
 *
 * This software was developed by Robert Watson for the TrustedBSD Project.
 *
 * This software was developed for the FreeBSD Project in part by McAfee
 * Research, the Security Research Division of McAfee, Inc. under
 * DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the DARPA
 * CHATS research program.
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
 * $FreeBSD: release/7.0.0/sys/security/mac_mls/mac_mls.c 173388 2007-11-06 14:46:59Z rwatson $
 */

/*
 * Developed by the TrustedBSD Project.
 *
 * MLS fixed label mandatory confidentiality policy.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/acl.h>
#include <sys/conf.h>
#include <sys/extattr.h>
#include <sys/kernel.h>
#include <sys/ksem.h>
#include <sys/mman.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/sbuf.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/sysent.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/pipe.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>

#include <fs/devfs/devfs.h>

#include <net/bpfdesc.h>
#include <net/if.h>
#include <net/if_types.h>
#include <net/if_var.h>

#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>

#include <vm/uma.h>
#include <vm/vm.h>

#include <security/mac/mac_policy.h>
#include <security/mac_mls/mac_mls.h>

SYSCTL_DECL(_security_mac);

SYSCTL_NODE(_security_mac, OID_AUTO, mls, CTLFLAG_RW, 0,
    "TrustedBSD mac_mls policy controls");

static int	mac_mls_label_size = sizeof(struct mac_mls);
SYSCTL_INT(_security_mac_mls, OID_AUTO, label_size, CTLFLAG_RD,
    &mac_mls_label_size, 0, "Size of struct mac_mls");

static int	mac_mls_enabled = 1;
SYSCTL_INT(_security_mac_mls, OID_AUTO, enabled, CTLFLAG_RW,
    &mac_mls_enabled, 0, "Enforce MAC/MLS policy");
TUNABLE_INT("security.mac.mls.enabled", &mac_mls_enabled);

static int	destroyed_not_inited;
SYSCTL_INT(_security_mac_mls, OID_AUTO, destroyed_not_inited, CTLFLAG_RD,
    &destroyed_not_inited, 0, "Count of labels destroyed but not inited");

static int	ptys_equal = 0;
SYSCTL_INT(_security_mac_mls, OID_AUTO, ptys_equal, CTLFLAG_RW,
    &ptys_equal, 0, "Label pty devices as mls/equal on create");
TUNABLE_INT("security.mac.mls.ptys_equal", &ptys_equal);

static int	revocation_enabled = 0;
SYSCTL_INT(_security_mac_mls, OID_AUTO, revocation_enabled, CTLFLAG_RW,
    &revocation_enabled, 0, "Revoke access to objects on relabel");
TUNABLE_INT("security.mac.mls.revocation_enabled", &revocation_enabled);

static int	max_compartments = MAC_MLS_MAX_COMPARTMENTS;
SYSCTL_INT(_security_mac_mls, OID_AUTO, max_compartments, CTLFLAG_RD,
    &max_compartments, 0, "Maximum compartments the policy supports");

static int	mac_mls_slot;
#define	SLOT(l)	((struct mac_mls *)mac_label_get((l), mac_mls_slot))
#define	SLOT_SET(l, val) mac_label_set((l), mac_mls_slot, (uintptr_t)(val))

static uma_zone_t	zone_mls;

static __inline int
mls_bit_set_empty(u_char *set) {
	int i;

	for (i = 0; i < MAC_MLS_MAX_COMPARTMENTS >> 3; i++)
		if (set[i] != 0)
			return (0);
	return (1);
}

static struct mac_mls *
mls_alloc(int flag)
{

	return (uma_zalloc(zone_mls, flag | M_ZERO));
}

static void
mls_free(struct mac_mls *mac_mls)
{

	if (mac_mls != NULL)
		uma_zfree(zone_mls, mac_mls);
	else
		atomic_add_int(&destroyed_not_inited, 1);
}

static int
mls_atmostflags(struct mac_mls *mac_mls, int flags)
{

	if ((mac_mls->mm_flags & flags) != mac_mls->mm_flags)
		return (EINVAL);
	return (0);
}

static int
mac_mls_dominate_element(struct mac_mls_element *a,
    struct mac_mls_element *b)
{
	int bit;

	switch (a->mme_type) {
	case MAC_MLS_TYPE_EQUAL:
	case MAC_MLS_TYPE_HIGH:
		return (1);

	case MAC_MLS_TYPE_LOW:
		switch (b->mme_type) {
		case MAC_MLS_TYPE_LEVEL:
		case MAC_MLS_TYPE_HIGH:
			return (0);

		case MAC_MLS_TYPE_EQUAL:
		case MAC_MLS_TYPE_LOW:
			return (1);

		default:
			panic("mac_mls_dominate_element: b->mme_type invalid");
		}

	case MAC_MLS_TYPE_LEVEL:
		switch (b->mme_type) {
		case MAC_MLS_TYPE_EQUAL:
		case MAC_MLS_TYPE_LOW:
			return (1);

		case MAC_MLS_TYPE_HIGH:
			return (0);

		case MAC_MLS_TYPE_LEVEL:
			for (bit = 1; bit <= MAC_MLS_MAX_COMPARTMENTS; bit++)
				if (!MAC_MLS_BIT_TEST(bit,
				    a->mme_compartments) &&
				    MAC_MLS_BIT_TEST(bit, b->mme_compartments))
					return (0);
			return (a->mme_level >= b->mme_level);

		default:
			panic("mac_mls_dominate_element: b->mme_type invalid");
		}

	default:
		panic("mac_mls_dominate_element: a->mme_type invalid");
	}

	return (0);
}

static int
mac_mls_range_in_range(struct mac_mls *rangea, struct mac_mls *rangeb)
{

	return (mac_mls_dominate_element(&rangeb->mm_rangehigh,
	    &rangea->mm_rangehigh) &&
	    mac_mls_dominate_element(&rangea->mm_rangelow,
	    &rangeb->mm_rangelow));
}

static int
mac_mls_effective_in_range(struct mac_mls *effective, struct mac_mls *range)
{

	KASSERT((effective->mm_flags & MAC_MLS_FLAG_EFFECTIVE) != 0,
	    ("mac_mls_effective_in_range: a not effective"));
	KASSERT((range->mm_flags & MAC_MLS_FLAG_RANGE) != 0,
	    ("mac_mls_effective_in_range: b not range"));

	return (mac_mls_dominate_element(&range->mm_rangehigh,
	    &effective->mm_effective) &&
	    mac_mls_dominate_element(&effective->mm_effective,
	    &range->mm_rangelow));

	return (1);
}

static int
mac_mls_dominate_effective(struct mac_mls *a, struct mac_mls *b)
{
	KASSERT((a->mm_flags & MAC_MLS_FLAG_EFFECTIVE) != 0,
	    ("mac_mls_dominate_effective: a not effective"));
	KASSERT((b->mm_flags & MAC_MLS_FLAG_EFFECTIVE) != 0,
	    ("mac_mls_dominate_effective: b not effective"));

	return (mac_mls_dominate_element(&a->mm_effective, &b->mm_effective));
}

static int
mac_mls_equal_element(struct mac_mls_element *a, struct mac_mls_element *b)
{

	if (a->mme_type == MAC_MLS_TYPE_EQUAL ||
	    b->mme_type == MAC_MLS_TYPE_EQUAL)
		return (1);

	return (a->mme_type == b->mme_type && a->mme_level == b->mme_level);
}

static int
mac_mls_equal_effective(struct mac_mls *a, struct mac_mls *b)
{

	KASSERT((a->mm_flags & MAC_MLS_FLAG_EFFECTIVE) != 0,
	    ("mac_mls_equal_effective: a not effective"));
	KASSERT((b->mm_flags & MAC_MLS_FLAG_EFFECTIVE) != 0,
	    ("mac_mls_equal_effective: b not effective"));

	return (mac_mls_equal_element(&a->mm_effective, &b->mm_effective));
}

static int
mac_mls_contains_equal(struct mac_mls *mac_mls)
{

	if (mac_mls->mm_flags & MAC_MLS_FLAG_EFFECTIVE)
		if (mac_mls->mm_effective.mme_type == MAC_MLS_TYPE_EQUAL)
			return (1);

	if (mac_mls->mm_flags & MAC_MLS_FLAG_RANGE) {
		if (mac_mls->mm_rangelow.mme_type == MAC_MLS_TYPE_EQUAL)
			return (1);
		if (mac_mls->mm_rangehigh.mme_type == MAC_MLS_TYPE_EQUAL)
			return (1);
	}

	return (0);
}

static int
mac_mls_subject_privileged(struct mac_mls *mac_mls)
{

	KASSERT((mac_mls->mm_flags & MAC_MLS_FLAGS_BOTH) ==
	    MAC_MLS_FLAGS_BOTH,
	    ("mac_mls_subject_privileged: subject doesn't have both labels"));

	/* If the effective is EQUAL, it's ok. */
	if (mac_mls->mm_effective.mme_type == MAC_MLS_TYPE_EQUAL)
		return (0);

	/* If either range endpoint is EQUAL, it's ok. */
	if (mac_mls->mm_rangelow.mme_type == MAC_MLS_TYPE_EQUAL ||
	    mac_mls->mm_rangehigh.mme_type == MAC_MLS_TYPE_EQUAL)
		return (0);

	/* If the range is low-high, it's ok. */
	if (mac_mls->mm_rangelow.mme_type == MAC_MLS_TYPE_LOW &&
	    mac_mls->mm_rangehigh.mme_type == MAC_MLS_TYPE_HIGH)
		return (0);

	/* It's not ok. */
	return (EPERM);
}

static int
mac_mls_valid(struct mac_mls *mac_mls)
{

	if (mac_mls->mm_flags & MAC_MLS_FLAG_EFFECTIVE) {
		switch (mac_mls->mm_effective.mme_type) {
		case MAC_MLS_TYPE_LEVEL:
			break;

		case MAC_MLS_TYPE_EQUAL:
		case MAC_MLS_TYPE_HIGH:
		case MAC_MLS_TYPE_LOW:
			if (mac_mls->mm_effective.mme_level != 0 ||
			    !MAC_MLS_BIT_SET_EMPTY(
			    mac_mls->mm_effective.mme_compartments))
				return (EINVAL);
			break;

		default:
			return (EINVAL);
		}
	} else {
		if (mac_mls->mm_effective.mme_type != MAC_MLS_TYPE_UNDEF)
			return (EINVAL);
	}

	if (mac_mls->mm_flags & MAC_MLS_FLAG_RANGE) {
		switch (mac_mls->mm_rangelow.mme_type) {
		case MAC_MLS_TYPE_LEVEL:
			break;

		case MAC_MLS_TYPE_EQUAL:
		case MAC_MLS_TYPE_HIGH:
		case MAC_MLS_TYPE_LOW:
			if (mac_mls->mm_rangelow.mme_level != 0 ||
			    !MAC_MLS_BIT_SET_EMPTY(
			    mac_mls->mm_rangelow.mme_compartments))
				return (EINVAL);
			break;

		default:
			return (EINVAL);
		}

		switch (mac_mls->mm_rangehigh.mme_type) {
		case MAC_MLS_TYPE_LEVEL:
			break;

		case MAC_MLS_TYPE_EQUAL:
		case MAC_MLS_TYPE_HIGH:
		case MAC_MLS_TYPE_LOW:
			if (mac_mls->mm_rangehigh.mme_level != 0 ||
			    !MAC_MLS_BIT_SET_EMPTY(
			    mac_mls->mm_rangehigh.mme_compartments))
				return (EINVAL);
			break;

		default:
			return (EINVAL);
		}
		if (!mac_mls_dominate_element(&mac_mls->mm_rangehigh,
		    &mac_mls->mm_rangelow))
			return (EINVAL);
	} else {
		if (mac_mls->mm_rangelow.mme_type != MAC_MLS_TYPE_UNDEF ||
		    mac_mls->mm_rangehigh.mme_type != MAC_MLS_TYPE_UNDEF)
			return (EINVAL);
	}

	return (0);
}

static void
mac_mls_set_range(struct mac_mls *mac_mls, u_short typelow,
    u_short levellow, u_char *compartmentslow, u_short typehigh,
    u_short levelhigh, u_char *compartmentshigh)
{

	mac_mls->mm_rangelow.mme_type = typelow;
	mac_mls->mm_rangelow.mme_level = levellow;
	if (compartmentslow != NULL)
		memcpy(mac_mls->mm_rangelow.mme_compartments,
		    compartmentslow,
		    sizeof(mac_mls->mm_rangelow.mme_compartments));
	mac_mls->mm_rangehigh.mme_type = typehigh;
	mac_mls->mm_rangehigh.mme_level = levelhigh;
	if (compartmentshigh != NULL)
		memcpy(mac_mls->mm_rangehigh.mme_compartments,
		    compartmentshigh,
		    sizeof(mac_mls->mm_rangehigh.mme_compartments));
	mac_mls->mm_flags |= MAC_MLS_FLAG_RANGE;
}

static void
mac_mls_set_effective(struct mac_mls *mac_mls, u_short type, u_short level,
    u_char *compartments)
{

	mac_mls->mm_effective.mme_type = type;
	mac_mls->mm_effective.mme_level = level;
	if (compartments != NULL)
		memcpy(mac_mls->mm_effective.mme_compartments, compartments,
		    sizeof(mac_mls->mm_effective.mme_compartments));
	mac_mls->mm_flags |= MAC_MLS_FLAG_EFFECTIVE;
}

static void
mac_mls_copy_range(struct mac_mls *labelfrom, struct mac_mls *labelto)
{

	KASSERT((labelfrom->mm_flags & MAC_MLS_FLAG_RANGE) != 0,
	    ("mac_mls_copy_range: labelfrom not range"));

	labelto->mm_rangelow = labelfrom->mm_rangelow;
	labelto->mm_rangehigh = labelfrom->mm_rangehigh;
	labelto->mm_flags |= MAC_MLS_FLAG_RANGE;
}

static void
mac_mls_copy_effective(struct mac_mls *labelfrom, struct mac_mls *labelto)
{

	KASSERT((labelfrom->mm_flags & MAC_MLS_FLAG_EFFECTIVE) != 0,
	    ("mac_mls_copy_effective: labelfrom not effective"));

	labelto->mm_effective = labelfrom->mm_effective;
	labelto->mm_flags |= MAC_MLS_FLAG_EFFECTIVE;
}

static void
mac_mls_copy(struct mac_mls *source, struct mac_mls *dest)
{

	if (source->mm_flags & MAC_MLS_FLAG_EFFECTIVE)
		mac_mls_copy_effective(source, dest);
	if (source->mm_flags & MAC_MLS_FLAG_RANGE)
		mac_mls_copy_range(source, dest);
}

/*
 * Policy module operations.
 */
static void
mac_mls_init(struct mac_policy_conf *conf)
{

	zone_mls = uma_zcreate("mac_mls", sizeof(struct mac_mls), NULL,
	    NULL, NULL, NULL, UMA_ALIGN_PTR, 0);
}

/*
 * Label operations.
 */
static void
mac_mls_init_label(struct label *label)
{

	SLOT_SET(label, mls_alloc(M_WAITOK));
}

static int
mac_mls_init_label_waitcheck(struct label *label, int flag)
{

	SLOT_SET(label, mls_alloc(flag));
	if (SLOT(label) == NULL)
		return (ENOMEM);

	return (0);
}

static void
mac_mls_destroy_label(struct label *label)
{

	mls_free(SLOT(label));
	SLOT_SET(label, NULL);
}

/*
 * mac_mls_element_to_string() accepts an sbuf and MLS element.  It
 * converts the MLS element to a string and stores the result in the
 * sbuf; if there isn't space in the sbuf, -1 is returned.
 */
static int
mac_mls_element_to_string(struct sbuf *sb, struct mac_mls_element *element)
{
	int i, first;

	switch (element->mme_type) {
	case MAC_MLS_TYPE_HIGH:
		return (sbuf_printf(sb, "high"));

	case MAC_MLS_TYPE_LOW:
		return (sbuf_printf(sb, "low"));

	case MAC_MLS_TYPE_EQUAL:
		return (sbuf_printf(sb, "equal"));

	case MAC_MLS_TYPE_LEVEL:
		if (sbuf_printf(sb, "%d", element->mme_level) == -1)
			return (-1);

		first = 1;
		for (i = 1; i <= MAC_MLS_MAX_COMPARTMENTS; i++) {
			if (MAC_MLS_BIT_TEST(i, element->mme_compartments)) {
				if (first) {
					if (sbuf_putc(sb, ':') == -1)
						return (-1);
					if (sbuf_printf(sb, "%d", i) == -1)
						return (-1);
					first = 0;
				} else {
					if (sbuf_printf(sb, "+%d", i) == -1)
						return (-1);
				}
			}
		}
		return (0);

	default:
		panic("mac_mls_element_to_string: invalid type (%d)",
		    element->mme_type);
	}
}

/*
 * mac_mls_to_string() converts an MLS label to a string, and places
 * the results in the passed sbuf.  It returns 0 on success, or EINVAL
 * if there isn't room in the sbuf.  Note: the sbuf will be modified
 * even in a failure case, so the caller may need to revert the sbuf
 * by restoring the offset if that's undesired.
 */
static int
mac_mls_to_string(struct sbuf *sb, struct mac_mls *mac_mls)
{

	if (mac_mls->mm_flags & MAC_MLS_FLAG_EFFECTIVE) {
		if (mac_mls_element_to_string(sb, &mac_mls->mm_effective)
		    == -1)
			return (EINVAL);
	}

	if (mac_mls->mm_flags & MAC_MLS_FLAG_RANGE) {
		if (sbuf_putc(sb, '(') == -1)
			return (EINVAL);

		if (mac_mls_element_to_string(sb, &mac_mls->mm_rangelow)
		    == -1)
			return (EINVAL);

		if (sbuf_putc(sb, '-') == -1)
			return (EINVAL);

		if (mac_mls_element_to_string(sb, &mac_mls->mm_rangehigh)
		    == -1)
			return (EINVAL);

		if (sbuf_putc(sb, ')') == -1)
			return (EINVAL);
	}

	return (0);
}

static int
mac_mls_externalize_label(struct label *label, char *element_name,
    struct sbuf *sb, int *claimed)
{
	struct mac_mls *mac_mls;

	if (strcmp(MAC_MLS_LABEL_NAME, element_name) != 0)
		return (0);

	(*claimed)++;

	mac_mls = SLOT(label);

	return (mac_mls_to_string(sb, mac_mls));
}

static int
mac_mls_parse_element(struct mac_mls_element *element, char *string)
{
	char *compartment, *end, *level;
	int value;

	if (strcmp(string, "high") == 0 ||
	    strcmp(string, "hi") == 0) {
		element->mme_type = MAC_MLS_TYPE_HIGH;
		element->mme_level = MAC_MLS_TYPE_UNDEF;
	} else if (strcmp(string, "low") == 0 ||
	    strcmp(string, "lo") == 0) {
		element->mme_type = MAC_MLS_TYPE_LOW;
		element->mme_level = MAC_MLS_TYPE_UNDEF;
	} else if (strcmp(string, "equal") == 0 ||
	    strcmp(string, "eq") == 0) {
		element->mme_type = MAC_MLS_TYPE_EQUAL;
		element->mme_level = MAC_MLS_TYPE_UNDEF;
	} else {
		element->mme_type = MAC_MLS_TYPE_LEVEL;

		/*
		 * Numeric level piece of the element.
		 */
		level = strsep(&string, ":");
		value = strtol(level, &end, 10);
		if (end == level || *end != '\0')
			return (EINVAL);
		if (value < 0 || value > 65535)
			return (EINVAL);
		element->mme_level = value;

		/*
		 * Optional compartment piece of the element.  If none
		 * are included, we assume that the label has no
		 * compartments.
		 */
		if (string == NULL)
			return (0);
		if (*string == '\0')
			return (0);

		while ((compartment = strsep(&string, "+")) != NULL) {
			value = strtol(compartment, &end, 10);
			if (compartment == end || *end != '\0')
				return (EINVAL);
			if (value < 1 || value > MAC_MLS_MAX_COMPARTMENTS)
				return (EINVAL);
			MAC_MLS_BIT_SET(value, element->mme_compartments);
		}
	}

	return (0);
}

/*
 * Note: destructively consumes the string, make a local copy before
 * calling if that's a problem.
 */
static int
mac_mls_parse(struct mac_mls *mac_mls, char *string)
{
	char *rangehigh, *rangelow, *effective;
	int error;

	effective = strsep(&string, "(");
	if (*effective == '\0')
		effective = NULL;

	if (string != NULL) {
		rangelow = strsep(&string, "-");
		if (string == NULL)
			return (EINVAL);
		rangehigh = strsep(&string, ")");
		if (string == NULL)
			return (EINVAL);
		if (*string != '\0')
			return (EINVAL);
	} else {
		rangelow = NULL;
		rangehigh = NULL;
	}

	KASSERT((rangelow != NULL && rangehigh != NULL) ||
	    (rangelow == NULL && rangehigh == NULL),
	    ("mac_mls_parse: range mismatch"));

	bzero(mac_mls, sizeof(*mac_mls));
	if (effective != NULL) {
		error = mac_mls_parse_element(&mac_mls->mm_effective, effective);
		if (error)
			return (error);
		mac_mls->mm_flags |= MAC_MLS_FLAG_EFFECTIVE;
	}

	if (rangelow != NULL) {
		error = mac_mls_parse_element(&mac_mls->mm_rangelow,
		    rangelow);
		if (error)
			return (error);
		error = mac_mls_parse_element(&mac_mls->mm_rangehigh,
		    rangehigh);
		if (error)
			return (error);
		mac_mls->mm_flags |= MAC_MLS_FLAG_RANGE;
	}

	error = mac_mls_valid(mac_mls);
	if (error)
		return (error);

	return (0);
}

static int
mac_mls_internalize_label(struct label *label, char *element_name,
    char *element_data, int *claimed)
{
	struct mac_mls *mac_mls, mac_mls_temp;
	int error;

	if (strcmp(MAC_MLS_LABEL_NAME, element_name) != 0)
		return (0);

	(*claimed)++;

	error = mac_mls_parse(&mac_mls_temp, element_data);
	if (error)
		return (error);

	mac_mls = SLOT(label);
	*mac_mls = mac_mls_temp;

	return (0);
}

static void
mac_mls_copy_label(struct label *src, struct label *dest)
{

	*SLOT(dest) = *SLOT(src);
}

/*
 * Labeling event operations: file system objects, and things that look
 * a lot like file system objects.
 */
static void
mac_mls_create_devfs_device(struct ucred *cred, struct mount *mp,
    struct cdev *dev, struct devfs_dirent *de, struct label *delabel)
{
	struct mac_mls *mac_mls;
	int mls_type;

	mac_mls = SLOT(delabel);
	if (strcmp(dev->si_name, "null") == 0 ||
	    strcmp(dev->si_name, "zero") == 0 ||
	    strcmp(dev->si_name, "random") == 0 ||
	    strncmp(dev->si_name, "fd/", strlen("fd/")) == 0)
		mls_type = MAC_MLS_TYPE_EQUAL;
	else if (strcmp(dev->si_name, "kmem") == 0 ||
	    strcmp(dev->si_name, "mem") == 0)
		mls_type = MAC_MLS_TYPE_HIGH;
	else if (ptys_equal &&
	    (strncmp(dev->si_name, "ttyp", strlen("ttyp")) == 0 ||
	    strncmp(dev->si_name, "ptyp", strlen("ptyp")) == 0))
		mls_type = MAC_MLS_TYPE_EQUAL;
	else
		mls_type = MAC_MLS_TYPE_LOW;
	mac_mls_set_effective(mac_mls, mls_type, 0, NULL);
}

static void
mac_mls_create_devfs_directory(struct mount *mp, char *dirname,
    int dirnamelen, struct devfs_dirent *de, struct label *delabel)
{
	struct mac_mls *mac_mls;

	mac_mls = SLOT(delabel);
	mac_mls_set_effective(mac_mls, MAC_MLS_TYPE_LOW, 0, NULL);
}

static void
mac_mls_create_devfs_symlink(struct ucred *cred, struct mount *mp,
    struct devfs_dirent *dd, struct label *ddlabel, struct devfs_dirent *de,
    struct label *delabel)
{
	struct mac_mls *source, *dest;

	source = SLOT(cred->cr_label);
	dest = SLOT(delabel);

	mac_mls_copy_effective(source, dest);
}

static void
mac_mls_create_mount(struct ucred *cred, struct mount *mp,
    struct label *mplabel)
{
	struct mac_mls *source, *dest;

	source = SLOT(cred->cr_label);
	dest = SLOT(mplabel);
	mac_mls_copy_effective(source, dest);
}

static void
mac_mls_relabel_vnode(struct ucred *cred, struct vnode *vp,
    struct label *vplabel, struct label *label)
{
	struct mac_mls *source, *dest;

	source = SLOT(label);
	dest = SLOT(vplabel);

	mac_mls_copy(source, dest);
}

static void
mac_mls_update_devfs(struct mount *mp, struct devfs_dirent *de,
    struct label *delabel, struct vnode *vp, struct label *vplabel)
{
	struct mac_mls *source, *dest;

	source = SLOT(vplabel);
	dest = SLOT(delabel);

	mac_mls_copy_effective(source, dest);
}

static void
mac_mls_associate_vnode_devfs(struct mount *mp, struct label *mplabel,
    struct devfs_dirent *de, struct label *delabel, struct vnode *vp,
    struct label *vplabel)
{
	struct mac_mls *source, *dest;

	source = SLOT(delabel);
	dest = SLOT(vplabel);

	mac_mls_copy_effective(source, dest);
}

static int
mac_mls_associate_vnode_extattr(struct mount *mp, struct label *mplabel,
    struct vnode *vp, struct label *vplabel)
{
	struct mac_mls temp, *source, *dest;
	int buflen, error;

	source = SLOT(mplabel);
	dest = SLOT(vplabel);

	buflen = sizeof(temp);
	bzero(&temp, buflen);

	error = vn_extattr_get(vp, IO_NODELOCKED, MAC_MLS_EXTATTR_NAMESPACE,
	    MAC_MLS_EXTATTR_NAME, &buflen, (char *) &temp, curthread);
	if (error == ENOATTR || error == EOPNOTSUPP) {
		/* Fall back to the mntlabel. */
		mac_mls_copy_effective(source, dest);
		return (0);
	} else if (error)
		return (error);

	if (buflen != sizeof(temp)) {
		printf("mac_mls_associate_vnode_extattr: bad size %d\n",
		    buflen);
		return (EPERM);
	}
	if (mac_mls_valid(&temp) != 0) {
		printf("mac_mls_associate_vnode_extattr: invalid\n");
		return (EPERM);
	}
	if ((temp.mm_flags & MAC_MLS_FLAGS_BOTH) != MAC_MLS_FLAG_EFFECTIVE) {
		printf("mac_mls_associated_vnode_extattr: not effective\n");
		return (EPERM);
	}

	mac_mls_copy_effective(&temp, dest);
	return (0);
}

static void
mac_mls_associate_vnode_singlelabel(struct mount *mp,
    struct label *mplabel, struct vnode *vp, struct label *vplabel)
{
	struct mac_mls *source, *dest;

	source = SLOT(mplabel);
	dest = SLOT(vplabel);

	mac_mls_copy_effective(source, dest);
}

static int
mac_mls_create_vnode_extattr(struct ucred *cred, struct mount *mp,
    struct label *mplabel, struct vnode *dvp, struct label *dvplabel,
    struct vnode *vp, struct label *vplabel, struct componentname *cnp)
{
	struct mac_mls *source, *dest, temp;
	size_t buflen;
	int error;

	buflen = sizeof(temp);
	bzero(&temp, buflen);

	source = SLOT(cred->cr_label);
	dest = SLOT(vplabel);
	mac_mls_copy_effective(source, &temp);

	error = vn_extattr_set(vp, IO_NODELOCKED, MAC_MLS_EXTATTR_NAMESPACE,
	    MAC_MLS_EXTATTR_NAME, buflen, (char *) &temp, curthread);
	if (error == 0)
		mac_mls_copy_effective(source, dest);
	return (error);
}

static int
mac_mls_setlabel_vnode_extattr(struct ucred *cred, struct vnode *vp,
    struct label *vplabel, struct label *intlabel)
{
	struct mac_mls *source, temp;
	size_t buflen;
	int error;

	buflen = sizeof(temp);
	bzero(&temp, buflen);

	source = SLOT(intlabel);
	if ((source->mm_flags & MAC_MLS_FLAG_EFFECTIVE) == 0)
		return (0);

	mac_mls_copy_effective(source, &temp);

	error = vn_extattr_set(vp, IO_NODELOCKED, MAC_MLS_EXTATTR_NAMESPACE,
	    MAC_MLS_EXTATTR_NAME, buflen, (char *) &temp, curthread);
	return (error);
}

/*
 * Labeling event operations: IPC object.
 */
static void
mac_mls_create_inpcb_from_socket(struct socket *so, struct label *solabel,
    struct inpcb *inp, struct label *inplabel)
{
	struct mac_mls *source, *dest;

	source = SLOT(solabel);
	dest = SLOT(inplabel);

	mac_mls_copy_effective(source, dest);
}

static void
mac_mls_create_mbuf_from_socket(struct socket *so, struct label *solabel,
    struct mbuf *m, struct label *mlabel)
{
	struct mac_mls *source, *dest;

	source = SLOT(solabel);
	dest = SLOT(mlabel);

	mac_mls_copy_effective(source, dest);
}

static void
mac_mls_create_socket(struct ucred *cred, struct socket *so,
    struct label *solabel)
{
	struct mac_mls *source, *dest;

	source = SLOT(cred->cr_label);
	dest = SLOT(solabel);

	mac_mls_copy_effective(source, dest);
}

static void
mac_mls_create_pipe(struct ucred *cred, struct pipepair *pp,
    struct label *pplabel)
{
	struct mac_mls *source, *dest;

	source = SLOT(cred->cr_label);
	dest = SLOT(pplabel);

	mac_mls_copy_effective(source, dest);
}

static void
mac_mls_create_posix_sem(struct ucred *cred, struct ksem *ks,
    struct label *kslabel)
{
	struct mac_mls *source, *dest;

	source = SLOT(cred->cr_label);
	dest = SLOT(kslabel);

	mac_mls_copy_effective(source, dest);
}

static void
mac_mls_create_socket_from_socket(struct socket *oldso,
    struct label *oldsolabel, struct socket *newso, struct label *newsolabel)
{
	struct mac_mls *source, *dest;

	source = SLOT(oldsolabel);
	dest = SLOT(newsolabel);

	mac_mls_copy_effective(source, dest);
}

static void
mac_mls_relabel_socket(struct ucred *cred, struct socket *so,
    struct label *solabel, struct label *newlabel)
{
	struct mac_mls *source, *dest;

	source = SLOT(newlabel);
	dest = SLOT(solabel);

	mac_mls_copy(source, dest);
}

static void
mac_mls_relabel_pipe(struct ucred *cred, struct pipepair *pp,
    struct label *pplabel, struct label *newlabel)
{
	struct mac_mls *source, *dest;

	source = SLOT(newlabel);
	dest = SLOT(pplabel);

	mac_mls_copy(source, dest);
}

static void
mac_mls_set_socket_peer_from_mbuf(struct mbuf *m, struct label *mlabel,
    struct socket *so, struct label *sopeerlabel)
{
	struct mac_mls *source, *dest;

	source = SLOT(mlabel);
	dest = SLOT(sopeerlabel);

	mac_mls_copy_effective(source, dest);
}

/*
 * Labeling event operations: System V IPC objects.
 */
static void
mac_mls_create_sysv_msgmsg(struct ucred *cred, struct msqid_kernel *msqkptr,
    struct label *msqlabel, struct msg *msgptr, struct label *msglabel)
{
	struct mac_mls *source, *dest;

	/* Ignore the msgq label. */
	source = SLOT(cred->cr_label);
	dest = SLOT(msglabel);

	mac_mls_copy_effective(source, dest);
}

static void
mac_mls_create_sysv_msgqueue(struct ucred *cred, struct msqid_kernel *msqkptr,
    struct label *msqlabel)
{
	struct mac_mls *source, *dest;

	source = SLOT(cred->cr_label);
	dest = SLOT(msqlabel);

	mac_mls_copy_effective(source, dest);
}

static void
mac_mls_create_sysv_sem(struct ucred *cred, struct semid_kernel *semakptr,
    struct label *semalabel)
{
	struct mac_mls *source, *dest;

	source = SLOT(cred->cr_label);
	dest = SLOT(semalabel);

	mac_mls_copy_effective(source, dest);
}

static void
mac_mls_create_sysv_shm(struct ucred *cred, struct shmid_kernel *shmsegptr,
    struct label *shmlabel)
{
	struct mac_mls *source, *dest;

	source = SLOT(cred->cr_label);
	dest = SLOT(shmlabel);

	mac_mls_copy_effective(source, dest);
}

/*
 * Labeling event operations: network objects.
 */
static void
mac_mls_set_socket_peer_from_socket(struct socket *oldso,
    struct label *oldsolabel, struct socket *newso,
    struct label *newsopeerlabel)
{
	struct mac_mls *source, *dest;

	source = SLOT(oldsolabel);
	dest = SLOT(newsopeerlabel);

	mac_mls_copy_effective(source, dest);
}

static void
mac_mls_create_bpfdesc(struct ucred *cred, struct bpf_d *d,
    struct label *dlabel)
{
	struct mac_mls *source, *dest;

	source = SLOT(cred->cr_label);
	dest = SLOT(dlabel);

	mac_mls_copy_effective(source, dest);
}

static void
mac_mls_create_ifnet(struct ifnet *ifp, struct label *ifplabel)
{
	struct mac_mls *dest;
	int type;

	dest = SLOT(ifplabel);

	if (ifp->if_type == IFT_LOOP)
		type = MAC_MLS_TYPE_EQUAL;
	else
		type = MAC_MLS_TYPE_LOW;

	mac_mls_set_effective(dest, type, 0, NULL);
	mac_mls_set_range(dest, type, 0, NULL, type, 0, NULL);
}

static void
mac_mls_create_ipq(struct mbuf *m, struct label *mlabel, struct ipq *ipq,
    struct label *ipqlabel)
{
	struct mac_mls *source, *dest;

	source = SLOT(mlabel);
	dest = SLOT(ipqlabel);

	mac_mls_copy_effective(source, dest);
}

static void
mac_mls_create_datagram_from_ipq(struct ipq *ipq, struct label *ipqlabel,
    struct mbuf *m, struct label *mlabel)
{
	struct mac_mls *source, *dest;

	source = SLOT(ipqlabel);
	dest = SLOT(mlabel);

	/* Just use the head, since we require them all to match. */
	mac_mls_copy_effective(source, dest);
}

static void
mac_mls_create_fragment(struct mbuf *m, struct label *mlabel,
    struct mbuf *frag, struct label *fraglabel)
{
	struct mac_mls *source, *dest;

	source = SLOT(mlabel);
	dest = SLOT(fraglabel);

	mac_mls_copy_effective(source, dest);
}

static void
mac_mls_create_mbuf_from_inpcb(struct inpcb *inp, struct label *inplabel,
    struct mbuf *m, struct label *mlabel)
{
	struct mac_mls *source, *dest;

	source = SLOT(inplabel);
	dest = SLOT(mlabel);

	mac_mls_copy_effective(source, dest);
}

static void
mac_mls_create_mbuf_linklayer(struct ifnet *ifp, struct label *ifplabel,
    struct mbuf *m, struct label *mlabel)
{
	struct mac_mls *dest;

	dest = SLOT(mlabel);

	mac_mls_set_effective(dest, MAC_MLS_TYPE_EQUAL, 0, NULL);
}

static void
mac_mls_create_mbuf_from_bpfdesc(struct bpf_d *d, struct label *dlabel,
    struct mbuf *m, struct label *mlabel)
{
	struct mac_mls *source, *dest;

	source = SLOT(dlabel);
	dest = SLOT(mlabel);

	mac_mls_copy_effective(source, dest);
}

static void
mac_mls_create_mbuf_from_ifnet(struct ifnet *ifp, struct label *ifplabel,
    struct mbuf *m, struct label *mlabel)
{
	struct mac_mls *source, *dest;

	source = SLOT(ifplabel);
	dest = SLOT(mlabel);

	mac_mls_copy_effective(source, dest);
}

static void
mac_mls_create_mbuf_multicast_encap(struct mbuf *m, struct label *mlabel,
    struct ifnet *ifp, struct label *ifplabel, struct mbuf *mnew,
    struct label *mnewlabel)
{
	struct mac_mls *source, *dest;

	source = SLOT(mlabel);
	dest = SLOT(mnewlabel);

	mac_mls_copy_effective(source, dest);
}

static void
mac_mls_create_mbuf_netlayer(struct mbuf *m, struct label *mlabel,
    struct mbuf *mnew, struct label *mnewlabel)
{
	struct mac_mls *source, *dest;

	source = SLOT(mlabel);
	dest = SLOT(mnewlabel);

	mac_mls_copy_effective(source, dest);
}

static int
mac_mls_fragment_match(struct mbuf *m, struct label *mlabel, struct ipq *ipq,
    struct label *ipqlabel)
{
	struct mac_mls *a, *b;

	a = SLOT(ipqlabel);
	b = SLOT(mlabel);

	return (mac_mls_equal_effective(a, b));
}

static void
mac_mls_relabel_ifnet(struct ucred *cred, struct ifnet *ifp,
    struct label *ifplabel, struct label *newlabel)
{
	struct mac_mls *source, *dest;

	source = SLOT(newlabel);
	dest = SLOT(ifplabel);

	mac_mls_copy(source, dest);
}

static void
mac_mls_update_ipq(struct mbuf *m, struct label *mlabel, struct ipq *ipq,
    struct label *ipqlabel)
{

	/* NOOP: we only accept matching labels, so no need to update */
}

static void
mac_mls_inpcb_sosetlabel(struct socket *so, struct label *solabel,
    struct inpcb *inp, struct label *inplabel)
{
	struct mac_mls *source, *dest;

	source = SLOT(solabel);
	dest = SLOT(inplabel);

	mac_mls_copy(source, dest);
}

static void
mac_mls_create_mbuf_from_firewall(struct mbuf *m, struct label *mlabel)
{
	struct mac_mls *dest;

	dest = SLOT(mlabel);

	/* XXX: where is the label for the firewall really comming from? */
	mac_mls_set_effective(dest, MAC_MLS_TYPE_EQUAL, 0, NULL);
}

static void
mac_mls_init_syncache_from_inpcb(struct label *label, struct inpcb *inp)
{
	struct mac_mls *source, *dest;

	source = SLOT(inp->inp_label);
	dest = SLOT(label);
	mac_mls_copy_effective(source, dest);
}

static void
mac_mls_create_mbuf_from_syncache(struct label *sc_label, struct mbuf *m,
    struct label *mlabel)
{
	struct mac_mls *source, *dest;

	source = SLOT(sc_label);
	dest = SLOT(mlabel);
	mac_mls_copy_effective(source, dest);
}

/*
 * Labeling event operations: processes.
 */
static void
mac_mls_create_proc0(struct ucred *cred)
{
	struct mac_mls *dest;

	dest = SLOT(cred->cr_label);

	mac_mls_set_effective(dest, MAC_MLS_TYPE_EQUAL, 0, NULL);
	mac_mls_set_range(dest, MAC_MLS_TYPE_LOW, 0, NULL, MAC_MLS_TYPE_HIGH,
	    0, NULL);
}

static void
mac_mls_create_proc1(struct ucred *cred)
{
	struct mac_mls *dest;

	dest = SLOT(cred->cr_label);

	mac_mls_set_effective(dest, MAC_MLS_TYPE_LOW, 0, NULL);
	mac_mls_set_range(dest, MAC_MLS_TYPE_LOW, 0, NULL, MAC_MLS_TYPE_HIGH,
	    0, NULL);
}

static void
mac_mls_relabel_cred(struct ucred *cred, struct label *newlabel)
{
	struct mac_mls *source, *dest;

	source = SLOT(newlabel);
	dest = SLOT(cred->cr_label);

	mac_mls_copy(source, dest);
}

/*
 * Label cleanup/flush operations.
 */
static void
mac_mls_cleanup_sysv_msgmsg(struct label *msglabel)
{

	bzero(SLOT(msglabel), sizeof(struct mac_mls));
}

static void
mac_mls_cleanup_sysv_msgqueue(struct label *msqlabel)
{

	bzero(SLOT(msqlabel), sizeof(struct mac_mls));
}

static void
mac_mls_cleanup_sysv_sem(struct label *semalabel)
{

	bzero(SLOT(semalabel), sizeof(struct mac_mls));
}

static void
mac_mls_cleanup_sysv_shm(struct label *shmlabel)
{

	bzero(SLOT(shmlabel), sizeof(struct mac_mls));
}

/*
 * Access control checks.
 */
static int
mac_mls_check_bpfdesc_receive(struct bpf_d *d, struct label *dlabel,
     struct ifnet *ifp, struct label *ifplabel)
{
	struct mac_mls *a, *b;

	if (!mac_mls_enabled)
		return (0);

	a = SLOT(dlabel);
	b = SLOT(ifplabel);

	if (mac_mls_equal_effective(a, b))
		return (0);
	return (EACCES);
}

static int
mac_mls_check_cred_relabel(struct ucred *cred, struct label *newlabel)
{
	struct mac_mls *subj, *new;
	int error;

	subj = SLOT(cred->cr_label);
	new = SLOT(newlabel);

	/*
	 * If there is an MLS label update for the credential, it may be
	 * an update of effective, range, or both.
	 */
	error = mls_atmostflags(new, MAC_MLS_FLAGS_BOTH);
	if (error)
		return (error);

	/*
	 * If the MLS label is to be changed, authorize as appropriate.
	 */
	if (new->mm_flags & MAC_MLS_FLAGS_BOTH) {
		/*
		 * If the change request modifies both the MLS label effective
		 * and range, check that the new effective will be in the
		 * new range.
		 */
		if ((new->mm_flags & MAC_MLS_FLAGS_BOTH) ==
		    MAC_MLS_FLAGS_BOTH &&
		    !mac_mls_effective_in_range(new, new))
			return (EINVAL);

		/*
		 * To change the MLS effective label on a credential, the
		 * new effective label must be in the current range.
		 */
		if (new->mm_flags & MAC_MLS_FLAG_EFFECTIVE &&
		    !mac_mls_effective_in_range(new, subj))
			return (EPERM);

		/*
		 * To change the MLS range label on a credential, the
		 * new range must be in the current range.
		 */
		if (new->mm_flags & MAC_MLS_FLAG_RANGE &&
		    !mac_mls_range_in_range(new, subj))
			return (EPERM);

		/*
		 * To have EQUAL in any component of the new credential
		 * MLS label, the subject must already have EQUAL in
		 * their label.
		 */
		if (mac_mls_contains_equal(new)) {
			error = mac_mls_subject_privileged(subj);
			if (error)
				return (error);
		}
	}

	return (0);
}

static int
mac_mls_check_cred_visible(struct ucred *cr1, struct ucred *cr2)
{
	struct mac_mls *subj, *obj;

	if (!mac_mls_enabled)
		return (0);

	subj = SLOT(cr1->cr_label);
	obj = SLOT(cr2->cr_label);

	/* XXX: range */
	if (!mac_mls_dominate_effective(subj, obj))
		return (ESRCH);

	return (0);
}

static int
mac_mls_check_ifnet_relabel(struct ucred *cred, struct ifnet *ifp,
    struct label *ifplabel, struct label *newlabel)
{
	struct mac_mls *subj, *new;
	int error;

	subj = SLOT(cred->cr_label);
	new = SLOT(newlabel);

	/*
	 * If there is an MLS label update for the interface, it may
	 * be an update of effective, range, or both.
	 */
	error = mls_atmostflags(new, MAC_MLS_FLAGS_BOTH);
	if (error)
		return (error);

	/*
	 * Relabeling network interfaces requires MLS privilege.
	 */
	error = mac_mls_subject_privileged(subj);

	return (0);
}

static int
mac_mls_check_ifnet_transmit(struct ifnet *ifp, struct label *ifplabel,
    struct mbuf *m, struct label *mlabel)
{
	struct mac_mls *p, *i;

	if (!mac_mls_enabled)
		return (0);

	p = SLOT(mlabel);
	i = SLOT(ifplabel);

	return (mac_mls_effective_in_range(p, i) ? 0 : EACCES);
}

static int
mac_mls_check_inpcb_deliver(struct inpcb *inp, struct label *inplabel,
    struct mbuf *m, struct label *mlabel)
{
	struct mac_mls *p, *i;

	if (!mac_mls_enabled)
		return (0);

	p = SLOT(mlabel);
	i = SLOT(inplabel);

	return (mac_mls_equal_effective(p, i) ? 0 : EACCES);
}

static int
mac_mls_check_sysv_msgrcv(struct ucred *cred, struct msg *msgptr,
    struct label *msglabel)
{
	struct mac_mls *subj, *obj;

	if (!mac_mls_enabled)
		return (0);

	subj = SLOT(cred->cr_label);
	obj = SLOT(msglabel);

	if (!mac_mls_dominate_effective(subj, obj))
		return (EACCES);

	return (0);
}

static int
mac_mls_check_sysv_msgrmid(struct ucred *cred, struct msg *msgptr,
    struct label *msglabel)
{
	struct mac_mls *subj, *obj;

	if (!mac_mls_enabled)
		return (0);

	subj = SLOT(cred->cr_label);
	obj = SLOT(msglabel);

	if (!mac_mls_dominate_effective(obj, subj))
		return (EACCES);

	return (0);
}

static int
mac_mls_check_sysv_msqget(struct ucred *cred, struct msqid_kernel *msqkptr,
    struct label *msqklabel)
{
	struct mac_mls *subj, *obj;

	if (!mac_mls_enabled)
		return (0);

	subj = SLOT(cred->cr_label);
	obj = SLOT(msqklabel);

	if (!mac_mls_dominate_effective(subj, obj))
		return (EACCES);

	return (0);
}

static int
mac_mls_check_sysv_msqsnd(struct ucred *cred, struct msqid_kernel *msqkptr,
    struct label *msqklabel)
{
	struct mac_mls *subj, *obj;

	if (!mac_mls_enabled)
		return (0);

	subj = SLOT(cred->cr_label);
	obj = SLOT(msqklabel);

	if (!mac_mls_dominate_effective(obj, subj))
		return (EACCES);

	return (0);
}

static int
mac_mls_check_sysv_msqrcv(struct ucred *cred, struct msqid_kernel *msqkptr,
    struct label *msqklabel)
{
	struct mac_mls *subj, *obj;

	if (!mac_mls_enabled)
		return (0);

	subj = SLOT(cred->cr_label);
	obj = SLOT(msqklabel);

	if (!mac_mls_dominate_effective(subj, obj))
		return (EACCES);

	return (0);
}

static int
mac_mls_check_sysv_msqctl(struct ucred *cred, struct msqid_kernel *msqkptr,
    struct label *msqklabel, int cmd)
{
	struct mac_mls *subj, *obj;

	if (!mac_mls_enabled)
		return (0);

	subj = SLOT(cred->cr_label);
	obj = SLOT(msqklabel);

	switch(cmd) {
	case IPC_RMID:
	case IPC_SET:
		if (!mac_mls_dominate_effective(obj, subj))
			return (EACCES);
		break;

	case IPC_STAT:
		if (!mac_mls_dominate_effective(subj, obj))
			return (EACCES);
		break;

	default:
		return (EACCES);
	}

	return (0);
}

static int
mac_mls_check_sysv_semctl(struct ucred *cred, struct semid_kernel *semakptr,
    struct label *semaklabel, int cmd)
{
	struct mac_mls *subj, *obj;

	if (!mac_mls_enabled)
		return (0);

	subj = SLOT(cred->cr_label);
	obj = SLOT(semaklabel);

	switch(cmd) {
	case IPC_RMID:
	case IPC_SET:
	case SETVAL:
	case SETALL:
		if (!mac_mls_dominate_effective(obj, subj))
			return (EACCES);
		break;

	case IPC_STAT:
	case GETVAL:
	case GETPID:
	case GETNCNT:
	case GETZCNT:
	case GETALL:
		if (!mac_mls_dominate_effective(subj, obj))
			return (EACCES);
		break;

	default:
		return (EACCES);
	}

	return (0);
}

static int
mac_mls_check_sysv_semget(struct ucred *cred, struct semid_kernel *semakptr,
    struct label *semaklabel)
{
	struct mac_mls *subj, *obj;

	if (!mac_mls_enabled)
		return (0);

	subj = SLOT(cred->cr_label);
	obj = SLOT(semaklabel);

	if (!mac_mls_dominate_effective(subj, obj))
		return (EACCES);

	return (0);
}

static int
mac_mls_check_sysv_semop(struct ucred *cred, struct semid_kernel *semakptr,
    struct label *semaklabel, size_t accesstype)
{
	struct mac_mls *subj, *obj;

	if (!mac_mls_enabled)
		return (0);

	subj = SLOT(cred->cr_label);
	obj = SLOT(semaklabel);

	if( accesstype & SEM_R )
		if (!mac_mls_dominate_effective(subj, obj))
			return (EACCES);

	if( accesstype & SEM_A )
		if (!mac_mls_dominate_effective(obj, subj))
			return (EACCES);

	return (0);
}

static int
mac_mls_check_sysv_shmat(struct ucred *cred, struct shmid_kernel *shmsegptr,
    struct label *shmseglabel, int shmflg)
{
	struct mac_mls *subj, *obj;

	if (!mac_mls_enabled)
		return (0);

	subj = SLOT(cred->cr_label);
	obj = SLOT(shmseglabel);

	if (!mac_mls_dominate_effective(subj, obj))
			return (EACCES);
	if ((shmflg & SHM_RDONLY) == 0)
		if (!mac_mls_dominate_effective(obj, subj))
			return (EACCES);
	
	return (0);
}

static int
mac_mls_check_sysv_shmctl(struct ucred *cred, struct shmid_kernel *shmsegptr,
    struct label *shmseglabel, int cmd)
{
	struct mac_mls *subj, *obj;

	if (!mac_mls_enabled)
		return (0);

	subj = SLOT(cred->cr_label);
	obj = SLOT(shmseglabel);

	switch(cmd) {
	case IPC_RMID:
	case IPC_SET:
		if (!mac_mls_dominate_effective(obj, subj))
			return (EACCES);
		break;

	case IPC_STAT:
	case SHM_STAT:
		if (!mac_mls_dominate_effective(subj, obj))
			return (EACCES);
		break;

	default:
		return (EACCES);
	}

	return (0);
}

static int
mac_mls_check_sysv_shmget(struct ucred *cred, struct shmid_kernel *shmsegptr,
    struct label *shmseglabel, int shmflg)
{
	struct mac_mls *subj, *obj;

	if (!mac_mls_enabled)
		return (0);

	subj = SLOT(cred->cr_label);
	obj = SLOT(shmseglabel);

	if (!mac_mls_dominate_effective(obj, subj))
		return (EACCES);

	return (0);
}

static int
mac_mls_check_mount_stat(struct ucred *cred, struct mount *mp,
    struct label *mntlabel)
{
	struct mac_mls *subj, *obj;

	if (!mac_mls_enabled)
		return (0);

	subj = SLOT(cred->cr_label);
	obj = SLOT(mntlabel);

	if (!mac_mls_dominate_effective(subj, obj))
		return (EACCES);

	return (0);
}

static int
mac_mls_check_pipe_ioctl(struct ucred *cred, struct pipepair *pp,
    struct label *pplabel, unsigned long cmd, void /* caddr_t */ *data)
{

	if(!mac_mls_enabled)
		return (0);

	/* XXX: This will be implemented soon... */

	return (0);
}

static int
mac_mls_check_pipe_poll(struct ucred *cred, struct pipepair *pp,
    struct label *pplabel)
{
	struct mac_mls *subj, *obj;

	if (!mac_mls_enabled)
		return (0);

	subj = SLOT(cred->cr_label);
	obj = SLOT(pplabel);

	if (!mac_mls_dominate_effective(subj, obj))
		return (EACCES);

	return (0);
}

static int
mac_mls_check_pipe_read(struct ucred *cred, struct pipepair *pp,
    struct label *pplabel)
{
	struct mac_mls *subj, *obj;

	if (!mac_mls_enabled)
		return (0);

	subj = SLOT(cred->cr_label);
	obj = SLOT(pplabel);

	if (!mac_mls_dominate_effective(subj, obj))
		return (EACCES);

	return (0);
}

static int
mac_mls_check_pipe_relabel(struct ucred *cred, struct pipepair *pp,
    struct label *pplabel, struct label *newlabel)
{
	struct mac_mls *subj, *obj, *new;
	int error;

	new = SLOT(newlabel);
	subj = SLOT(cred->cr_label);
	obj = SLOT(pplabel);

	/*
	 * If there is an MLS label update for a pipe, it must be a
	 * effective update.
	 */
	error = mls_atmostflags(new, MAC_MLS_FLAG_EFFECTIVE);
	if (error)
		return (error);

	/*
	 * To perform a relabel of a pipe (MLS label or not), MLS must
	 * authorize the relabel.
	 */
	if (!mac_mls_effective_in_range(obj, subj))
		return (EPERM);

	/*
	 * If the MLS label is to be changed, authorize as appropriate.
	 */
	if (new->mm_flags & MAC_MLS_FLAG_EFFECTIVE) {
		/*
		 * To change the MLS label on a pipe, the new pipe label
		 * must be in the subject range.
		 */
		if (!mac_mls_effective_in_range(new, subj))
			return (EPERM);

		/*
		 * To change the MLS label on a pipe to be EQUAL, the
		 * subject must have appropriate privilege.
		 */
		if (mac_mls_contains_equal(new)) {
			error = mac_mls_subject_privileged(subj);
			if (error)
				return (error);
		}
	}

	return (0);
}

static int
mac_mls_check_pipe_stat(struct ucred *cred, struct pipepair *pp,
    struct label *pplabel)
{
	struct mac_mls *subj, *obj;

	if (!mac_mls_enabled)
		return (0);

	subj = SLOT(cred->cr_label);
	obj = SLOT(pplabel);

	if (!mac_mls_dominate_effective(subj, obj))
		return (EACCES);

	return (0);
}

static int
mac_mls_check_pipe_write(struct ucred *cred, struct pipepair *pp,
    struct label *pplabel)
{
	struct mac_mls *subj, *obj;

	if (!mac_mls_enabled)
		return (0);

	subj = SLOT(cred->cr_label);
	obj = SLOT(pplabel);

	if (!mac_mls_dominate_effective(obj, subj))
		return (EACCES);

	return (0);
}

static int
mac_mls_check_posix_sem_write(struct ucred *cred, struct ksem *ks,
    struct label *kslabel)
{
	struct mac_mls *subj, *obj;

	if (!mac_mls_enabled)
		return (0);

	subj = SLOT(cred->cr_label);
	obj = SLOT(kslabel);

	if (!mac_mls_dominate_effective(obj, subj))
		return (EACCES);

	return (0);
}

static int
mac_mls_check_posix_sem_rdonly(struct ucred *cred, struct ksem *ks,
    struct label *kslabel)
{
	struct mac_mls *subj, *obj;

	if (!mac_mls_enabled)
		return (0);

	subj = SLOT(cred->cr_label);
	obj = SLOT(kslabel);

	if (!mac_mls_dominate_effective(subj, obj))
		return (EACCES);

	return (0);
}

static int
mac_mls_check_proc_debug(struct ucred *cred, struct proc *p)
{
	struct mac_mls *subj, *obj;

	if (!mac_mls_enabled)
		return (0);

	subj = SLOT(cred->cr_label);
	obj = SLOT(p->p_ucred->cr_label);

	/* XXX: range checks */
	if (!mac_mls_dominate_effective(subj, obj))
		return (ESRCH);
	if (!mac_mls_dominate_effective(obj, subj))
		return (EACCES);

	return (0);
}

static int
mac_mls_check_proc_sched(struct ucred *cred, struct proc *p)
{
	struct mac_mls *subj, *obj;

	if (!mac_mls_enabled)
		return (0);

	subj = SLOT(cred->cr_label);
	obj = SLOT(p->p_ucred->cr_label);

	/* XXX: range checks */
	if (!mac_mls_dominate_effective(subj, obj))
		return (ESRCH);
	if (!mac_mls_dominate_effective(obj, subj))
		return (EACCES);

	return (0);
}

static int
mac_mls_check_proc_signal(struct ucred *cred, struct proc *p, int signum)
{
	struct mac_mls *subj, *obj;

	if (!mac_mls_enabled)
		return (0);

	subj = SLOT(cred->cr_label);
	obj = SLOT(p->p_ucred->cr_label);

	/* XXX: range checks */
	if (!mac_mls_dominate_effective(subj, obj))
		return (ESRCH);
	if (!mac_mls_dominate_effective(obj, subj))
		return (EACCES);

	return (0);
}

static int
mac_mls_check_socket_deliver(struct socket *so, struct label *solabel,
    struct mbuf *m, struct label *mlabel)
{
	struct mac_mls *p, *s;

	if (!mac_mls_enabled)
		return (0);

	p = SLOT(mlabel);
	s = SLOT(solabel);

	return (mac_mls_equal_effective(p, s) ? 0 : EACCES);
}

static int
mac_mls_check_socket_relabel(struct ucred *cred, struct socket *so,
    struct label *solabel, struct label *newlabel)
{
	struct mac_mls *subj, *obj, *new;
	int error;

	new = SLOT(newlabel);
	subj = SLOT(cred->cr_label);
	obj = SLOT(solabel);

	/*
	 * If there is an MLS label update for the socket, it may be
	 * an update of effective.
	 */
	error = mls_atmostflags(new, MAC_MLS_FLAG_EFFECTIVE);
	if (error)
		return (error);

	/*
	 * To relabel a socket, the old socket effective must be in the subject
	 * range.
	 */
	if (!mac_mls_effective_in_range(obj, subj))
		return (EPERM);

	/*
	 * If the MLS label is to be changed, authorize as appropriate.
	 */
	if (new->mm_flags & MAC_MLS_FLAG_EFFECTIVE) {
		/*
		 * To relabel a socket, the new socket effective must be in
		 * the subject range.
		 */
		if (!mac_mls_effective_in_range(new, subj))
			return (EPERM);

		/*
		 * To change the MLS label on the socket to contain EQUAL,
		 * the subject must have appropriate privilege.
		 */
		if (mac_mls_contains_equal(new)) {
			error = mac_mls_subject_privileged(subj);
			if (error)
				return (error);
		}
	}

	return (0);
}

static int
mac_mls_check_socket_visible(struct ucred *cred, struct socket *so,
    struct label *solabel)
{
	struct mac_mls *subj, *obj;

	if (!mac_mls_enabled)
		return (0);

	subj = SLOT(cred->cr_label);
	obj = SLOT(solabel);

	if (!mac_mls_dominate_effective(subj, obj))
		return (ENOENT);

	return (0);
}

static int
mac_mls_check_system_acct(struct ucred *cred, struct vnode *vp,
    struct label *vplabel)
{
	struct mac_mls *subj, *obj;

	if (!mac_mls_enabled)
		return (0);

	subj = SLOT(cred->cr_label);
	obj = SLOT(vplabel);

	if (!mac_mls_dominate_effective(obj, subj) ||
	    !mac_mls_dominate_effective(subj, obj))
		return (EACCES);

	return (0);
}

static int
mac_mls_check_system_auditctl(struct ucred *cred, struct vnode *vp,
    struct label *vplabel)
{
	struct mac_mls *subj, *obj;

	if (!mac_mls_enabled)
		return (0);

	subj = SLOT(cred->cr_label);
	obj = SLOT(vplabel);

	if (!mac_mls_dominate_effective(obj, subj) ||
	    !mac_mls_dominate_effective(subj, obj))
		return (EACCES);

	return (0);
}

static int
mac_mls_check_system_swapon(struct ucred *cred, struct vnode *vp,
    struct label *vplabel)
{
	struct mac_mls *subj, *obj;

	if (!mac_mls_enabled)
		return (0);

	subj = SLOT(cred->cr_label);
	obj = SLOT(vplabel);

	if (!mac_mls_dominate_effective(obj, subj) ||
	    !mac_mls_dominate_effective(subj, obj))
		return (EACCES);

	return (0);
}

static int
mac_mls_check_vnode_chdir(struct ucred *cred, struct vnode *dvp,
    struct label *dvplabel)
{
	struct mac_mls *subj, *obj;

	if (!mac_mls_enabled)
		return (0);

	subj = SLOT(cred->cr_label);
	obj = SLOT(dvplabel);

	if (!mac_mls_dominate_effective(subj, obj))
		return (EACCES);

	return (0);
}

static int
mac_mls_check_vnode_chroot(struct ucred *cred, struct vnode *dvp,
    struct label *dvplabel)
{
	struct mac_mls *subj, *obj;

	if (!mac_mls_enabled)
		return (0);

	subj = SLOT(cred->cr_label);
	obj = SLOT(dvplabel);

	if (!mac_mls_dominate_effective(subj, obj))
		return (EACCES);

	return (0);
}

static int
mac_mls_check_vnode_create(struct ucred *cred, struct vnode *dvp,
    struct label *dvplabel, struct componentname *cnp, struct vattr *vap)
{
	struct mac_mls *subj, *obj;

	if (!mac_mls_enabled)
		return (0);

	subj = SLOT(cred->cr_label);
	obj = SLOT(dvplabel);

	if (!mac_mls_dominate_effective(obj, subj))
		return (EACCES);

	return (0);
}

static int
mac_mls_check_vnode_deleteacl(struct ucred *cred, struct vnode *vp,
    struct label *vplabel, acl_type_t type)
{
	struct mac_mls *subj, *obj;

	if (!mac_mls_enabled)
		return (0);

	subj = SLOT(cred->cr_label);
	obj = SLOT(vplabel);

	if (!mac_mls_dominate_effective(obj, subj))
		return (EACCES);

	return (0);
}

static int
mac_mls_check_vnode_deleteextattr(struct ucred *cred, struct vnode *vp,
    struct label *vplabel, int attrnamespace, const char *name)
{
	struct mac_mls *subj, *obj;

	if (!mac_mls_enabled)
		return (0);

	subj = SLOT(cred->cr_label);
	obj = SLOT(vplabel);

	if (!mac_mls_dominate_effective(obj, subj))
		return (EACCES);

	return (0);
}

static int
mac_mls_check_vnode_exec(struct ucred *cred, struct vnode *vp,
    struct label *vplabel, struct image_params *imgp,
    struct label *execlabel)
{
	struct mac_mls *subj, *obj, *exec;
	int error;

	if (execlabel != NULL) {
		/*
		 * We currently don't permit labels to be changed at
		 * exec-time as part of MLS, so disallow non-NULL
		 * MLS label elements in the execlabel.
		 */
		exec = SLOT(execlabel);
		error = mls_atmostflags(exec, 0);
		if (error)
			return (error);
	}

	if (!mac_mls_enabled)
		return (0);

	subj = SLOT(cred->cr_label);
	obj = SLOT(vplabel);

	if (!mac_mls_dominate_effective(subj, obj))
		return (EACCES);

	return (0);
}

static int
mac_mls_check_vnode_getacl(struct ucred *cred, struct vnode *vp,
    struct label *vplabel, acl_type_t type)
{
	struct mac_mls *subj, *obj;

	if (!mac_mls_enabled)
		return (0);

	subj = SLOT(cred->cr_label);
	obj = SLOT(vplabel);

	if (!mac_mls_dominate_effective(subj, obj))
		return (EACCES);

	return (0);
}

static int
mac_mls_check_vnode_getextattr(struct ucred *cred, struct vnode *vp,
    struct label *vplabel, int attrnamespace, const char *name,
    struct uio *uio)
{
	struct mac_mls *subj, *obj;

	if (!mac_mls_enabled)
		return (0);

	subj = SLOT(cred->cr_label);
	obj = SLOT(vplabel);

	if (!mac_mls_dominate_effective(subj, obj))
		return (EACCES);

	return (0);
}

static int
mac_mls_check_vnode_link(struct ucred *cred, struct vnode *dvp,
    struct label *dvplabel, struct vnode *vp, struct label *vplabel,
    struct componentname *cnp)
{
	struct mac_mls *subj, *obj;

	if (!mac_mls_enabled)
		return (0);

	subj = SLOT(cred->cr_label);
	obj = SLOT(dvplabel);

	if (!mac_mls_dominate_effective(obj, subj))
		return (EACCES);

	obj = SLOT(vplabel);
	if (!mac_mls_dominate_effective(obj, subj))
		return (EACCES);

	return (0);
}

static int
mac_mls_check_vnode_listextattr(struct ucred *cred, struct vnode *vp,
    struct label *vplabel, int attrnamespace)
{

	struct mac_mls *subj, *obj;

	if (!mac_mls_enabled)
		return (0);

	subj = SLOT(cred->cr_label);
	obj = SLOT(vplabel);

	if (!mac_mls_dominate_effective(subj, obj))
		return (EACCES);

	return (0);
}

static int
mac_mls_check_vnode_lookup(struct ucred *cred, struct vnode *dvp,
    struct label *dvplabel, struct componentname *cnp)
{
	struct mac_mls *subj, *obj;

	if (!mac_mls_enabled)
		return (0);

	subj = SLOT(cred->cr_label);
	obj = SLOT(dvplabel);

	if (!mac_mls_dominate_effective(subj, obj))
		return (EACCES);

	return (0);
}

static int
mac_mls_check_vnode_mmap(struct ucred *cred, struct vnode *vp,
    struct label *vplabel, int prot, int flags)
{
	struct mac_mls *subj, *obj;

	/*
	 * Rely on the use of open()-time protections to handle
	 * non-revocation cases.
	 */
	if (!mac_mls_enabled || !revocation_enabled)
		return (0);

	subj = SLOT(cred->cr_label);
	obj = SLOT(vplabel);

	if (prot & (VM_PROT_READ | VM_PROT_EXECUTE)) {
		if (!mac_mls_dominate_effective(subj, obj))
			return (EACCES);
	}
	if (((prot & VM_PROT_WRITE) != 0) && ((flags & MAP_SHARED) != 0)) {
		if (!mac_mls_dominate_effective(obj, subj))
			return (EACCES);
	}

	return (0);
}

static int
mac_mls_check_vnode_open(struct ucred *cred, struct vnode *vp,
    struct label *vplabel, int acc_mode)
{
	struct mac_mls *subj, *obj;

	if (!mac_mls_enabled)
		return (0);

	subj = SLOT(cred->cr_label);
	obj = SLOT(vplabel);

	/* XXX privilege override for admin? */
	if (acc_mode & (VREAD | VEXEC | VSTAT)) {
		if (!mac_mls_dominate_effective(subj, obj))
			return (EACCES);
	}
	if (acc_mode & (VWRITE | VAPPEND | VADMIN)) {
		if (!mac_mls_dominate_effective(obj, subj))
			return (EACCES);
	}

	return (0);
}

static int
mac_mls_check_vnode_poll(struct ucred *active_cred, struct ucred *file_cred,
    struct vnode *vp, struct label *vplabel)
{
	struct mac_mls *subj, *obj;

	if (!mac_mls_enabled || !revocation_enabled)
		return (0);

	subj = SLOT(active_cred->cr_label);
	obj = SLOT(vplabel);

	if (!mac_mls_dominate_effective(subj, obj))
		return (EACCES);

	return (0);
}

static int
mac_mls_check_vnode_read(struct ucred *active_cred, struct ucred *file_cred,
    struct vnode *vp, struct label *vplabel)
{
	struct mac_mls *subj, *obj;

	if (!mac_mls_enabled || !revocation_enabled)
		return (0);

	subj = SLOT(active_cred->cr_label);
	obj = SLOT(vplabel);

	if (!mac_mls_dominate_effective(subj, obj))
		return (EACCES);

	return (0);
}

static int
mac_mls_check_vnode_readdir(struct ucred *cred, struct vnode *dvp,
    struct label *dvplabel)
{
	struct mac_mls *subj, *obj;

	if (!mac_mls_enabled)
		return (0);

	subj = SLOT(cred->cr_label);
	obj = SLOT(dvplabel);

	if (!mac_mls_dominate_effective(subj, obj))
		return (EACCES);

	return (0);
}

static int
mac_mls_check_vnode_readlink(struct ucred *cred, struct vnode *vp,
    struct label *vplabel)
{
	struct mac_mls *subj, *obj;

	if (!mac_mls_enabled)
		return (0);

	subj = SLOT(cred->cr_label);
	obj = SLOT(vplabel);

	if (!mac_mls_dominate_effective(subj, obj))
		return (EACCES);

	return (0);
}

static int
mac_mls_check_vnode_relabel(struct ucred *cred, struct vnode *vp,
    struct label *vplabel, struct label *newlabel)
{
	struct mac_mls *old, *new, *subj;
	int error;

	old = SLOT(vplabel);
	new = SLOT(newlabel);
	subj = SLOT(cred->cr_label);

	/*
	 * If there is an MLS label update for the vnode, it must be a
	 * effective label.
	 */
	error = mls_atmostflags(new, MAC_MLS_FLAG_EFFECTIVE);
	if (error)
		return (error);

	/*
	 * To perform a relabel of the vnode (MLS label or not), MLS must
	 * authorize the relabel.
	 */
	if (!mac_mls_effective_in_range(old, subj))
		return (EPERM);

	/*
	 * If the MLS label is to be changed, authorize as appropriate.
	 */
	if (new->mm_flags & MAC_MLS_FLAG_EFFECTIVE) {
		/*
		 * To change the MLS label on a vnode, the new vnode label
		 * must be in the subject range.
		 */
		if (!mac_mls_effective_in_range(new, subj))
			return (EPERM);

		/*
		 * To change the MLS label on the vnode to be EQUAL,
		 * the subject must have appropriate privilege.
		 */
		if (mac_mls_contains_equal(new)) {
			error = mac_mls_subject_privileged(subj);
			if (error)
				return (error);
		}
	}

	return (0);
}

static int
mac_mls_check_vnode_rename_from(struct ucred *cred, struct vnode *dvp,
    struct label *dvplabel, struct vnode *vp, struct label *vplabel,
    struct componentname *cnp)
{
	struct mac_mls *subj, *obj;

	if (!mac_mls_enabled)
		return (0);

	subj = SLOT(cred->cr_label);
	obj = SLOT(dvplabel);

	if (!mac_mls_dominate_effective(obj, subj))
		return (EACCES);

	obj = SLOT(vplabel);

	if (!mac_mls_dominate_effective(obj, subj))
		return (EACCES);

	return (0);
}

static int
mac_mls_check_vnode_rename_to(struct ucred *cred, struct vnode *dvp,
    struct label *dvplabel, struct vnode *vp, struct label *vplabel,
    int samedir, struct componentname *cnp)
{
	struct mac_mls *subj, *obj;

	if (!mac_mls_enabled)
		return (0);

	subj = SLOT(cred->cr_label);
	obj = SLOT(dvplabel);

	if (!mac_mls_dominate_effective(obj, subj))
		return (EACCES);

	if (vp != NULL) {
		obj = SLOT(vplabel);

		if (!mac_mls_dominate_effective(obj, subj))
			return (EACCES);
	}

	return (0);
}

static int
mac_mls_check_vnode_revoke(struct ucred *cred, struct vnode *vp,
    struct label *vplabel)
{
	struct mac_mls *subj, *obj;

	if (!mac_mls_enabled)
		return (0);

	subj = SLOT(cred->cr_label);
	obj = SLOT(vplabel);

	if (!mac_mls_dominate_effective(obj, subj))
		return (EACCES);

	return (0);
}

static int
mac_mls_check_vnode_setacl(struct ucred *cred, struct vnode *vp,
    struct label *vplabel, acl_type_t type, struct acl *acl)
{
	struct mac_mls *subj, *obj;

	if (!mac_mls_enabled)
		return (0);

	subj = SLOT(cred->cr_label);
	obj = SLOT(vplabel);

	if (!mac_mls_dominate_effective(obj, subj))
		return (EACCES);

	return (0);
}

static int
mac_mls_check_vnode_setextattr(struct ucred *cred, struct vnode *vp,
    struct label *vplabel, int attrnamespace, const char *name,
    struct uio *uio)
{
	struct mac_mls *subj, *obj;

	if (!mac_mls_enabled)
		return (0);

	subj = SLOT(cred->cr_label);
	obj = SLOT(vplabel);

	if (!mac_mls_dominate_effective(obj, subj))
		return (EACCES);

	/* XXX: protect the MAC EA in a special way? */

	return (0);
}

static int
mac_mls_check_vnode_setflags(struct ucred *cred, struct vnode *vp,
    struct label *vplabel, u_long flags)
{
	struct mac_mls *subj, *obj;

	if (!mac_mls_enabled)
		return (0);

	subj = SLOT(cred->cr_label);
	obj = SLOT(vplabel);

	if (!mac_mls_dominate_effective(obj, subj))
		return (EACCES);

	return (0);
}

static int
mac_mls_check_vnode_setmode(struct ucred *cred, struct vnode *vp,
    struct label *vplabel, mode_t mode)
{
	struct mac_mls *subj, *obj;

	if (!mac_mls_enabled)
		return (0);

	subj = SLOT(cred->cr_label);
	obj = SLOT(vplabel);

	if (!mac_mls_dominate_effective(obj, subj))
		return (EACCES);

	return (0);
}

static int
mac_mls_check_vnode_setowner(struct ucred *cred, struct vnode *vp,
    struct label *vplabel, uid_t uid, gid_t gid)
{
	struct mac_mls *subj, *obj;

	if (!mac_mls_enabled)
		return (0);

	subj = SLOT(cred->cr_label);
	obj = SLOT(vplabel);

	if (!mac_mls_dominate_effective(obj, subj))
		return (EACCES);

	return (0);
}

static int
mac_mls_check_vnode_setutimes(struct ucred *cred, struct vnode *vp,
    struct label *vplabel, struct timespec atime, struct timespec mtime)
{
	struct mac_mls *subj, *obj;

	if (!mac_mls_enabled)
		return (0);

	subj = SLOT(cred->cr_label);
	obj = SLOT(vplabel);

	if (!mac_mls_dominate_effective(obj, subj))
		return (EACCES);

	return (0);
}

static int
mac_mls_check_vnode_stat(struct ucred *active_cred, struct ucred *file_cred,
    struct vnode *vp, struct label *vplabel)
{
	struct mac_mls *subj, *obj;

	if (!mac_mls_enabled)
		return (0);

	subj = SLOT(active_cred->cr_label);
	obj = SLOT(vplabel);

	if (!mac_mls_dominate_effective(subj, obj))
		return (EACCES);

	return (0);
}

static int
mac_mls_check_vnode_unlink(struct ucred *cred, struct vnode *dvp,
    struct label *dvplabel, struct vnode *vp, struct label *vplabel,
    struct componentname *cnp)
{
	struct mac_mls *subj, *obj;

	if (!mac_mls_enabled)
		return (0);

	subj = SLOT(cred->cr_label);
	obj = SLOT(dvplabel);

	if (!mac_mls_dominate_effective(obj, subj))
		return (EACCES);

	obj = SLOT(vplabel);

	if (!mac_mls_dominate_effective(obj, subj))
		return (EACCES);

	return (0);
}

static int
mac_mls_check_vnode_write(struct ucred *active_cred, struct ucred *file_cred,
    struct vnode *vp, struct label *vplabel)
{
	struct mac_mls *subj, *obj;

	if (!mac_mls_enabled || !revocation_enabled)
		return (0);

	subj = SLOT(active_cred->cr_label);
	obj = SLOT(vplabel);

	if (!mac_mls_dominate_effective(obj, subj))
		return (EACCES);

	return (0);
}

static void
mac_mls_associate_nfsd_label(struct ucred *cred) 
{
	struct mac_mls *label;

	label = SLOT(cred->cr_label);
	mac_mls_set_effective(label, MAC_MLS_TYPE_LOW, 0, NULL);
	mac_mls_set_range(label, MAC_MLS_TYPE_LOW, 0, NULL,
	    MAC_MLS_TYPE_HIGH, 0, NULL);
}

static struct mac_policy_ops mac_mls_ops =
{
	.mpo_init = mac_mls_init,
	.mpo_init_bpfdesc_label = mac_mls_init_label,
	.mpo_init_cred_label = mac_mls_init_label,
	.mpo_init_devfs_label = mac_mls_init_label,
	.mpo_init_ifnet_label = mac_mls_init_label,
	.mpo_init_inpcb_label = mac_mls_init_label_waitcheck,
	.mpo_init_syncache_label = mac_mls_init_label_waitcheck,
	.mpo_init_sysv_msgmsg_label = mac_mls_init_label,
	.mpo_init_sysv_msgqueue_label = mac_mls_init_label,
	.mpo_init_sysv_sem_label = mac_mls_init_label,
	.mpo_init_sysv_shm_label = mac_mls_init_label,
	.mpo_init_ipq_label = mac_mls_init_label_waitcheck,
	.mpo_init_mbuf_label = mac_mls_init_label_waitcheck,
	.mpo_init_mount_label = mac_mls_init_label,
	.mpo_init_pipe_label = mac_mls_init_label,
	.mpo_init_posix_sem_label = mac_mls_init_label,
	.mpo_init_socket_label = mac_mls_init_label_waitcheck,
	.mpo_init_socket_peer_label = mac_mls_init_label_waitcheck,
	.mpo_init_vnode_label = mac_mls_init_label,
	.mpo_destroy_bpfdesc_label = mac_mls_destroy_label,
	.mpo_destroy_cred_label = mac_mls_destroy_label,
	.mpo_destroy_devfs_label = mac_mls_destroy_label,
	.mpo_destroy_ifnet_label = mac_mls_destroy_label,
	.mpo_destroy_inpcb_label = mac_mls_destroy_label,
	.mpo_destroy_syncache_label = mac_mls_destroy_label,
	.mpo_destroy_sysv_msgmsg_label = mac_mls_destroy_label,
	.mpo_destroy_sysv_msgqueue_label = mac_mls_destroy_label,
	.mpo_destroy_sysv_sem_label = mac_mls_destroy_label,
	.mpo_destroy_sysv_shm_label = mac_mls_destroy_label,
	.mpo_destroy_ipq_label = mac_mls_destroy_label,
	.mpo_destroy_mbuf_label = mac_mls_destroy_label,
	.mpo_destroy_mount_label = mac_mls_destroy_label,
	.mpo_destroy_pipe_label = mac_mls_destroy_label,
	.mpo_destroy_posix_sem_label = mac_mls_destroy_label,
	.mpo_destroy_socket_label = mac_mls_destroy_label,
	.mpo_destroy_socket_peer_label = mac_mls_destroy_label,
	.mpo_destroy_vnode_label = mac_mls_destroy_label,
	.mpo_copy_cred_label = mac_mls_copy_label,
	.mpo_copy_ifnet_label = mac_mls_copy_label,
	.mpo_copy_mbuf_label = mac_mls_copy_label,
	.mpo_copy_pipe_label = mac_mls_copy_label,
	.mpo_copy_socket_label = mac_mls_copy_label,
	.mpo_copy_vnode_label = mac_mls_copy_label,
	.mpo_externalize_cred_label = mac_mls_externalize_label,
	.mpo_externalize_ifnet_label = mac_mls_externalize_label,
	.mpo_externalize_pipe_label = mac_mls_externalize_label,
	.mpo_externalize_socket_label = mac_mls_externalize_label,
	.mpo_externalize_socket_peer_label = mac_mls_externalize_label,
	.mpo_externalize_vnode_label = mac_mls_externalize_label,
	.mpo_internalize_cred_label = mac_mls_internalize_label,
	.mpo_internalize_ifnet_label = mac_mls_internalize_label,
	.mpo_internalize_pipe_label = mac_mls_internalize_label,
	.mpo_internalize_socket_label = mac_mls_internalize_label,
	.mpo_internalize_vnode_label = mac_mls_internalize_label,
	.mpo_create_devfs_device = mac_mls_create_devfs_device,
	.mpo_create_devfs_directory = mac_mls_create_devfs_directory,
	.mpo_create_devfs_symlink = mac_mls_create_devfs_symlink,
	.mpo_create_mount = mac_mls_create_mount,
	.mpo_relabel_vnode = mac_mls_relabel_vnode,
	.mpo_update_devfs = mac_mls_update_devfs,
	.mpo_associate_vnode_devfs = mac_mls_associate_vnode_devfs,
	.mpo_associate_vnode_extattr = mac_mls_associate_vnode_extattr,
	.mpo_associate_vnode_singlelabel = mac_mls_associate_vnode_singlelabel,
	.mpo_create_vnode_extattr = mac_mls_create_vnode_extattr,
	.mpo_setlabel_vnode_extattr = mac_mls_setlabel_vnode_extattr,
	.mpo_create_mbuf_from_socket = mac_mls_create_mbuf_from_socket,
	.mpo_create_mbuf_from_syncache = mac_mls_create_mbuf_from_syncache,
	.mpo_create_pipe = mac_mls_create_pipe,
	.mpo_create_posix_sem = mac_mls_create_posix_sem,
	.mpo_create_socket = mac_mls_create_socket,
	.mpo_create_socket_from_socket = mac_mls_create_socket_from_socket,
	.mpo_relabel_pipe = mac_mls_relabel_pipe,
	.mpo_relabel_socket = mac_mls_relabel_socket,
	.mpo_set_socket_peer_from_mbuf = mac_mls_set_socket_peer_from_mbuf,
	.mpo_set_socket_peer_from_socket = mac_mls_set_socket_peer_from_socket,
	.mpo_create_bpfdesc = mac_mls_create_bpfdesc,
	.mpo_create_datagram_from_ipq = mac_mls_create_datagram_from_ipq,
	.mpo_create_fragment = mac_mls_create_fragment,
	.mpo_create_ifnet = mac_mls_create_ifnet,
	.mpo_create_inpcb_from_socket = mac_mls_create_inpcb_from_socket,
	.mpo_init_syncache_from_inpcb = mac_mls_init_syncache_from_inpcb,
	.mpo_create_ipq = mac_mls_create_ipq,
	.mpo_create_sysv_msgmsg = mac_mls_create_sysv_msgmsg,
	.mpo_create_sysv_msgqueue = mac_mls_create_sysv_msgqueue,
	.mpo_create_sysv_sem = mac_mls_create_sysv_sem,
	.mpo_create_sysv_shm = mac_mls_create_sysv_shm,
	.mpo_create_mbuf_from_inpcb = mac_mls_create_mbuf_from_inpcb,
	.mpo_create_mbuf_linklayer = mac_mls_create_mbuf_linklayer,
	.mpo_create_mbuf_from_bpfdesc = mac_mls_create_mbuf_from_bpfdesc,
	.mpo_create_mbuf_from_ifnet = mac_mls_create_mbuf_from_ifnet,
	.mpo_create_mbuf_multicast_encap = mac_mls_create_mbuf_multicast_encap,
	.mpo_create_mbuf_netlayer = mac_mls_create_mbuf_netlayer,
	.mpo_fragment_match = mac_mls_fragment_match,
	.mpo_relabel_ifnet = mac_mls_relabel_ifnet,
	.mpo_update_ipq = mac_mls_update_ipq,
	.mpo_inpcb_sosetlabel = mac_mls_inpcb_sosetlabel,
	.mpo_create_proc0 = mac_mls_create_proc0,
	.mpo_create_proc1 = mac_mls_create_proc1,
	.mpo_relabel_cred = mac_mls_relabel_cred,
	.mpo_cleanup_sysv_msgmsg = mac_mls_cleanup_sysv_msgmsg,
	.mpo_cleanup_sysv_msgqueue = mac_mls_cleanup_sysv_msgqueue,
	.mpo_cleanup_sysv_sem = mac_mls_cleanup_sysv_sem,
	.mpo_cleanup_sysv_shm = mac_mls_cleanup_sysv_shm,
	.mpo_check_bpfdesc_receive = mac_mls_check_bpfdesc_receive,
	.mpo_check_cred_relabel = mac_mls_check_cred_relabel,
	.mpo_check_cred_visible = mac_mls_check_cred_visible,
	.mpo_check_ifnet_relabel = mac_mls_check_ifnet_relabel,
	.mpo_check_ifnet_transmit = mac_mls_check_ifnet_transmit,
	.mpo_check_inpcb_deliver = mac_mls_check_inpcb_deliver,
	.mpo_check_sysv_msgrcv = mac_mls_check_sysv_msgrcv,
	.mpo_check_sysv_msgrmid = mac_mls_check_sysv_msgrmid,
	.mpo_check_sysv_msqget = mac_mls_check_sysv_msqget,
	.mpo_check_sysv_msqsnd = mac_mls_check_sysv_msqsnd,
	.mpo_check_sysv_msqrcv = mac_mls_check_sysv_msqrcv,
	.mpo_check_sysv_msqctl = mac_mls_check_sysv_msqctl,
	.mpo_check_sysv_semctl = mac_mls_check_sysv_semctl,
	.mpo_check_sysv_semget = mac_mls_check_sysv_semget,
	.mpo_check_sysv_semop = mac_mls_check_sysv_semop,
	.mpo_check_sysv_shmat = mac_mls_check_sysv_shmat,
	.mpo_check_sysv_shmctl = mac_mls_check_sysv_shmctl,
	.mpo_check_sysv_shmget = mac_mls_check_sysv_shmget,
	.mpo_check_mount_stat = mac_mls_check_mount_stat,
	.mpo_check_pipe_ioctl = mac_mls_check_pipe_ioctl,
	.mpo_check_pipe_poll = mac_mls_check_pipe_poll,
	.mpo_check_pipe_read = mac_mls_check_pipe_read,
	.mpo_check_pipe_relabel = mac_mls_check_pipe_relabel,
	.mpo_check_pipe_stat = mac_mls_check_pipe_stat,
	.mpo_check_pipe_write = mac_mls_check_pipe_write,
	.mpo_check_posix_sem_destroy = mac_mls_check_posix_sem_write,
	.mpo_check_posix_sem_getvalue = mac_mls_check_posix_sem_rdonly,
	.mpo_check_posix_sem_open = mac_mls_check_posix_sem_write,
	.mpo_check_posix_sem_post = mac_mls_check_posix_sem_write,
	.mpo_check_posix_sem_unlink = mac_mls_check_posix_sem_write,
	.mpo_check_posix_sem_wait = mac_mls_check_posix_sem_write,
	.mpo_check_proc_debug = mac_mls_check_proc_debug,
	.mpo_check_proc_sched = mac_mls_check_proc_sched,
	.mpo_check_proc_signal = mac_mls_check_proc_signal,
	.mpo_check_socket_deliver = mac_mls_check_socket_deliver,
	.mpo_check_socket_relabel = mac_mls_check_socket_relabel,
	.mpo_check_socket_visible = mac_mls_check_socket_visible,
	.mpo_check_system_acct = mac_mls_check_system_acct,
	.mpo_check_system_auditctl = mac_mls_check_system_auditctl,
	.mpo_check_system_swapon = mac_mls_check_system_swapon,
	.mpo_check_vnode_access = mac_mls_check_vnode_open,
	.mpo_check_vnode_chdir = mac_mls_check_vnode_chdir,
	.mpo_check_vnode_chroot = mac_mls_check_vnode_chroot,
	.mpo_check_vnode_create = mac_mls_check_vnode_create,
	.mpo_check_vnode_deleteacl = mac_mls_check_vnode_deleteacl,
	.mpo_check_vnode_deleteextattr = mac_mls_check_vnode_deleteextattr,
	.mpo_check_vnode_exec = mac_mls_check_vnode_exec,
	.mpo_check_vnode_getacl = mac_mls_check_vnode_getacl,
	.mpo_check_vnode_getextattr = mac_mls_check_vnode_getextattr,
	.mpo_check_vnode_link = mac_mls_check_vnode_link,
	.mpo_check_vnode_listextattr = mac_mls_check_vnode_listextattr,
	.mpo_check_vnode_lookup = mac_mls_check_vnode_lookup,
	.mpo_check_vnode_mmap = mac_mls_check_vnode_mmap,
	.mpo_check_vnode_open = mac_mls_check_vnode_open,
	.mpo_check_vnode_poll = mac_mls_check_vnode_poll,
	.mpo_check_vnode_read = mac_mls_check_vnode_read,
	.mpo_check_vnode_readdir = mac_mls_check_vnode_readdir,
	.mpo_check_vnode_readlink = mac_mls_check_vnode_readlink,
	.mpo_check_vnode_relabel = mac_mls_check_vnode_relabel,
	.mpo_check_vnode_rename_from = mac_mls_check_vnode_rename_from,
	.mpo_check_vnode_rename_to = mac_mls_check_vnode_rename_to,
	.mpo_check_vnode_revoke = mac_mls_check_vnode_revoke,
	.mpo_check_vnode_setacl = mac_mls_check_vnode_setacl,
	.mpo_check_vnode_setextattr = mac_mls_check_vnode_setextattr,
	.mpo_check_vnode_setflags = mac_mls_check_vnode_setflags,
	.mpo_check_vnode_setmode = mac_mls_check_vnode_setmode,
	.mpo_check_vnode_setowner = mac_mls_check_vnode_setowner,
	.mpo_check_vnode_setutimes = mac_mls_check_vnode_setutimes,
	.mpo_check_vnode_stat = mac_mls_check_vnode_stat,
	.mpo_check_vnode_unlink = mac_mls_check_vnode_unlink,
	.mpo_check_vnode_write = mac_mls_check_vnode_write,
	.mpo_associate_nfsd_label = mac_mls_associate_nfsd_label,
	.mpo_create_mbuf_from_firewall = mac_mls_create_mbuf_from_firewall,
};

MAC_POLICY_SET(&mac_mls_ops, mac_mls, "TrustedBSD MAC/MLS",
    MPC_LOADTIME_FLAG_NOTLATE | MPC_LOADTIME_FLAG_LABELMBUFS, &mac_mls_slot);
