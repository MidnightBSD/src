/* $FreeBSD: stable/11/contrib/gcc/config/riscv64/freebsd.h 294634 2016-01-23 15:33:11Z br $ */

#undef INIT_SECTION_ASM_OP
#undef FINI_SECTION_ASM_OP
#define INIT_ARRAY_SECTION_ASM_OP "\t.section\t.init_array,\"aw\",%init_array"
#define FINI_ARRAY_SECTION_ASM_OP "\t.section\t.fini_array,\"aw\",%fini_array"
