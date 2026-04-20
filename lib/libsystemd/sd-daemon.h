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

#ifndef _SD_DAEMON_H_
#define _SD_DAEMON_H_

#include <sys/types.h>
#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * sd-daemon.h - Reference-compatible shim for libsystemd sd-daemon APIs.
 */

#define SD_LISTEN_FDS_START 3

int sd_listen_fds(int unset_environment);
int sd_listen_fds_with_names(int unset_environment, char ***names);

int sd_is_socket(int fd, int family, int type, int listening);
int sd_is_socket_inet(int fd, int family, int type, int listening, uint16_t port);
int sd_is_socket_unix(int fd, int type, int listening, const char *path, size_t length);
int sd_is_mq(int fd, const char *path);
int sd_is_fifo(int fd, const char *path);
int sd_is_special(int fd, const char *path);

int sd_notify(int unset_environment, const char *state);
int sd_notifyf(int unset_environment, const char *format, ...) __attribute__((format(printf, 2, 3)));
int sd_pid_notify(pid_t pid, int unset_environment, const char *state);
int sd_pid_notifyf(pid_t pid, int unset_environment, const char *format, ...) __attribute__((format(printf, 3, 4)));

int sd_booted(void);

int sd_watchdog_enabled(int unset_environment, uint64_t *usec);

#ifdef __cplusplus
}
#endif

#endif /* _SD_DAEMON_H_ */
