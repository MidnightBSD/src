/* $MidnightBSD: src/crypto/openssh/version.h,v 1.13 2009/05/02 16:56:00 laffer1 Exp $ */
/* $OpenBSD: version.h,v 1.56 2009/06/30 14:54:40 markus Exp $ */

#ifndef SSH_VERSION

#define SSH_VERSION             (ssh_version_get())
#define SSH_RELEASE             (ssh_version_get())
#define SSH_VERSION_BASE        "OpenSSH_5.3p1"
#define SSH_VERSION_ADDENDUM    "MidnightBSD-20091126"

const char *ssh_version_get(void);
void ssh_version_set_addendum(const char *add);
#endif /* SSH_VERSION */
