dnl $Id: Makefile.m4,v 1.1.1.3 2006-08-04 02:03:05 laffer1 Exp $
define(`confREQUIRE_LIBUNIX')
include(confBUILDTOOLSDIR`/M4/switch.m4')

define(`confREQUIRE_LIBSM', `true')
PREPENDDEF(`confENVDEF', `confMAPDEF')
bldPRODUCT_START(`library', `libsm')
define(`bldSOURCES', ` assert.c debug.c errstring.c exc.c heap.c match.c rpool.c strdup.c strerror.c strl.c clrerr.c fclose.c feof.c ferror.c fflush.c fget.c fpos.c findfp.c flags.c fopen.c fprintf.c fpurge.c fput.c fread.c fscanf.c fseek.c fvwrite.c fwalk.c fwrite.c get.c makebuf.c put.c refill.c rewind.c setvbuf.c smstdio.c snprintf.c sscanf.c stdio.c strio.c ungetc.c vasprintf.c vfprintf.c vfscanf.c vprintf.c vsnprintf.c wbuf.c wsetup.c string.c stringf.c xtrap.c strto.c test.c path.c strcasecmp.c strrevcmp.c signal.c clock.c config.c shm.c sem.c mbdb.c strexit.c cf.c ldap.c niprop.c mpeix.c memstat.c ')
bldPRODUCT_END
dnl msg.c
dnl syslogio.c

include(confBUILDTOOLSDIR`/M4/'bldM4_TYPE_DIR`/sm-test.m4')
smtest(`t-event', `run')
smtest(`t-exc', `run')
smtest(`t-rpool', `run')
smtest(`t-string', `run')
smtest(`t-smstdio', `run')
smtest(`t-match', `run')
smtest(`t-strio', `run')
smtest(`t-heap', `run')
smtest(`t-fopen', `run')
smtest(`t-strl', `run')
smtest(`t-strrevcmp', `run')
smtest(`t-types', `run')
smtest(`t-path', `run')
smtest(`t-float', `run')
smtest(`t-scanf', `run')
smtest(`t-shm', `run')
smtest(`t-sem', `run')
dnl smtest(`t-msg', `run')
smtest(`t-cf')
smtest(`b-strcmp')
dnl SM_CONF_STRL cannot be turned off
dnl smtest(`b-strl')
smtest(`t-memstat')

bldFINISH
