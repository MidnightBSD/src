#include <sys/cdefs.h>
__FBSDID("$FreeBSD: stable/11/stand/common/load_elf32_obj.c 261504 2014-02-05 04:39:03Z jhb $");

#define __ELF_WORD_SIZE 32
#define	_MACHINE_ELF_WANT_32BIT

#include "load_elf_obj.c"
