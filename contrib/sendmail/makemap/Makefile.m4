dnl $Id: Makefile.m4,v 1.1.1.1 2006-02-25 02:26:20 laffer1 Exp $
include(confBUILDTOOLSDIR`/M4/switch.m4')

define(`confREQUIRE_LIBSM', `true')
# sendmail dir
SMSRCDIR=	ifdef(`confSMSRCDIR', `confSMSRCDIR', `${SRCDIR}/sendmail')
PREPENDDEF(`confENVDEF', `confMAPDEF')
PREPENDDEF(`confINCDIRS', `-I${SMSRCDIR} ')

bldPRODUCT_START(`executable', `makemap')
define(`bldSOURCES', `makemap.c ')
define(`bldINSTALL_DIR', `S')
bldPUSH_SMLIB(`sm')
bldPUSH_SMLIB(`smutil')
bldPUSH_SMLIB(`smdb')
APPENDDEF(`confENVDEF', `-DNOT_SENDMAIL')
bldPRODUCT_END

bldPRODUCT_START(`manpage', `makemap')
define(`bldSOURCES', `makemap.8')
bldPRODUCT_END

bldFINISH
