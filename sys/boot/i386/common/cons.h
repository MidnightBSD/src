/*-
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
 *
 * $FreeBSD: release/10.0.0/sys/boot/i386/common/cons.h 213136 2010-09-24 19:49:12Z pjd $
 */

#ifndef _CONS_H_
#define	_CONS_H_

#define IO_KEYBOARD	1
#define IO_SERIAL	2

extern uint8_t ioctrl;

void putc(int c);
void xputc(int c);
void putchar(int c);
int getc(int fn);
int xgetc(int fn);
int keyhit(unsigned int secs);
void getstr(char *cmdstr, size_t cmdstrsize);

#endif	/* !_CONS_H_ */
