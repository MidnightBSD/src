/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2007 Robert N. M. Watson
 * Copyright (c) 2015 Allan Jude <allanjude@freebsd.org>
 * Copyright (c) 2017 Dell EMC
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
 *
 * $FreeBSD$
 */

#include <libxo/xo.h>

#ifndef PROCSTAT_H
#define	PROCSTAT_H

#define PROCSTAT_XO_VERSION "1"

extern int	hflag, nflag, Cflag, Hflag;

struct kinfo_proc;
void	kinfo_proc_sort(struct kinfo_proc *kipp, int count);
const char *	kinfo_proc_thread_name(const struct kinfo_proc *kipp);

void	procstat_args(struct procstat *prstat, struct kinfo_proc *kipp);
void	procstat_auxv(struct procstat *prstat, struct kinfo_proc *kipp);
void	procstat_basic(struct kinfo_proc *kipp);
void	procstat_bin(struct procstat *prstat, struct kinfo_proc *kipp);
void	procstat_cred(struct procstat *prstat, struct kinfo_proc *kipp);
void	procstat_cs(struct procstat *prstat, struct kinfo_proc *kipp);
void	procstat_env(struct procstat *prstat, struct kinfo_proc *kipp);
void	procstat_files(struct procstat *prstat, struct kinfo_proc *kipp);
void	procstat_kstack(struct procstat *prstat, struct kinfo_proc *kipp,
    int kflag);
void	procstat_ptlwpinfo(struct procstat *prstat);
void	procstat_rlimit(struct procstat *prstat, struct kinfo_proc *kipp);
void	procstat_rusage(struct procstat *prstat, struct kinfo_proc *kipp);
void	procstat_sigs(struct procstat *prstat, struct kinfo_proc *kipp);
void	procstat_threads(struct procstat *prstat, struct kinfo_proc *kipp);
void	procstat_threads_sigs(struct procstat *prstat, struct kinfo_proc *kipp);
void	procstat_vm(struct procstat *prstat, struct kinfo_proc *kipp);

#endif /* !PROCSTAT_H */
