/* $FreeBSD: release/10.0.0/tools/tools/ncpus/ncpus.c 156362 2006-03-06 21:51:27Z sam $ */

#include <stdio.h>

extern int acpi_detect(void);
extern int biosmptable_detect(void);

int
main(void)
{
	printf("acpi: %d\n", acpi_detect());
#if defined(__amd64__) || defined(__i386__)
	printf("mptable: %d\n", biosmptable_detect());
#endif
	return 0;
}
