/* $MidnightBSD: src/crypto/openssh/version.h,v 1.15 2011/02/04 13:35:56 laffer1 Exp $ */
/* $OpenBSD: version.h,v 1.60 2011/01/22 09:18:53 djm Exp $ */

#ifndef SSH_VERSION

#define SSH_VERSION             (ssh_version_get())
#define SSH_RELEASE             (ssh_version_get())
#define SSH_VERSION_BASE        "OpenSSH_5.7"
#define SSH_VERSION_ADDENDUM    "MidnightBSD-20110204"

const char *ssh_version_get(void);
void ssh_version_set_addendum(const char *add);
#endif /* SSH_VERSION */
