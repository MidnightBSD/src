dnl	DO NOT EDIT THIS FILE.
dnl	Place personal settings in devtools/Site/site.config.m4

#
define(`confCC', `cc -pipe ${Extra_CC_Flags}')
define(`confMAPDEF', `-DNEWDB -DNIS -DMAP_REGEX')
define(`confENVDEF', `-DDARWIN=230000 -DBIND_8_COMPAT -DNETINET6')
define(`confLDOPTS', `${Extra_LD_Flags}')
define(`confMTLDOPTS', `-lpthread')
define(`confMILTER_STATIC', `')
define(`confDEPEND_TYPE', `CC-M')
define(`confOPTIMIZE', `-O3')
define(`confRANLIBOPTS', `-c')
define(`confHFDIR', `/usr/share/sendmail')
define(`confINSTALL_RAWMAN')
define(`confMANOWN', `root')
define(`confMANGRP', `wheel')
define(`confUBINOWN', `root')
define(`confUBINGRP', `wheel')
define(`confSBINOWN', `root')
define(`confSBINGRP', `wheel')
define(`confLDOPTS_SO', `-dynamiclib -flat_namespace -undefined suppress -single_module')
define(`confSHAREDLIB_EXT', `.dylib')
APPENDDEF(`conf_sendmail_LIBS', `-lresolv')
