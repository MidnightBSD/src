# $FreeBSD: release/10.0.0/gnu/usr.bin/binutils/ld/elf64_ia64_fbsd.sh 219876 2011-03-22 17:19:35Z marcel $
. ${srcdir}/emulparams/elf64_ia64.sh
TEXT_START_ADDR="0x0000000100000000"
unset DATA_ADDR
unset SMALL_DATA_CTOR
unset SMALL_DATA_DTOR
. ${srcdir}/emulparams/elf_fbsd.sh
OUTPUT_FORMAT="elf64-ia64-freebsd"
