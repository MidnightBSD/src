/* Definitions for ia64-linux target.  */

/* This macro is a C statement to print on `stderr' a string describing the
   particular machine description choice.  */

#define TARGET_VERSION fprintf (stderr, " (IA-64) Linux");

/* This is for -profile to use -lc_p instead of -lc.  */
#undef CC1_SPEC
#define CC1_SPEC "%{profile:-p} %{G*}"

/* Target OS builtins.  */
#define TARGET_OS_CPP_BUILTINS()		\
do {						\
	LINUX_TARGET_OS_CPP_BUILTINS();		\
	builtin_define("_LONGLONG");		\
} while (0)

/* Need to override linux.h STARTFILE_SPEC, since it has crtbeginT.o in.  */
#undef STARTFILE_SPEC
#ifdef HAVE_LD_PIE
#define STARTFILE_SPEC \
  "%{!shared: %{pg|p|profile:gcrt1.o%s;pie:Scrt1.o%s;:crt1.o%s}}\
   crti.o%s %{shared|pie:crtbeginS.o%s;:crtbegin.o%s}"
#else
#define STARTFILE_SPEC \
  "%{!shared: %{pg|p|profile:gcrt1.o%s;:crt1.o%s}}\
   crti.o%s %{shared|pie:crtbeginS.o%s;:crtbegin.o%s}"
#endif

/* Similar to standard Linux, but adding -ffast-math support.  */
#undef  ENDFILE_SPEC
#define ENDFILE_SPEC \
  "%{ffast-math|funsafe-math-optimizations:crtfastmath.o%s} \
   %{shared|pie:crtendS.o%s;:crtend.o%s} crtn.o%s"

/* Define this for shared library support because it isn't in the main
   linux.h file.  */

#undef LINK_SPEC
#define LINK_SPEC "\
  %{shared:-shared} \
  %{!shared: \
    %{!static: \
      %{rdynamic:-export-dynamic} \
      %{!dynamic-linker:-dynamic-linker /lib/ld-linux-ia64.so.2}} \
      %{static:-static}}"


#define JMP_BUF_SIZE  76

/* Override linux.h LINK_EH_SPEC definition.
   Signalize that because we have fde-glibc, we don't need all C shared libs
   linked against -lgcc_s.  */
#undef LINK_EH_SPEC
#define LINK_EH_SPEC ""

/* Do code reading to identify a signal frame, and set the frame
   state data appropriately.  See unwind-dw2.c for the structs.  */

/* This works only for glibc-2.3 and later, because sigcontext is different
   in glibc-2.2.4.  */

#if __GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 3)

#ifdef IN_LIBGCC2
#include <signal.h>
#include <sys/ucontext.h>

#define IA64_GATE_AREA_START 0xa000000000000100LL
#define IA64_GATE_AREA_END   0xa000000000030000LL

#define MD_FALLBACK_FRAME_STATE_FOR(CONTEXT, FS, SUCCESS)		\
  if ((CONTEXT)->rp >= IA64_GATE_AREA_START				\
      && (CONTEXT)->rp < IA64_GATE_AREA_END)				\
    {									\
      struct sigframe {							\
	char scratch[16];						\
	unsigned long sig_number;					\
	struct siginfo *info;						\
	struct sigcontext *sc;						\
      } *frame_ = (struct sigframe *)(CONTEXT)->psp;			\
      struct sigcontext *sc_ = frame_->sc;				\
									\
      /* Restore scratch registers in case the unwinder needs to	\
	 refer to a value stored in one of them.  */			\
      {									\
	int i_;								\
									\
	for (i_ = 2; i_ < 4; i_++)					\
	  (CONTEXT)->ireg[i_ - 2].loc = &sc_->sc_gr[i_];		\
	for (i_ = 8; i_ < 12; i_++)					\
	  (CONTEXT)->ireg[i_ - 2].loc = &sc_->sc_gr[i_];		\
	for (i_ = 14; i_ < 32; i_++)					\
	  (CONTEXT)->ireg[i_ - 2].loc = &sc_->sc_gr[i_];		\
      }									\
	  								\
      (CONTEXT)->fpsr_loc = &(sc_->sc_ar_fpsr);				\
      (CONTEXT)->pfs_loc = &(sc_->sc_ar_pfs);				\
      (CONTEXT)->lc_loc = &(sc_->sc_ar_lc);				\
      (CONTEXT)->unat_loc = &(sc_->sc_ar_unat);				\
      (CONTEXT)->br_loc[0] = &(sc_->sc_br[0]);				\
      (CONTEXT)->br_loc[6] = &(sc_->sc_br[6]);				\
      (CONTEXT)->br_loc[7] = &(sc_->sc_br[7]);				\
      (CONTEXT)->pr = sc_->sc_pr;					\
      (CONTEXT)->psp = sc_->sc_gr[12];					\
      (CONTEXT)->gp = sc_->sc_gr[1];					\
      /* Signal frame doesn't have an associated reg. stack frame 	\
         other than what we adjust for below.	  */			\
      (FS) -> no_reg_stack_frame = 1;					\
									\
      if (sc_->sc_rbs_base)						\
	{								\
	  /* Need to switch from alternate register backing store.  */	\
	  long ndirty, loadrs = sc_->sc_loadrs >> 16;			\
	  unsigned long alt_bspstore = (CONTEXT)->bsp - loadrs;		\
	  unsigned long bspstore;					\
	  unsigned long *ar_bsp = (unsigned long *)(sc_->sc_ar_bsp);	\
									\
	  ndirty = ia64_rse_num_regs ((unsigned long *) alt_bspstore,	\
				      (unsigned long *) (CONTEXT)->bsp);\
	  bspstore = (unsigned long)					\
		     ia64_rse_skip_regs (ar_bsp, -ndirty);		\
	  ia64_copy_rbs ((CONTEXT), bspstore, alt_bspstore, loadrs,	\
			 sc_->sc_ar_rnat);				\
	}								\
									\
      /* Don't touch the branch registers o.t. b0, b6 and b7.		\
	 The kernel doesn't pass the preserved branch registers		\
	 in the sigcontext but leaves them intact, so there's no	\
	 need to do anything with them here.  */			\
      {									\
	unsigned long sof = sc_->sc_cfm & 0x7f;				\
	(CONTEXT)->bsp = (unsigned long)				\
	  ia64_rse_skip_regs ((unsigned long *)(sc_->sc_ar_bsp), -sof); \
      }									\
									\
      (FS)->curr.reg[UNW_REG_RP].where = UNW_WHERE_SPREL;		\
      (FS)->curr.reg[UNW_REG_RP].val 					\
	= (unsigned long)&(sc_->sc_ip) - (CONTEXT)->psp;		\
      (FS)->curr.reg[UNW_REG_RP].when = -1;				\
									\
      goto SUCCESS;							\
    }

#define MD_HANDLE_UNWABI(CONTEXT, FS)					\
  if ((FS)->unwabi == ((3 << 8) | 's')					\
      || (FS)->unwabi == ((0 << 8) | 's'))				\
    {									\
      struct sigframe {							\
	char scratch[16];						\
	unsigned long sig_number;					\
	struct siginfo *info;						\
	struct sigcontext *sc;						\
      } *frame_ = (struct sigframe *)(CONTEXT)->psp;			\
      struct sigcontext *sc_ = frame_->sc;				\
									\
      /* Restore scratch registers in case the unwinder needs to	\
	 refer to a value stored in one of them.  */			\
      {									\
	int i_;								\
									\
	for (i_ = 2; i_ < 4; i_++)					\
	  (CONTEXT)->ireg[i_ - 2].loc = &sc_->sc_gr[i_];		\
	for (i_ = 8; i_ < 12; i_++)					\
	  (CONTEXT)->ireg[i_ - 2].loc = &sc_->sc_gr[i_];		\
	for (i_ = 14; i_ < 32; i_++)					\
	  (CONTEXT)->ireg[i_ - 2].loc = &sc_->sc_gr[i_];		\
      }									\
	  								\
      (CONTEXT)->pfs_loc = &(sc_->sc_ar_pfs);				\
      (CONTEXT)->lc_loc = &(sc_->sc_ar_lc);				\
      (CONTEXT)->unat_loc = &(sc_->sc_ar_unat);				\
      (CONTEXT)->br_loc[0] = &(sc_->sc_br[0]);				\
      (CONTEXT)->br_loc[6] = &(sc_->sc_br[6]);				\
      (CONTEXT)->br_loc[7] = &(sc_->sc_br[7]);				\
      (CONTEXT)->pr = sc_->sc_pr;					\
      (CONTEXT)->gp = sc_->sc_gr[1];					\
      /* Signal frame doesn't have an associated reg. stack frame 	\
         other than what we adjust for below.	  */			\
      (FS) -> no_reg_stack_frame = 1;					\
									\
      if (sc_->sc_rbs_base)						\
	{								\
	  /* Need to switch from alternate register backing store.  */	\
	  long ndirty, loadrs = sc_->sc_loadrs >> 16;			\
	  unsigned long alt_bspstore = (CONTEXT)->bsp - loadrs;		\
	  unsigned long bspstore;					\
	  unsigned long *ar_bsp = (unsigned long *)(sc_->sc_ar_bsp);	\
									\
	  ndirty = ia64_rse_num_regs ((unsigned long *) alt_bspstore,	\
				      (unsigned long *) (CONTEXT)->bsp);\
	  bspstore = (unsigned long)					\
		     ia64_rse_skip_regs (ar_bsp, -ndirty);		\
	  ia64_copy_rbs ((CONTEXT), bspstore, alt_bspstore, loadrs,	\
			 sc_->sc_ar_rnat);				\
	}								\
									\
      /* Don't touch the branch registers o.t. b0, b6 and b7.		\
	 The kernel doesn't pass the preserved branch registers		\
	 in the sigcontext but leaves them intact, so there's no	\
	 need to do anything with them here.  */			\
      {									\
	unsigned long sof = sc_->sc_cfm & 0x7f;				\
	(CONTEXT)->bsp = (unsigned long)				\
	  ia64_rse_skip_regs ((unsigned long *)(sc_->sc_ar_bsp), -sof); \
      }									\
									\
      /* pfs_loc already set above.  Without this pfs_loc would point	\
	 incorrectly to sc_cfm instead of sc_ar_pfs.  */		\
      (FS)->curr.reg[UNW_REG_PFS].where = UNW_WHERE_NONE;		\
    }

#endif /* IN_LIBGCC2 */
#endif /* glibc-2.3 or better */
