/* $FreeBSD: release/10.0.0/sys/boot/sparc64/boot1/_start.s 125717 2004-02-11 21:17:04Z ru $ */

	.text
	.globl	_start
_start:
	call	ofw_init
	 nop
	sir
