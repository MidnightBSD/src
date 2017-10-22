/*-
 * Copyright (c) 1999-2002, 2007 Robert N. M. Watson
 * Copyright (c) 2001-2002 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed by Robert Watson for the TrustedBSD Project.
 *
 * This software was developed for the FreeBSD Project in part by Network
 * Associates Laboratories, the Security Research Division of Network
 * Associates, Inc. under DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"),
 * as part of the DARPA CHATS research program.
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
 * $FreeBSD: release/7.0.0/sys/security/mac_seeotheruids/mac_seeotheruids.c 170587 2007-06-12 00:12:01Z rwatson $
 */

/*
 * Developed by the TrustedBSD Project.
 *
 * Prevent processes owned by a particular uid from seeing various transient
 * kernel objects associated with other uids.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>

#include <security/mac/mac_policy.h>

SYSCTL_DECL(_security_mac);

SYSCTL_NODE(_security_mac, OID_AUTO, seeotheruids, CTLFLAG_RW, 0,
    "TrustedBSD mac_seeotheruids policy controls");

static int	mac_seeotheruids_enabled = 1;
SYSCTL_INT(_security_mac_seeotheruids, OID_AUTO, enabled, CTLFLAG_RW,
    &mac_seeotheruids_enabled, 0, "Enforce seeotheruids policy");

/*
 * Exception: allow credentials to be aware of other credentials with the
 * same primary gid.
 */
static int	primarygroup_enabled = 0;
SYSCTL_INT(_security_mac_seeotheruids, OID_AUTO, primarygroup_enabled,
    CTLFLAG_RW, &primarygroup_enabled, 0, "Make an exception for credentials "
    "with the same real primary group id");

/*
 * Exception: allow the root user to be aware of other credentials by virtue
 * of privilege.
 */
static int	suser_privileged = 1;
SYSCTL_INT(_security_mac_seeotheruids, OID_AUTO, suser_privileged,
    CTLFLAG_RW, &suser_privileged, 0, "Make an exception for superuser");

/*
 * Exception: allow processes with a specific gid to be exempt from the
 * policy.  One sysctl enables this functionality; the other sets the
 * exempt gid.
 */
static int	specificgid_enabled = 0;
SYSCTL_INT(_security_mac_seeotheruids, OID_AUTO, specificgid_enabled,
    CTLFLAG_RW, &specificgid_enabled, 0, "Make an exception for credentials "
    "with a specific gid as their real primary group id or group set");

static gid_t	specificgid = 0;
SYSCTL_INT(_security_mac_seeotheruids, OID_AUTO, specificgid, CTLFLAG_RW,
    &specificgid, 0, "Specific gid to be exempt from seeotheruids policy");

static int
mac_seeotheruids_check(struct ucred *cr1, struct ucred *cr2)
{

	if (!mac_seeotheruids_enabled)
		return (0);

	if (primarygroup_enabled) {
		if (cr1->cr_rgid == cr2->cr_rgid)
			return (0);
	}

	if (specificgid_enabled) {
		if (cr1->cr_rgid == specificgid ||
		    groupmember(specificgid, cr1))
			return (0);
	}

	if (cr1->cr_ruid == cr2->cr_ruid)
		return (0);

	if (suser_privileged) {
		if (priv_check_cred(cr1, PRIV_SEEOTHERUIDS, 0) == 0)
			return (0);
	}

	return (ESRCH);
}

static int
mac_seeotheruids_check_cred_visible(struct ucred *cr1, struct ucred *cr2)
{

	return (mac_seeotheruids_check(cr1, cr2));
}

static int
mac_seeotheruids_check_proc_signal(struct ucred *cred, struct proc *p,
    int signum)
{

	return (mac_seeotheruids_check(cred, p->p_ucred));
}

static int
mac_seeotheruids_check_proc_sched(struct ucred *cred, struct proc *p)
{

	return (mac_seeotheruids_check(cred, p->p_ucred));
}

static int
mac_seeotheruids_check_proc_debug(struct ucred *cred, struct proc *p)
{

	return (mac_seeotheruids_check(cred, p->p_ucred));
}

static int
mac_seeotheruids_check_socket_visible(struct ucred *cred, struct socket *so,
    struct label *solabel)
{

	return (mac_seeotheruids_check(cred, so->so_cred));
}

static struct mac_policy_ops mac_seeotheruids_ops =
{
	.mpo_check_cred_visible = mac_seeotheruids_check_cred_visible,
	.mpo_check_proc_debug = mac_seeotheruids_check_proc_debug,
	.mpo_check_proc_sched = mac_seeotheruids_check_proc_sched,
	.mpo_check_proc_signal = mac_seeotheruids_check_proc_signal,
	.mpo_check_socket_visible = mac_seeotheruids_check_socket_visible,
};

MAC_POLICY_SET(&mac_seeotheruids_ops, mac_seeotheruids,
    "TrustedBSD MAC/seeotheruids", MPC_LOADTIME_FLAG_UNLOADOK, NULL);
