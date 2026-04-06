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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pwd.h>
#include <time.h>
#include <grp.h>
#include <sys/wait.h>
#include <fcntl.h>

#define SOCKET_PATH "/var/run/aged/aged.sock"

static int valid_age(const char *);
static int valid_dob(const char *);
static int calculate_age(const char *s);
static int update_age_groups(const char *, int);
static int run_pw_command(const char *, const char *, const char *);
static ssize_t write_all(int, const void *, size_t);
static ssize_t read_all(int, void *, size_t);


static void
usage(const char *progname)
{
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "  Query: %s\n", progname);
	fprintf(stderr, "  Set Age (Root): %s -a <age> <username>\n", progname);
	fprintf(stderr, "  Set DOB (Root): %s -b <YYYY-MM-DD> <username>\n", progname);
	fprintf(stderr, "  Set Region (Root): %s -r <region>\n", progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	int fd;
	struct sockaddr_un addr;
	char buf[256] = {0};
	int ch;
	char *set_val = NULL;
	char *target_user = NULL;
	int mode = 0;		/* 0 = query, 1 = set age, 2 = set dob, 3 = set region */
	int update_failed = 0;

	while ((ch = getopt(argc, argv, "a:b:r:")) != -1) {
		switch (ch) {
		case 'a':
			if (!valid_age(optarg))
				errx(1, "invalid age '%s'", optarg);
			mode = 1;
			set_val = optarg;
			break;
		case 'b':
			if (!valid_dob(optarg))
				errx(1, "invalid date of birth '%s' (expected YYYY-MM-DD)", optarg);
			mode = 2;
			set_val = optarg;
			break;
		case 'r':
			mode = 3;
			set_val = optarg;
			break;
		default:
			usage(argv[0]);
		}
	}

	if (mode > 0 && mode < 3) {
		if (optind >= argc)
			usage(argv[0]);
		target_user = argv[optind];
	} else if (mode == 3) {
		if (optind < argc)
			usage(argv[0]);
	}

	if (mode > 0 && geteuid() != 0)
		errx(1, "root privileges required for this operation");

	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
		perror("connect (is aged running?)");
		exit(1);
	}

	if (mode == 0) {
		if (write_all(fd, "GET", 3) == -1) {
			perror("write");
			exit(1);
		}
	} else if (mode == 3) {
		snprintf(buf, sizeof(buf), "REG %s", set_val);
		if (write_all(fd, buf, strlen(buf)) == -1) {
			perror("write");
			exit(1);
		}
	} else {
		struct passwd *pw = getpwnam(target_user);
		int age = -1;

		if (!pw) {
			fprintf(stderr, "Unknown user: %s\n", target_user);
			close(fd);
			exit(1);
		}

		if (mode == 1) {
			snprintf(buf, sizeof(buf), "SET %d age %s", pw->pw_uid, set_val);
			age = atoi(set_val);
		} else if (mode == 2) {
			snprintf(buf, sizeof(buf), "SET %d dob %s", pw->pw_uid, set_val);
		
			age = calculate_age(set_val);
			if (age < 0) {
				fprintf(stderr, "Failed to compute age from dob '%s'\n", set_val);
				close(fd);
				exit(1);
			}
		}
		if (write_all(fd, buf, strlen(buf)) == -1) {
			perror("write");
			exit(1);
		}

		memset(buf, 0, sizeof(buf));
		ssize_t n = read_all(fd, buf, sizeof(buf) - 1);

		if (n > 0) {
			printf("%s\n", buf);
			if (strncmp(buf, "ERR", 3) != 0) {
				if (update_age_groups(target_user, age) != 0)
					update_failed = 1;
			} else {
				update_failed = 1;
			}
		} else {
			if (n == -1)
				perror("read");
			update_failed = 1;
		}
	}

	if (mode == 0 || mode == 3) {
		memset(buf, 0, sizeof(buf));
		ssize_t n = read_all(fd, buf, sizeof(buf) - 1);

		if (n > 0) {
			printf("%s\n", buf);
		}
	}

	close(fd);

	if (update_failed)
		return 1;

	return 0;
}

static ssize_t
write_all(int fd, const void *buf, size_t n)
{
	size_t pos = 0;
	ssize_t ret;

	while (pos < n) {
		ret = write(fd, (const char *)buf + pos, n - pos);
		if (ret == -1) {
			if (errno == EINTR)
				continue;
			return -1;
		}
		if (ret == 0)
			break;
		pos += ret;
	}
	return (ssize_t)pos;
}

static ssize_t
read_all(int fd, void *buf, size_t n)
{
	size_t pos = 0;
	ssize_t ret;

	while (pos < n) {
		ret = read(fd, (char *)buf + pos, n - pos);
		if (ret == -1) {
			if (errno == EINTR)
				continue;
			return -1;
		}
		if (ret == 0)
			break;
		pos += ret;
	}
	return (ssize_t)pos;
}

int
valid_age(const char *s)
{
	char *end;
	long v;

	if (s == NULL || *s == '\0')
		return 0;

	errno = 0;
	v = strtol(s, &end, 10);
	if (errno != 0 || *end != '\0')
		return 0;
	if (v < 2 || v > 125)
		return 0;

	return 1;
}

static int
valid_dob(const char *s)
{
	int i;
	long year, month, day;
	char buf[5];
	char *end;

	/* Expect YYYY-MM-DD */
	if (s == NULL || strlen(s) != 10)
		return 0;

	for (i = 0; i < 10; i++) {
		if (i == 4 || i == 7) {
			if (s[i] != '-')
				return 0;
		} else {
			if (!isdigit((unsigned char)s[i]))
				return 0;
		}
	}

	/* Year */
	memcpy(buf, s, 4);
	buf[4] = '\0';
	year = strtol(buf, &end, 10);
	if (*end != '\0')
		return 0;

	/* Month */
	memcpy(buf, s + 5, 2);
	buf[2] = '\0';
	month = strtol(buf, &end, 10);
	if (*end != '\0')
		return 0;

	/* Day */
	memcpy(buf, s + 8, 2);
	buf[2] = '\0';
	day = strtol(buf, &end, 10);
	if (*end != '\0')
		return 0;

	if (year < 1900 || year > 2100)
		return 0;
	if (month < 1 || month > 12)
		return 0;
	if (day < 1 || day > 31)
		return 0;

	return 1;
}	

int
calculate_age(const char *dob_str)
{
	struct tm tm_dob = {0};

	if (strptime(dob_str, "%Y-%m-%d", &tm_dob) == NULL)
		return -1;

	time_t t = time(NULL);
	struct tm *tm_now = localtime(&t);
	if (tm_now == NULL) return -1;

	int age = (tm_now->tm_year + 1900) - (tm_dob.tm_year + 1900);

	if (tm_now->tm_mon < tm_dob.tm_mon ||
	    (tm_now->tm_mon == tm_dob.tm_mon && tm_now->tm_mday < tm_dob.tm_mday)) {
		age--;
	}
	return age;
}

static int
run_pw_command(const char *group, const char *user, const char *action)
{
	pid_t pid;
	int status;
	const char *pw_path = "/usr/sbin/pw";
	char *const argv[] = {
		__DECONST(char *, pw_path),
		__DECONST(char *, "groupmod"), 
		__DECONST(char *, group),
		__DECONST(char *, action), 
		__DECONST(char *, user),
		 NULL
	};
	char *const envp[] = {
		__DECONST(char *, "PATH=/bin:/usr/bin:/sbin:/usr/sbin"),
		 NULL
	};

	pid = fork();
	if (pid == -1) {
		fprintf(stderr, "fork failed: %m");
		return -1;
	} else if (pid == 0) {
		int fd = open("/dev/null", O_WRONLY);
		if (fd != -1) {
			dup2(fd, STDOUT_FILENO);
			dup2(fd, STDERR_FILENO);
			close(fd);
		}
		
		execve(pw_path, argv, envp);
		_exit(127);
	}

	int ret;
	do {
		ret = waitpid(pid, &status, 0);
	} while (ret == -1 && errno == EINTR);

	if (ret == -1) {
		fprintf(stderr, "waitpid failed: %m");
		return -1;
	}

	if (WIFEXITED(status)) {
		return WEXITSTATUS(status);
	}

	return -1;
}

static int
update_age_groups(const char *username, int age)
{
	const char *groups[] = {"age4p", "age13p", "age16p", "age18p"};
	int min_ages[] = {4, 13, 16, 18};
	int num_groups = sizeof(groups) / sizeof(groups[0]);
	int code = 0;

	for (int i = 0; i < num_groups; i++) {
		struct group *grp = getgrnam(groups[i]);
		if (grp == NULL) {
			fprintf(stderr, "Group %s not found.\n", groups[i]);
			code = 1;
			continue;
		}

		int in_group = 0;
		if (grp->gr_mem != NULL) {
			for (int j = 0; grp->gr_mem[j] != NULL; j++) {
				if (strcmp(grp->gr_mem[j], username) == 0) {
					in_group = 1;
					break;
				}
			}
		}

		if (age >= min_ages[i]) {
			if (!in_group) {
				if (run_pw_command(groups[i], username, "-m") != 0) {
					code = 1;
					fprintf(stderr, "Failed to add user %s to group %s\n", username, groups[i]);
				} else {
					fprintf(stderr, "Added user %s to group %s\n", username, groups[i]);
				}
			}
		} else {
			if (in_group) {
				if (run_pw_command(groups[i], username, "-d") != 0) {
					code = 1;
					fprintf(stderr, "Failed to remove user %s from group %s\n", username, groups[i]);
				} else {
					fprintf(stderr, "Removed user %s from group %s\n", username, groups[i]);
				}
			}
		}
	}

	return code;
}
