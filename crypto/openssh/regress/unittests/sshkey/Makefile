<<<<<<< HEAD
#	$OpenBSD: Makefile,v 1.11 2021/01/09 12:24:31 dtucker Exp $
=======
#	$OpenBSD: Makefile,v 1.12 2023/01/15 23:35:10 djm Exp $
>>>>>>> c024a4a4b9b8d31c7c8b451791cc3c76d3d143ab

PROG=test_sshkey
SRCS=tests.c test_sshkey.c test_file.c test_fuzz.c common.c

# From usr.bin/ssh
SRCS+=sshbuf-getput-basic.c sshbuf-getput-crypto.c sshbuf-misc.c sshbuf.c
SRCS+=sshbuf-io.c atomicio.c sshkey.c authfile.c cipher.c log.c ssh-rsa.c
SRCS+=ssh-dss.c ssh-ecdsa.c ssh-ed25519.c mac.c umac.c umac128.c hmac.c misc.c
SRCS+=ssherr.c uidswap.c cleanup.c xmalloc.c match.c krl.c fatal.c
SRCS+=addr.c addrmatch.c bitmap.c
<<<<<<< HEAD
SRCS+=ed25519.c hash.c ge25519.c fe25519.c sc25519.c verify.c
=======
SRCS+=ed25519.c hash.c
>>>>>>> c024a4a4b9b8d31c7c8b451791cc3c76d3d143ab
SRCS+=cipher-chachapoly.c chacha.c poly1305.c ssh-ecdsa-sk.c ssh-sk.c
SRCS+=ssh-ed25519-sk.c sk-usbhid.c

SRCS+=digest-openssl.c
#SRCS+=digest-libc.c
SRCS+=utf8.c

REGRESS_TARGETS=run-regress-${PROG}

run-regress-${PROG}: ${PROG}
	env ${TEST_ENV} ./${PROG} ${UNITTEST_ARGS} -d ${.CURDIR}/testdata

.include <bsd.regress.mk>

