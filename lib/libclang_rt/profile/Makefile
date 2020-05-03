# $FreeBSD: stable/11/lib/libclang_rt/profile/Makefile 331838 2018-03-31 11:38:16Z dim $

.include <bsd.init.mk>

LIB=		clang_rt.profile-${CRTARCH}

CFLAGS+=	-DCOMPILER_RT_HAS_ATOMICS=1
CFLAGS+=	-DCOMPILER_RT_HAS_FCNTL_LCK=1
CFLAGS+=	-DCOMPILER_RT_HAS_UNAME=1

# This is needed for --coverage
CFLAGS+=	-fvisibility=default

SRCS+=		profile/GCDAProfiling.c
SRCS+=		profile/InstrProfiling.c
SRCS+=		profile/InstrProfilingBuffer.c
SRCS+=		profile/InstrProfilingFile.c
SRCS+=		profile/InstrProfilingMerge.c
SRCS+=		profile/InstrProfilingMergeFile.c
SRCS+=		profile/InstrProfilingNameVar.c
SRCS+=		profile/InstrProfilingPlatformDarwin.c
SRCS+=		profile/InstrProfilingPlatformLinux.c
SRCS+=		profile/InstrProfilingPlatformOther.c
SRCS+=		profile/InstrProfilingRuntime.cc
SRCS+=		profile/InstrProfilingUtil.c
SRCS+=		profile/InstrProfilingValue.c
SRCS+=		profile/InstrProfilingWriter.c

.include <bsd.lib.mk>
