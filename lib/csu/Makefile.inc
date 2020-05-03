# $FreeBSD: stable/11/lib/csu/Makefile.inc 288074 2015-09-21 18:39:13Z bdrewery $

SSP_CFLAGS=

SED_FIX_NOTE = -i "" -e '/\.note\.tag/s/progbits/note/'

NO_WMISSING_VARIABLE_DECLARATIONS=

.include "../Makefile.inc"
