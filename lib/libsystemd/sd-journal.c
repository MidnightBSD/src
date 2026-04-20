/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
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
#include <sys/uio.h>

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "sd-journal.h"

int
sd_journal_print(int priority, const char *format, ...)
{
	va_list ap;
	int r;

	va_start(ap, format);
	r = sd_journal_printv(priority, format, ap);
	va_end(ap);

	return (r);
}

int
sd_journal_printv(int priority, const char *format, va_list ap)
{
	vsyslog(priority, format, ap);
	return (0);
}

int
sd_journal_send(const char *format, ...)
{
	va_list ap;
	const char *p;
	const char *msg = NULL;
	int priority = LOG_INFO;

	va_start(ap, format);
	p = format;
	while (p != NULL) {
		if (strncmp(p, "MESSAGE=", 8) == 0)
			msg = p + 8;
		else if (strncmp(p, "PRIORITY=", 9) == 0) {
			int pri = atoi(p + 9);
			if (pri >= LOG_EMERG && pri <= LOG_DEBUG)
				priority = pri;
		}

		p = va_arg(ap, const char *);
	}
	va_end(ap);

	if (msg)
		syslog(priority, "%s", msg);

	return (0);
}

int
sd_journal_sendv(const struct iovec *iov, int n)
{
	const char *msg = NULL;
	int priority = LOG_INFO;

	for (int i = 0; i < n; i++) {
		const char *p = (const char *)iov[i].iov_base;
		size_t len = iov[i].iov_len;

		if (len > 8 && strncmp(p, "MESSAGE=", 8) == 0)
			msg = p + 8;
		else if (len > 9 && strncmp(p, "PRIORITY=", 9) == 0) {
			int pri = atoi(p + 9);
			if (pri >= LOG_EMERG && pri <= LOG_DEBUG)
				priority = pri;
		}
	}

	if (msg)
		syslog(priority, "%s", msg);

	return (0);
}

int
sd_journal_perror(const char *message)
{
	syslog(LOG_ERR, "%s: %m", message);
	return (0);
}
