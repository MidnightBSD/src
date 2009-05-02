/* $MidnightBSD: src/crypto/openssh/version.h,v 1.11 2008/04/18 00:20:44 laffer1 Exp $ */
/* $OpenBSD: version.h,v 1.53 2008/04/03 09:50:14 djm Exp $ */

#ifndef SSH_VERSION

#define SSH_VERSION             (ssh_version_get())
#define SSH_RELEASE             (ssh_version_get())
#define SSH_VERSION_BASE        "OpenSSH_5.2p1"
#define SSH_VERSION_ADDENDUM    "MidnightBSD-20090502"

const char *ssh_version_get(void);
void ssh_version_set_addendum(const char *add);
#endif /* SSH_VERSION */
