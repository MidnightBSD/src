# $FreeBSD: release/10.0.0/lib/csu/Makefile.inc 249657 2013-04-19 19:45:00Z ed $

SSP_CFLAGS=

SED_FIX_NOTE = -i "" -e '/\.note\.tag/s/progbits/note/'

NO_WMISSING_VARIABLE_DECLARATIONS=

.include "../Makefile.inc"
