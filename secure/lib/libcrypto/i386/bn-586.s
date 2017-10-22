	# $FreeBSD$






	.file	"/usr/src/secure/lib/libcrypto/../../../crypto/openssl/crypto/bn/asm/bn-586.s"
	.version	"01.01"
gcc2_compiled.:
.text
	.align 16
.globl bn_mul_add_words
	.type	bn_mul_add_words,@function
bn_mul_add_words:
	pushl	%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi


	xorl	%esi,		%esi
	movl	20(%esp),	%edi
	movl	28(%esp),	%ecx
	movl	24(%esp),	%ebx
	andl	$4294967288,	%ecx
	movl	32(%esp),	%ebp
	pushl	%ecx
	jz	.L000maw_finish
.L001maw_loop:
	movl	%ecx,		(%esp)

	movl	(%ebx),		%eax
	mull	%ebp
	addl	%esi,		%eax
	movl	(%edi),		%esi
	adcl	$0,		%edx
	addl	%esi,		%eax
	adcl	$0,		%edx
	movl	%eax,		(%edi)
	movl	%edx,		%esi

	movl	4(%ebx),	%eax
	mull	%ebp
	addl	%esi,		%eax
	movl	4(%edi),	%esi
	adcl	$0,		%edx
	addl	%esi,		%eax
	adcl	$0,		%edx
	movl	%eax,		4(%edi)
	movl	%edx,		%esi

	movl	8(%ebx),	%eax
	mull	%ebp
	addl	%esi,		%eax
	movl	8(%edi),	%esi
	adcl	$0,		%edx
	addl	%esi,		%eax
	adcl	$0,		%edx
	movl	%eax,		8(%edi)
	movl	%edx,		%esi

	movl	12(%ebx),	%eax
	mull	%ebp
	addl	%esi,		%eax
	movl	12(%edi),	%esi
	adcl	$0,		%edx
	addl	%esi,		%eax
	adcl	$0,		%edx
	movl	%eax,		12(%edi)
	movl	%edx,		%esi

	movl	16(%ebx),	%eax
	mull	%ebp
	addl	%esi,		%eax
	movl	16(%edi),	%esi
	adcl	$0,		%edx
	addl	%esi,		%eax
	adcl	$0,		%edx
	movl	%eax,		16(%edi)
	movl	%edx,		%esi

	movl	20(%ebx),	%eax
	mull	%ebp
	addl	%esi,		%eax
	movl	20(%edi),	%esi
	adcl	$0,		%edx
	addl	%esi,		%eax
	adcl	$0,		%edx
	movl	%eax,		20(%edi)
	movl	%edx,		%esi

	movl	24(%ebx),	%eax
	mull	%ebp
	addl	%esi,		%eax
	movl	24(%edi),	%esi
	adcl	$0,		%edx
	addl	%esi,		%eax
	adcl	$0,		%edx
	movl	%eax,		24(%edi)
	movl	%edx,		%esi

	movl	28(%ebx),	%eax
	mull	%ebp
	addl	%esi,		%eax
	movl	28(%edi),	%esi
	adcl	$0,		%edx
	addl	%esi,		%eax
	adcl	$0,		%edx
	movl	%eax,		28(%edi)
	movl	%edx,		%esi

	movl	(%esp),		%ecx
	addl	$32,		%ebx
	addl	$32,		%edi
	subl	$8,		%ecx
	jnz	.L001maw_loop
.L000maw_finish:
	movl	32(%esp),	%ecx
	andl	$7,		%ecx
	jnz	.L002maw_finish2
	jmp	.L003maw_end
.align 16
.L002maw_finish2:

	movl	(%ebx),		%eax
	mull	%ebp
	addl	%esi,		%eax
	movl	(%edi),		%esi
	adcl	$0,		%edx
	addl	%esi,		%eax
	adcl	$0,		%edx
	decl	%ecx
	movl	%eax,		(%edi)
	movl	%edx,		%esi
	jz	.L003maw_end

	movl	4(%ebx),	%eax
	mull	%ebp
	addl	%esi,		%eax
	movl	4(%edi),	%esi
	adcl	$0,		%edx
	addl	%esi,		%eax
	adcl	$0,		%edx
	decl	%ecx
	movl	%eax,		4(%edi)
	movl	%edx,		%esi
	jz	.L003maw_end

	movl	8(%ebx),	%eax
	mull	%ebp
	addl	%esi,		%eax
	movl	8(%edi),	%esi
	adcl	$0,		%edx
	addl	%esi,		%eax
	adcl	$0,		%edx
	decl	%ecx
	movl	%eax,		8(%edi)
	movl	%edx,		%esi
	jz	.L003maw_end

	movl	12(%ebx),	%eax
	mull	%ebp
	addl	%esi,		%eax
	movl	12(%edi),	%esi
	adcl	$0,		%edx
	addl	%esi,		%eax
	adcl	$0,		%edx
	decl	%ecx
	movl	%eax,		12(%edi)
	movl	%edx,		%esi
	jz	.L003maw_end

	movl	16(%ebx),	%eax
	mull	%ebp
	addl	%esi,		%eax
	movl	16(%edi),	%esi
	adcl	$0,		%edx
	addl	%esi,		%eax
	adcl	$0,		%edx
	decl	%ecx
	movl	%eax,		16(%edi)
	movl	%edx,		%esi
	jz	.L003maw_end

	movl	20(%ebx),	%eax
	mull	%ebp
	addl	%esi,		%eax
	movl	20(%edi),	%esi
	adcl	$0,		%edx
	addl	%esi,		%eax
	adcl	$0,		%edx
	decl	%ecx
	movl	%eax,		20(%edi)
	movl	%edx,		%esi
	jz	.L003maw_end

	movl	24(%ebx),	%eax
	mull	%ebp
	addl	%esi,		%eax
	movl	24(%edi),	%esi
	adcl	$0,		%edx
	addl	%esi,		%eax
	adcl	$0,		%edx
	movl	%eax,		24(%edi)
	movl	%edx,		%esi
.L003maw_end:
	movl	%esi,		%eax
	popl	%ecx
	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret
.L_bn_mul_add_words_end:
	.size	bn_mul_add_words,.L_bn_mul_add_words_end-bn_mul_add_words
.ident	"bn_mul_add_words"
.text
	.align 16
.globl bn_mul_words
	.type	bn_mul_words,@function
bn_mul_words:
	pushl	%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi


	xorl	%esi,		%esi
	movl	20(%esp),	%edi
	movl	24(%esp),	%ebx
	movl	28(%esp),	%ebp
	movl	32(%esp),	%ecx
	andl	$4294967288,	%ebp
	jz	.L004mw_finish
.L005mw_loop:

	movl	(%ebx),		%eax
	mull	%ecx
	addl	%esi,		%eax
	adcl	$0,		%edx
	movl	%eax,		(%edi)
	movl	%edx,		%esi

	movl	4(%ebx),	%eax
	mull	%ecx
	addl	%esi,		%eax
	adcl	$0,		%edx
	movl	%eax,		4(%edi)
	movl	%edx,		%esi

	movl	8(%ebx),	%eax
	mull	%ecx
	addl	%esi,		%eax
	adcl	$0,		%edx
	movl	%eax,		8(%edi)
	movl	%edx,		%esi

	movl	12(%ebx),	%eax
	mull	%ecx
	addl	%esi,		%eax
	adcl	$0,		%edx
	movl	%eax,		12(%edi)
	movl	%edx,		%esi

	movl	16(%ebx),	%eax
	mull	%ecx
	addl	%esi,		%eax
	adcl	$0,		%edx
	movl	%eax,		16(%edi)
	movl	%edx,		%esi

	movl	20(%ebx),	%eax
	mull	%ecx
	addl	%esi,		%eax
	adcl	$0,		%edx
	movl	%eax,		20(%edi)
	movl	%edx,		%esi

	movl	24(%ebx),	%eax
	mull	%ecx
	addl	%esi,		%eax
	adcl	$0,		%edx
	movl	%eax,		24(%edi)
	movl	%edx,		%esi

	movl	28(%ebx),	%eax
	mull	%ecx
	addl	%esi,		%eax
	adcl	$0,		%edx
	movl	%eax,		28(%edi)
	movl	%edx,		%esi

	addl	$32,		%ebx
	addl	$32,		%edi
	subl	$8,		%ebp
	jz	.L004mw_finish
	jmp	.L005mw_loop
.L004mw_finish:
	movl	28(%esp),	%ebp
	andl	$7,		%ebp
	jnz	.L006mw_finish2
	jmp	.L007mw_end
.align 16
.L006mw_finish2:

	movl	(%ebx),		%eax
	mull	%ecx
	addl	%esi,		%eax
	adcl	$0,		%edx
	movl	%eax,		(%edi)
	movl	%edx,		%esi
	decl	%ebp
	jz	.L007mw_end

	movl	4(%ebx),	%eax
	mull	%ecx
	addl	%esi,		%eax
	adcl	$0,		%edx
	movl	%eax,		4(%edi)
	movl	%edx,		%esi
	decl	%ebp
	jz	.L007mw_end

	movl	8(%ebx),	%eax
	mull	%ecx
	addl	%esi,		%eax
	adcl	$0,		%edx
	movl	%eax,		8(%edi)
	movl	%edx,		%esi
	decl	%ebp
	jz	.L007mw_end

	movl	12(%ebx),	%eax
	mull	%ecx
	addl	%esi,		%eax
	adcl	$0,		%edx
	movl	%eax,		12(%edi)
	movl	%edx,		%esi
	decl	%ebp
	jz	.L007mw_end

	movl	16(%ebx),	%eax
	mull	%ecx
	addl	%esi,		%eax
	adcl	$0,		%edx
	movl	%eax,		16(%edi)
	movl	%edx,		%esi
	decl	%ebp
	jz	.L007mw_end

	movl	20(%ebx),	%eax
	mull	%ecx
	addl	%esi,		%eax
	adcl	$0,		%edx
	movl	%eax,		20(%edi)
	movl	%edx,		%esi
	decl	%ebp
	jz	.L007mw_end

	movl	24(%ebx),	%eax
	mull	%ecx
	addl	%esi,		%eax
	adcl	$0,		%edx
	movl	%eax,		24(%edi)
	movl	%edx,		%esi
.L007mw_end:
	movl	%esi,		%eax
	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret
.L_bn_mul_words_end:
	.size	bn_mul_words,.L_bn_mul_words_end-bn_mul_words
.ident	"bn_mul_words"
.text
	.align 16
.globl bn_sqr_words
	.type	bn_sqr_words,@function
bn_sqr_words:
	pushl	%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi


	movl	20(%esp),	%esi
	movl	24(%esp),	%edi
	movl	28(%esp),	%ebx
	andl	$4294967288,	%ebx
	jz	.L008sw_finish
.L009sw_loop:

	movl	(%edi),		%eax
	mull	%eax
	movl	%eax,		(%esi)
	movl	%edx,		4(%esi)

	movl	4(%edi),	%eax
	mull	%eax
	movl	%eax,		8(%esi)
	movl	%edx,		12(%esi)

	movl	8(%edi),	%eax
	mull	%eax
	movl	%eax,		16(%esi)
	movl	%edx,		20(%esi)

	movl	12(%edi),	%eax
	mull	%eax
	movl	%eax,		24(%esi)
	movl	%edx,		28(%esi)

	movl	16(%edi),	%eax
	mull	%eax
	movl	%eax,		32(%esi)
	movl	%edx,		36(%esi)

	movl	20(%edi),	%eax
	mull	%eax
	movl	%eax,		40(%esi)
	movl	%edx,		44(%esi)

	movl	24(%edi),	%eax
	mull	%eax
	movl	%eax,		48(%esi)
	movl	%edx,		52(%esi)

	movl	28(%edi),	%eax
	mull	%eax
	movl	%eax,		56(%esi)
	movl	%edx,		60(%esi)

	addl	$32,		%edi
	addl	$64,		%esi
	subl	$8,		%ebx
	jnz	.L009sw_loop
.L008sw_finish:
	movl	28(%esp),	%ebx
	andl	$7,		%ebx
	jz	.L010sw_end

	movl	(%edi),		%eax
	mull	%eax
	movl	%eax,		(%esi)
	decl	%ebx
	movl	%edx,		4(%esi)
	jz	.L010sw_end

	movl	4(%edi),	%eax
	mull	%eax
	movl	%eax,		8(%esi)
	decl	%ebx
	movl	%edx,		12(%esi)
	jz	.L010sw_end

	movl	8(%edi),	%eax
	mull	%eax
	movl	%eax,		16(%esi)
	decl	%ebx
	movl	%edx,		20(%esi)
	jz	.L010sw_end

	movl	12(%edi),	%eax
	mull	%eax
	movl	%eax,		24(%esi)
	decl	%ebx
	movl	%edx,		28(%esi)
	jz	.L010sw_end

	movl	16(%edi),	%eax
	mull	%eax
	movl	%eax,		32(%esi)
	decl	%ebx
	movl	%edx,		36(%esi)
	jz	.L010sw_end

	movl	20(%edi),	%eax
	mull	%eax
	movl	%eax,		40(%esi)
	decl	%ebx
	movl	%edx,		44(%esi)
	jz	.L010sw_end

	movl	24(%edi),	%eax
	mull	%eax
	movl	%eax,		48(%esi)
	movl	%edx,		52(%esi)
.L010sw_end:
	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret
.L_bn_sqr_words_end:
	.size	bn_sqr_words,.L_bn_sqr_words_end-bn_sqr_words
.ident	"bn_sqr_words"
.text
	.align 16
.globl bn_div_words
	.type	bn_div_words,@function
bn_div_words:
	pushl	%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi

	movl	20(%esp),	%edx
	movl	24(%esp),	%eax
	movl	28(%esp),	%ebx
	divl	%ebx
	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret
.L_bn_div_words_end:
	.size	bn_div_words,.L_bn_div_words_end-bn_div_words
.ident	"bn_div_words"
.text
	.align 16
.globl bn_add_words
	.type	bn_add_words,@function
bn_add_words:
	pushl	%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi


	movl	20(%esp),	%ebx
	movl	24(%esp),	%esi
	movl	28(%esp),	%edi
	movl	32(%esp),	%ebp
	xorl	%eax,		%eax
	andl	$4294967288,	%ebp
	jz	.L011aw_finish
.L012aw_loop:

	movl	(%esi),		%ecx
	movl	(%edi),		%edx
	addl	%eax,		%ecx
	movl	$0,		%eax
	adcl	%eax,		%eax
	addl	%edx,		%ecx
	adcl	$0,		%eax
	movl	%ecx,		(%ebx)

	movl	4(%esi),	%ecx
	movl	4(%edi),	%edx
	addl	%eax,		%ecx
	movl	$0,		%eax
	adcl	%eax,		%eax
	addl	%edx,		%ecx
	adcl	$0,		%eax
	movl	%ecx,		4(%ebx)

	movl	8(%esi),	%ecx
	movl	8(%edi),	%edx
	addl	%eax,		%ecx
	movl	$0,		%eax
	adcl	%eax,		%eax
	addl	%edx,		%ecx
	adcl	$0,		%eax
	movl	%ecx,		8(%ebx)

	movl	12(%esi),	%ecx
	movl	12(%edi),	%edx
	addl	%eax,		%ecx
	movl	$0,		%eax
	adcl	%eax,		%eax
	addl	%edx,		%ecx
	adcl	$0,		%eax
	movl	%ecx,		12(%ebx)

	movl	16(%esi),	%ecx
	movl	16(%edi),	%edx
	addl	%eax,		%ecx
	movl	$0,		%eax
	adcl	%eax,		%eax
	addl	%edx,		%ecx
	adcl	$0,		%eax
	movl	%ecx,		16(%ebx)

	movl	20(%esi),	%ecx
	movl	20(%edi),	%edx
	addl	%eax,		%ecx
	movl	$0,		%eax
	adcl	%eax,		%eax
	addl	%edx,		%ecx
	adcl	$0,		%eax
	movl	%ecx,		20(%ebx)

	movl	24(%esi),	%ecx
	movl	24(%edi),	%edx
	addl	%eax,		%ecx
	movl	$0,		%eax
	adcl	%eax,		%eax
	addl	%edx,		%ecx
	adcl	$0,		%eax
	movl	%ecx,		24(%ebx)

	movl	28(%esi),	%ecx
	movl	28(%edi),	%edx
	addl	%eax,		%ecx
	movl	$0,		%eax
	adcl	%eax,		%eax
	addl	%edx,		%ecx
	adcl	$0,		%eax
	movl	%ecx,		28(%ebx)

	addl	$32,		%esi
	addl	$32,		%edi
	addl	$32,		%ebx
	subl	$8,		%ebp
	jnz	.L012aw_loop
.L011aw_finish:
	movl	32(%esp),	%ebp
	andl	$7,		%ebp
	jz	.L013aw_end

	movl	(%esi),		%ecx
	movl	(%edi),		%edx
	addl	%eax,		%ecx
	movl	$0,		%eax
	adcl	%eax,		%eax
	addl	%edx,		%ecx
	adcl	$0,		%eax
	decl	%ebp
	movl	%ecx,		(%ebx)
	jz	.L013aw_end

	movl	4(%esi),	%ecx
	movl	4(%edi),	%edx
	addl	%eax,		%ecx
	movl	$0,		%eax
	adcl	%eax,		%eax
	addl	%edx,		%ecx
	adcl	$0,		%eax
	decl	%ebp
	movl	%ecx,		4(%ebx)
	jz	.L013aw_end

	movl	8(%esi),	%ecx
	movl	8(%edi),	%edx
	addl	%eax,		%ecx
	movl	$0,		%eax
	adcl	%eax,		%eax
	addl	%edx,		%ecx
	adcl	$0,		%eax
	decl	%ebp
	movl	%ecx,		8(%ebx)
	jz	.L013aw_end

	movl	12(%esi),	%ecx
	movl	12(%edi),	%edx
	addl	%eax,		%ecx
	movl	$0,		%eax
	adcl	%eax,		%eax
	addl	%edx,		%ecx
	adcl	$0,		%eax
	decl	%ebp
	movl	%ecx,		12(%ebx)
	jz	.L013aw_end

	movl	16(%esi),	%ecx
	movl	16(%edi),	%edx
	addl	%eax,		%ecx
	movl	$0,		%eax
	adcl	%eax,		%eax
	addl	%edx,		%ecx
	adcl	$0,		%eax
	decl	%ebp
	movl	%ecx,		16(%ebx)
	jz	.L013aw_end

	movl	20(%esi),	%ecx
	movl	20(%edi),	%edx
	addl	%eax,		%ecx
	movl	$0,		%eax
	adcl	%eax,		%eax
	addl	%edx,		%ecx
	adcl	$0,		%eax
	decl	%ebp
	movl	%ecx,		20(%ebx)
	jz	.L013aw_end

	movl	24(%esi),	%ecx
	movl	24(%edi),	%edx
	addl	%eax,		%ecx
	movl	$0,		%eax
	adcl	%eax,		%eax
	addl	%edx,		%ecx
	adcl	$0,		%eax
	movl	%ecx,		24(%ebx)
.L013aw_end:
	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret
.L_bn_add_words_end:
	.size	bn_add_words,.L_bn_add_words_end-bn_add_words
.ident	"bn_add_words"
.text
	.align 16
.globl bn_sub_words
	.type	bn_sub_words,@function
bn_sub_words:
	pushl	%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi


	movl	20(%esp),	%ebx
	movl	24(%esp),	%esi
	movl	28(%esp),	%edi
	movl	32(%esp),	%ebp
	xorl	%eax,		%eax
	andl	$4294967288,	%ebp
	jz	.L014aw_finish
.L015aw_loop:

	movl	(%esi),		%ecx
	movl	(%edi),		%edx
	subl	%eax,		%ecx
	movl	$0,		%eax
	adcl	%eax,		%eax
	subl	%edx,		%ecx
	adcl	$0,		%eax
	movl	%ecx,		(%ebx)

	movl	4(%esi),	%ecx
	movl	4(%edi),	%edx
	subl	%eax,		%ecx
	movl	$0,		%eax
	adcl	%eax,		%eax
	subl	%edx,		%ecx
	adcl	$0,		%eax
	movl	%ecx,		4(%ebx)

	movl	8(%esi),	%ecx
	movl	8(%edi),	%edx
	subl	%eax,		%ecx
	movl	$0,		%eax
	adcl	%eax,		%eax
	subl	%edx,		%ecx
	adcl	$0,		%eax
	movl	%ecx,		8(%ebx)

	movl	12(%esi),	%ecx
	movl	12(%edi),	%edx
	subl	%eax,		%ecx
	movl	$0,		%eax
	adcl	%eax,		%eax
	subl	%edx,		%ecx
	adcl	$0,		%eax
	movl	%ecx,		12(%ebx)

	movl	16(%esi),	%ecx
	movl	16(%edi),	%edx
	subl	%eax,		%ecx
	movl	$0,		%eax
	adcl	%eax,		%eax
	subl	%edx,		%ecx
	adcl	$0,		%eax
	movl	%ecx,		16(%ebx)

	movl	20(%esi),	%ecx
	movl	20(%edi),	%edx
	subl	%eax,		%ecx
	movl	$0,		%eax
	adcl	%eax,		%eax
	subl	%edx,		%ecx
	adcl	$0,		%eax
	movl	%ecx,		20(%ebx)

	movl	24(%esi),	%ecx
	movl	24(%edi),	%edx
	subl	%eax,		%ecx
	movl	$0,		%eax
	adcl	%eax,		%eax
	subl	%edx,		%ecx
	adcl	$0,		%eax
	movl	%ecx,		24(%ebx)

	movl	28(%esi),	%ecx
	movl	28(%edi),	%edx
	subl	%eax,		%ecx
	movl	$0,		%eax
	adcl	%eax,		%eax
	subl	%edx,		%ecx
	adcl	$0,		%eax
	movl	%ecx,		28(%ebx)

	addl	$32,		%esi
	addl	$32,		%edi
	addl	$32,		%ebx
	subl	$8,		%ebp
	jnz	.L015aw_loop
.L014aw_finish:
	movl	32(%esp),	%ebp
	andl	$7,		%ebp
	jz	.L016aw_end

	movl	(%esi),		%ecx
	movl	(%edi),		%edx
	subl	%eax,		%ecx
	movl	$0,		%eax
	adcl	%eax,		%eax
	subl	%edx,		%ecx
	adcl	$0,		%eax
	decl	%ebp
	movl	%ecx,		(%ebx)
	jz	.L016aw_end

	movl	4(%esi),	%ecx
	movl	4(%edi),	%edx
	subl	%eax,		%ecx
	movl	$0,		%eax
	adcl	%eax,		%eax
	subl	%edx,		%ecx
	adcl	$0,		%eax
	decl	%ebp
	movl	%ecx,		4(%ebx)
	jz	.L016aw_end

	movl	8(%esi),	%ecx
	movl	8(%edi),	%edx
	subl	%eax,		%ecx
	movl	$0,		%eax
	adcl	%eax,		%eax
	subl	%edx,		%ecx
	adcl	$0,		%eax
	decl	%ebp
	movl	%ecx,		8(%ebx)
	jz	.L016aw_end

	movl	12(%esi),	%ecx
	movl	12(%edi),	%edx
	subl	%eax,		%ecx
	movl	$0,		%eax
	adcl	%eax,		%eax
	subl	%edx,		%ecx
	adcl	$0,		%eax
	decl	%ebp
	movl	%ecx,		12(%ebx)
	jz	.L016aw_end

	movl	16(%esi),	%ecx
	movl	16(%edi),	%edx
	subl	%eax,		%ecx
	movl	$0,		%eax
	adcl	%eax,		%eax
	subl	%edx,		%ecx
	adcl	$0,		%eax
	decl	%ebp
	movl	%ecx,		16(%ebx)
	jz	.L016aw_end

	movl	20(%esi),	%ecx
	movl	20(%edi),	%edx
	subl	%eax,		%ecx
	movl	$0,		%eax
	adcl	%eax,		%eax
	subl	%edx,		%ecx
	adcl	$0,		%eax
	decl	%ebp
	movl	%ecx,		20(%ebx)
	jz	.L016aw_end

	movl	24(%esi),	%ecx
	movl	24(%edi),	%edx
	subl	%eax,		%ecx
	movl	$0,		%eax
	adcl	%eax,		%eax
	subl	%edx,		%ecx
	adcl	$0,		%eax
	movl	%ecx,		24(%ebx)
.L016aw_end:
	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret
.L_bn_sub_words_end:
	.size	bn_sub_words,.L_bn_sub_words_end-bn_sub_words
.ident	"bn_sub_words"
