/* $FreeBSD: src/sys/boot/sparc64/boot1/_start.s,v 1.2 2004/02/11 21:17:04 ru Exp $ */

	.text
	.globl	_start
_start:
	call	ofw_init
	 nop
	sir
