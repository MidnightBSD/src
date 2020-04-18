#include <sys/cdefs.h>
__FBSDID("$FreeBSD: stable/11/sys/amd64/linux/linux_genassym.c 283424 2015-05-24 16:07:11Z dchagin $");

#include <sys/param.h>
#include <sys/assym.h>
#include <sys/systm.h>

#include <amd64/linux/linux.h>
#include <compat/linux/linux_mib.h>

ASSYM(LINUX_RT_SIGF_HANDLER, offsetof(struct l_rt_sigframe, sf_handler));
ASSYM(LINUX_RT_SIGF_UC, offsetof(struct l_rt_sigframe, sf_sc));
ASSYM(LINUX_RT_SIGF_SC, offsetof(struct l_ucontext, uc_mcontext));
ASSYM(LINUX_VERSION_CODE, LINUX_VERSION_CODE);
ASSYM(LINUX_SC_RSP, offsetof(struct l_sigcontext, sc_rsp));
