/* $FreeBSD$  */

#ifndef _DIST_H_INCLUDE
#define _DIST_H_INCLUDE

/* Bitfields for distributions - hope we never have more than 32! :-) */
#define DIST_BASE		0x00001
#define DIST_GAMES		0x00002
#define DIST_MANPAGES		0x00004
#define DIST_PROFLIBS		0x00008
#define DIST_DICT		0x00010
#define DIST_SRC		0x00020
/* Documentation from FreeBSD docproj */
#define DIST_DOC		0x00040
#define DIST_INFO		0x00080
#define DIST_CATPAGES		0x00200
#define DIST_PORTS		0x00400
#define DIST_LOCAL		0x00800
#if defined(__amd64__) || defined(__powerpc64__)
#define DIST_LIB32		0x01000
#endif
#define	DIST_KERNEL		0x02000
/* Userland documentation */
#define DIST_DOCUSERLAND	0x04000
#define DIST_ALL		0xFFFFF

/* Subtypes for DOC packages */
#define DIST_DOC_BN		0x00001
#define DIST_DOC_DA		0x00002
#define DIST_DOC_DE		0x00004
#define DIST_DOC_EL		0x00008
#define DIST_DOC_EN		0x00010
#define DIST_DOC_ES		0x00020
#define DIST_DOC_FR		0x00040
#define DIST_DOC_HU		0x00080
#define DIST_DOC_IT		0x00100
#define DIST_DOC_JA		0x00200
#define DIST_DOC_MN		0x00400
#define DIST_DOC_NL		0x00800
#define DIST_DOC_PL		0x01000
#define DIST_DOC_PT		0x02000
#define DIST_DOC_RU		0x04000
#define DIST_DOC_SR		0x08000
#define DIST_DOC_TR		0x10000
#define DIST_DOC_ZH_CN		0x20000
#define DIST_DOC_ZH_TW		0x40000
#define DIST_DOC_ALL		0xFFFFF

/* Subtypes for SRC distribution */
#define DIST_SRC_BASE		0x00001
#define DIST_SRC_CONTRIB	0x00002
#define DIST_SRC_GNU		0x00004
#define DIST_SRC_ETC		0x00008
#define DIST_SRC_GAMES		0x00010
#define DIST_SRC_INCLUDE	0x00020
#define DIST_SRC_LIB		0x00040
#define DIST_SRC_LIBEXEC	0x00080
#define DIST_SRC_TOOLS		0x00100
#define DIST_SRC_RELEASE	0x00200
#define DIST_SRC_SBIN		0x00400
#define DIST_SRC_SHARE		0x00800
#define DIST_SRC_SYS		0x01000
#define DIST_SRC_UBIN		0x02000
#define DIST_SRC_USBIN		0x04000
#define DIST_SRC_BIN		0x08000
#define DIST_SRC_SCRYPTO	0x10000
#define DIST_SRC_SSECURE	0x20000
#define DIST_SRC_SKERBEROS5	0x40000
#define DIST_SRC_RESCUE		0x80000
#define DIST_SRC_CDDL		0x100000
#define DIST_SRC_ALL		0x3FFFFF

/* Subtypes for KERNEL distribution */
#define DIST_KERNEL_GENERIC	0x00001
#define DIST_KERNEL_SMP		0x00002
#define DIST_KERNEL_ALL		0xFFFFF

#ifdef __powerpc64__
#define GENERIC_KERNEL_NAME	"GENERIC64"
#else
#define GENERIC_KERNEL_NAME	"GENERIC"
#endif

/* Canned distribution sets */

#define _DIST_USER \
	( DIST_BASE | DIST_KERNEL | DIST_DOC | DIST_DOCUSERLAND | DIST_MANPAGES | DIST_DICT )

#define _DIST_DEVELOPER \
	( _DIST_USER | DIST_PROFLIBS | DIST_INFO | DIST_SRC )

#endif	/* _DIST_H_INCLUDE */
