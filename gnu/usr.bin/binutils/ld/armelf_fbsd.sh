# $FreeBSD: stable/9/gnu/usr.bin/binutils/ld/armelf_fbsd.sh 218822 2011-02-18 20:54:12Z dim $
. ${srcdir}/emulparams/armelf.sh
. ${srcdir}/emulparams/elf_fbsd.sh
MAXPAGESIZE=0x8000
GENERATE_PIE_SCRIPT=yes

unset STACK_ADDR
unset EMBEDDED
