/* $FreeBSD: stable/11/sys/dev/videomode/test.c 278798 2015-02-15 11:37:40Z hselasky $ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "videomode.h"

int
main(int argc, char **argv)
{
	int	i, j;

	for (i = 1; i < argc ; i++) {
		for (j = 0; j < videomode_count; j++) {
			if (strcmp(videomode_list[j].name, argv[i]) == 0) {
				printf("dotclock for mode %s = %d, flags %x\n",
				    argv[i],
				    videomode_list[j].dot_clock,
				    videomode_list[j].flags);
				break;
			}
		}
		if (j == videomode_count) {
			printf("dotclock for mode %s not found\n", argv[i]);
		}
	}
}
