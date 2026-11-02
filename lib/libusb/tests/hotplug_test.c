/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The MidnightBSD Project. All rights reserved.
 */

#include <atf-c.h>

/* Exercise the private notification reader without exporting a new API. */
#include "../libusb10_hotplug.c"

static void
send_event(int fd, const char *event)
{
	size_t len = strlen(event);

	ATF_REQUIRE_EQ(send(fd, event, len, 0), (ssize_t)len);
}

ATF_TC_WITHOUT_HEAD(usb_events);
ATF_TC_BODY(usb_events, tc)
{
	struct pollfd pfd;
	int pair[2];
	char buf[2048];

	ATF_REQUIRE_EQ(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, pair), 0);
	pfd.fd = pair[0];
	pfd.events = POLLIN;
	send_event(pair[1],
	    "!system=USB subsystem=DEVICE type=ATTACH ugen=ugen0.2\n");
	ATF_CHECK_EQ(libusb_hotplug_wait(&pfd, 0), 1);
	send_event(pair[1],
	    "!system=USB subsystem=DEVICE type=DETACH ugen=ugen0.2\n");
	ATF_CHECK_EQ(libusb_hotplug_wait(&pfd, 0), 1);

	/* Oversized packets still carry the complete event prefix. */
	memset(buf, 'x', sizeof(buf));
	memcpy(buf, "!system=USB subsystem=DEVICE ",
	    sizeof("!system=USB subsystem=DEVICE ") - 1);
	ATF_REQUIRE_EQ(send(pair[1], buf, sizeof(buf), 0), sizeof(buf));
	ATF_CHECK_EQ(libusb_hotplug_wait(&pfd, 0), 1);
	ATF_CHECK_EQ(pfd.fd, pair[0]);
	close(pair[0]);
	close(pair[1]);
}

ATF_TC_WITHOUT_HEAD(unrelated_events);
ATF_TC_BODY(unrelated_events, tc)
{
	struct pollfd pfd;
	int pair[2];

	ATF_REQUIRE_EQ(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, pair), 0);
	pfd.fd = pair[0];
	pfd.events = POLLIN;
	send_event(pair[1], "!system=IFNET subsystem=em0 type=LINK_UP\n");
	ATF_CHECK_EQ(libusb_hotplug_wait(&pfd, 0), 0);
	send_event(pair[1], "!system=USB subsystem=INTERFACE type=ATTACH\n");
	ATF_CHECK_EQ(libusb_hotplug_wait(&pfd, 0), 0);
	send_event(pair[1], "!system=USB subsystem=DEVICE_OTHER type=ATTACH\n");
	ATF_CHECK_EQ(libusb_hotplug_wait(&pfd, 0), 0);
	send_event(pair[1], "!system=USB");
	ATF_CHECK_EQ(libusb_hotplug_wait(&pfd, 0), 0);
	close(pair[0]);
	close(pair[1]);
}

ATF_TC_WITHOUT_HEAD(disconnect);
ATF_TC_BODY(disconnect, tc)
{
	struct pollfd pfd;
	int pair[2];

	ATF_REQUIRE_EQ(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, pair), 0);
	pfd.fd = pair[0];
	pfd.events = POLLIN;
	close(pair[1]);
	ATF_CHECK_EQ(libusb_hotplug_wait(&pfd, 0), 1);
	ATF_CHECK_EQ(pfd.fd, -1);
	ATF_CHECK_EQ(fcntl(pair[0], F_GETFD), -1);
	ATF_CHECK_EQ(errno, EBADF);
	ATF_CHECK_EQ(libusb_hotplug_wait(&pfd, 0), 1);
}

ATF_TC_WITHOUT_HEAD(periodic_scan);
ATF_TC_BODY(periodic_scan, tc)
{
	struct pollfd pfd;
	int pair[2];

	ATF_REQUIRE_EQ(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, pair), 0);
	pfd.fd = pair[0];
	pfd.events = POLLIN;
	/* Idle connected sockets still request periodic reconciliation. */
	ATF_CHECK_EQ(libusb_hotplug_wait(&pfd, 0), 1);
	ATF_CHECK_EQ(pfd.fd, pair[0]);
	close(pair[0]);
	close(pair[1]);
	/* No devd connection uses the same periodic reconciliation path. */
	pfd.fd = -1;
	ATF_CHECK_EQ(libusb_hotplug_wait(&pfd, 0), 1);
}

ATF_TC_WITHOUT_HEAD(invalid_context);
ATF_TC_BODY(invalid_context, tc)
{

	ATF_CHECK_EQ(libusb_hotplug_register_callback(NULL,
	    LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED, 0, -1, -1, -1, NULL, NULL,
	    NULL), LIBUSB_ERROR_INVALID_PARAM);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, usb_events);
	ATF_TP_ADD_TC(tp, unrelated_events);
	ATF_TP_ADD_TC(tp, disconnect);
	ATF_TP_ADD_TC(tp, periodic_scan);
	ATF_TP_ADD_TC(tp, invalid_context);
	return (atf_no_error());
}
