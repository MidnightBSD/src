dnl $Id: Makefile.m4,v 1.1.1.2 2006-02-25 02:34:00 laffer1 Exp $
include(confBUILDTOOLSDIR`/M4/switch.m4')

bldPRODUCT_START(`executable', `test')
define(`bldSOURCES', `t_dropgid.c ')
bldPRODUCT_END

include(confBUILDTOOLSDIR`/M4/'bldM4_TYPE_DIR`/sm-test.m4')
dnl smtest(`getipnode')
smtest(`t_dropgid')
smtest(`t_exclopen')
smtest(`t_pathconf')
smtest(`t_seteuid')
smtest(`t_setgid')
smtest(`t_setreuid')
smtest(`t_setuid')
dnl smtest(`t_snprintf')

bldFINISH
