/* $FreeBSD: src/gnu/usr.bin/patch/EXTERN.h,v 1.6 1999/09/05 17:31:54 peter Exp $
 *
 * $Log: not supported by cvs2svn $
 * Revision 2.0  86/09/17  15:35:37  lwall
 * Baseline for netwide release.
 *
 */

#ifdef EXT
#undef EXT
#endif
#define EXT extern

#ifdef INIT
#undef INIT
#endif
#define INIT(x)

#ifdef DOINIT
#undef DOINIT
#endif
