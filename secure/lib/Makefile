.include <src.opts.mk>

SUBDIR= 
.if ${MK_OPENSSL} != "no"
SUBDIR+=libcrypto libssl libmj \
	.WAIT \
	libnetpgp
.if ${MK_OPENSSH} != "no"
SUBDIR+=libssh
.endif
.endif

SUBDIR_DEPEND_libmj= libcrypto
SUBDIR_DEPEND_libnetpgp= libcrypto libmj

SUBDIR.${MK_TESTS}+= tests

.include <bsd.subdir.mk>
