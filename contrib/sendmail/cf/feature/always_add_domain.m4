divert(-1)
#
# Copyright (c) 1998-2000 Sendmail, Inc. and its suppliers.
#	All rights reserved.
# Copyright (c) 1983 Eric P. Allman.  All rights reserved.
# Copyright (c) 1988, 1993
#	The Regents of the University of California.  All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#

divert(0)
VERSIONID(`$Id: always_add_domain.m4,v 1.1.1.2 2006-02-25 02:33:54 laffer1 Exp $')
divert(-1)

ifdef(`_MAILER_local_',
	`errprint(`*** MAILER(`local') must appear after FEATURE(`always_add_domain')')
')dnl
define(`_ALWAYS_ADD_DOMAIN_', ifelse(len(X`'_ARG_),`1',`',_ARG_))
