/* crypto/bn/bn_exp.c */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 * 
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 * 
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from 
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 * 
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
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
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */
/* ====================================================================
 * Copyright (c) 1998-2000 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */


#include "cryptlib.h"
#include "bn_lcl.h"

#define TABLE_SIZE	32

/* this one works - simple but works */
int BN_exp(BIGNUM *r, const BIGNUM *a, const BIGNUM *p, BN_CTX *ctx)
	{
	int i,bits,ret=0;
	BIGNUM *v,*rr;

	BN_CTX_start(ctx);
	if ((r == a) || (r == p))
		rr = BN_CTX_get(ctx);
	else
		rr = r;
	if ((v = BN_CTX_get(ctx)) == NULL) goto err;

	if (BN_copy(v,a) == NULL) goto err;
	bits=BN_num_bits(p);

	if (BN_is_odd(p))
		{ if (BN_copy(rr,a) == NULL) goto err; }
	else	{ if (!BN_one(rr)) goto err; }

	for (i=1; i<bits; i++)
		{
		if (!BN_sqr(v,v,ctx)) goto err;
		if (BN_is_bit_set(p,i))
			{
			if (!BN_mul(rr,rr,v,ctx)) goto err;
			}
		}
	ret=1;
err:
	if (r != rr) BN_copy(r,rr);
	BN_CTX_end(ctx);
	return(ret);
	}


int BN_mod_exp(BIGNUM *r, const BIGNUM *a, const BIGNUM *p, const BIGNUM *m,
	       BN_CTX *ctx)
	{
	int ret;

	bn_check_top(a);
	bn_check_top(p);
	bn_check_top(m);

	/* For even modulus  m = 2^k*m_odd,  it might make sense to compute
	 * a^p mod m_odd  and  a^p mod 2^k  separately (with Montgomery
	 * exponentiation for the odd part), using appropriate exponent
	 * reductions, and combine the results using the CRT.
	 *
	 * For now, we use Montgomery only if the modulus is odd; otherwise,
	 * exponentiation using the reciprocal-based quick remaindering
	 * algorithm is used.
	 *
	 * (Timing obtained with expspeed.c [computations  a^p mod m
	 * where  a, p, m  are of the same length: 256, 512, 1024, 2048,
	 * 4096, 8192 bits], compared to the running time of the
	 * standard algorithm:
	 *
	 *   BN_mod_exp_mont   33 .. 40 %  [AMD K6-2, Linux, debug configuration]
         *                     55 .. 77 %  [UltraSparc processor, but
	 *                                  debug-solaris-sparcv8-gcc conf.]
	 * 
	 *   BN_mod_exp_recp   50 .. 70 %  [AMD K6-2, Linux, debug configuration]
	 *                     62 .. 118 % [UltraSparc, debug-solaris-sparcv8-gcc]
	 *
	 * On the Sparc, BN_mod_exp_recp was faster than BN_mod_exp_mont
	 * at 2048 and more bits, but at 512 and 1024 bits, it was
	 * slower even than the standard algorithm!
	 *
	 * "Real" timings [linux-elf, solaris-sparcv9-gcc configurations]
	 * should be obtained when the new Montgomery reduction code
	 * has been integrated into OpenSSL.)
	 */

#define MONT_MUL_MOD
#define MONT_EXP_WORD
#define RECP_MUL_MOD

#ifdef MONT_MUL_MOD
	/* I have finally been able to take out this pre-condition of
	 * the top bit being set.  It was caused by an error in BN_div
	 * with negatives.  There was also another problem when for a^b%m
	 * a >= m.  eay 07-May-97 */
/*	if ((m->d[m->top-1]&BN_TBIT) && BN_is_odd(m)) */

	if (BN_is_odd(m))
		{
#  ifdef MONT_EXP_WORD
		if (a->top == 1 && !a->neg)
			{
			BN_ULONG A = a->d[0];
			ret=BN_mod_exp_mont_word(r,A,p,m,ctx,NULL);
			}
		else
#  endif
			ret=BN_mod_exp_mont(r,a,p,m,ctx,NULL);
		}
	else
#endif
#ifdef RECP_MUL_MOD
		{ ret=BN_mod_exp_recp(r,a,p,m,ctx); }
#else
		{ ret=BN_mod_exp_simple(r,a,p,m,ctx); }
#endif

	return(ret);
	}


int BN_mod_exp_recp(BIGNUM *r, const BIGNUM *a, const BIGNUM *p,
		    const BIGNUM *m, BN_CTX *ctx)
	{
	int i,j,bits,ret=0,wstart,wend,window,wvalue;
	int start=1,ts=0;
	BIGNUM *aa;
	BIGNUM val[TABLE_SIZE];
	BN_RECP_CTX recp;

	bits=BN_num_bits(p);

	if (bits == 0)
		{
		ret = BN_one(r);
		return ret;
		}

	BN_CTX_start(ctx);
	if ((aa = BN_CTX_get(ctx)) == NULL) goto err;

	BN_RECP_CTX_init(&recp);
	if (m->neg)
		{
		/* ignore sign of 'm' */
		if (!BN_copy(aa, m)) goto err;
		aa->neg = 0;
		if (BN_RECP_CTX_set(&recp,aa,ctx) <= 0) goto err;
		}
	else
		{
		if (BN_RECP_CTX_set(&recp,m,ctx) <= 0) goto err;
		}

	BN_init(&(val[0]));
	ts=1;

	if (!BN_nnmod(&(val[0]),a,m,ctx)) goto err;		/* 1 */
	if (BN_is_zero(&(val[0])))
		{
		ret = BN_zero(r);
		goto err;
		}

	window = BN_window_bits_for_exponent_size(bits);
	if (window > 1)
		{
		if (!BN_mod_mul_reciprocal(aa,&(val[0]),&(val[0]),&recp,ctx))
			goto err;				/* 2 */
		j=1<<(window-1);
		for (i=1; i<j; i++)
			{
			BN_init(&val[i]);
			if (!BN_mod_mul_reciprocal(&(val[i]),&(val[i-1]),aa,&recp,ctx))
				goto err;
			}
		ts=i;
		}
		
	start=1;	/* This is used to avoid multiplication etc
			 * when there is only the value '1' in the
			 * buffer. */
	wvalue=0;	/* The 'value' of the window */
	wstart=bits-1;	/* The top bit of the window */
	wend=0;		/* The bottom bit of the window */

	if (!BN_one(r)) goto err;

	for (;;)
		{
		if (BN_is_bit_set(p,wstart) == 0)
			{
			if (!start)
				if (!BN_mod_mul_reciprocal(r,r,r,&recp,ctx))
				goto err;
			if (wstart == 0) break;
			wstart--;
			continue;
			}
		/* We now have wstart on a 'set' bit, we now need to work out
		 * how bit a window to do.  To do this we need to scan
		 * forward until the last set bit before the end of the
		 * window */
		j=wstart;
		wvalue=1;
		wend=0;
		for (i=1; i<window; i++)
			{
			if (wstart-i < 0) break;
			if (BN_is_bit_set(p,wstart-i))
				{
				wvalue<<=(i-wend);
				wvalue|=1;
				wend=i;
				}
			}

		/* wend is the size of the current window */
		j=wend+1;
		/* add the 'bytes above' */
		if (!start)
			for (i=0; i<j; i++)
				{
				if (!BN_mod_mul_reciprocal(r,r,r,&recp,ctx))
					goto err;
				}
		
		/* wvalue will be an odd number < 2^window */
		if (!BN_mod_mul_reciprocal(r,r,&(val[wvalue>>1]),&recp,ctx))
			goto err;

		/* move the 'window' down further */
		wstart-=wend+1;
		wvalue=0;
		start=0;
		if (wstart < 0) break;
		}
	ret=1;
err:
	BN_CTX_end(ctx);
	for (i=0; i<ts; i++)
		BN_clear_free(&(val[i]));
	BN_RECP_CTX_free(&recp);
	return(ret);
	}


int BN_mod_exp_mont(BIGNUM *rr, const BIGNUM *a, const BIGNUM *p,
		    const BIGNUM *m, BN_CTX *ctx, BN_MONT_CTX *in_mont)
	{
	int i,j,bits,ret=0,wstart,wend,window,wvalue;
	int start=1,ts=0;
	BIGNUM *d,*r;
	const BIGNUM *aa;
	BIGNUM val[TABLE_SIZE];
	BN_MONT_CTX *mont=NULL;

	bn_check_top(a);
	bn_check_top(p);
	bn_check_top(m);

	if (!(m->d[0] & 1))
		{
		BNerr(BN_F_BN_MOD_EXP_MONT,BN_R_CALLED_WITH_EVEN_MODULUS);
		return(0);
		}
	bits=BN_num_bits(p);
	if (bits == 0)
		{
		ret = BN_one(rr);
		return ret;
		}

	BN_CTX_start(ctx);
	d = BN_CTX_get(ctx);
	r = BN_CTX_get(ctx);
	if (d == NULL || r == NULL) goto err;

	/* If this is not done, things will break in the montgomery
	 * part */

	if (in_mont != NULL)
		mont=in_mont;
	else
		{
		if ((mont=BN_MONT_CTX_new()) == NULL) goto err;
		if (!BN_MONT_CTX_set(mont,m,ctx)) goto err;
		}

	BN_init(&val[0]);
	ts=1;
	if (a->neg || BN_ucmp(a,m) >= 0)
		{
		if (!BN_nnmod(&(val[0]),a,m,ctx))
			goto err;
		aa= &(val[0]);
		}
	else
		aa=a;
	if (BN_is_zero(aa))
		{
		ret = BN_zero(rr);
		goto err;
		}
	if (!BN_to_montgomery(&(val[0]),aa,mont,ctx)) goto err; /* 1 */

	window = BN_window_bits_for_exponent_size(bits);
	if (window > 1)
		{
		if (!BN_mod_mul_montgomery(d,&(val[0]),&(val[0]),mont,ctx)) goto err; /* 2 */
		j=1<<(window-1);
		for (i=1; i<j; i++)
			{
			BN_init(&(val[i]));
			if (!BN_mod_mul_montgomery(&(val[i]),&(val[i-1]),d,mont,ctx))
				goto err;
			}
		ts=i;
		}

	start=1;	/* This is used to avoid multiplication etc
			 * when there is only the value '1' in the
			 * buffer. */
	wvalue=0;	/* The 'value' of the window */
	wstart=bits-1;	/* The top bit of the window */
	wend=0;		/* The bottom bit of the window */

	if (!BN_to_montgomery(r,BN_value_one(),mont,ctx)) goto err;
	for (;;)
		{
		if (BN_is_bit_set(p,wstart) == 0)
			{
			if (!start)
				{
				if (!BN_mod_mul_montgomery(r,r,r,mont,ctx))
				goto err;
				}
			if (wstart == 0) break;
			wstart--;
			continue;
			}
		/* We now have wstart on a 'set' bit, we now need to work out
		 * how bit a window to do.  To do this we need to scan
		 * forward until the last set bit before the end of the
		 * window */
		j=wstart;
		wvalue=1;
		wend=0;
		for (i=1; i<window; i++)
			{
			if (wstart-i < 0) break;
			if (BN_is_bit_set(p,wstart-i))
				{
				wvalue<<=(i-wend);
				wvalue|=1;
				wend=i;
				}
			}

		/* wend is the size of the current window */
		j=wend+1;
		/* add the 'bytes above' */
		if (!start)
			for (i=0; i<j; i++)
				{
				if (!BN_mod_mul_montgomery(r,r,r,mont,ctx))
					goto err;
				}
		
		/* wvalue will be an odd number < 2^window */
		if (!BN_mod_mul_montgomery(r,r,&(val[wvalue>>1]),mont,ctx))
			goto err;

		/* move the 'window' down further */
		wstart-=wend+1;
		wvalue=0;
		start=0;
		if (wstart < 0) break;
		}
	if (!BN_from_montgomery(rr,r,mont,ctx)) goto err;
	ret=1;
err:
	if ((in_mont == NULL) && (mont != NULL)) BN_MONT_CTX_free(mont);
	BN_CTX_end(ctx);
	for (i=0; i<ts; i++)
		BN_clear_free(&(val[i]));
	return(ret);
	}

int BN_mod_exp_mont_word(BIGNUM *rr, BN_ULONG a, const BIGNUM *p,
                         const BIGNUM *m, BN_CTX *ctx, BN_MONT_CTX *in_mont)
	{
	BN_MONT_CTX *mont = NULL;
	int b, bits, ret=0;
	int r_is_one;
	BN_ULONG w, next_w;
	BIGNUM *d, *r, *t;
	BIGNUM *swap_tmp;
#define BN_MOD_MUL_WORD(r, w, m) \
		(BN_mul_word(r, (w)) && \
		(/* BN_ucmp(r, (m)) < 0 ? 1 :*/  \
			(BN_mod(t, r, m, ctx) && (swap_tmp = r, r = t, t = swap_tmp, 1))))
		/* BN_MOD_MUL_WORD is only used with 'w' large,
		 * so the BN_ucmp test is probably more overhead
		 * than always using BN_mod (which uses BN_copy if
		 * a similar test returns true). */
		/* We can use BN_mod and do not need BN_nnmod because our
		 * accumulator is never negative (the result of BN_mod does
		 * not depend on the sign of the modulus).
		 */
#define BN_TO_MONTGOMERY_WORD(r, w, mont) \
		(BN_set_word(r, (w)) && BN_to_montgomery(r, r, (mont), ctx))

	bn_check_top(p);
	bn_check_top(m);

	if (m->top == 0 || !(m->d[0] & 1))
		{
		BNerr(BN_F_BN_MOD_EXP_MONT_WORD,BN_R_CALLED_WITH_EVEN_MODULUS);
		return(0);
		}
	if (m->top == 1)
		a %= m->d[0]; /* make sure that 'a' is reduced */

	bits = BN_num_bits(p);
	if (bits == 0)
		{
		ret = BN_one(rr);
		return ret;
		}
	if (a == 0)
		{
		ret = BN_zero(rr);
		return ret;
		}

	BN_CTX_start(ctx);
	d = BN_CTX_get(ctx);
	r = BN_CTX_get(ctx);
	t = BN_CTX_get(ctx);
	if (d == NULL || r == NULL || t == NULL) goto err;

	if (in_mont != NULL)
		mont=in_mont;
	else
		{
		if ((mont = BN_MONT_CTX_new()) == NULL) goto err;
		if (!BN_MONT_CTX_set(mont, m, ctx)) goto err;
		}

	r_is_one = 1; /* except for Montgomery factor */

	/* bits-1 >= 0 */

	/* The result is accumulated in the product r*w. */
	w = a; /* bit 'bits-1' of 'p' is always set */
	for (b = bits-2; b >= 0; b--)
		{
		/* First, square r*w. */
		next_w = w*w;
		if ((next_w/w) != w) /* overflow */
			{
			if (r_is_one)
				{
				if (!BN_TO_MONTGOMERY_WORD(r, w, mont)) goto err;
				r_is_one = 0;
				}
			else
				{
				if (!BN_MOD_MUL_WORD(r, w, m)) goto err;
				}
			next_w = 1;
			}
		w = next_w;
		if (!r_is_one)
			{
			if (!BN_mod_mul_montgomery(r, r, r, mont, ctx)) goto err;
			}

		/* Second, multiply r*w by 'a' if exponent bit is set. */
		if (BN_is_bit_set(p, b))
			{
			next_w = w*a;
			if ((next_w/a) != w) /* overflow */
				{
				if (r_is_one)
					{
					if (!BN_TO_MONTGOMERY_WORD(r, w, mont)) goto err;
					r_is_one = 0;
					}
				else
					{
					if (!BN_MOD_MUL_WORD(r, w, m)) goto err;
					}
				next_w = a;
				}
			w = next_w;
			}
		}

	/* Finally, set r:=r*w. */
	if (w != 1)
		{
		if (r_is_one)
			{
			if (!BN_TO_MONTGOMERY_WORD(r, w, mont)) goto err;
			r_is_one = 0;
			}
		else
			{
			if (!BN_MOD_MUL_WORD(r, w, m)) goto err;
			}
		}

	if (r_is_one) /* can happen only if a == 1*/
		{
		if (!BN_one(rr)) goto err;
		}
	else
		{
		if (!BN_from_montgomery(rr, r, mont, ctx)) goto err;
		}
	ret = 1;
err:
	if ((in_mont == NULL) && (mont != NULL)) BN_MONT_CTX_free(mont);
	BN_CTX_end(ctx);
	return(ret);
	}


/* The old fallback, simple version :-) */
int BN_mod_exp_simple(BIGNUM *r,
	const BIGNUM *a, const BIGNUM *p, const BIGNUM *m,
	BN_CTX *ctx)
	{
	int i,j,bits,ret=0,wstart,wend,window,wvalue,ts=0;
	int start=1;
	BIGNUM *d;
	BIGNUM val[TABLE_SIZE];

	bits=BN_num_bits(p);

	if (bits == 0)
		{
		ret = BN_one(r);
		return ret;
		}

	BN_CTX_start(ctx);
	if ((d = BN_CTX_get(ctx)) == NULL) goto err;

	BN_init(&(val[0]));
	ts=1;
	if (!BN_nnmod(&(val[0]),a,m,ctx)) goto err;		/* 1 */
	if (BN_is_zero(&(val[0])))
		{
		ret = BN_zero(r);
		goto err;
		}

	window = BN_window_bits_for_exponent_size(bits);
	if (window > 1)
		{
		if (!BN_mod_mul(d,&(val[0]),&(val[0]),m,ctx))
			goto err;				/* 2 */
		j=1<<(window-1);
		for (i=1; i<j; i++)
			{
			BN_init(&(val[i]));
			if (!BN_mod_mul(&(val[i]),&(val[i-1]),d,m,ctx))
				goto err;
			}
		ts=i;
		}

	start=1;	/* This is used to avoid multiplication etc
			 * when there is only the value '1' in the
			 * buffer. */
	wvalue=0;	/* The 'value' of the window */
	wstart=bits-1;	/* The top bit of the window */
	wend=0;		/* The bottom bit of the window */

	if (!BN_one(r)) goto err;

	for (;;)
		{
		if (BN_is_bit_set(p,wstart) == 0)
			{
			if (!start)
				if (!BN_mod_mul(r,r,r,m,ctx))
				goto err;
			if (wstart == 0) break;
			wstart--;
			continue;
			}
		/* We now have wstart on a 'set' bit, we now need to work out
		 * how bit a window to do.  To do this we need to scan
		 * forward until the last set bit before the end of the
		 * window */
		j=wstart;
		wvalue=1;
		wend=0;
		for (i=1; i<window; i++)
			{
			if (wstart-i < 0) break;
			if (BN_is_bit_set(p,wstart-i))
				{
				wvalue<<=(i-wend);
				wvalue|=1;
				wend=i;
				}
			}

		/* wend is the size of the current window */
		j=wend+1;
		/* add the 'bytes above' */
		if (!start)
			for (i=0; i<j; i++)
				{
				if (!BN_mod_mul(r,r,r,m,ctx))
					goto err;
				}
		
		/* wvalue will be an odd number < 2^window */
		if (!BN_mod_mul(r,r,&(val[wvalue>>1]),m,ctx))
			goto err;

		/* move the 'window' down further */
		wstart-=wend+1;
		wvalue=0;
		start=0;
		if (wstart < 0) break;
		}
	ret=1;
err:
	BN_CTX_end(ctx);
	for (i=0; i<ts; i++)
		BN_clear_free(&(val[i]));
	return(ret);
	}

