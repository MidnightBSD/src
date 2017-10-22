/* $FreeBSD: release/10.0.0/gnu/usr.bin/cc/cc_tools/elfos-undef.h 182627 2008-09-01 18:46:03Z obrien $ */

/* This header exists to avoid editing contrib/gcc/config/elfos.h - which
   isn't coded to be defensive as it should... */

#undef  ASM_DECLARE_OBJECT_NAME
#undef  ASM_OUTPUT_IDENT
#undef  IDENT_ASM_OP
#undef  READONLY_DATA_SECTION_ASM_OP
