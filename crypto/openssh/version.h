/* $MidnightBSD: src/crypto/openssh/version.h,v 1.8 2007/03/13 21:52:10 laffer1 Exp $ */
/* $OpenBSD: version.h,v 1.52 2008/03/27 00:16:49 djm Exp $ */

#define SSH_VERSION             (ssh_version_get())
#define SSH_RELEASE             (ssh_version_get())
#define SSH_VERSION_BASE        "OpenSSH_4.9p1"
#define SSH_VERSION_ADDENDUM    "MidnightBSD-20080406"

const char *ssh_version_get(void);
void ssh_version_set_addendum(const char *add);
#endif /* SSH_VERSION */
