
CONFS=	openssl.cnf
CONFSDIR=	/etc/ssl
PROG=	openssl
PACKAGE=	openssl

LIBADD=	ssl crypto

.if exists(Makefile.man)
.include "Makefile.man"
.endif
.include "../../lib/libcrypto/Makefile.inc"

CFLAGS+=	-I${LCRYPTO_SRC}/apps
CFLAGS+=	-I${LCRYPTO_SRC}/apps/include
CFLAGS+=	-I${OBJTOP}/secure/lib/libcrypto

SRCS=	app_lib_engine.c app_libctx.c app_params.c app_provider.c app_rand.c
SRCS+=	app_x509.c apps.c apps_ui.c asn1parse.c ca.c ciphers.c cms.c columns.c
SRCS+=	crl.c crl2pkcs7.c dgst.c dhparam.c dsa.c dsaparam.c ec.c ecparam.c
SRCS+=	enc.c engine.c engine_loader.c errstr.c fipsinstall.c fmt.c gendsa.c
SRCS+=	genpkey.c genrsa.c http_server.c info.c kdf.c list.c mac.c names.c
SRCS+=	nseq.c ocsp.c openssl.c opt.c passwd.c pkcs12.c pkcs7.c pkcs8.c pkey.c
SRCS+=	pkeyparam.c pkeyutl.c prime.c progs.c rand.c rehash.c req.c rsa.c
SRCS+=	rsautl.c s_cb.c s_client.c s_server.c s_socket.c s_time.c sess_id.c
SRCS+=	smime.c speed.c spkac.c srp.c storeutl.c tlssrp_depr.c ts.c verify.c
SRCS+=	version.c x509.c

.include <bsd.prog.mk>

.PATH:	${LCRYPTO_SRC}/apps \
	${LCRYPTO_SRC}/apps/lib \
	${.CURDIR}/man
