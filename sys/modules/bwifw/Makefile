# $MidnightBSD$

.PATH: ${.CURDIR}/../../contrib/dev/bwi

FWNAME=	bwi_v3
FWFILE= wl_apsta.o
_FWIMGS=ucode11 ucode2 ucode4 ucode5 \
	a0g0bsinitvals2 a0g0bsinitvals5 a0g0initvals2 \
	a0g0initvals5 a0g1bsinitvals5 a0g1initvals5 \
	b0g0bsinitvals2 b0g0bsinitvals5 b0g0initvals2 b0g0initvals5 \
	pcm4 pcm5
FWIMGS=	${_FWIMGS:C/.*/&.fw/}
#CLEANFILES+=${FWFILE} ${FWIMGS} v3

#SUBDIR=	fwcutter
#FWCUTTER= fwcutter/b43-fwcutter

KMOD=	${FWNAME}
FIRMWS=	${_FWIMGS:C/.*/&.fw:${KMOD}_&/}

bwi_v3:	${FWIMGS}

# NB: the cutter has v3/v4 pathname prefixes built-in; yech
#${FWIMGS}: ${FWCUTTER} ${FWFILE}
#	ln -s . v3; ${FWCUTTER} -w . ${FWFILE}; rm v3

${FWCUTTER}: fwcutter

# NB: can't get make to use .PATH so do it explicitly
#BWI	= ${.CURDIR}/../../contrib/dev/bwi

#${FWFILE}: ${FWFILE}.uu
#	uudecode -p < ${BWI}/${FWFILE}.uu > ${.TARGET}

.include <bsd.kmod.mk>
