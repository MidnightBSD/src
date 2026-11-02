/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The MidnightBSD Project. All rights reserved.
 */

#include <sys/types.h>

#include <atf-c.h>
#include <poll.h>
#include <time.h>

#define libusb_device_handle libusb20_device
#include "../libusb.h"

static int failing_clock_gettime(clockid_t, struct timespec *);
static int interrupted_poll(struct pollfd *, nfds_t, int);
static ssize_t empty_device_list(libusb_context *, libusb_device ***);

/* Run the real worker with deterministic clock and wait failures. */
#define clock_gettime failing_clock_gettime
#define poll interrupted_poll
#define libusb_get_device_list empty_device_list
#include "../libusb10_hotplug.c"
#undef libusb_get_device_list
#undef poll
#undef clock_gettime

static libusb_context *worker_ctx;
static unsigned waits, scans;

static int
failing_clock_gettime(clockid_t clock, struct timespec *now)
{

	(void)now;
	ATF_REQUIRE_EQ(clock, CLOCK_MONOTONIC);
	errno = EIO;
	return (-1);
}

static int
interrupted_poll(struct pollfd *fds, nfds_t nfds, int timeout)
{

	ATF_REQUIRE_EQ(nfds, 1);
	ATF_REQUIRE_EQ(fds[0].fd, -1);
	ATF_REQUIRE_EQ(timeout, 4000);
	/* Stop after three interrupted waits, without relying on wall time. */
	if (++waits == 4)
		worker_ctx->hotplug_handler = NO_THREAD;
	errno = EINTR;
	return (-1);
}

static ssize_t
empty_device_list(libusb_context *ctx, libusb_device ***devices)
{

	ATF_REQUIRE_EQ(ctx, worker_ctx);
	*devices = calloc(1, sizeof(**devices));
	ATF_REQUIRE(*devices != NULL);
	scans++;
	return (0);
}

ATF_TC_WITHOUT_HEAD(interrupted_clock_fallback);
ATF_TC_BODY(interrupted_clock_fallback, tc)
{
	libusb_context ctx;

	memset(&ctx, 0, sizeof(ctx));
	TAILQ_INIT(&ctx.hotplug_cbh);
	TAILQ_INIT(&ctx.hotplug_devs);
	ATF_REQUIRE_EQ(pthread_mutex_init(&ctx.hotplug_lock, NULL), 0);
	ctx.hotplug_handler = pthread_self();
	worker_ctx = &ctx;
	waits = scans = 0;
	ATF_CHECK(libusb_hotplug_scan(&ctx) == NULL);
	ATF_CHECK_EQ(waits, 4);
	ATF_CHECK_EQ(scans, 3);
	ATF_CHECK_EQ(ctx.hotplug_handler, NO_THREAD);
	ATF_CHECK_EQ(pthread_mutex_destroy(&ctx.hotplug_lock), 0);
	worker_ctx = NULL;
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, interrupted_clock_fallback);
	return (atf_no_error());
}
