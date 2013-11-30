/*
 * $Id: keynames.c,v 1.1.1.2 2006-02-25 02:33:43 laffer1 Exp $
 */

#include <test.priv.h>

int
main(int argc GCC_UNUSED, char *argv[]GCC_UNUSED)
{
    int n;
    for (n = -1; n < 512; n++) {
	printf("%d(%5o):%s\n", n, n, keyname(n));
    }
    ExitProgram(EXIT_SUCCESS);
}
