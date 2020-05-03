# $FreeBSD: stable/11/lib/libclang_rt/asan_cxx/Makefile 331838 2018-03-31 11:38:16Z dim $

.include <bsd.init.mk>

LIB=		clang_rt.asan_cxx-${CRTARCH}

CXXFLAGS+=	-frtti

SRCS+=		asan/asan_new_delete.cc
SRCS+=		ubsan/ubsan_handlers_cxx.cc
SRCS+=		ubsan/ubsan_type_hash.cc
SRCS+=		ubsan/ubsan_type_hash_itanium.cc

.include <bsd.lib.mk>
