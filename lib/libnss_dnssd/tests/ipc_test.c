/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The MidnightBSD Project.
 */

/*
 * Root-only integration fixture. Run with the built nss_dnssd.so.1 path.
 * All files and the daemon socket live in a temporary chroot. DNS fallback
 * uses a temporary responder bound only to a loopback address.
 */
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>

#include <netinet/in.h>

#include <arpa/inet.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char rootdir[] = "/tmp/nss-dnssd-ipc.XXXXXX";
static const char *dirs[] = { "/etc", "/lib", "/libexec", "/usr", "/usr/lib",
	"/var", "/var/run" };
static const char *libs[] = { "/libexec/ld-elf.so.1", "/lib/libc.so.7",
	"/lib/libthr.so.3", "/usr/lib/libdns_sd.so.1" };
static const char *files[] = { "/etc/hosts", "/etc/nsswitch.conf",
	"/etc/resolv.conf", "/usr/lib/nss_dnssd.so.1",
	"/var/run/mDNSResponder" };

static void
path(char *buf, size_t len, const char *suffix)
{
	if (snprintf(buf, len, "%s%s", rootdir, suffix) >= (int)len)
		errx(1, "temporary path too long");
}

static void
cleanup(void)
{
	char buf[1024];
	size_t i;

	for (i = 0; i < sizeof(files) / sizeof(files[0]); i++) {
		path(buf, sizeof(buf), files[i]);
		unlink(buf);
	}
	for (i = 0; i < sizeof(libs) / sizeof(libs[0]); i++) {
		path(buf, sizeof(buf), libs[i]);
		unlink(buf);
	}
	for (i = sizeof(dirs) / sizeof(dirs[0]); i != 0; i--) {
		path(buf, sizeof(buf), dirs[i - 1]);
		rmdir(buf);
	}
	rmdir(rootdir);
}

static void
copy_file(const char *source, const char *target)
{
	char buf[16384], dest[1024];
	ssize_t n;
	int in, out;

	path(dest, sizeof(dest), target);
	if ((in = open(source, O_RDONLY)) < 0 ||
	    (out = open(dest, O_WRONLY | O_CREAT | O_EXCL, 0555)) < 0)
		err(1, "copy fixture dependency");
	while ((n = read(in, buf, sizeof(buf))) > 0) {
		if (write(out, buf, n) != n)
			err(1, "write dependency");
	}
	if (n < 0)
		err(1, "read dependency");
	close(in);
	close(out);
}

static void
text_file(const char *target, const char *text)
{
	char dest[1024];
	FILE *fp;

	path(dest, sizeof(dest), target);
	if ((fp = fopen(dest, "w")) == NULL || fputs(text, fp) == EOF ||
	    fclose(fp) != 0)
		err(1, "write fixture configuration");
}

static void
put32(unsigned char *p, uint32_t n)
{
	n = htonl(n);
	memcpy(p, &n, sizeof(n));
}

static uint32_t
get32(const unsigned char *p)
{
	uint32_t n;

	memcpy(&n, p, sizeof(n));
	return (ntohl(n));
}

static void
put16(unsigned char *p, uint16_t n)
{
	n = htons(n);
	memcpy(p, &n, sizeof(n));
}

static uint16_t
get16(const unsigned char *p)
{
	uint16_t n;

	memcpy(&n, p, sizeof(n));
	return (ntohs(n));
}

static void
read_exact(int fd, void *buf, size_t len)
{
	ssize_t n;
	unsigned char *p = buf;

	while (len != 0) {
		if ((n = read(fd, p, len)) <= 0)
			errx(1, "short IPC request");
		p += n;
		len -= n;
	}
}

enum scenario { SUCCESS, NEGATIVE, MALFORMED, UNAVAILABLE, HOSTS };

/* QueryRecord's non-shared connection returns its handshake on the same fd. */
static int
ipc_reply(int listener, enum scenario scenario)
{
	unsigned char hdr[28], request[1024], reply[2048], address[16];
	const char *name;
	size_t len, pos, addrlen;
	uint16_t type;
	int fd;
	struct timeval timeout = { .tv_sec = 2 };

	if ((fd = accept(listener, NULL, NULL)) < 0)
		err(1, "accept private daemon client");
	if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout,
		sizeof(timeout)) != 0)
		err(1, "set private daemon receive timeout");
	read_exact(fd, hdr, sizeof(hdr));
	len = get32(hdr + 4);
	if (get32(hdr) != 1 || get32(hdr + 12) != 8 || len < 13 ||
	    len > sizeof(request))
		errx(1, "unexpected QueryRecord IPC request");
	read_exact(fd, request, len);
	name = (char *)request + 8;
	if (memchr(name, '\0', len - 8) == NULL ||
	    strcmp(name, "ipc.example.") != 0)
		errx(1, "unexpected query name");
	pos = 8 + strlen(name) + 1;
	if (pos + 4 > len || get32(request + 4) != 0 ||
	    (get32(request) & 0x11000) != 0x11000 ||
	    get16(request + pos + 2) != 1)
		errx(1, "unexpected query flags/class");
	type = get16(request + pos);
	if (type != 1 && type != 28)
		errx(1, "unexpected query type");
	memset(reply, 0, sizeof(reply));
	if (write(fd, reply, 4) != 4)
		err(1, "write IPC handshake");
	/*
	 * Payload: flags, interface, error, name, type, class, data, TTL.
	 */
	pos = 28;
	put32(reply + pos, 2);
	put32(reply + pos + 8, scenario == NEGATIVE ? (uint32_t)-65554 : 0);
	pos += 12;
	memcpy(reply + pos, name, strlen(name) + 1);
	pos += strlen(name) + 1;
	put16(reply + pos, type);
	put16(reply + pos + 2, 1);
	addrlen = type == 1 ? 4 : 16;
	if (scenario == NEGATIVE)
		addrlen = 0;
	else if (scenario == MALFORMED)
		addrlen = 3;
	put16(reply + pos + 4, addrlen);
	pos += 6;
	inet_pton(type == 1 ? AF_INET : AF_INET6,
	    type == 1 ? "192.0.2.42" : "2001:db8::42", address);
	memcpy(reply + pos, address, addrlen);
	pos += addrlen;
	put32(reply + pos, 60);
	pos += 4;
	put32(reply, 1);
	put32(reply + 4, pos - 28);
	put32(reply + 12, 68);
	memcpy(reply + 16, hdr + 16, 8);
	if (write(fd, reply, pos) != (ssize_t)pos)
		err(1, "write IPC callback");
	return (fd);
}

static void
dns_reply(int fd)
{
	unsigned char packet[1024], address[16];
	struct sockaddr_storage from;
	socklen_t fromlen = sizeof(from);
	ssize_t n;
	size_t pos = 12, addrlen;
	uint16_t type;

	n = recvfrom(fd, packet, sizeof(packet) - 32, 0,
	    (struct sockaddr *)&from, &fromlen);
	if (n < 17 || get16(packet + 4) != 1)
		errx(1, "invalid fallback DNS query");
	while (pos < (size_t)n && packet[pos] != 0) {
		if (packet[pos] > 63 || pos + packet[pos] + 1 >= (size_t)n)
			errx(1, "invalid DNS label");
		pos += packet[pos] + 1;
	}
	if (pos + 5 != (size_t)n)
		errx(1, "invalid DNS question length");
	type = get16(packet + pos + 1);
	addrlen = type == 1 ? 4 : 16;
	if (type != 1 && type != 28)
		errx(1, "unexpected fallback DNS type");
	put16(packet + 2, 0x8180);
	put16(packet + 6, 1);
	put16(packet + 8, 0);
	put16(packet + 10, 0);
	pos = n;
	put16(packet + pos, 0xc00c);
	put16(packet + pos + 2, type);
	put16(packet + pos + 4, 1);
	put32(packet + pos + 6, 60);
	put16(packet + pos + 10, addrlen);
	inet_pton(type == 1 ? AF_INET : AF_INET6,
	    type == 1 ? "198.51.100.77" : "2001:db8::77", address);
	memcpy(packet + pos + 12, address, addrlen);
	pos += 12 + addrlen;
	if (sendto(fd, packet, pos, 0, (struct sockaddr *)&from, fromlen) !=
	    (ssize_t)pos)
		err(1, "reply to loopback DNS query");
}

static int
client(enum scenario scenario)
{
	struct addrinfo hints = { .ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
		.ai_flags = AI_CANONNAME };
	struct addrinfo *list = NULL, *ai;
	char host[NI_MAXHOST];
	unsigned int count = 0;
	int error;

	unsetenv("DNSSD_UDS_PATH");
	unsetenv("LOCALDOMAIN");
	unsetenv("RES_OPTIONS");
	unsetenv("HOSTALIASES");
	if (chroot(rootdir) != 0 || chdir("/") != 0)
		return (10);
	error = getaddrinfo(scenario == HOSTS ? "file.example" : "ipc.example.",
	    "443", &hints, &list);
	if (scenario == NEGATIVE)
		return (error == EAI_NONAME ? 0 : 11);
	if (error != 0) {
		fprintf(stderr, "scenario %d: %s\n", scenario,
		    gai_strerror(error));
		return (12);
	}
	if (list == NULL)
		return (20);
	for (ai = list; ai != NULL; ai = ai->ai_next) {
		if (getnameinfo(ai->ai_addr, ai->ai_addrlen, host, sizeof(host),
			NULL, 0, NI_NUMERICHOST) != 0)
			return (13);
		if (scenario == SUCCESS && strcmp(host, "192.0.2.42") != 0 &&
		    strcmp(host, "2001:db8::42") != 0)
			return (14);
		if ((scenario == UNAVAILABLE || scenario == MALFORMED) &&
		    strcmp(host, "198.51.100.77") != 0 &&
		    strcmp(host, "2001:db8::77") != 0)
			return (15);
		if (scenario == HOSTS && strcmp(host, "198.51.100.88") != 0)
			return (16);
		if (ntohs(((struct sockaddr_in *)(void *)ai->ai_addr)
			    ->sin_port) != 443)
			return (17);
		count++;
	}
	if (list->ai_canonname == NULL ||
	    (scenario != HOSTS &&
		strcmp(list->ai_canonname, "ipc.example") != 0))
		return (18);
	freeaddrinfo(list);
	return (count == (scenario == HOSTS ? 1U : 2U) ? 0 : 19);
}

static void
run_case(enum scenario scenario, int dns)
{
	struct sockaddr_un addr = { .sun_family = AF_UNIX };
	struct pollfd fds[2] = { { .fd = -1, .events = POLLIN },
		{ .fd = dns, .events = POLLIN } };
	char sockpath[1024];
	pid_t childpid;
	unsigned int requests = 0, dnsrequests = 0;
	int status = 0, turns, clients[2];

	path(sockpath, sizeof(sockpath), "/var/run/mDNSResponder");
	if (scenario != UNAVAILABLE && scenario != HOSTS) {
		strlcpy(addr.sun_path, sockpath, sizeof(addr.sun_path));
		addr.sun_len = sizeof(addr);
		if ((fds[0].fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0 ||
		    bind(fds[0].fd, (struct sockaddr *)&addr, sizeof(addr)) !=
			0 ||
		    listen(fds[0].fd, 4) != 0)
			err(1, "private daemon listener");
	}
	fflush(NULL);
	if ((childpid = fork()) < 0)
		err(1, "fork resolver client");
	if (childpid == 0) {
		close(dns);
		if (fds[0].fd >= 0)
			close(fds[0].fd);
		_exit(client(scenario));
	}
	for (turns = 0; turns < 100; turns++) {
		if (poll(fds, 2, 100) < 0 && errno != EINTR)
			err(1, "poll fixture");
		if ((fds[0].revents & POLLIN) != 0) {
			if (requests == 2)
				errx(1, "too many IPC queries");
			clients[requests++] = ipc_reply(fds[0].fd, scenario);
		}
		if ((fds[1].revents & POLLIN) != 0) {
			dns_reply(dns);
			dnsrequests++;
		}
		if (waitpid(childpid, &status, WNOHANG) == childpid)
			break;
	}
	if (turns == 100) {
		kill(childpid, SIGKILL);
		waitpid(childpid, &status, 0);
		errx(1, "resolver client exceeded fixture deadline");
	}
	for (unsigned int i = 0; i < requests; i++)
		close(clients[i]);
	if (fds[0].fd >= 0)
		close(fds[0].fd);
	unlink(sockpath);
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
		errx(1, "scenario %d client failed: status %d", scenario,
		    status);
	if ((scenario == SUCCESS || scenario == NEGATIVE ||
		scenario == MALFORMED) &&
	    requests != 2)
		errx(1, "module did not send both family requests");
	if (((scenario == MALFORMED || scenario == UNAVAILABLE) &&
		dnsrequests == 0) ||
	    ((scenario == SUCCESS || scenario == NEGATIVE ||
		 scenario == HOSTS) &&
		dnsrequests != 0))
		errx(1, "unexpected fallback DNS use in scenario %d", scenario);
	printf("scenario %d passed: IPC %u, DNS fallback %u\n", scenario,
	    requests, dnsrequests);
}

int
main(int argc, char **argv)
{
	struct sockaddr_in addr = { .sin_len = sizeof(addr),
		.sin_family = AF_INET,
		.sin_port = htons(53) };
	struct timeval timeout = { .tv_sec = 2 };
	char buf[1024];
	int dns;
	size_t i;

	if (argc != 2)
		errx(1, "usage: ipc_test /absolute/path/to/nss_dnssd.so.1");
	if (geteuid() != 0) {
		puts("SKIP: private chroot integration requires root");
		return (0);
	}
	if (mkdtemp(rootdir) == NULL)
		err(1, "mkdtemp");
	atexit(cleanup);
	for (i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++) {
		path(buf, sizeof(buf), dirs[i]);
		if (mkdir(buf, 0755) != 0)
			err(1, "mkdir fixture");
	}
	for (i = 0; i < sizeof(libs) / sizeof(libs[0]); i++)
		copy_file(libs[i], libs[i]);
	copy_file(argv[1], "/usr/lib/nss_dnssd.so.1");
	text_file("/etc/hosts", "198.51.100.88 file.example\n");
	text_file("/etc/nsswitch.conf",
	    "hosts: files dnssd [NOTFOUND=return] dns\n");
	text_file("/etc/resolv.conf",
	    "nameserver 127.0.0.1\noptions attempts:1 timeout:1\n");
	inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
	if ((dns = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		err(1, "create loopback DNS socket");
	if (bind(dns, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
		if (errno == EADDRINUSE || errno == EADDRNOTAVAIL) {
			puts("SKIP: loopback DNS port unavailable");
			close(dns);
			return (0);
		}
		err(1, "bind private loopback DNS responder");
	}
	if (setsockopt(dns, SOL_SOCKET, SO_RCVTIMEO, &timeout,
		sizeof(timeout)) != 0)
		err(1, "set DNS responder receive timeout");
	for (enum scenario s = SUCCESS; s <= HOSTS; s++)
		run_case(s, dns);
	close(dns);
	return (0);
}
