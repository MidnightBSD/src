/* $OpenBSD: version.h,v 1.46 2006/02/01 11:27:22 markus Exp $ */
/* $FreeBSD: src/crypto/openssh/version.h,v 1.32 2006/03/22 20:41:37 des Exp $ */

#ifndef SSH_VERSION

#define SSH_VERSION             (ssh_version_get())
#define SSH_RELEASE             (ssh_version_get())
#define SSH_VERSION_BASE        "OpenSSH_4.3p1"
#define SSH_VERSION_ADDENDUM    "MidnightBSD-20060819"

const char *ssh_version_get(void);
void ssh_version_set_addendum(const char *add);
#endif /* SSH_VERSION */
