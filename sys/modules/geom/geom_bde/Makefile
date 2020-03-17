# $FreeBSD: stable/11/sys/modules/geom/geom_bde/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/geom/bde
.PATH: ${SRCTOP}/sys/crypto/rijndael
.PATH: ${SRCTOP}/sys/crypto/sha2

KMOD=	geom_bde
SRCS=	g_bde.c g_bde_crypt.c g_bde_lock.c g_bde_work.c
SRCS+=	rijndael-alg-fst.c rijndael-api-fst.c sha256c.c sha512c.c

.include <bsd.kmod.mk>
