#define SOCKET_FLAGS_PRIV

#include <sys/socket.h>

#ifdef NEED_SOCKET_FLAGS

#include <fcntl.h>

int
_socket(int domain, int type, int protocol)
{
	int s = socket(domain, type & ~(SOCK_CLOEXEC | SOCK_NONBLOCK), protocol);
	int flags;
	if (s == -1)
		return s;

	if (type & SOCK_CLOEXEC) {
		flags = fcntl(s, F_GETFD);
		fcntl(s, F_SETFD, flags | FD_CLOEXEC);
	}

	if (type & SOCK_NONBLOCK) {
		flags = fcntl(s, F_GETFL);
		fcntl(s, F_SETFL, flags | O_NONBLOCK);
	}
	return s;
}

int
_socketpair(int domain, int type, int protocol, int socket_vector[2])
{
	int rc = socketpair(domain, type & ~(SOCK_CLOEXEC | SOCK_NONBLOCK), protocol, socket_vector);
	int flags, i;
	if (rc == -1)
		return rc;

	for (i = 0; i < 2; i++) {
		if (type & SOCK_CLOEXEC) {
			flags = fcntl(socket_vector[i], F_GETFD);
			fcntl(socket_vector[i], F_SETFD, flags | FD_CLOEXEC);
		}

		if (type & SOCK_NONBLOCK) {
			flags = fcntl(socket_vector[i], F_GETFL);
			fcntl(socket_vector[i], F_SETFL, flags | O_NONBLOCK);
		}
	}
	return rc;
}

#endif
