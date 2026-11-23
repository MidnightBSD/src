/* SPDX-License-Identifier: BSD-2-Clause */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>

#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int
main(int argc, char **argv)
{
	struct sockaddr_un sun;
	struct stat sb;
	const char *path;
	char gate[sizeof(sun.sun_path)];
	char *slash;
	int fd, i;

	if (argc == 5 && strcmp(argv[1], "query") == 0) {
		struct group *wheel;
		struct passwd *pw;
		gid_t groups[2];

		wheel = getgrnam("wheel");
		if (wheel == NULL)
			return (1);
		groups[1] = wheel->gr_gid;
		pw = getpwnam("nobody");
		if (pw == NULL)
			return (1);
		groups[0] = pw->pw_gid;
		if (setgid(pw->pw_gid) == -1 || setgroups(2, groups) == -1 ||
		    setuid(pw->pw_uid) == -1)
			return (1);
		execl(argv[2], argv[2], "-s", argv[3], "status", argv[4],
		    (char *)NULL);
		return (1);
	}
	if (argc == 1) {
		/* No supervisor sockets should survive exec into the service. */
		for (i = 3; i < 256; i++) {
			if (fstat(i, &sb) == 0 && S_ISSOCK(sb.st_mode))
				return (1);
		}
		path = getenv("NOTIFY_SOCKET");
		if (path == NULL || strlcpy(gate, path, sizeof(gate)) >=
		    sizeof(gate))
			return (1);
		slash = strrchr(gate, '/');
		if (slash == NULL)
			return (1);
		*slash = '\0';
		slash = strrchr(gate, '/');
		if (slash == NULL)
			return (1);
		strlcpy(slash, "/notify.gate", sizeof(gate) - (slash - gate));
		for (i = 0; i < 200 && access(gate, F_OK) == -1; i++)
			usleep(100000);
		if (access(gate, F_OK) == -1)
			return (1);
	} else if (argc == 2)
		path = argv[1];
	else
		return (1);
	if (path == NULL || strlen(path) >= sizeof(sun.sun_path))
		return (1);
	fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (fd == -1)
		return (1);
	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;
	strlcpy(sun.sun_path, path, sizeof(sun.sun_path));
	if (sendto(fd, "READY=1", 7, 0, (struct sockaddr *)&sun,
	    sizeof(sun)) != 7)
		return (1);
	close(fd);
	if (argc == 1)
		(void)sleep(30);
	return (0);
}
