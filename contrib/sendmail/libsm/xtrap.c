/*
 * Copyright (c) 2000 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 */

#include <sm/gen.h>
SM_RCSID("@(#)$Id: xtrap.c,v 1.1.1.2 2006-02-25 02:33:56 laffer1 Exp $")

#include <sm/xtrap.h>

SM_ATOMIC_UINT_T SmXtrapCount;

SM_DEBUG_T SmXtrapDebug = SM_DEBUG_INITIALIZER("sm_xtrap",
	"@(#)$Debug: sm_xtrap - raise exception at N'th xtrap point $");

SM_DEBUG_T SmXtrapReport = SM_DEBUG_INITIALIZER("sm_xtrap_report",
	"@(#)$Debug: sm_xtrap_report - report xtrap count on exit $");
