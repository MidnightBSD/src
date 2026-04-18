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

/*
 * prowlctl(8) - command-line interface to prowld(8).
 *
 * Connects to /var/run/prowld/prowld.sock and sends length-prefixed
 * JSON requests, then prints the JSON response in a human-readable form.
 */

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#include <arpa/inet.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define PROWLD_SOCK_PATH	"/var/run/prowld/prowld.sock"
#define MSG_MAX			65536	/* must match IPC_MSG_MAX in prowld.h */
#define REQ_MAX			1024

/* Exit codes per spec §13.3 */
#define EXIT_OK			0
#define EXIT_NOT_FOUND		1
#define EXIT_FAILED		2
#define EXIT_NOT_PERMITTED	3
#define EXIT_NO_DAEMON		4

static int g_sock = -1;
static bool g_json = false;	/* --json flag */

/*
 * Validate a user-supplied label before embedding it in JSON.
 * Mirrors label_valid() in prowld/job.c to prevent JSON injection.
 */
static bool
validate_label_input(const char *s)
{
	const char *p;

	if (s == NULL || *s == '\0' || *s == '.')
		return (false);
	for (p = s; *p != '\0'; p++) {
		unsigned char c = (unsigned char)*p;
		if (!isalnum(c) && c != '.' && c != '-' && c != '_')
			return (false);
	}
	if (strstr(s, "..") != NULL)
		return (false);
	return (true);
}

static void
usage(void)
{
	fprintf(stderr,
	    "usage: prowlctl [--json] <command> [args]\n"
	    "\n"
	    "Commands:\n"
	    "  list [--enabled] [--failed] [--type=TYPE]\n"
	    "  status <label>\n"
	    "  start <label>\n"
	    "  stop <label>\n"
	    "  restart <label>\n"
	    "  reload <label>\n"
	    "  enable <label>\n"
	    "  disable <label>\n"
	    "  mask <label>\n"
	    "  unmask <label>\n"
	    "  show <label>\n"
	    "  reload-config\n"
	    "  daemon-reexec\n"
	    "  dependencies <label>\n"
	    "  graph [--format=dot]\n"
	    "  logs <label> [--follow] [--lines=N]\n"
	);
	exit(1);
}

static int
connect_daemon(void)
{
	struct sockaddr_un sun;
	int fd;

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd == -1)
		return (-1);

	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;
	strlcpy(sun.sun_path, PROWLD_SOCK_PATH, sizeof(sun.sun_path));

	if (connect(fd, (struct sockaddr *)&sun, sizeof(sun)) == -1) {
		close(fd);
		return (-1);
	}

	return (fd);
}

/*
 * Send exactly len bytes, retrying on EINTR.
 */
static int
send_all(int fd, const void *buf, size_t len)
{
	const char *p = (const char *)buf;
	ssize_t n;

	while (len > 0) {
		n = send(fd, p, len, 0);
		if (n == -1) {
			if (errno == EINTR)
				continue;
			return (-1);
		}
		p += n;
		len -= (size_t)n;
	}
	return (0);
}

/*
 * Send a length-prefixed JSON request.
 */
static int
send_request(int fd, const char *json)
{
	size_t len = strlen(json);
	uint32_t nlen = htonl((uint32_t)len);

	if (send_all(fd, &nlen, 4) == -1)
		return (-1);
	if (send_all(fd, json, len) == -1)
		return (-1);

	return (0);
}

/*
 * Read a length-prefixed JSON response.
 * Caller must free() the returned buffer.
 */
static char *
recv_response(int fd)
{
	uint32_t nlen;
	uint32_t msglen;
	char *buf;
	ssize_t n;
	size_t got = 0;

	/* Read 4-byte length header */
	n = recv(fd, &nlen, 4, MSG_WAITALL);
	if (n != 4)
		return (NULL);

	msglen = ntohl(nlen);
	if (msglen == 0 || msglen > MSG_MAX)
		return (NULL);

	buf = malloc(msglen + 1);
	if (buf == NULL)
		return (NULL);

	while (got < msglen) {
		n = recv(fd, buf + got, msglen - got, 0);
		if (n <= 0) {
			free(buf);
			return (NULL);
		}
		got += (size_t)n;
	}
	buf[msglen] = '\0';
	return (buf);
}

/*
 * Simple JSON field extractor: returns the string value of "key" from a
 * flat JSON object, or NULL.  This handles only simple string values and
 * avoids pulling in a full JSON library for the client.
 */
static const char *
json_get_str(const char *json, const char *key, char *out, size_t outsz)
{
	char search[128];
	const char *p, *q;
	size_t i;

	snprintf(search, sizeof(search), "\"%s\":\"", key);
	p = strstr(json, search);
	if (p == NULL)
		return (NULL);

	p += strlen(search);
	q = p;

	/* Find closing quote, handling simple escapes */
	i = 0;
	while (*q != '\0' && *q != '"' && i < outsz - 1) {
		if (*q == '\\' && *(q + 1) != '\0') {
			out[i++] = *(q + 1);
			q += 2;
		} else {
			out[i++] = *q++;
		}
	}
	out[i] = '\0';
	return (out);
}

/*
 * Check whether the response indicates an error.
 * Returns 0 for ok, non-zero for error.
 */
static int
check_response(const char *resp)
{
	char status[32];
	char msg[256];
	bool has_msg;

	if (resp == NULL) {
		fprintf(stderr, "prowlctl: no response from daemon\n");
		return (EXIT_NO_DAEMON);
	}

	if (json_get_str(resp, "status", status, sizeof(status)) == NULL) {
		fprintf(stderr, "prowlctl: malformed response\n");
		return (EXIT_NO_DAEMON);
	}

	if (strcmp(status, "ok") != 0) {
		has_msg = json_get_str(resp, "message", msg,
		    sizeof(msg)) != NULL;

		if (has_msg)
			fprintf(stderr, "prowlctl: %s\n", msg);
		else
			fprintf(stderr, "prowlctl: error response\n");

		if (has_msg && strcmp(msg, "job not found") == 0)
			return (EXIT_NOT_FOUND);
		if (has_msg && strcmp(msg, "operation not permitted") == 0)
			return (EXIT_NOT_PERMITTED);
		return (EXIT_FAILED);
	}

	return (EXIT_OK);
}

/*
 * Print a JSON object representing a single job in a human-readable table row.
 */
static void
print_job_line(const char *json)
{
	char label[256], state[32], type[32], pid[16];

	json_get_str(json, "label", label, sizeof(label));
	json_get_str(json, "state", state, sizeof(state));
	json_get_str(json, "type", type, sizeof(type));
	json_get_str(json, "pid", pid, sizeof(pid));

	printf("%-40s %-10s %-8s %s\n", label, state, type, pid);
}

/*
 * Print a JSON array of job objects.
 */
static void
print_job_list(const char *json)
{
	const char *p = json;
	char item[1024];
	size_t depth = 0;
	size_t i = 0;
	bool in_string = false;

	printf("%-40s %-10s %-8s %s\n",
	    "LABEL", "STATE", "TYPE", "PID");
	printf("%-40s %-10s %-8s %s\n",
	    "-----", "-----", "----", "---");

	/* Simple JSON array iterator — assumes well-formed input */
	while (*p != '\0') {
		if (!in_string) {
			if (*p == '[' || *p == '{') {
				if (*p == '{' && depth == 1) {
					i = 0;
				}
				depth++;
			} else if (*p == ']' || *p == '}') {
				depth--;
				if (*p == '}' && depth == 1) {
					if (i < sizeof(item) - 1) {
						item[i++] = '}';
						item[i] = '\0';
						print_job_line(item);
					}
					i = 0;
				}
			} else if (*p == '"') {
				in_string = true;
			}
			if (depth >= 2 && i < sizeof(item) - 1)
				item[i++] = *p;
		} else {
			if (*p == '"' && (i == 0 || item[i - 1] != '\\'))
				in_string = false;
			if (depth >= 2 && i < sizeof(item) - 1)
				item[i++] = *p;
		}
		p++;
	}
}

/*
 * Print a job status in human-readable detail.
 */
static void
print_job_detail(const char *json)
{
	char label[256], desc[512], state[32], type[32];
	char pid[16], rc_name[256], enabled[8], restart_count[16];

	json_get_str(json, "label", label, sizeof(label));
	json_get_str(json, "description", desc, sizeof(desc));
	json_get_str(json, "state", state, sizeof(state));
	json_get_str(json, "type", type, sizeof(type));
	json_get_str(json, "pid", pid, sizeof(pid));
	json_get_str(json, "rc_name", rc_name, sizeof(rc_name));
	json_get_str(json, "enabled", enabled, sizeof(enabled));
	json_get_str(json, "restart_count", restart_count,
	    sizeof(restart_count));

	printf("     Label: %s\n", label);
	printf("      Desc: %s\n", desc);
	printf("     State: %s\n", state);
	printf("      Type: %s\n", type);
	printf("   rc_name: %s\n", rc_name);
	printf("   Enabled: %s\n", enabled);
	printf("       PID: %s\n", pid);
	printf("  Restarts: %s\n", restart_count);
}

/* ------------------------------------------------------------------ */
/* Command implementations                                              */
/* ------------------------------------------------------------------ */

static int
do_list(int argc, char *argv[])
{
	char req[REQ_MAX];
	char *resp;
	const char *result;
	int ret;
	bool show_enabled = false;
	bool show_failed = false;
	const char *type_filter = NULL;
	int i;

	for (i = 0; i < argc; i++) {
		if (strcmp(argv[i], "--enabled") == 0)
			show_enabled = true;
		else if (strcmp(argv[i], "--failed") == 0)
			show_failed = true;
		else if (strncmp(argv[i], "--type=", 7) == 0)
			type_filter = argv[i] + 7;
	}

	if (show_enabled)
		snprintf(req, sizeof(req),
		    "{\"verb\":\"list\",\"filter\":{\"state\":\"running\"}}");
	else if (show_failed)
		snprintf(req, sizeof(req),
		    "{\"verb\":\"list\",\"filter\":{\"state\":\"failed\"}}");
	else if (type_filter != NULL) {
		if (!validate_label_input(type_filter)) {
			fprintf(stderr, "prowlctl: invalid type filter '%s'\n",
			    type_filter);
			return (EXIT_FAILED);
		}
		snprintf(req, sizeof(req),
		    "{\"verb\":\"list\",\"filter\":{\"type\":\"%s\"}}",
		    type_filter);
	}
	else
		snprintf(req, sizeof(req), "{\"verb\":\"list\"}");

	if (send_request(g_sock, req) == -1)
		err(EXIT_NO_DAEMON, "send");

	resp = recv_response(g_sock);
	ret = check_response(resp);
	if (ret != EXIT_OK) {
		free(resp);
		return (ret);
	}

	if (g_json) {
		puts(resp);
		free(resp);
		return (EXIT_OK);
	}

	/* Extract result array from response JSON */
	result = strstr(resp, "\"result\":");
	if (result != NULL) {
		result += strlen("\"result\":");
		print_job_list(result);
	}

	free(resp);
	return (EXIT_OK);
}

static int
do_status(int argc, char *argv[])
{
	char req[REQ_MAX];
	char *resp;
	const char *result;
	int ret;

	if (argc < 1) {
		fprintf(stderr, "prowlctl: status requires a label\n");
		return (EXIT_NOT_FOUND);
	}

	if (!validate_label_input(argv[0])) {
		fprintf(stderr, "prowlctl: invalid label '%s'\n", argv[0]);
		return (EXIT_NOT_FOUND);
	}

	snprintf(req, sizeof(req),
	    "{\"verb\":\"status\",\"target\":\"%s\"}", argv[0]);

	if (send_request(g_sock, req) == -1)
		err(EXIT_NO_DAEMON, "send");

	resp = recv_response(g_sock);
	ret = check_response(resp);
	if (ret != EXIT_OK) {
		free(resp);
		return (ret);
	}

	if (g_json) {
		puts(resp);
		free(resp);
		return (EXIT_OK);
	}

	result = strstr(resp, "\"result\":");
	if (result != NULL) {
		result += strlen("\"result\":");
		print_job_detail(result);
	}

	free(resp);
	return (EXIT_OK);
}

static int
do_simple_target(const char *verb, int argc, char *argv[])
{
	char req[REQ_MAX];
	char *resp;
	int ret;

	if (argc < 1) {
		fprintf(stderr, "prowlctl: %s requires a label\n", verb);
		return (EXIT_NOT_FOUND);
	}

	if (!validate_label_input(argv[0])) {
		fprintf(stderr, "prowlctl: invalid label '%s'\n", argv[0]);
		return (EXIT_NOT_FOUND);
	}

	snprintf(req, sizeof(req),
	    "{\"verb\":\"%s\",\"target\":\"%s\"}", verb, argv[0]);

	if (send_request(g_sock, req) == -1)
		err(EXIT_NO_DAEMON, "send");

	resp = recv_response(g_sock);
	ret = check_response(resp);
	if (ret == EXIT_OK && !g_json)
		printf("ok\n");
	else if (g_json && resp != NULL)
		puts(resp);

	free(resp);
	return (ret);
}

/*
 * Print a JSON array of dependency objects.
 * Format: [{"name":"<n>","hard":<bool>}, ...]
 */
static void
print_dep_list(const char *json)
{
	const char *p = json;
	char item[512];
	char name[256];
	char hard[8];
	size_t depth = 0;
	size_t i = 0;
	bool in_string = false;

	while (*p != '\0') {
		if (!in_string) {
			if (*p == '[' || *p == '{') {
				if (*p == '{' && depth == 1)
					i = 0;
				depth++;
			} else if (*p == ']' || *p == '}') {
				depth--;
				if (*p == '}' && depth == 1) {
					if (i < sizeof(item) - 1) {
						item[i++] = '}';
						item[i] = '\0';
						json_get_str(item, "name",
						    name, sizeof(name));
						json_get_str(item, "hard",
						    hard, sizeof(hard));
						printf("  %s %s\n",
						    strcmp(hard, "true") == 0
						    ? "[require]" : "[want]  ",
						    name);
					}
					i = 0;
				}
			} else if (*p == '"')
				in_string = true;
			if (depth >= 2 && i < sizeof(item) - 1)
				item[i++] = *p;
		} else {
			if (*p == '"' && (i == 0 || item[i - 1] != '\\'))
				in_string = false;
			if (depth >= 2 && i < sizeof(item) - 1)
				item[i++] = *p;
		}
		p++;
	}
}

static int
do_dependencies(int argc, char *argv[])
{
	char req[REQ_MAX];
	char *resp;
	const char *result;
	int ret;

	if (argc < 1) {
		fprintf(stderr, "prowlctl: dependencies requires a label\n");
		return (EXIT_NOT_FOUND);
	}

	if (!validate_label_input(argv[0])) {
		fprintf(stderr, "prowlctl: invalid label '%s'\n", argv[0]);
		return (EXIT_NOT_FOUND);
	}

	snprintf(req, sizeof(req),
	    "{\"verb\":\"dependencies\",\"target\":\"%s\"}", argv[0]);

	if (send_request(g_sock, req) == -1)
		err(EXIT_NO_DAEMON, "send");

	resp = recv_response(g_sock);
	ret = check_response(resp);
	if (ret != EXIT_OK) {
		free(resp);
		return (ret);
	}

	if (g_json) {
		puts(resp);
		free(resp);
		return (EXIT_OK);
	}

	printf("Dependencies of %s:\n", argv[0]);
	result = strstr(resp, "\"result\":");
	if (result != NULL) {
		result += strlen("\"result\":");
		print_dep_list(result);
	}

	free(resp);
	return (EXIT_OK);
}

/*
 * Print the dependency graph edges in DOT format.
 * Input: JSON array of {"from":"...","to":"...","hard":<bool>}
 */
static void
print_graph_dot(const char *json)
{
	const char *p = json;
	char item[512];
	char from[256], to[256], hard[8];
	size_t depth = 0;
	size_t i = 0;
	bool in_string = false;

	printf("digraph prowld {\n");
	printf("  rankdir=LR;\n");

	while (*p != '\0') {
		if (!in_string) {
			if (*p == '[' || *p == '{') {
				if (*p == '{' && depth == 1)
					i = 0;
				depth++;
			} else if (*p == ']' || *p == '}') {
				depth--;
				if (*p == '}' && depth == 1) {
					if (i < sizeof(item) - 1) {
						item[i++] = '}';
						item[i] = '\0';
						json_get_str(item, "from",
						    from, sizeof(from));
						json_get_str(item, "to",
						    to, sizeof(to));
						json_get_str(item, "hard",
						    hard, sizeof(hard));
						printf("  \"%s\" -> \"%s\"",
						    from, to);
						if (strcmp(hard, "true") != 0)
							printf(
							    " [style=dashed]");
						printf(";\n");
					}
					i = 0;
				}
			} else if (*p == '"')
				in_string = true;
			if (depth >= 2 && i < sizeof(item) - 1)
				item[i++] = *p;
		} else {
			if (*p == '"' && (i == 0 || item[i - 1] != '\\'))
				in_string = false;
			if (depth >= 2 && i < sizeof(item) - 1)
				item[i++] = *p;
		}
		p++;
	}

	printf("}\n");
}

/*
 * Print the dependency graph edges in plain text.
 */
static void
print_graph_text(const char *json)
{
	const char *p = json;
	char item[512];
	char from[256], to[256], hard[8];
	size_t depth = 0;
	size_t i = 0;
	bool in_string = false;

	printf("%-40s  %-8s  %s\n", "FROM", "TYPE", "TO");
	printf("%-40s  %-8s  %s\n", "----", "----", "--");

	while (*p != '\0') {
		if (!in_string) {
			if (*p == '[' || *p == '{') {
				if (*p == '{' && depth == 1)
					i = 0;
				depth++;
			} else if (*p == ']' || *p == '}') {
				depth--;
				if (*p == '}' && depth == 1) {
					if (i < sizeof(item) - 1) {
						item[i++] = '}';
						item[i] = '\0';
						json_get_str(item, "from",
						    from, sizeof(from));
						json_get_str(item, "to",
						    to, sizeof(to));
						json_get_str(item, "hard",
						    hard, sizeof(hard));
						printf("%-40s  %-8s  %s\n",
						    from,
						    strcmp(hard, "true") == 0
						    ? "require" : "want",
						    to);
					}
					i = 0;
				}
			} else if (*p == '"')
				in_string = true;
			if (depth >= 2 && i < sizeof(item) - 1)
				item[i++] = *p;
		} else {
			if (*p == '"' && (i == 0 || item[i - 1] != '\\'))
				in_string = false;
			if (depth >= 2 && i < sizeof(item) - 1)
				item[i++] = *p;
		}
		p++;
	}
}

static int
do_graph(int argc, char *argv[])
{
	char req[REQ_MAX];
	char *resp;
	const char *result;
	bool dot_format = false;
	int ret, i;

	for (i = 0; i < argc; i++) {
		if (strcmp(argv[i], "--format=dot") == 0)
			dot_format = true;
	}

	snprintf(req, sizeof(req), "{\"verb\":\"graph\"}");

	if (send_request(g_sock, req) == -1)
		err(EXIT_NO_DAEMON, "send");

	resp = recv_response(g_sock);
	ret = check_response(resp);
	if (ret != EXIT_OK) {
		free(resp);
		return (ret);
	}

	if (g_json) {
		puts(resp);
		free(resp);
		return (EXIT_OK);
	}

	result = strstr(resp, "\"result\":");
	if (result != NULL) {
		result += strlen("\"result\":");
		if (dot_format)
			print_graph_dot(result);
		else
			print_graph_text(result);
	}

	free(resp);
	return (EXIT_OK);
}

#define LOGS_TAIL_DEFAULT	50

static int
do_logs(int argc, char *argv[])
{
	char label[256];
	char path[1024];
	bool follow = false;
	int lines = LOGS_TAIL_DEFAULT;
	int i, fd, ret;
	char buf[4096];
	ssize_t n;
	off_t fsize, start;
	int newlines;

	if (argc < 1) {
		fprintf(stderr, "prowlctl: logs requires a label\n");
		return (EXIT_NOT_FOUND);
	}

	if (!validate_label_input(argv[0])) {
		fprintf(stderr, "prowlctl: invalid label '%s'\n", argv[0]);
		return (EXIT_NOT_FOUND);
	}

	strlcpy(label, argv[0], sizeof(label));

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--follow") == 0)
			follow = true;
		else if (strncmp(argv[i], "--lines=", 8) == 0) {
			lines = atoi(argv[i] + 8);
			if (lines <= 0)
				lines = LOGS_TAIL_DEFAULT;
		}
	}

	snprintf(path, sizeof(path), "/var/log/prowld/jobs/%s.log", label);

	fd = open(path, O_RDONLY);
	if (fd == -1) {
		if (errno == ENOENT)
			fprintf(stderr,
			    "prowlctl: no log file for '%s'\n", label);
		else
			fprintf(stderr, "prowlctl: open %s: %s\n",
			    path, strerror(errno));
		return (EXIT_NOT_FOUND);
	}

	/* Seek to show only the last N lines */
	fsize = lseek(fd, 0, SEEK_END);
	if (fsize > 0) {
		start = fsize;
		newlines = 0;
		/* Walk backwards counting newlines */
		while (start > 0 && newlines <= lines) {
			off_t chunk = (start > (off_t)sizeof(buf))
			    ? (off_t)sizeof(buf) : start;
			start -= chunk;
			lseek(fd, start, SEEK_SET);
			n = read(fd, buf, (size_t)chunk);
			if (n <= 0)
				break;
			for (i = (int)n - 1; i >= 0; i--) {
				if (buf[i] == '\n') {
					newlines++;
					if (newlines > lines) {
						start += (off_t)(i + 1);
						break;
					}
				}
			}
			if (newlines > lines)
				break;
		}
		lseek(fd, start, SEEK_SET);
	}

	/* Emit lines from current position */
	while ((n = read(fd, buf, sizeof(buf))) > 0)
		write(STDOUT_FILENO, buf, (size_t)n);

	ret = EXIT_OK;

	if (follow) {
		/* Simple tail -f: seek to end and poll */
		lseek(fd, 0, SEEK_END);
		for (;;) {
			n = read(fd, buf, sizeof(buf));
			if (n > 0) {
				write(STDOUT_FILENO, buf, (size_t)n);
			} else {
				/* No data: sleep briefly and retry */
				struct timespec ts = { 0, 200000000L };
				nanosleep(&ts, NULL);
			}
		}
	}

	close(fd);
	return (ret);
}

static int
do_no_target(const char *verb)
{
	char req[REQ_MAX];
	char *resp;
	int ret;

	snprintf(req, sizeof(req), "{\"verb\":\"%s\"}", verb);

	if (send_request(g_sock, req) == -1)
		err(EXIT_NO_DAEMON, "send");

	resp = recv_response(g_sock);
	ret = check_response(resp);
	if (ret == EXIT_OK && !g_json)
		printf("ok\n");
	else if (g_json && resp != NULL)
		puts(resp);

	free(resp);
	return (ret);
}

int
main(int argc, char *argv[])
{
	const char *cmd;
	int ret = EXIT_OK;
	int i;

	/* Strip argv[0] */
	argc--;
	argv++;

	/* Parse global flags */
	for (i = 0; i < argc; i++) {
		if (strcmp(argv[i], "--json") == 0) {
			g_json = true;
			/* Remove from argv by shifting */
			memmove(&argv[i], &argv[i + 1],
			    (size_t)(argc - i - 1) * sizeof(char *));
			argc--;
			i--;
		}
	}

	if (argc < 1)
		usage();

	cmd = argv[0];
	argc--;
	argv++;

	g_sock = connect_daemon();
	if (g_sock == -1) {
		if (errno == ENOENT || errno == ECONNREFUSED)
			errx(EXIT_NO_DAEMON,
			    "cannot connect to prowld (is it running?)");
		err(EXIT_NO_DAEMON, "connect");
	}

	if (strcmp(cmd, "list") == 0)
		ret = do_list(argc, argv);
	else if (strcmp(cmd, "status") == 0)
		ret = do_status(argc, argv);
	else if (strcmp(cmd, "start") == 0)
		ret = do_simple_target("start", argc, argv);
	else if (strcmp(cmd, "stop") == 0)
		ret = do_simple_target("stop", argc, argv);
	else if (strcmp(cmd, "restart") == 0)
		ret = do_simple_target("restart", argc, argv);
	else if (strcmp(cmd, "reload") == 0)
		ret = do_simple_target("reload", argc, argv);
	else if (strcmp(cmd, "enable") == 0)
		ret = do_simple_target("enable", argc, argv);
	else if (strcmp(cmd, "disable") == 0)
		ret = do_simple_target("disable", argc, argv);
	else if (strcmp(cmd, "mask") == 0)
		ret = do_simple_target("mask", argc, argv);
	else if (strcmp(cmd, "unmask") == 0)
		ret = do_simple_target("unmask", argc, argv);
	else if (strcmp(cmd, "show") == 0)
		ret = do_simple_target("show", argc, argv);
	else if (strcmp(cmd, "reload-config") == 0)
		ret = do_no_target("reload-config");
	else if (strcmp(cmd, "daemon-reexec") == 0)
		ret = do_no_target("daemon-reexec");
	else if (strcmp(cmd, "dependencies") == 0)
		ret = do_dependencies(argc, argv);
	else if (strcmp(cmd, "graph") == 0)
		ret = do_graph(argc, argv);
	else if (strcmp(cmd, "logs") == 0)
		ret = do_logs(argc, argv);
	else {
		fprintf(stderr, "prowlctl: unknown command '%s'\n", cmd);
		usage();
	}

	close(g_sock);
	return (ret);
}
