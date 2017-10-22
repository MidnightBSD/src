/* $OpenBSD: version.h,v 1.48 2006/11/07 10:31:31 markus Exp $ */
/* $FreeBSD: release/7.0.0/crypto/openssh/version.h 172506 2007-10-10 16:59:15Z cvs2svn $ */

#ifndef SSH_VERSION

#define SSH_VERSION             (ssh_version_get())
#define SSH_RELEASE             (ssh_version_get())
#define SSH_VERSION_BASE        "OpenSSH_4.5p1"
#define SSH_VERSION_ADDENDUM    "FreeBSD-20061110"

const char *ssh_version_get(void);
void ssh_version_set_addendum(const char *add);
#endif /* SSH_VERSION */
