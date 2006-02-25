/*
 * Copyright (c) 2001 Sendmail, Inc. and its suppliers.
 *      All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 *	$Id: cf.h,v 1.1.1.1 2006-02-25 02:26:20 laffer1 Exp $
 */

#ifndef SM_CF_H
# define SM_CF_H

#include <sm/gen.h>

typedef struct
{
	char *opt_name;
	char *opt_val;
} SM_CF_OPT_T;

extern int
sm_cf_getopt __P((
	char *path,
	int optc,
	SM_CF_OPT_T *optv));

#endif /* ! SM_CF_H */
