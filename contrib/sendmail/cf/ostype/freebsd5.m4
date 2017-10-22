divert(-1)
#
# Copyright (c) 2001 Sendmail, Inc. and its suppliers.
#	All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#

divert(0)
VERSIONID(`$Id: freebsd5.m4,v 1.1 2001/10/08 22:25:34 gshapiro Exp $')
ifdef(`STATUS_FILE',, `define(`STATUS_FILE', `/var/log/sendmail.st')')dnl
dnl turn on S flag for local mailer
MODIFY_MAILER_FLAGS(`LOCAL', `+S')dnl
ifdef(`LOCAL_MAILER_PATH',, `define(`LOCAL_MAILER_PATH', /usr/libexec/mail.local)')dnl
ifdef(`LOCAL_MAILER_ARGS',, `define(`LOCAL_MAILER_ARGS', `mail $u')')dnl
ifdef(`UUCP_MAILER_PATH',, `define(`UUCP_MAILER_PATH', `/usr/local/bin/uux')')dnl
ifdef(`UUCP_MAILER_ARGS',, `define(`UUCP_MAILER_ARGS', `uux - -r -z -a$g $h!rmail ($u)')')dnl
