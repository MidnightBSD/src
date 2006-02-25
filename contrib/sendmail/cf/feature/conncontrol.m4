divert(-1)
#
# Copyright (c) 2003, 2004 Sendmail, Inc. and its suppliers.
#	All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#

divert(0)
VERSIONID(`$Id: conncontrol.m4,v 1.1.1.2 2006-02-25 02:33:54 laffer1 Exp $')

divert(-1)
ifdef(`_ACCESS_TABLE_', `
	define(`_CONN_CONTROL_', `1')
	ifelse(defn(`_ARG_'), `', `',
		strcasecmp(defn(`_ARG_'), `nodelay'), `1',
		`ifdef(`_DELAY_CHECKS_',
			`
			define(`_CONN_CONTROL_IMMEDIATE_', `1')
			define(`_CONTROL_IMMEDIATE_', `1')
			',
			`errprint(`*** ERROR: FEATURE(`conncontrol', `nodelay') requires FEATURE(`delay_checks')')'
		)',
		`errprint(`*** ERROR: unknown parameter '"defn(`_ARG_')"` for FEATURE(`conncontrol')')')
	define(`_FFR_SRCHLIST_A', `1')
	ifelse(len(X`'_ARG2_), `1', `',
		_ARG2_, `terminate', `define(`_CONN_CONTROL_REPLY', `421')',
		`errprint(`*** ERROR: FEATURE(`conncontrol'): unknown argument '"_ARG2_"
)'
		)
	', `errprint(`*** ERROR: FEATURE(`conncontrol') requires FEATURE(`access_db')
')')
ifdef(`_CONN_CONTROL_REPLY',,`define(`_CONN_CONTROL_REPLY', `452')')
