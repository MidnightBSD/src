/*
 * System call switch table.
 *
 * DO NOT EDIT-- this file is automatically @generated.
 */

#include <sys/param.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <i386/ibcs2/ibcs2_types.h>
#include <i386/ibcs2/ibcs2_signal.h>
#include <i386/ibcs2/ibcs2_proto.h>
#include <i386/ibcs2/ibcs2_xenix.h>

#define AS(name) (sizeof(struct name) / sizeof(register_t))

/* The casts are bogus but will do for now. */
struct sysent isc_sysent[] = {
	{ 0, (sy_call_t *)nosys, AUE_NULL, NULL, 0, 0, 0, SY_THR_ABSENT },			/* 0 = nosys */
	{ 0, (sy_call_t *)nosys, AUE_NULL, NULL, 0, 0, 0, SY_THR_ABSENT },			/* 1 = isc_setostype */
	{ AS(ibcs2_rename_args), (sy_call_t *)ibcs2_rename, AUE_RENAME, NULL, 0, 0, 0, SY_THR_STATIC },	/* 2 = ibcs2_rename */
	{ AS(ibcs2_sigaction_args), (sy_call_t *)ibcs2_sigaction, AUE_NULL, NULL, 0, 0, 0, SY_THR_STATIC },	/* 3 = ibcs2_sigaction */
	{ AS(ibcs2_sigprocmask_args), (sy_call_t *)ibcs2_sigprocmask, AUE_NULL, NULL, 0, 0, 0, SY_THR_STATIC },	/* 4 = ibcs2_sigprocmask */
	{ AS(ibcs2_sigpending_args), (sy_call_t *)ibcs2_sigpending, AUE_NULL, NULL, 0, 0, 0, SY_THR_STATIC },	/* 5 = ibcs2_sigpending */
	{ AS(getgroups_args), (sy_call_t *)sys_getgroups, AUE_GETGROUPS, NULL, 0, 0, 0, SY_THR_STATIC },	/* 6 = getgroups */
	{ AS(setgroups_args), (sy_call_t *)sys_setgroups, AUE_SETGROUPS, NULL, 0, 0, 0, SY_THR_STATIC },	/* 7 = setgroups */
	{ AS(ibcs2_pathconf_args), (sy_call_t *)ibcs2_pathconf, AUE_PATHCONF, NULL, 0, 0, 0, SY_THR_STATIC },	/* 8 = ibcs2_pathconf */
	{ AS(ibcs2_fpathconf_args), (sy_call_t *)ibcs2_fpathconf, AUE_FPATHCONF, NULL, 0, 0, 0, SY_THR_STATIC },	/* 9 = ibcs2_fpathconf */
	{ 0, (sy_call_t *)nosys, AUE_NULL, NULL, 0, 0, 0, SY_THR_ABSENT },			/* 10 = nosys */
	{ AS(ibcs2_wait_args), (sy_call_t *)ibcs2_wait, AUE_WAIT4, NULL, 0, 0, 0, SY_THR_STATIC },	/* 11 = ibcs2_wait */
	{ 0, (sy_call_t *)sys_setsid, AUE_SETSID, NULL, 0, 0, 0, SY_THR_STATIC },	/* 12 = setsid */
	{ 0, (sy_call_t *)sys_getpid, AUE_GETPID, NULL, 0, 0, 0, SY_THR_STATIC },	/* 13 = getpid */
	{ 0, (sy_call_t *)nosys, AUE_NULL, NULL, 0, 0, 0, SY_THR_ABSENT },			/* 14 = isc_adduser */
	{ 0, (sy_call_t *)nosys, AUE_NULL, NULL, 0, 0, 0, SY_THR_ABSENT },			/* 15 = isc_setuser */
	{ AS(ibcs2_sysconf_args), (sy_call_t *)ibcs2_sysconf, AUE_NULL, NULL, 0, 0, 0, SY_THR_STATIC },	/* 16 = ibcs2_sysconf */
	{ AS(ibcs2_sigsuspend_args), (sy_call_t *)ibcs2_sigsuspend, AUE_NULL, NULL, 0, 0, 0, SY_THR_STATIC },	/* 17 = ibcs2_sigsuspend */
	{ AS(ibcs2_symlink_args), (sy_call_t *)ibcs2_symlink, AUE_SYMLINK, NULL, 0, 0, 0, SY_THR_STATIC },	/* 18 = ibcs2_symlink */
	{ AS(ibcs2_readlink_args), (sy_call_t *)ibcs2_readlink, AUE_READLINK, NULL, 0, 0, 0, SY_THR_STATIC },	/* 19 = ibcs2_readlink */
	{ 0, (sy_call_t *)nosys, AUE_NULL, NULL, 0, 0, 0, SY_THR_ABSENT },			/* 20 = isc_getmajor */
};
