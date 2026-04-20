
LIB=		systemd
SHLIB_MAJOR=	0
SRCS=		sd-daemon.c sd-journal.c
INCS=		sd-daemon.h sd-journal.h

# Compatibility with legacy libsystemd-daemon naming
SYMLINKS=	libsystemd.so ${LIBDIR}/libsystemd-daemon.so
SYMLINKS+=	libsystemd.a ${LIBDIR}/libsystemd-daemon.a

# Install headers in a systemd/ subdirectory for compatibility
INCSDIR=	${INCLUDEDIR}/systemd

.include <bsd.lib.mk>
