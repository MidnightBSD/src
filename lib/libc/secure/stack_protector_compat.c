/*
 * Written by Alexander Kabaev <kan@FreeBSD.org>
 * The file is in public domain.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: stable/11/lib/libc/secure/stack_protector_compat.c 286760 2015-08-14 03:03:13Z pfg $");

void __stack_chk_fail(void);

#ifdef PIC
void
__stack_chk_fail_local_hidden(void)
{

	__stack_chk_fail();
}

__sym_compat(__stack_chk_fail_local, __stack_chk_fail_local_hidden, FBSD_1.0);
#endif
