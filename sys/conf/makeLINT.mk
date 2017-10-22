# $FreeBSD: release/7.0.0/sys/conf/makeLINT.mk 174854 2007-12-22 06:32:46Z cvs2svn $

all:
	@echo "make LINT only"

clean:
	rm -f LINT

NOTES=	../../conf/NOTES NOTES
LINT: ${NOTES} ../../conf/makeLINT.sed
	cat ${NOTES} | sed -E -n -f ../../conf/makeLINT.sed > ${.TARGET}
