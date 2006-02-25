dnl $Id: Makefile.m4,v 1.1.1.2 2006-02-25 02:33:57 laffer1 Exp $
include(confBUILDTOOLSDIR`/M4/switch.m4')

define(`confREQUIRE_LIBSM', `true')
# sendmail dir
SMSRCDIR=	ifdef(`confSMSRCDIR', `confSMSRCDIR', `${SRCDIR}/sendmail')
PREPENDDEF(`confENVDEF', `confMAPDEF')
PREPENDDEF(`confINCDIRS', `-I${SMSRCDIR} ')

bldPRODUCT_START(`executable', `smrsh')
define(`bldINSTALL_DIR', `E')
define(`bldSOURCES', `smrsh.c ')
bldPUSH_SMLIB(`sm')
APPENDDEF(`confENVDEF', `-DNOT_SENDMAIL')
bldPRODUCT_END

bldPRODUCT_START(`manpage', `smrsh')
define(`bldSOURCES', `smrsh.8')
bldPRODUCT_END

bldFINISH
