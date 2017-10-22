/*
 * Copyright (c) 1998 Robert Nordier
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are freely
 * permitted provided that the above copyright notice and this
 * paragraph and the following disclaimer are duplicated in all
 * such forms.
 *
 * This software is provided "AS IS" and without any express or
 * implied warranties, including, without limitation, the implied
 * warranties of merchantability and fitness for a particular
 * purpose.
 */

/*
 * $FreeBSD: release/7.0.0/sys/boot/i386/boot2/lib.h 149212 2005-08-18 00:42:45Z iedowse $
 */

void sio_init(int);
void sio_flush(void);
void sio_putc(int);
int sio_getc(void);
int sio_ischar(void);
