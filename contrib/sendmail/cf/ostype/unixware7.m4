divert(-1)
#
# Copyright (c) 1998-2000 Sendmail, Inc. and its suppliers.
#	All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#

divert(0)
VERSIONID(`$Id: unixware7.m4,v 1.1.1.2 2006-02-25 02:33:55 laffer1 Exp $')
ifdef(`QUEUE_DIR',, `define(`QUEUE_DIR', /var/spool/mqueue)')dnl
define(`confEBINDIR', `/usr/lib')dnl
define(`confTIME_ZONE', `USE_TZ')dnl
ifdef(`LOCAL_MAILER_PATH',, `define(`LOCAL_MAILER_PATH', /etc/mail/slocal)')dnl
_DEFIFNOT(`LOCAL_MAILER_FLAGS', `Puho9')dnl
ifdef(`LOCAL_MAILER_ARGS',, `define(`LOCAL_MAILER_ARGS', `slocal -user $u')')dnl
ifdef(`LOCAL_SHELL_FLAGS',, `define(`LOCAL_SHELL_FLAGS', Peu)')dnl
