/*
 * Written by Alexander Kabaev <kan@FreeBSD.org>
 * The file is in public domain.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: stable/11/lib/libssp_nonshared/libssp_nonshared.c 356356 2020-01-04 20:19:25Z kevans $");

void __stack_chk_fail(void);
void __stack_chk_fail_local(void);

void __hidden
__stack_chk_fail_local(void)
{

	__stack_chk_fail();
}
