/* $FreeBSD: release/10.0.0/tools/regression/ccd/layout/a.c 109416 2003-01-17 12:23:44Z phk $ */
#include <unistd.h>

static uint32_t buf[512/4];
main()
{
	u_int u = 0;

	while (1) {
		buf[0] = u++;

		if (512 != write(1, buf, sizeof buf))
			break;
	}
	exit (0);
}
