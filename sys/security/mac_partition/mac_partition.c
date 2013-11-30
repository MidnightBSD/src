/*-
 * Copyright (c) 1999-2002 Robert N. M. Watson
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
 * $FreeBSD: src/sys/security/mac_partition/mac_partition.c,v 1.10.8.1 2005/09/26 14:36:53 phk Exp $
 */

/*
 * Developed by the TrustedBSD Project.
 * Experiment with a partition-like model.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/mac.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/sbuf.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/sysent.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sx.h>
#include <sys/sysctl.h>

#include <fs/devfs/devfs.h>

#include <net/bpfdesc.h>
#include <net/if.h>
#include <net/if_types.h>
#include <net/if_var.h>

#include <vm/vm.h>

#include <sys/mac_policy.h>

#include <security/mac_partition/mac_partition.h>

SYSCTL_DECL(_security_mac);

SYSCTL_NODE(_security_mac, OID_AUTO, partition, CTLFLAG_RW, 0,
    "TrustedBSD mac_partition policy controls");

static int	mac_partition_enabled = 1;
SYSCTL_INT(_security_mac_partition, OID_AUTO, enabled, CTLFLAG_RW,
    &mac_partition_enabled, 0, "Enforce partition policy");

static int	partition_slot;
#define	SLOT(l)	(LABEL_TO_SLOT((l), partition_slot).l_long)

static void
mac_partition_init(struct mac_policy_conf *conf)
{

}

static void
mac_partition_init_label(struct label *label)
{

	SLOT(label) = 0;
}

static void
mac_partition_destroy_label(struct label *label)
{

	SLOT(label) = 0;
}

static void
mac_partition_copy_label(struct label *src, struct label *dest)
{

	SLOT(dest) = SLOT(src);
}

static int
mac_partition_externalize_label(struct label *label, char *element_name,
    struct sbuf *sb, int *claimed)
{

	if (strcmp(MAC_PARTITION_LABEL_NAME, element_name) != 0)
		return (0);

	(*claimed)++;

	if (sbuf_printf(sb, "%ld", SLOT(label)) == -1)
		return (EINVAL);
	else
		return (0);
}

static int
mac_partition_internalize_label(struct label *label, char *element_name,
    char *element_data, int *claimed)
{

	if (strcmp(MAC_PARTITION_LABEL_NAME, element_name) != 0)
		return (0);

	(*claimed)++;
	SLOT(label) = strtol(element_data, NULL, 10);
	return (0);
}

static void
mac_partition_create_proc0(struct ucred *cred)
{

	SLOT(cred->cr_label) = 0;
}

static void
mac_partition_create_proc1(struct ucred *cred)
{

	SLOT(cred->cr_label) = 0;
}

static void
mac_partition_relabel_cred(struct ucred *cred, struct label *newlabel)
{

	if (SLOT(newlabel) != 0)
		SLOT(cred->cr_label) = SLOT(newlabel);
}

static int
label_on_label(struct label *subject, struct label *object)
{

	if (mac_partition_enabled == 0)
		return (0);

	if (SLOT(subject) == 0)
		return (0);

	if (SLOT(subject) == SLOT(object))
		return (0);

	return (EPERM);
}

static int
mac_partition_check_cred_relabel(struct ucred *cred, struct label *newlabel)
{
	int error;

	error = 0;

	/* Treat "0" as a no-op request. */
	if (SLOT(newlabel) != 0) {
		/*
		 * Require BSD privilege in order to change the partition.
		 * Originally we also required that the process not be
		 * in a partition in the first place, but this didn't
		 * interact well with sendmail.
		 */
		error = suser_cred(cred, 0);
	}

	return (error);
}

static int
mac_partition_check_cred_visible(struct ucred *u1, struct ucred *u2)
{
	int error;

	error = label_on_label(u1->cr_label, u2->cr_label);

	return (error == 0 ? 0 : ESRCH);
}

static int
mac_partition_check_proc_debug(struct ucred *cred, struct proc *proc)
{
	int error;

	error = label_on_label(cred->cr_label, proc->p_ucred->cr_label);

	return (error ? ESRCH : 0);
}

static int
mac_partition_check_proc_sched(struct ucred *cred, struct proc *proc)
{
	int error;

	error = label_on_label(cred->cr_label, proc->p_ucred->cr_label);

	return (error ? ESRCH : 0);
}

static int
mac_partition_check_proc_signal(struct ucred *cred, struct proc *proc,
    int signum)
{
	int error;

	error = label_on_label(cred->cr_label, proc->p_ucred->cr_label);

	return (error ? ESRCH : 0);
}

static int
mac_partition_check_socket_visible(struct ucred *cred, struct socket *socket,
    struct label *socketlabel)
{
	int error;

	error = label_on_label(cred->cr_label, socketlabel);

	return (error ? ENOENT : 0);
}

static int
mac_partition_check_vnode_exec(struct ucred *cred, struct vnode *vp,
    struct label *label, struct image_params *imgp, struct label *execlabel)
{

	if (execlabel != NULL) {
		/*
		 * We currently don't permit labels to be changed at
		 * exec-time as part of the partition model, so disallow
		 * non-NULL partition label changes in execlabel.
		 */
		if (SLOT(execlabel) != 0)
			return (EINVAL);
	}

	return (0);
}

static struct mac_policy_ops mac_partition_ops =
{
	.mpo_init = mac_partition_init,
	.mpo_init_cred_label = mac_partition_init_label,
	.mpo_destroy_cred_label = mac_partition_destroy_label,
	.mpo_copy_cred_label = mac_partition_copy_label,
	.mpo_externalize_cred_label = mac_partition_externalize_label,
	.mpo_internalize_cred_label = mac_partition_internalize_label,
	.mpo_create_proc0 = mac_partition_create_proc0,
	.mpo_create_proc1 = mac_partition_create_proc1,
	.mpo_relabel_cred = mac_partition_relabel_cred,
	.mpo_check_cred_relabel = mac_partition_check_cred_relabel,
	.mpo_check_cred_visible = mac_partition_check_cred_visible,
	.mpo_check_proc_debug = mac_partition_check_proc_debug,
	.mpo_check_proc_sched = mac_partition_check_proc_sched,
	.mpo_check_proc_signal = mac_partition_check_proc_signal,
	.mpo_check_socket_visible = mac_partition_check_socket_visible,
	.mpo_check_vnode_exec = mac_partition_check_vnode_exec,
};

MAC_POLICY_SET(&mac_partition_ops, mac_partition, "TrustedBSD MAC/Partition",
    MPC_LOADTIME_FLAG_UNLOADOK, &partition_slot);
