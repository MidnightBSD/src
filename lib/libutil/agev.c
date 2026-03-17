/*-
 * Copyright (c) 2026 Lucas Holt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>

#include "libutil.h"

#define SOCKET_PATH "/var/run/aged/aged.sock"

int *
agev_get_age_bracket(const char *username)
{
	int fd;
	struct sockaddr_un addr;
	char buf[256] = {0};
	int *ages = NULL;
	struct passwd *pw;

	if (username == NULL || username[0] == '\0') {
		errno = EINVAL;
		return NULL;
	}

	pw = getpwnam(username);
	if (!pw) {
		errno = EINVAL;
		return NULL;
	}

	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		return NULL;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
		close(fd);
		return NULL;
	}

	snprintf(buf, sizeof(buf), "GET %d", pw->pw_uid);
	if (write(fd, buf, strlen(buf)) == -1) {
		close(fd);
		return NULL;
	}

	memset(buf, 0, sizeof(buf));
	ssize_t n = read(fd, buf, sizeof(buf) - 1);

	if (n > 0) {
		char *p = strchr(buf, '-');
		if (p) {
			*p = '\0';
			ages = malloc(sizeof(int) * 2);
			if (ages) {
				ages[0] = atoi(buf);
				ages[1] = atoi(p + 1);
			}
		}
	}

	close(fd);
	return ages;
}

int
agev_set_age(const char *username, int age)
{
	int fd;
	struct sockaddr_un addr;
	char buf[256] = {0};
	struct passwd *pw;

	if (age < 2 || age > 125) {
		errno = EINVAL;
		return -1;
	}

	if (geteuid() != 0) {
		errno = EPERM;
		return -1;
	}

	pw = getpwnam(username);
	if (!pw) {
		errno = EINVAL;
		return -1;
	}

	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
		close(fd);
		return -1;
	}

	snprintf(buf, sizeof(buf), "SET %d age %d", pw->pw_uid, age);
	if (write(fd, buf, strlen(buf)) == -1) {
		close(fd);
		return -1;
	}
	
	close(fd);
	return 0;
}

int
agev_set_dob(const char *username, const char *dob)
{
	int fd;
	struct sockaddr_un addr;
	char buf[256] = {0};
	struct passwd *pw;

	if (dob == NULL || dob[0] == '\0') {
		errno = EINVAL;
		return -1;
	}

	size_t dob_len = strlen(dob);
	if (dob_len != 10 ||
	    !isdigit((unsigned char)dob[0]) ||
	    !isdigit((unsigned char)dob[1]) ||
	    !isdigit((unsigned char)dob[2]) ||
	    !isdigit((unsigned char)dob[3]) ||
	    dob[4] != '-' ||
	    !isdigit((unsigned char)dob[5]) ||
	    !isdigit((unsigned char)dob[6]) ||
	    dob[7] != '-' ||
	    !isdigit((unsigned char)dob[8]) ||
	    !isdigit((unsigned char)dob[9])) {
		errno = EINVAL;
		return -1;
	}

	if (geteuid() != 0) {
		errno = EPERM;
		return -1;
	}

	pw = getpwnam(username);
	if (!pw) {
		errno = EINVAL;
		return -1;
	}

	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
		close(fd);
		return -1;
	}

	snprintf(buf, sizeof(buf), "SET %d dob %s", pw->pw_uid, dob);
	if (write(fd, buf, strlen(buf)) == -1) {
		close(fd);
		return -1;
	}
	
	close(fd);
	return 0;
}
