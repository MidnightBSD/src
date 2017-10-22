/* $FreeBSD: release/10.0.0/tools/regression/ccd/layout/b.c 109416 2003-01-17 12:23:44Z phk $ */

#include <unistd.h>
#include <fcntl.h>

static uint32_t buf[512/4];
main()
{
	u_int u = 0;

	while (1) {

		if (512 != read(0, buf, sizeof buf))
			break;

		printf("%u %u\n", u++, buf[0]);
	}
	exit (0);
}
