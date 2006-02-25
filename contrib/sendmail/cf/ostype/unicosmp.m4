divert(-1)
#
# Copyright (c) 2003 Sendmail, Inc. and its suppliers.
#	All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#
#

#
# Notes:
# - In UNICOSMP seems that TZ variable is needed that correct time is marked
#   to syslog
#

divert(0)
VERSIONID(`$Id: unicosmp.m4,v 1.1.1.2 2006-02-25 02:33:55 laffer1 Exp $')
_DEFIFNOT(`LOCAL_MAILER_FLAGS', `Ehm9')dnl
ifdef(`LOCAL_MAILER_ARGS',, `define(`LOCAL_MAILER_ARGS', `mail -s -d $u')')dnl
ifdef(`QUEUE_DIR',, `define(`QUEUE_DIR', /var/spool/mqueue)')dnl
ifdef(`STATUS_FILE',, `define(`STATUS_FILE', `/var/log/sendmail.st')')dnl
define(`LOCAL_MAILER_PATH', `/usr/bin/mail')dnl
define(`confTIME_ZONE', USE_TZ)dnl
define(`confEBINDIR', `/usr/lib')dnl
