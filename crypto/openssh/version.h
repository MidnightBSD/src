/* $MidnightBSD: src/crypto/openssh/version.h,v 1.10 2008/04/06 06:00:19 laffer1 Exp $ */
/* $OpenBSD: version.h,v 1.53 2008/04/03 09:50:14 djm Exp $ */

#ifndef SSH_VERSION

#define SSH_VERSION             (ssh_version_get())
#define SSH_RELEASE             (ssh_version_get())
#define SSH_VERSION_BASE        "OpenSSH_5.0p1"
#define SSH_VERSION_ADDENDUM    "MidnightBSD-20080417"

const char *ssh_version_get(void);
void ssh_version_set_addendum(const char *add);
#endif /* SSH_VERSION */
