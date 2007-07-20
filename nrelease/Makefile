# $MidnightBSD$
# $DragonFly: src/nrelease/Makefile,v 1.1 2003/12/01 09:59:43 dillon Exp $
#
ISODIR ?= /usr/release
ISOFILE ?= ${ISODIR}/mbsd.iso
ISOROOT = ${ISODIR}/root
OBJSYS= ${.OBJDIR}/../sys
KERNCONF ?= GENERIC

release:	check clean buildworld1 buildkernel1 buildiso mkiso

quickrel:	check clean buildworld2 buildkernel2 buildiso mkiso

realquickrel:	check clean buildiso mkiso

check:
	if [ ! -f /usr/local/bin/mkisofs ]; then \
		echo "You need to install the cdrtools port for this target"; \
		exit 1; fi

buildworld1:
	( cd ${.CURDIR}/..; make buildworld )

buildworld2:
	( cd ${.CURDIR}/..; make -DNOTOOLS -DNOCLEAN buildworld )

buildkernel1:
	( cd ${.CURDIR}/..; make buildkernel KERNCONF=${KERNCONF} )

buildkernel2:
	( cd ${.CURDIR}/..; make -DNOCLEAN buildkernel KERNCONF=${KERNCONF} )

buildiso:
	if [ ! -d ${ISOROOT} ]; then mkdir -p ${ISOROOT}; fi
	( cd ${.CURDIR}/..; make DESTDIR=${ISOROOT} installworld )
	( cd ${.CURDIR}/../etc; make DESTDIR=${ISOROOT} distribution )
	cpdup -X cpignore -o ${.CURDIR}/root ${ISOROOT} -vv
	( cd ${.CURDIR}/..; make DESTDIR=${ISOROOT} \
		installkernel KERNCONF=${KERNCONF} )
	mtree -deU -f ${.CURDIR}/../etc/mtree/BSD.local.dist -p ${ISOROOT}/usr/local/
	mtree -deU -f ${.CURDIR}/../etc/mtree/BSD.var.dist -p ${ISOROOT}/var
	cp /usr/local/bin/mkisofs ${ISOROOT}/usr/local/bin
	cp /usr/local/man/man8/mkisofs.8.gz ${ISOROOT}/usr/local/man/man8

mkiso:
	( cd ${ISOROOT}; mkisofs -b boot/cdboot -no-emul-boot \
		-R -J -V MidnightBSD -o ${ISOFILE} . )

clean:
	if [ -d ${ISOROOT} ]; then chflags -R noschg ${ISOROOT}; fi
	if [ -d ${ISOROOT} ]; then rm -rf ${ISOROOT}; fi

realclean:	clean
	rm -rf ${OBJSYS}/${KERNCONF}

.include <bsd.prog.mk>
