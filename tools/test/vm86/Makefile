# $FreeBSD: stable/11/tools/test/vm86/Makefile 333534 2018-05-12 11:53:49Z kib $

all:	vm86_test

vm86_test:	vm86_test_asm.s vm86_test.c
	$(CC) -Wall -Wextra -g -m32 -O -o vm86_test vm86_test_asm.s vm86_test.c

