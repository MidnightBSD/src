/*-
 * Copyright (c) 2000 Michael Smith
 * Copyright (c) 2000 BSDi
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
 * $FreeBSD: src/sys/dev/aac/aac_compat.h,v 1.3 2001/09/05 20:43:02 scottl Exp $
 */
/*
 * Backwards compatibility support.
 */

/*
 * Handle the new/old bio/buf changeover
 */

#if __FreeBSD_version < 500003		/* old buf style */
#include <sys/buf.h>
#define FREEBSD_4
#define bio					buf
#define bioq_init(x)				bufq_init(x)
#define bioq_insert_tail(x, y)			bufq_insert_tail(x, y)
#define bioq_remove(x, y)			bufq_remove(x, y)
#define bioq_first(x)				bufq_first(x)
#define bio_queue_head				buf_queue_head
#define bio_bcount				b_bcount
#define bio_blkno				b_blkno
#define bio_caller1				b_caller1
#define bio_data				b_data
#define bio_dev					b_dev
#define bio_driver1				b_driver1
#define bio_driver2				b_driver2
#define bio_error				b_error
#define bio_flags				b_flags
#define bio_pblkno				b_pblkno
#define bio_resid				b_resid
#define BIO_ERROR				B_ERROR
#define devstat_end_transaction_bio(x, y)      devstat_end_transaction_buf(x, y)
#define BIO_IS_READ(x)				((x)->b_flags & B_READ)

#else					/* new bio style */
#include <sys/bio.h>
#define BIO_IS_READ(x)				((x)->bio_cmd == BIO_READ)
#endif
