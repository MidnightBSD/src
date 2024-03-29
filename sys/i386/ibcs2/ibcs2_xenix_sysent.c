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
#include <i386/ibcs2/ibcs2_xenix.h>

#define AS(name) (sizeof(struct name) / sizeof(register_t))

/* The casts are bogus but will do for now. */
struct sysent xenix_sysent[] = {
	{ 0, (sy_call_t *)nosys, AUE_NULL, NULL, 0, 0, 0, SY_THR_ABSENT },			/* 0 = nosys */
	{ 0, (sy_call_t *)nosys, AUE_NULL, NULL, 0, 0, 0, SY_THR_ABSENT },			/* 1 = xenix_xlocking */
	{ 0, (sy_call_t *)nosys, AUE_NULL, NULL, 0, 0, 0, SY_THR_ABSENT },			/* 2 = xenix_creatsem */
	{ 0, (sy_call_t *)nosys, AUE_NULL, NULL, 0, 0, 0, SY_THR_ABSENT },			/* 3 = xenix_opensem */
	{ 0, (sy_call_t *)nosys, AUE_NULL, NULL, 0, 0, 0, SY_THR_ABSENT },			/* 4 = xenix_sigsem */
	{ 0, (sy_call_t *)nosys, AUE_NULL, NULL, 0, 0, 0, SY_THR_ABSENT },			/* 5 = xenix_waitsem */
	{ 0, (sy_call_t *)nosys, AUE_NULL, NULL, 0, 0, 0, SY_THR_ABSENT },			/* 6 = xenix_nbwaitsem */
	{ AS(xenix_rdchk_args), (sy_call_t *)xenix_rdchk, AUE_NULL, NULL, 0, 0, 0, SY_THR_STATIC },	/* 7 = xenix_rdchk */
	{ 0, (sy_call_t *)nosys, AUE_NULL, NULL, 0, 0, 0, SY_THR_ABSENT },			/* 8 = nosys */
	{ 0, (sy_call_t *)nosys, AUE_NULL, NULL, 0, 0, 0, SY_THR_ABSENT },			/* 9 = nosys */
	{ AS(xenix_chsize_args), (sy_call_t *)xenix_chsize, AUE_FTRUNCATE, NULL, 0, 0, 0, SY_THR_STATIC },	/* 10 = xenix_chsize */
	{ AS(xenix_ftime_args), (sy_call_t *)xenix_ftime, AUE_NULL, NULL, 0, 0, 0, SY_THR_STATIC },	/* 11 = xenix_ftime */
	{ AS(xenix_nap_args), (sy_call_t *)xenix_nap, AUE_NULL, NULL, 0, 0, 0, SY_THR_STATIC },	/* 12 = xenix_nap */
	{ 0, (sy_call_t *)nosys, AUE_NULL, NULL, 0, 0, 0, SY_THR_ABSENT },			/* 13 = xenix_sdget */
	{ 0, (sy_call_t *)nosys, AUE_NULL, NULL, 0, 0, 0, SY_THR_ABSENT },			/* 14 = xenix_sdfree */
	{ 0, (sy_call_t *)nosys, AUE_NULL, NULL, 0, 0, 0, SY_THR_ABSENT },			/* 15 = xenix_sdenter */
	{ 0, (sy_call_t *)nosys, AUE_NULL, NULL, 0, 0, 0, SY_THR_ABSENT },			/* 16 = xenix_sdleave */
	{ 0, (sy_call_t *)nosys, AUE_NULL, NULL, 0, 0, 0, SY_THR_ABSENT },			/* 17 = xenix_sdgetv */
	{ 0, (sy_call_t *)nosys, AUE_NULL, NULL, 0, 0, 0, SY_THR_ABSENT },			/* 18 = xenix_sdwaitv */
	{ 0, (sy_call_t *)nosys, AUE_NULL, NULL, 0, 0, 0, SY_THR_ABSENT },			/* 19 = nosys */
	{ 0, (sy_call_t *)nosys, AUE_NULL, NULL, 0, 0, 0, SY_THR_ABSENT },			/* 20 = nosys */
	{ 0, (sy_call_t *)xenix_scoinfo, AUE_NULL, NULL, 0, 0, 0, SY_THR_STATIC },	/* 21 = xenix_scoinfo */
	{ 0, (sy_call_t *)nosys, AUE_NULL, NULL, 0, 0, 0, SY_THR_ABSENT },			/* 22 = nosys */
	{ 0, (sy_call_t *)nosys, AUE_NULL, NULL, 0, 0, 0, SY_THR_ABSENT },			/* 23 = nosys */
	{ 0, (sy_call_t *)nosys, AUE_NULL, NULL, 0, 0, 0, SY_THR_ABSENT },			/* 24 = nosys */
	{ 0, (sy_call_t *)nosys, AUE_NULL, NULL, 0, 0, 0, SY_THR_ABSENT },			/* 25 = nosys */
	{ 0, (sy_call_t *)nosys, AUE_NULL, NULL, 0, 0, 0, SY_THR_ABSENT },			/* 26 = nosys */
	{ 0, (sy_call_t *)nosys, AUE_NULL, NULL, 0, 0, 0, SY_THR_ABSENT },			/* 27 = nosys */
	{ 0, (sy_call_t *)nosys, AUE_NULL, NULL, 0, 0, 0, SY_THR_ABSENT },			/* 28 = nosys */
	{ 0, (sy_call_t *)nosys, AUE_NULL, NULL, 0, 0, 0, SY_THR_ABSENT },			/* 29 = nosys */
	{ 0, (sy_call_t *)nosys, AUE_NULL, NULL, 0, 0, 0, SY_THR_ABSENT },			/* 30 = nosys */
	{ 0, (sy_call_t *)nosys, AUE_NULL, NULL, 0, 0, 0, SY_THR_ABSENT },			/* 31 = nosys */
	{ 0, (sy_call_t *)nosys, AUE_NULL, NULL, 0, 0, 0, SY_THR_ABSENT },			/* 32 = xenix_proctl */
	{ 0, (sy_call_t *)nosys, AUE_NULL, NULL, 0, 0, 0, SY_THR_ABSENT },			/* 33 = xenix_execseg */
	{ 0, (sy_call_t *)nosys, AUE_NULL, NULL, 0, 0, 0, SY_THR_ABSENT },			/* 34 = xenix_unexecseg */
	{ 0, (sy_call_t *)nosys, AUE_NULL, NULL, 0, 0, 0, SY_THR_ABSENT },			/* 35 = nosys */
	{ AS(select_args), (sy_call_t *)sys_select, AUE_SELECT, NULL, 0, 0, 0, SY_THR_STATIC },	/* 36 = select */
	{ AS(xenix_eaccess_args), (sy_call_t *)xenix_eaccess, AUE_EACCESS, NULL, 0, 0, 0, SY_THR_STATIC },	/* 37 = xenix_eaccess */
	{ 0, (sy_call_t *)nosys, AUE_NULL, NULL, 0, 0, 0, SY_THR_ABSENT },			/* 38 = xenix_paccess */
	{ AS(ibcs2_sigaction_args), (sy_call_t *)ibcs2_sigaction, AUE_NULL, NULL, 0, 0, 0, SY_THR_STATIC },	/* 39 = ibcs2_sigaction */
	{ AS(ibcs2_sigprocmask_args), (sy_call_t *)ibcs2_sigprocmask, AUE_NULL, NULL, 0, 0, 0, SY_THR_STATIC },	/* 40 = ibcs2_sigprocmask */
	{ AS(ibcs2_sigpending_args), (sy_call_t *)ibcs2_sigpending, AUE_NULL, NULL, 0, 0, 0, SY_THR_STATIC },	/* 41 = ibcs2_sigpending */
	{ AS(ibcs2_sigsuspend_args), (sy_call_t *)ibcs2_sigsuspend, AUE_NULL, NULL, 0, 0, 0, SY_THR_STATIC },	/* 42 = ibcs2_sigsuspend */
	{ AS(ibcs2_getgroups_args), (sy_call_t *)ibcs2_getgroups, AUE_GETGROUPS, NULL, 0, 0, 0, SY_THR_STATIC },	/* 43 = ibcs2_getgroups */
	{ AS(ibcs2_setgroups_args), (sy_call_t *)ibcs2_setgroups, AUE_SETGROUPS, NULL, 0, 0, 0, SY_THR_STATIC },	/* 44 = ibcs2_setgroups */
	{ AS(ibcs2_sysconf_args), (sy_call_t *)ibcs2_sysconf, AUE_NULL, NULL, 0, 0, 0, SY_THR_STATIC },	/* 45 = ibcs2_sysconf */
	{ AS(ibcs2_pathconf_args), (sy_call_t *)ibcs2_pathconf, AUE_PATHCONF, NULL, 0, 0, 0, SY_THR_STATIC },	/* 46 = ibcs2_pathconf */
	{ AS(ibcs2_fpathconf_args), (sy_call_t *)ibcs2_fpathconf, AUE_FPATHCONF, NULL, 0, 0, 0, SY_THR_STATIC },	/* 47 = ibcs2_fpathconf */
	{ AS(ibcs2_rename_args), (sy_call_t *)ibcs2_rename, AUE_RENAME, NULL, 0, 0, 0, SY_THR_STATIC },	/* 48 = ibcs2_rename */
	{ 0, (sy_call_t *)nosys, AUE_NULL, NULL, 0, 0, 0, SY_THR_ABSENT },			/* 49 = nosys */
	{ AS(xenix_utsname_args), (sy_call_t *)xenix_utsname, AUE_NULL, NULL, 0, 0, 0, SY_THR_STATIC },	/* 50 = xenix_utsname */
	{ 0, (sy_call_t *)nosys, AUE_NULL, NULL, 0, 0, 0, SY_THR_ABSENT },			/* 51 = nosys */
	{ 0, (sy_call_t *)nosys, AUE_NULL, NULL, 0, 0, 0, SY_THR_ABSENT },			/* 52 = nosys */
	{ 0, (sy_call_t *)nosys, AUE_NULL, NULL, 0, 0, 0, SY_THR_ABSENT },			/* 53 = nosys */
	{ 0, (sy_call_t *)nosys, AUE_NULL, NULL, 0, 0, 0, SY_THR_ABSENT },			/* 54 = nosys */
	{ AS(getitimer_args), (sy_call_t *)sys_getitimer, AUE_GETITIMER, NULL, 0, 0, 0, SY_THR_STATIC },	/* 55 = getitimer */
	{ AS(setitimer_args), (sy_call_t *)sys_setitimer, AUE_SETITIMER, NULL, 0, 0, 0, SY_THR_STATIC },	/* 56 = setitimer */
};
