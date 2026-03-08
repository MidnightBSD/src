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
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pwd.h>

#define SOCKET_PATH "/var/run/aged/aged.sock"

static void
usage(const char *progname)
{
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  Query: %s\n", progname);
    fprintf(stderr, "  Set Age (Root): %s -a <age> <username>\n", progname);
    fprintf(stderr, "  Set DOB (Root): %s -b <YYYY-MM-DD> <username>\n", progname);
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
    int mode = 0; /* 0 = query, 1 = set age, 2 = set dob */

    while ((ch = getopt(argc, argv, "a:b:")) != -1) {
        switch (ch) {
            case 'a':
                mode = 1;
                set_val = optarg;
                break;
            case 'b':
                mode = 2;
                set_val = optarg;
                break;
            default:
                usage(argv[0]);
        }
    }

    if (mode > 0) {
        if (optind >= argc) usage(argv[0]);
        target_user = argv[optind];
    }

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
        write(fd, "GET", 3);
    } else {
        struct passwd *pw = getpwnam(target_user);
        if (!pw) {
            fprintf(stderr, "Unknown user: %s\n", target_user);
            close(fd);
            exit(1);
        }

        if (mode == 1) {
            snprintf(buf, sizeof(buf), "SET %d age %s", pw->pw_uid, set_val);
        } else {
            snprintf(buf, sizeof(buf), "SET %d dob %s", pw->pw_uid, set_val);
        }
        write(fd, buf, strlen(buf));
    }

    memset(buf, 0, sizeof(buf));
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    if (n > 0) {
        printf("%s\n", buf);
    }

    close(fd);
    return 0;
}
