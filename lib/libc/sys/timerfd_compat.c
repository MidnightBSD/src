/*-
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/cdefs.h>
#include <sys/timerfd.h>

#include "libc_private.h"

int
timerfd_create_compat(int clockid, int flags)
{
	return (timerfd_create(clockid, flags));
}

int
timerfd_gettime_compat(int fd, struct itimerspec *value)
{
	return (timerfd_gettime(fd, value));
}

int
timerfd_settime_compat(int fd, int flags, const struct itimerspec *value,
    struct itimerspec *old_value)
{
	return (timerfd_settime(fd, flags, value, old_value));
}

__sym_compat(timerfd_create, timerfd_create_compat, FBSD_1.7);
__sym_compat(timerfd_gettime, timerfd_gettime_compat, FBSD_1.7);
__sym_compat(timerfd_settime, timerfd_settime_compat, FBSD_1.7);
