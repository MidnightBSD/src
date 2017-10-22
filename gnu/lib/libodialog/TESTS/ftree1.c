/*
 * ftree1.c
 *
 * small test-driver for new dialog functionality
 *
 * Copyright (c) 1998, Anatoly A. Orehovsky
 *
 * file ./ftree1.test with xterm widget tree from 
 * direct editres(1) dump needed !!!
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: stable/9/gnu/lib/libodialog/TESTS/ftree1.c 114603 2003-05-03 21:31:08Z obrien $");

#include <stdio.h>
#include <stdlib.h>
#include <dialog.h>

int
main(int argc, char **argv)
{
	int retval;
	unsigned char *tresult;

	init_dialog();
	retval = dialog_ftree("ftree1.test", '\t',
		"ftree dialog box example",
		"xterm widget tree from direct editres(1) dump", 
		-1, -1, 15,
                            &tresult);

	dialog_update();
	
	dialog_clear();
	
	end_dialog();

	if (!retval)
	{
 		puts(tresult);
  		free(tresult);
  	}
  	
	exit(retval);
}
