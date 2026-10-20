/*-
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>
#include <sys/inotify.h>
#include <sys/specialfd.h>
#include <sys/syscall.h>

#include <atf-c.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

ATF_TC_WITHOUT_HEAD(directory_events);
ATF_TC_BODY(directory_events, tc)
{
	struct specialfd_inotify init;
	struct inotify_event ev;
	struct pollfd pfd;
	char dir[] = "inotify.XXXXXX";
	char path[sizeof(dir) + sizeof("/sample")];
	char oldpath[sizeof(dir) + sizeof("/before")];
	char newpath[sizeof(dir) + sizeof("/after")];
	char buf[4096];
	const char *name;
	size_t offset;
	ssize_t n;
	uint32_t seen, from_cookie, to_cookie;
	int fd, ifd, len, wd;

	ATF_REQUIRE(mkdtemp(dir) != NULL);
	len = snprintf(oldpath, sizeof(oldpath), "%s/before", dir);
	ATF_REQUIRE(len > 0 && (size_t)len < sizeof(oldpath));
	len = snprintf(newpath, sizeof(newpath), "%s/after", dir);
	ATF_REQUIRE(len > 0 && (size_t)len < sizeof(newpath));
	fd = open(oldpath, O_CREAT | O_WRONLY, 0600);
	ATF_REQUIRE(fd >= 0);
	ATF_REQUIRE(close(fd) == 0);
	len = snprintf(path, sizeof(path), "%s/sample", dir);
	ATF_REQUIRE(len > 0 && (size_t)len < sizeof(path));
	init.flags = IN_NONBLOCK;
	ifd = syscall(SYS___specialfd, SPECIALFD_INOTIFY, &init,
	    sizeof(init));
	ATF_REQUIRE(ifd >= 0);
	wd = syscall(SYS_inotify_add_watch_at, ifd, AT_FDCWD, dir,
	    IN_CREATE | IN_MODIFY | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO);
	ATF_REQUIRE(wd >= 0);

	fd = open(path, O_CREAT | O_WRONLY, 0600);
	ATF_REQUIRE(fd >= 0);
	ATF_REQUIRE(write(fd, "x", 1) == 1);
	ATF_REQUIRE(close(fd) == 0);
	ATF_REQUIRE(unlink(path) == 0);

	pfd.fd = ifd;
	pfd.events = POLLIN;
	ATF_REQUIRE_MSG(poll(&pfd, 1, 3000) == 1,
	    "no inotify events arrived");
	n = read(ifd, buf, sizeof(buf));
	ATF_REQUIRE(n >= (ssize_t)sizeof(ev));
	seen = 0;
	offset = 0;
	while (offset < (size_t)n) {
		ATF_REQUIRE((size_t)n - offset >= sizeof(ev));
		memcpy(&ev, buf + offset, sizeof(ev));
		ATF_REQUIRE(ev.len <= (size_t)n - offset - sizeof(ev));
		ATF_REQUIRE(ev.wd == wd);
		ATF_REQUIRE(ev.len >= sizeof("sample"));
		name = buf + offset + sizeof(ev);
		ATF_REQUIRE(memchr(name, '\0', ev.len) != NULL);
		ATF_REQUIRE(strcmp(name, "sample") == 0);
		seen |= ev.mask;
		offset += sizeof(ev) + ev.len;
	}
	ATF_REQUIRE((seen & (IN_CREATE | IN_MODIFY | IN_DELETE)) ==
	    (IN_CREATE | IN_MODIFY | IN_DELETE));

	ATF_REQUIRE(rename(oldpath, newpath) == 0);
	ATF_REQUIRE_MSG(poll(&pfd, 1, 3000) == 1,
	    "no inotify rename events arrived");
	n = read(ifd, buf, sizeof(buf));
	ATF_REQUIRE(n >= (ssize_t)sizeof(ev));
	from_cookie = to_cookie = 0;
	offset = 0;
	while (offset < (size_t)n) {
		ATF_REQUIRE((size_t)n - offset >= sizeof(ev));
		memcpy(&ev, buf + offset, sizeof(ev));
		ATF_REQUIRE(ev.len <= (size_t)n - offset - sizeof(ev));
		ATF_REQUIRE(ev.wd == wd);
		name = buf + offset + sizeof(ev);
		ATF_REQUIRE(memchr(name, '\0', ev.len) != NULL);
		if ((ev.mask & IN_MOVED_FROM) != 0) {
			ATF_REQUIRE(strcmp(name, "before") == 0);
			from_cookie = ev.cookie;
		} else if ((ev.mask & IN_MOVED_TO) != 0) {
			ATF_REQUIRE(strcmp(name, "after") == 0);
			to_cookie = ev.cookie;
		} else {
			ATF_REQUIRE_MSG(0, "unexpected inotify rename event");
		}
		offset += sizeof(ev) + ev.len;
	}
	ATF_REQUIRE_MSG(from_cookie != 0 && from_cookie == to_cookie,
	    "rename cookies differ or are zero: from=%u to=%u",
	    from_cookie, to_cookie);
	ATF_REQUIRE(close(ifd) == 0);
	ATF_REQUIRE(unlink(newpath) == 0);
	ATF_REQUIRE(rmdir(dir) == 0);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, directory_events);
	return (atf_no_error());
}
