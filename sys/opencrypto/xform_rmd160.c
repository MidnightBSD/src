/*	$OpenBSD: xform.c,v 1.16 2001/08/28 12:20:43 ben Exp $	*/
/*-
 * The authors of this code are John Ioannidis (ji@tla.org),
 * Angelos D. Keromytis (kermit@csd.uch.gr),
 * Niels Provos (provos@physnet.uni-hamburg.de) and
 * Damien Miller (djm@mindrot.org).
 *
 * This code was written by John Ioannidis for BSD/OS in Athens, Greece,
 * in November 1995.
 *
 * Ported to OpenBSD and NetBSD, with additional transforms, in December 1996,
 * by Angelos D. Keromytis.
 *
 * Additional transforms and features in 1997 and 1998 by Angelos D. Keromytis
 * and Niels Provos.
 *
 * Additional features in 1999 by Angelos D. Keromytis.
 *
 * AES XTS implementation in 2008 by Damien Miller
 *
 * Copyright (C) 1995, 1996, 1997, 1998, 1999 by John Ioannidis,
 * Angelos D. Keromytis and Niels Provos.
 *
 * Copyright (C) 2001, Angelos D. Keromytis.
 *
 * Copyright (C) 2008, Damien Miller
 * Copyright (c) 2014 The FreeBSD Foundation
 * All rights reserved.
 *
 * Portions of this software were developed by John-Mark Gurney
 * under sponsorship of the FreeBSD Foundation and
 * Rubicon Communications, LLC (Netgate).
 *
 * Permission to use, copy, and modify this software with or without fee
 * is hereby granted, provided that this entire notice is included in
 * all copies of any software which is or includes a copy or
 * modification of this software.
 * You may use this code under the GNU public license if you so wish. Please
 * contribute changes back to the authors under this freer than GPL license
 * so that we may further the use of strong encryption without limitations to
 * all.
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTY. IN PARTICULAR, NONE OF THE AUTHORS MAKES ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE
 * MERCHANTABILITY OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR
 * PURPOSE.
 */

#include <sys/cdefs.h>
#include <opencrypto/rmd160.h>
#include <opencrypto/xform_auth.h>

static	void RMD160Init_int(void *);
static	int RMD160Update_int(void *, const void *, u_int);
static	void RMD160Final_int(uint8_t *, void *);

/* Authentication instances */
struct auth_hash auth_hash_hmac_ripemd_160 = {
	.type = CRYPTO_RIPEMD160_HMAC,
	.name = "HMAC-RIPEMD-160",
	.keysize = RIPEMD160_BLOCK_LEN,
	.hashsize = RIPEMD160_HASH_LEN,
	.ctxsize = sizeof(RMD160_CTX),
	.blocksize = RIPEMD160_BLOCK_LEN,
	.Init = RMD160Init_int,
	.Update = RMD160Update_int,
	.Final = RMD160Final_int,
};

static void
RMD160Init_int(void *ctx)
{
	RMD160Init(ctx);
}

static int
RMD160Update_int(void *ctx, const void *buf, u_int len)
{
	RMD160Update(ctx, buf, len);
	return 0;
}

static void
RMD160Final_int(uint8_t *digest, void *ctx)
{
	RMD160Final(digest, ctx);
}
