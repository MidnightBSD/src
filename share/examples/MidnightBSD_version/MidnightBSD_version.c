#include <stdio.h>
#include <osreldate.h>

extern int getosreldate(void);

int main(void) {
	printf("Compilation release date: %d\n", __MidnightBSD_version);
	printf("Execution environment release date: %d\n", getosreldate());

	return 0;
}
