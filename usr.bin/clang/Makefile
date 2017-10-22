# $FreeBSD: stable/9/usr.bin/clang/Makefile 245431 2013-01-14 19:51:26Z dim $

.include <bsd.own.mk>

SUBDIR=	clang clang-tblgen tblgen

.if ${MK_CLANG_EXTRAS} != "no" && !defined(TOOLS_PREFIX)
SUBDIR+=bugpoint \
	llc \
	lli \
	llvm-ar \
	llvm-as \
	llvm-bcanalyzer \
	llvm-diff \
	llvm-dis \
	llvm-extract \
	llvm-link \
	llvm-mc \
	llvm-nm \
	llvm-objdump \
	llvm-prof \
	llvm-ranlib \
	llvm-rtdyld \
	macho-dump \
	opt
.endif

.include <bsd.subdir.mk>
