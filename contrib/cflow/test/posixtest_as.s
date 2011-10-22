	.file	"posixtest.c"
	.text
	.p2align 4,,15
.globl main
	.type	main, @function
main:
	leal	4(%esp), %ecx
	andl	$-16, %esp
	pushl	-4(%ecx)
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ecx
	subl	$4, %esp
	call	f
	call	g
	call	f
	addl	$4, %esp
	popl	%ecx
	popl	%ebp
	leal	-4(%ecx), %esp
	ret
	.size	main, .-main
	.p2align 4,,15
.globl f
	.type	f, @function
f:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$8, %esp
	call	h
	movl	%eax, i
	leave
	ret
	.size	f, .-f
	.comm	i,4,4
	.ident	"GCC: (GNU) 4.2.1 20070719  [FreeBSD]"
