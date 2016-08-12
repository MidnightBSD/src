/*
 * Copyright (c) 2006 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#include <sys/types.h>
#include <Availability.h>
#include <errno.h>
#include <sys/resource.h>

extern int __iopolicysys(int, struct _iopol_param_t *);

int
getiopolicy_np(int iotype, int scope)
{
	int policy, error;
	struct _iopol_param_t iop_param;

	if (iotype != IOPOL_TYPE_DISK ||
		(scope != IOPOL_SCOPE_PROCESS && scope != IOPOL_SCOPE_THREAD)) {
		errno = EINVAL;
		policy = -1;
		goto exit;
	}

	iop_param.iop_scope = scope;
	iop_param.iop_iotype = iotype;
	error = __iopolicysys(IOPOL_CMD_GET, &iop_param);
	if (error != 0) {
		errno = error;
		policy = -1;
		goto exit;
	}

	policy = iop_param.iop_policy;

  exit:
	return policy;
}

int
setiopolicy_np(int iotype, int scope, int policy)
{
	/* kernel validates the indiv values, no need to repeat it */
	struct _iopol_param_t iop_param;
	
	iop_param.iop_scope = scope;
	iop_param.iop_iotype = iotype;
	iop_param.iop_policy = policy;

	return( __iopolicysys(IOPOL_CMD_SET, &iop_param));
}
