/*-
 * Copyright (c) 2001 The FreeBSD Project.
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
 * $FreeBSD: src/include/complex.h,v 1.6 2004/08/14 18:03:21 stefanf Exp $
 */

#ifndef _COMPLEX_H
#define	_COMPLEX_H

#ifdef __GNUC__
#if __STDC_VERSION__ < 199901
#define	_Complex	__complex__
#endif
#define	_Complex_I	1.0fi
#endif

#define	complex		_Complex
#define	I		_Complex_I

#include <sys/cdefs.h>

__BEGIN_DECLS

double		cabs(double complex);
float		cabsf(float complex);
double		cimag(double complex);
float		cimagf(float complex);
long double	cimagl(long double complex);
double complex	conj(double complex);
float complex	conjf(float complex);
long double complex
		conjl(long double complex);
double		creal(double complex);
float		crealf(float complex);
long double	creall(long double complex);

__END_DECLS

#endif /* _COMPLEX_H */
