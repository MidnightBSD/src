divert(-1)
#
# Copyright (c) 1998, 1999 Sendmail, Inc. and its suppliers.
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
VERSIONID(`$Id: nocanonify.m4,v 1.1.1.2 2006-02-25 02:33:54 laffer1 Exp $')
divert(-1)

define(`_NO_CANONIFY_', 1)
ifelse(defn(`_ARG_'), `', `',
	strcasecmp(defn(`_ARG_'), `canonify_hosts'), `1',
	`define(`_CANONIFY_HOSTS_', 1)',
	`errprint(`*** ERROR: unknown parameter '"defn(`_ARG_')"` for FEATURE(`nocanonify')
')')
