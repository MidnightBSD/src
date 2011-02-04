/* $MidnightBSD: src/crypto/openssh/version.h,v 1.14 2009/11/26 16:07:27 laffer1 Exp $ */
/* $OpenBSD: version.h,v 1.61 2011/02/04 00:44:43 djm Exp $ */

#ifndef SSH_VERSION

#define SSH_VERSION             (ssh_version_get())
#define SSH_RELEASE             (ssh_version_get())
#define SSH_VERSION_BASE        "OpenSSH_5.8"
#define SSH_VERSION_ADDENDUM    "MidnightBSD-20110204"

const char *ssh_version_get(void);
void ssh_version_set_addendum(const char *add);
#endif /* SSH_VERSION */
