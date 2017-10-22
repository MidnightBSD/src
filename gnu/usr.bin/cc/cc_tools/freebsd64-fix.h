/*
 * config/i386/freebsd64.h neglects to override the default bogus mcount
 * function name.  In order to avoid touching vendor source while gcc3.4
 * is in progress, try a minimal workaround.
 *
 * $FreeBSD: release/7.0.0/gnu/usr.bin/cc/cc_tools/freebsd64-fix.h 130324 2004-06-10 22:18:33Z peter $
 */
#undef MCOUNT_NAME
#define MCOUNT_NAME ".mcount"
