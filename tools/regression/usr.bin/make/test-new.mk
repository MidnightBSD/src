# $FreeBSD: stable/9/tools/regression/usr.bin/make/test-new.mk 237100 2012-06-14 20:44:56Z obrien $

NEW_DIR!=	make -C ${.CURDIR}/../../../../usr.bin/make -V .OBJDIR

all:
	rm -rf /tmp/${USER}.make.test
	env MAKE_PROG=${NEW_DIR}/make ${.SHELL} ./all.sh

.include <bsd.obj.mk>
