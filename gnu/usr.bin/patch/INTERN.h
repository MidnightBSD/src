/* $FreeBSD: stable/9/gnu/usr.bin/patch/INTERN.h 50965 1999-09-05 17:31:55Z peter $
 *
 * $Log: INTERN.h,v $
 * Revision 2.0  86/09/17  15:35:58  lwall
 * Baseline for netwide release.
 *
 */

#ifdef EXT
#undef EXT
#endif
#define EXT

#ifdef INIT
#undef INIT
#endif
#define INIT(x) = x

#define DOINIT
