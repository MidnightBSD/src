/*
 * Copyright (c) 1997 - 2001 Kungliga Tekniska H�gskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 *
 * 3. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 */

/* 
 * $Id: ktutil_locl.h,v 1.18 2002/09/10 20:03:45 joda Exp $
 * $FreeBSD: release/7.0.0/crypto/heimdal/admin/ktutil_locl.h 172506 2007-10-10 16:59:15Z cvs2svn $
 */

#ifndef __KTUTIL_LOCL_H__
#define __KTUTIL_LOCL_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <parse_time.h>
#include <roken.h>

#include "crypto-headers.h"
#include <krb5.h>
#include <kadm5/admin.h>
#include <kadm5/kadm5_err.h>

#include <sl.h>
#include <getarg.h>

extern krb5_context context;

extern int verbose_flag;
extern char *keytab_string; 

krb5_keytab ktutil_open_keytab(void);

int kt_add (int argc, char **argv);
int kt_change (int argc, char **argv);
int kt_copy (int argc, char **argv);
int kt_get (int argc, char **argv);
int kt_list(int argc, char **argv);
int kt_purge(int argc, char **argv);
int kt_remove(int argc, char **argv);
int kt_rename(int argc, char **argv);
int srvconv(int argc, char **argv);
int srvcreate(int argc, char **argv);

#endif /* __KTUTIL_LOCL_H__ */
