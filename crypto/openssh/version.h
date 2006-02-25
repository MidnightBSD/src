/* $OpenBSD: version.h,v 1.45 2005/08/31 09:28:42 markus Exp $ */
/* $FreeBSD: src/crypto/openssh/version.h,v 1.30.2.1 2005/09/11 16:50:35 des Exp $ */

#ifndef SSH_VERSION

#define SSH_VERSION             (ssh_version_get())
#define SSH_RELEASE             (ssh_version_get())
#define SSH_VERSION_BASE        "OpenSSH_4.2p1"
#define SSH_VERSION_ADDENDUM    "FreeBSD-20050903"

const char *ssh_version_get(void);
void ssh_version_set_addendum(const char *add);
#endif /* SSH_VERSION */
