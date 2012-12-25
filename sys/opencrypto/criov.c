/*      $OpenBSD: criov.c,v 1.9 2002/01/29 15:48:29 jason Exp $	*/

/*-
 * Copyright (c) 1999 Theo de Raadt
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/uio.h>

#include <opencrypto/cryptodev.h>

/*
 * This macro is only for avoiding code duplication, as we need to skip
 * given number of bytes in the same way in three functions below.
 */
#define	CUIO_SKIP()	do {						\
	KASSERT(off >= 0, ("%s: off %d < 0", __func__, off));		\
	KASSERT(len >= 0, ("%s: len %d < 0", __func__, len));		\
	while (off > 0) {						\
		KASSERT(iol >= 0, ("%s: empty in skip", __func__));	\
		if (off < iov->iov_len)					\
			break;						\
		off -= iov->iov_len;					\
		iol--;							\
		iov++;							\
	}								\
} while (0)

void
cuio_copydata(struct uio* uio, int off, int len, caddr_t cp)
{
	struct iovec *iov = uio->uio_iov;
	int iol = uio->uio_iovcnt;
	unsigned count;

	CUIO_SKIP();
	while (len > 0) {
		KASSERT(iol >= 0, ("%s: empty", __func__));
		count = min(iov->iov_len - off, len);
		bcopy(((caddr_t)iov->iov_base) + off, cp, count);
		len -= count;
		cp += count;
		off = 0;
		iol--;
		iov++;
	}
}

void
cuio_copyback(struct uio* uio, int off, int len, caddr_t cp)
{
	struct iovec *iov = uio->uio_iov;
	int iol = uio->uio_iovcnt;
	unsigned count;

	CUIO_SKIP();
	while (len > 0) {
		KASSERT(iol >= 0, ("%s: empty", __func__));
		count = min(iov->iov_len - off, len);
		bcopy(cp, ((caddr_t)iov->iov_base) + off, count);
		len -= count;
		cp += count;
		off = 0;
		iol--;
		iov++;
	}
}

/*
 * Return a pointer to iov/offset of location in iovec list.
 */
struct iovec *
cuio_getptr(struct uio *uio, int loc, int *off)
{
	struct iovec *iov = uio->uio_iov;
	int iol = uio->uio_iovcnt;

	while (loc >= 0) {
		/* Normal end of search */
		if (loc < iov->iov_len) {
	    		*off = loc;
	    		return (iov);
		}

		loc -= iov->iov_len;
		if (iol == 0) {
			if (loc == 0) {
				/* Point at the end of valid data */
				*off = iov->iov_len;
				return (iov);
			} else
				return (NULL);
		} else {
			iov++, iol--;
		}
    	}

	return (NULL);
}

/*
 * Apply function f to the data in an iovec list starting "off" bytes from
 * the beginning, continuing for "len" bytes.
 */
int
cuio_apply(struct uio *uio, int off, int len, int (*f)(void *, void *, u_int),
    void *arg)
{
	struct iovec *iov = uio->uio_iov;
	int iol = uio->uio_iovcnt;
	unsigned count;
	int rval;

	CUIO_SKIP();
	while (len > 0) {
		KASSERT(iol >= 0, ("%s: empty", __func__));
		count = min(iov->iov_len - off, len);
		rval = (*f)(arg, ((caddr_t)iov->iov_base) + off, count);
		if (rval)
			return (rval);
		len -= count;
		off = 0;
		iol--;
		iov++;
	}
	return (0);
}

void
crypto_copyback(int flags, caddr_t buf, int off, int size, caddr_t in)
{

	if ((flags & CRYPTO_F_IMBUF) != 0)
		m_copyback((struct mbuf *)buf, off, size, in);
	else if ((flags & CRYPTO_F_IOV) != 0)
		cuio_copyback((struct uio *)buf, off, size, in);
	else
		bcopy(in, buf + off, size);
}

void
crypto_copydata(int flags, caddr_t buf, int off, int size, caddr_t out)
{

	if ((flags & CRYPTO_F_IMBUF) != 0)
		m_copydata((struct mbuf *)buf, off, size, out);
	else if ((flags & CRYPTO_F_IOV) != 0)
		cuio_copydata((struct uio *)buf, off, size, out);
	else
		bcopy(buf + off, out, size);
}

int
crypto_apply(int flags, caddr_t buf, int off, int len,
    int (*f)(void *, void *, u_int), void *arg)
{
	int error;

	if ((flags & CRYPTO_F_IMBUF) != 0)
		error = m_apply((struct mbuf *)buf, off, len, f, arg);
	else if ((flags & CRYPTO_F_IOV) != 0)
		error = cuio_apply((struct uio *)buf, off, len, f, arg);
	else
		error = (*f)(arg, buf + off, len);
	return (error);
}
