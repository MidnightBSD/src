/* Subroutines used for code generation on IBM RS/6000.
   Copyright (C) 1991, 1993, 1994, 1995, 1996, 1997, 1998, 1999, 
   2000, 2001, 2002, 2003, 2004, 2005 Free Software Foundation, Inc.
   Contributed by Richard Kenner (kenner@vlsi1.ultra.nyu.edu)

   This file is part of GCC.

   GCC is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published
   by the Free Software Foundation; either version 2, or (at your
   option) any later version.

   GCC is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License
   along with GCC; see the file COPYING.  If not, write to the
   Free Software Foundation, 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA.  */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "rtl.h"
#include "regs.h"
#include "hard-reg-set.h"
#include "real.h"
#include "insn-config.h"
#include "conditions.h"
#include "insn-attr.h"
#include "flags.h"
#include "recog.h"
#include "obstack.h"
#include "tree.h"
#include "expr.h"
#include "optabs.h"
#include "except.h"
#include "function.h"
#include "output.h"
#include "basic-block.h"
#include "integrate.h"
#include "toplev.h"
#include "ggc.h"
#include "hashtab.h"
#include "tm_p.h"
#include "target.h"
#include "target-def.h"
#include "langhooks.h"
#include "reload.h"
#include "cfglayout.h"
#include "sched-int.h"
#if TARGET_XCOFF
#include "xcoffout.h"  /* get declarations of xcoff_*_section_name */
#endif

#ifndef TARGET_NO_PROTOTYPE
#define TARGET_NO_PROTOTYPE 0
#endif

#define EASY_VECTOR_15(n) ((n) >= -16 && (n) <= 15)
#define EASY_VECTOR_15_ADD_SELF(n) ((n) >= 0x10 && (n) <= 0x1e \
                                          && !((n) & 1))

#define min(A,B)	((A) < (B) ? (A) : (B))
#define max(A,B)	((A) > (B) ? (A) : (B))

/* Structure used to define the rs6000 stack */
typedef struct rs6000_stack {
  int first_gp_reg_save;	/* first callee saved GP register used */
  int first_fp_reg_save;	/* first callee saved FP register used */
  int first_altivec_reg_save;	/* first callee saved AltiVec register used */
  int lr_save_p;		/* true if the link reg needs to be saved */
  int cr_save_p;		/* true if the CR reg needs to be saved */
  unsigned int vrsave_mask;	/* mask of vec registers to save */
  int toc_save_p;		/* true if the TOC needs to be saved */
  int push_p;			/* true if we need to allocate stack space */
  int calls_p;			/* true if the function makes any calls */
  enum rs6000_abi abi;		/* which ABI to use */
  int gp_save_offset;		/* offset to save GP regs from initial SP */
  int fp_save_offset;		/* offset to save FP regs from initial SP */
  int altivec_save_offset;	/* offset to save AltiVec regs from initial SP */
  int lr_save_offset;		/* offset to save LR from initial SP */
  int cr_save_offset;		/* offset to save CR from initial SP */
  int vrsave_save_offset;	/* offset to save VRSAVE from initial SP */
  int spe_gp_save_offset;	/* offset to save spe 64-bit gprs  */
  int toc_save_offset;		/* offset to save the TOC pointer */
  int varargs_save_offset;	/* offset to save the varargs registers */
  int ehrd_offset;		/* offset to EH return data */
  int reg_size;			/* register size (4 or 8) */
  int varargs_size;		/* size to hold V.4 args passed in regs */
  HOST_WIDE_INT vars_size;	/* variable save area size */
  int parm_size;		/* outgoing parameter size */
  int save_size;		/* save area size */
  int fixed_size;		/* fixed size of stack frame */
  int gp_size;			/* size of saved GP registers */
  int fp_size;			/* size of saved FP registers */
  int altivec_size;		/* size of saved AltiVec registers */
  int cr_size;			/* size to hold CR if not in save_size */
  int lr_size;			/* size to hold LR if not in save_size */
  int vrsave_size;		/* size to hold VRSAVE if not in save_size */
  int altivec_padding_size;	/* size of altivec alignment padding if
				   not in save_size */
  int spe_gp_size;		/* size of 64-bit GPR save size for SPE */
  int spe_padding_size;
  int toc_size;			/* size to hold TOC if not in save_size */
  HOST_WIDE_INT total_size;	/* total bytes allocated for stack */
  int spe_64bit_regs_used;
} rs6000_stack_t;

/* Target cpu type */

enum processor_type rs6000_cpu;
struct rs6000_cpu_select rs6000_select[3] =
{
  /* switch		name,			tune	arch */
  { (const char *)0,	"--with-cpu=",		1,	1 },
  { (const char *)0,	"-mcpu=",		1,	1 },
  { (const char *)0,	"-mtune=",		1,	0 },
};

/* Always emit branch hint bits.  */
static GTY(()) bool rs6000_always_hint;

/* Schedule instructions for group formation.  */
static GTY(()) bool rs6000_sched_groups;

/* Support adjust_priority scheduler hook 
   and -mprioritize-restricted-insns= option.  */
const char *rs6000_sched_restricted_insns_priority_str;
int rs6000_sched_restricted_insns_priority;

/* Support for -msched-costly-dep option.  */
const char *rs6000_sched_costly_dep_str;
enum rs6000_dependence_cost rs6000_sched_costly_dep;

/* Support for -minsert-sched-nops option.  */
const char *rs6000_sched_insert_nops_str;
enum rs6000_nop_insertion rs6000_sched_insert_nops;

/* Size of long double */
const char *rs6000_long_double_size_string;
int rs6000_long_double_type_size;

/* Whether -mabi=altivec has appeared */
int rs6000_altivec_abi;

/* Whether VRSAVE instructions should be generated.  */
int rs6000_altivec_vrsave;

/* String from -mvrsave= option.  */
const char *rs6000_altivec_vrsave_string;

/* Nonzero if we want SPE ABI extensions.  */
int rs6000_spe_abi;

/* Whether isel instructions should be generated.  */
int rs6000_isel;

/* Whether SPE simd instructions should be generated.  */
int rs6000_spe;

/* Nonzero if floating point operations are done in the GPRs.  */
int rs6000_float_gprs = 0;

/* String from -mfloat-gprs=.  */
const char *rs6000_float_gprs_string;

/* String from -misel=.  */
const char *rs6000_isel_string;

/* String from -mspe=.  */
const char *rs6000_spe_string;

/* Set to nonzero once AIX common-mode calls have been defined.  */
static GTY(()) int common_mode_defined;

/* Save information from a "cmpxx" operation until the branch or scc is
   emitted.  */
rtx rs6000_compare_op0, rs6000_compare_op1;
int rs6000_compare_fp_p;

/* Label number of label created for -mrelocatable, to call to so we can
   get the address of the GOT section */
int rs6000_pic_labelno;

#ifdef USING_ELFOS_H
/* Which abi to adhere to */
const char *rs6000_abi_name;

/* Semantics of the small data area */
enum rs6000_sdata_type rs6000_sdata = SDATA_DATA;

/* Which small data model to use */
const char *rs6000_sdata_name = (char *)0;

/* Counter for labels which are to be placed in .fixup.  */
int fixuplabelno = 0;
#endif

/* Bit size of immediate TLS offsets and string from which it is decoded.  */
int rs6000_tls_size = 32;
const char *rs6000_tls_size_string;

/* ABI enumeration available for subtarget to use.  */
enum rs6000_abi rs6000_current_abi;

/* ABI string from -mabi= option.  */
const char *rs6000_abi_string;

/* Debug flags */
const char *rs6000_debug_name;
int rs6000_debug_stack;		/* debug stack applications */
int rs6000_debug_arg;		/* debug argument handling */

/* Opaque types.  */
static GTY(()) tree opaque_V2SI_type_node;
static GTY(()) tree opaque_V2SF_type_node;
static GTY(()) tree opaque_p_V2SI_type_node;

/* AltiVec requires a few more basic types in addition to the vector
   types already defined in tree.c.  */
static GTY(()) tree bool_char_type_node;	/* __bool char */
static GTY(()) tree bool_short_type_node;	/* __bool short */
static GTY(()) tree bool_int_type_node;		/* __bool int */
static GTY(()) tree pixel_type_node;		/* __pixel */
static GTY(()) tree bool_V16QI_type_node;	/* __vector __bool char */
static GTY(()) tree bool_V8HI_type_node;	/* __vector __bool short */
static GTY(()) tree bool_V4SI_type_node;	/* __vector __bool int */
static GTY(()) tree pixel_V8HI_type_node;	/* __vector __pixel */

int rs6000_warn_altivec_long = 1;		/* On by default. */
const char *rs6000_warn_altivec_long_switch;

const char *rs6000_traceback_name;
static enum {
  traceback_default = 0,
  traceback_none,
  traceback_part,
  traceback_full
} rs6000_traceback;

/* Flag to say the TOC is initialized */
int toc_initialized;
char toc_label_name[10];

/* Alias set for saves and restores from the rs6000 stack.  */
static GTY(()) int rs6000_sr_alias_set;

/* Call distance, overridden by -mlongcall and #pragma longcall(1).
   The only place that looks at this is rs6000_set_default_type_attributes;
   everywhere else should rely on the presence or absence of a longcall
   attribute on the function declaration.  Exception: init_cumulative_args
   looks at it too, for libcalls.  */
int rs6000_default_long_calls;
const char *rs6000_longcall_switch;

/* Control alignment for fields within structures.  */
/* String from -malign-XXXXX.  */
const char *rs6000_alignment_string;
int rs6000_alignment_flags;

struct builtin_description
{
  /* mask is not const because we're going to alter it below.  This
     nonsense will go away when we rewrite the -march infrastructure
     to give us more target flag bits.  */
  unsigned int mask;
  const enum insn_code icode;
  const char *const name;
  const enum rs6000_builtins code;
};

static bool rs6000_function_ok_for_sibcall (tree, tree);
static int num_insns_constant_wide (HOST_WIDE_INT);
static void validate_condition_mode (enum rtx_code, enum machine_mode);
static rtx rs6000_generate_compare (enum rtx_code);
static void rs6000_maybe_dead (rtx);
static void rs6000_emit_stack_tie (void);
static void rs6000_frame_related (rtx, rtx, HOST_WIDE_INT, rtx, rtx);
static rtx spe_synthesize_frame_save (rtx);
static bool spe_func_has_64bit_regs_p (void);
static void emit_frame_save (rtx, rtx, enum machine_mode, unsigned int,
			     int, HOST_WIDE_INT);
static rtx gen_frame_mem_offset (enum machine_mode, rtx, int);
static void rs6000_emit_allocate_stack (HOST_WIDE_INT, int);
static unsigned rs6000_hash_constant (rtx);
static unsigned toc_hash_function (const void *);
static int toc_hash_eq (const void *, const void *);
static int constant_pool_expr_1 (rtx, int *, int *);
static bool constant_pool_expr_p (rtx);
static bool toc_relative_expr_p (rtx);
static bool legitimate_small_data_p (enum machine_mode, rtx);
static bool legitimate_offset_address_p (enum machine_mode, rtx, int);
static bool legitimate_indexed_address_p (rtx, int);
static bool legitimate_indirect_address_p (rtx, int);
static bool macho_lo_sum_memory_operand (rtx x, enum machine_mode mode);
static bool legitimate_lo_sum_address_p (enum machine_mode, rtx, int);
static struct machine_function * rs6000_init_machine_status (void);
static bool rs6000_assemble_integer (rtx, unsigned int, int);
#ifdef HAVE_GAS_HIDDEN
static void rs6000_assemble_visibility (tree, int);
#endif
static int rs6000_ra_ever_killed (void);
static tree rs6000_handle_longcall_attribute (tree *, tree, tree, int, bool *);
static tree rs6000_handle_altivec_attribute (tree *, tree, tree, int, bool *);
static const char *rs6000_mangle_fundamental_type (tree);
extern const struct attribute_spec rs6000_attribute_table[];
static void rs6000_set_default_type_attributes (tree);
static void rs6000_output_function_prologue (FILE *, HOST_WIDE_INT);
static void rs6000_output_function_epilogue (FILE *, HOST_WIDE_INT);
static void rs6000_output_mi_thunk (FILE *, tree, HOST_WIDE_INT, HOST_WIDE_INT,
				    tree);
static rtx rs6000_emit_set_long_const (rtx, HOST_WIDE_INT, HOST_WIDE_INT);
static bool rs6000_return_in_memory (tree, tree);
static void rs6000_file_start (void);
#if TARGET_ELF
static unsigned int rs6000_elf_section_type_flags (tree, const char *, int);
static void rs6000_elf_asm_out_constructor (rtx, int);
static void rs6000_elf_asm_out_destructor (rtx, int);
static void rs6000_elf_end_indicate_exec_stack (void) ATTRIBUTE_UNUSED;
static void rs6000_elf_select_section (tree, int, unsigned HOST_WIDE_INT);
static void rs6000_elf_unique_section (tree, int);
static void rs6000_elf_select_rtx_section (enum machine_mode, rtx,
					   unsigned HOST_WIDE_INT);
static void rs6000_elf_encode_section_info (tree, rtx, int)
     ATTRIBUTE_UNUSED;
static bool rs6000_elf_in_small_data_p (tree);
#endif
#if TARGET_XCOFF
static void rs6000_xcoff_asm_globalize_label (FILE *, const char *);
static void rs6000_xcoff_asm_named_section (const char *, unsigned int);
static void rs6000_xcoff_select_section (tree, int, unsigned HOST_WIDE_INT);
static void rs6000_xcoff_unique_section (tree, int);
static void rs6000_xcoff_select_rtx_section (enum machine_mode, rtx,
					     unsigned HOST_WIDE_INT);
static const char * rs6000_xcoff_strip_name_encoding (const char *);
static unsigned int rs6000_xcoff_section_type_flags (tree, const char *, int);
static void rs6000_xcoff_file_start (void);
static void rs6000_xcoff_file_end (void);
#endif
#if TARGET_MACHO
static bool rs6000_binds_local_p (tree);
#endif
static int rs6000_use_dfa_pipeline_interface (void);
static int rs6000_variable_issue (FILE *, int, rtx, int);
static bool rs6000_rtx_costs (rtx, int, int, int *);
static int rs6000_adjust_cost (rtx, rtx, rtx, int);
static bool is_microcoded_insn (rtx);
static int is_dispatch_slot_restricted (rtx);
static bool is_cracked_insn (rtx);
static bool is_branch_slot_insn (rtx);
static int rs6000_adjust_priority (rtx, int);
static int rs6000_issue_rate (void);
static bool rs6000_is_costly_dependence (rtx, rtx, rtx, int, int);
static rtx get_next_active_insn (rtx, rtx);
static bool insn_terminates_group_p (rtx , enum group_termination);
static bool is_costly_group (rtx *, rtx);
static int force_new_group (int, FILE *, rtx *, rtx, bool *, int, int *);
static int redefine_groups (FILE *, int, rtx, rtx);
static int pad_groups (FILE *, int, rtx, rtx);
static void rs6000_sched_finish (FILE *, int);
static int rs6000_use_sched_lookahead (void);

static void rs6000_init_builtins (void);
static rtx rs6000_expand_unop_builtin (enum insn_code, tree, rtx);
static rtx rs6000_expand_binop_builtin (enum insn_code, tree, rtx);
static rtx rs6000_expand_ternop_builtin (enum insn_code, tree, rtx);
static rtx rs6000_expand_builtin (tree, rtx, rtx, enum machine_mode, int);
static void altivec_init_builtins (void);
static void rs6000_common_init_builtins (void);
static void rs6000_init_libfuncs (void);

static void enable_mask_for_builtins (struct builtin_description *, int,
				      enum rs6000_builtins,
				      enum rs6000_builtins);
static void spe_init_builtins (void);
static rtx spe_expand_builtin (tree, rtx, bool *);
static rtx spe_expand_stv_builtin (enum insn_code, tree);
static rtx spe_expand_predicate_builtin (enum insn_code, tree, rtx);
static rtx spe_expand_evsel_builtin (enum insn_code, tree, rtx);
static int rs6000_emit_int_cmove (rtx, rtx, rtx, rtx);
static rs6000_stack_t *rs6000_stack_info (void);
static void debug_stack_info (rs6000_stack_t *);

static rtx altivec_expand_builtin (tree, rtx, bool *);
static rtx altivec_expand_ld_builtin (tree, rtx, bool *);
static rtx altivec_expand_st_builtin (tree, rtx, bool *);
static rtx altivec_expand_dst_builtin (tree, rtx, bool *);
static rtx altivec_expand_abs_builtin (enum insn_code, tree, rtx);
static rtx altivec_expand_predicate_builtin (enum insn_code, 
					    const char *, tree, rtx);
static rtx altivec_expand_lv_builtin (enum insn_code, tree, rtx);
static rtx altivec_expand_stv_builtin (enum insn_code, tree);
static void rs6000_parse_abi_options (void);
static void rs6000_parse_alignment_option (void);
static void rs6000_parse_tls_size_option (void);
static void rs6000_parse_yes_no_option (const char *, const char *, int *);
static int first_altivec_reg_to_save (void);
static unsigned int compute_vrsave_mask (void);
static void is_altivec_return_reg (rtx, void *);
static rtx generate_set_vrsave (rtx, rs6000_stack_t *, int);
int easy_vector_constant (rtx, enum machine_mode);
static int easy_vector_same (rtx, enum machine_mode);
static int easy_vector_splat_const (int, enum machine_mode);
static bool is_ev64_opaque_type (tree);
static rtx rs6000_dwarf_register_span (rtx);
static rtx rs6000_legitimize_tls_address (rtx, enum tls_model);
static rtx rs6000_tls_get_addr (void);
static rtx rs6000_got_sym (void);
static inline int rs6000_tls_symbol_ref_1 (rtx *, void *);
static const char *rs6000_get_some_local_dynamic_name (void);
static int rs6000_get_some_local_dynamic_name_1 (rtx *, void *);
static rtx rs6000_complex_function_value (enum machine_mode);
static rtx rs6000_spe_function_arg (CUMULATIVE_ARGS *,
				    enum machine_mode, tree);
static rtx rs6000_mixed_function_arg (enum machine_mode, tree, int);
static void rs6000_move_block_from_reg (int regno, rtx x, int nregs);
static void setup_incoming_varargs (CUMULATIVE_ARGS *,
				    enum machine_mode, tree,
				    int *, int);
#if TARGET_MACHO
static void macho_branch_islands (void);
static void add_compiler_branch_island (tree, tree, int);
static int no_previous_def (tree function_name);
static tree get_prev_label (tree function_name);
#endif

static tree rs6000_build_builtin_va_list (void);

/* Hash table stuff for keeping track of TOC entries.  */

struct toc_hash_struct GTY(())
{
  /* `key' will satisfy CONSTANT_P; in fact, it will satisfy
     ASM_OUTPUT_SPECIAL_POOL_ENTRY_P.  */
  rtx key;
  enum machine_mode key_mode;
  int labelno;
};

static GTY ((param_is (struct toc_hash_struct))) htab_t toc_hash_table;

/* Default register names.  */
char rs6000_reg_names[][8] =
{
      "0",  "1",  "2",  "3",  "4",  "5",  "6",  "7",
      "8",  "9", "10", "11", "12", "13", "14", "15",
     "16", "17", "18", "19", "20", "21", "22", "23",
     "24", "25", "26", "27", "28", "29", "30", "31",
      "0",  "1",  "2",  "3",  "4",  "5",  "6",  "7",
      "8",  "9", "10", "11", "12", "13", "14", "15",
     "16", "17", "18", "19", "20", "21", "22", "23",
     "24", "25", "26", "27", "28", "29", "30", "31",
     "mq", "lr", "ctr","ap",
      "0",  "1",  "2",  "3",  "4",  "5",  "6",  "7",
      "xer",
      /* AltiVec registers.  */
      "0",  "1",  "2",  "3",  "4",  "5",  "6", "7",
      "8",  "9",  "10", "11", "12", "13", "14", "15",
      "16", "17", "18", "19", "20", "21", "22", "23",
      "24", "25", "26", "27", "28", "29", "30", "31",
      "vrsave", "vscr",
      /* SPE registers.  */
      "spe_acc", "spefscr"
};

#ifdef TARGET_REGNAMES
static const char alt_reg_names[][8] =
{
   "%r0",   "%r1",  "%r2",  "%r3",  "%r4",  "%r5",  "%r6",  "%r7",
   "%r8",   "%r9", "%r10", "%r11", "%r12", "%r13", "%r14", "%r15",
  "%r16",  "%r17", "%r18", "%r19", "%r20", "%r21", "%r22", "%r23",
  "%r24",  "%r25", "%r26", "%r27", "%r28", "%r29", "%r30", "%r31",
   "%f0",   "%f1",  "%f2",  "%f3",  "%f4",  "%f5",  "%f6",  "%f7",
   "%f8",   "%f9", "%f10", "%f11", "%f12", "%f13", "%f14", "%f15",
  "%f16",  "%f17", "%f18", "%f19", "%f20", "%f21", "%f22", "%f23",
  "%f24",  "%f25", "%f26", "%f27", "%f28", "%f29", "%f30", "%f31",
    "mq",    "lr",  "ctr",   "ap",
  "%cr0",  "%cr1", "%cr2", "%cr3", "%cr4", "%cr5", "%cr6", "%cr7",
   "xer",
  /* AltiVec registers.  */
   "%v0",  "%v1",  "%v2",  "%v3",  "%v4",  "%v5",  "%v6", "%v7",
   "%v8",  "%v9", "%v10", "%v11", "%v12", "%v13", "%v14", "%v15",
  "%v16", "%v17", "%v18", "%v19", "%v20", "%v21", "%v22", "%v23",
  "%v24", "%v25", "%v26", "%v27", "%v28", "%v29", "%v30", "%v31",
  "vrsave", "vscr",
  /* SPE registers.  */
  "spe_acc", "spefscr"
};
#endif

#ifndef MASK_STRICT_ALIGN
#define MASK_STRICT_ALIGN 0
#endif
#ifndef TARGET_PROFILE_KERNEL
#define TARGET_PROFILE_KERNEL 0
#endif

/* The VRSAVE bitmask puts bit %v0 as the most significant bit.  */
#define ALTIVEC_REG_BIT(REGNO) (0x80000000 >> ((REGNO) - FIRST_ALTIVEC_REGNO))

/* Return 1 for a symbol ref for a thread-local storage symbol.  */
#define RS6000_SYMBOL_REF_TLS_P(RTX) \
  (GET_CODE (RTX) == SYMBOL_REF && SYMBOL_REF_TLS_MODEL (RTX) != 0)

/* Initialize the GCC target structure.  */
#undef TARGET_ATTRIBUTE_TABLE
#define TARGET_ATTRIBUTE_TABLE rs6000_attribute_table
#undef TARGET_SET_DEFAULT_TYPE_ATTRIBUTES
#define TARGET_SET_DEFAULT_TYPE_ATTRIBUTES rs6000_set_default_type_attributes

#undef TARGET_ASM_ALIGNED_DI_OP
#define TARGET_ASM_ALIGNED_DI_OP DOUBLE_INT_ASM_OP

/* Default unaligned ops are only provided for ELF.  Find the ops needed
   for non-ELF systems.  */
#ifndef OBJECT_FORMAT_ELF
#if TARGET_XCOFF
/* For XCOFF.  rs6000_assemble_integer will handle unaligned DIs on
   64-bit targets.  */
#undef TARGET_ASM_UNALIGNED_HI_OP
#define TARGET_ASM_UNALIGNED_HI_OP "\t.vbyte\t2,"
#undef TARGET_ASM_UNALIGNED_SI_OP
#define TARGET_ASM_UNALIGNED_SI_OP "\t.vbyte\t4,"
#undef TARGET_ASM_UNALIGNED_DI_OP
#define TARGET_ASM_UNALIGNED_DI_OP "\t.vbyte\t8,"
#else
/* For Darwin.  */
#undef TARGET_ASM_UNALIGNED_HI_OP
#define TARGET_ASM_UNALIGNED_HI_OP "\t.short\t"
#undef TARGET_ASM_UNALIGNED_SI_OP
#define TARGET_ASM_UNALIGNED_SI_OP "\t.long\t"
#endif
#endif

/* This hook deals with fixups for relocatable code and DI-mode objects
   in 64-bit code.  */
#undef TARGET_ASM_INTEGER
#define TARGET_ASM_INTEGER rs6000_assemble_integer

#ifdef HAVE_GAS_HIDDEN
#undef TARGET_ASM_ASSEMBLE_VISIBILITY
#define TARGET_ASM_ASSEMBLE_VISIBILITY rs6000_assemble_visibility
#endif

#undef TARGET_HAVE_TLS
#define TARGET_HAVE_TLS HAVE_AS_TLS

#undef TARGET_CANNOT_FORCE_CONST_MEM
#define TARGET_CANNOT_FORCE_CONST_MEM rs6000_tls_referenced_p

#undef TARGET_ASM_FUNCTION_PROLOGUE
#define TARGET_ASM_FUNCTION_PROLOGUE rs6000_output_function_prologue
#undef TARGET_ASM_FUNCTION_EPILOGUE
#define TARGET_ASM_FUNCTION_EPILOGUE rs6000_output_function_epilogue

#undef  TARGET_SCHED_USE_DFA_PIPELINE_INTERFACE 
#define TARGET_SCHED_USE_DFA_PIPELINE_INTERFACE rs6000_use_dfa_pipeline_interface
#undef  TARGET_SCHED_VARIABLE_ISSUE
#define TARGET_SCHED_VARIABLE_ISSUE rs6000_variable_issue

#undef TARGET_SCHED_ISSUE_RATE
#define TARGET_SCHED_ISSUE_RATE rs6000_issue_rate
#undef TARGET_SCHED_ADJUST_COST
#define TARGET_SCHED_ADJUST_COST rs6000_adjust_cost
#undef TARGET_SCHED_ADJUST_PRIORITY
#define TARGET_SCHED_ADJUST_PRIORITY rs6000_adjust_priority
#undef TARGET_SCHED_IS_COSTLY_DEPENDENCE      
#define TARGET_SCHED_IS_COSTLY_DEPENDENCE rs6000_is_costly_dependence
#undef TARGET_SCHED_FINISH
#define TARGET_SCHED_FINISH rs6000_sched_finish

#undef TARGET_SCHED_FIRST_CYCLE_MULTIPASS_DFA_LOOKAHEAD
#define TARGET_SCHED_FIRST_CYCLE_MULTIPASS_DFA_LOOKAHEAD rs6000_use_sched_lookahead

#undef TARGET_INIT_BUILTINS
#define TARGET_INIT_BUILTINS rs6000_init_builtins

#undef TARGET_EXPAND_BUILTIN
#define TARGET_EXPAND_BUILTIN rs6000_expand_builtin

#undef TARGET_MANGLE_FUNDAMENTAL_TYPE
#define TARGET_MANGLE_FUNDAMENTAL_TYPE rs6000_mangle_fundamental_type

#undef TARGET_INIT_LIBFUNCS
#define TARGET_INIT_LIBFUNCS rs6000_init_libfuncs

#if TARGET_MACHO
#undef TARGET_BINDS_LOCAL_P
#define TARGET_BINDS_LOCAL_P rs6000_binds_local_p
#endif

#undef TARGET_ASM_OUTPUT_MI_THUNK
#define TARGET_ASM_OUTPUT_MI_THUNK rs6000_output_mi_thunk

#undef TARGET_ASM_CAN_OUTPUT_MI_THUNK
#define TARGET_ASM_CAN_OUTPUT_MI_THUNK hook_bool_tree_hwi_hwi_tree_true

#undef TARGET_FUNCTION_OK_FOR_SIBCALL
#define TARGET_FUNCTION_OK_FOR_SIBCALL rs6000_function_ok_for_sibcall

#undef TARGET_RTX_COSTS
#define TARGET_RTX_COSTS rs6000_rtx_costs
#undef TARGET_ADDRESS_COST
#define TARGET_ADDRESS_COST hook_int_rtx_0

#undef TARGET_VECTOR_OPAQUE_P
#define TARGET_VECTOR_OPAQUE_P is_ev64_opaque_type

#undef TARGET_DWARF_REGISTER_SPAN
#define TARGET_DWARF_REGISTER_SPAN rs6000_dwarf_register_span

/* On rs6000, function arguments are promoted, as are function return
   values.  */
#undef TARGET_PROMOTE_FUNCTION_ARGS
#define TARGET_PROMOTE_FUNCTION_ARGS hook_bool_tree_true
#undef TARGET_PROMOTE_FUNCTION_RETURN
#define TARGET_PROMOTE_FUNCTION_RETURN hook_bool_tree_true

/* Structure return values are passed as an extra parameter.  */
#undef TARGET_STRUCT_VALUE_RTX
#define TARGET_STRUCT_VALUE_RTX hook_rtx_tree_int_null

#undef TARGET_RETURN_IN_MEMORY
#define TARGET_RETURN_IN_MEMORY rs6000_return_in_memory

#undef TARGET_SETUP_INCOMING_VARARGS
#define TARGET_SETUP_INCOMING_VARARGS setup_incoming_varargs

/* Always strict argument naming on rs6000.  */
#undef TARGET_STRICT_ARGUMENT_NAMING
#define TARGET_STRICT_ARGUMENT_NAMING hook_bool_CUMULATIVE_ARGS_true
#undef TARGET_PRETEND_OUTGOING_VARARGS_NAMED
#define TARGET_PRETEND_OUTGOING_VARARGS_NAMED hook_bool_CUMULATIVE_ARGS_true
#undef TARGET_SPLIT_COMPLEX_ARG
#define TARGET_SPLIT_COMPLEX_ARG hook_bool_tree_true

#undef TARGET_BUILD_BUILTIN_VA_LIST
#define TARGET_BUILD_BUILTIN_VA_LIST rs6000_build_builtin_va_list

struct gcc_target targetm = TARGET_INITIALIZER;

/* Override command line options.  Mostly we process the processor
   type and sometimes adjust other TARGET_ options.  */

void
rs6000_override_options (const char *default_cpu)
{
  size_t i, j;
  struct rs6000_cpu_select *ptr;
  int set_masks;

  /* Simplifications for entries below.  */

  enum {
    POWERPC_BASE_MASK = MASK_POWERPC | MASK_NEW_MNEMONICS,
    POWERPC_7400_MASK = POWERPC_BASE_MASK | MASK_PPC_GFXOPT | MASK_ALTIVEC
  };

  /* This table occasionally claims that a processor does not support
     a particular feature even though it does, but the feature is slower
     than the alternative.  Thus, it shouldn't be relied on as a
     complete description of the processor's support.  

     Please keep this list in order, and don't forget to update the
     documentation in invoke.texi when adding a new processor or
     flag.  */
  static struct ptt
    {
      const char *const name;		/* Canonical processor name.  */
      const enum processor_type processor; /* Processor type enum value.  */
      const int target_enable;	/* Target flags to enable.  */
    } const processor_target_table[]
      = {{"401", PROCESSOR_PPC403, POWERPC_BASE_MASK | MASK_SOFT_FLOAT},
	 {"403", PROCESSOR_PPC403,
	  POWERPC_BASE_MASK | MASK_SOFT_FLOAT | MASK_STRICT_ALIGN},
	 {"405", PROCESSOR_PPC405, POWERPC_BASE_MASK | MASK_SOFT_FLOAT},
	 {"405fp", PROCESSOR_PPC405, POWERPC_BASE_MASK},
	 {"440", PROCESSOR_PPC440, POWERPC_BASE_MASK | MASK_SOFT_FLOAT},
	 {"440fp", PROCESSOR_PPC440, POWERPC_BASE_MASK},
	 {"505", PROCESSOR_MPCCORE, POWERPC_BASE_MASK},
	 {"601", PROCESSOR_PPC601,
	  MASK_POWER | POWERPC_BASE_MASK | MASK_MULTIPLE | MASK_STRING},
	 {"602", PROCESSOR_PPC603, POWERPC_BASE_MASK | MASK_PPC_GFXOPT},
	 {"603", PROCESSOR_PPC603, POWERPC_BASE_MASK | MASK_PPC_GFXOPT},
	 {"603e", PROCESSOR_PPC603, POWERPC_BASE_MASK | MASK_PPC_GFXOPT},
	 {"604", PROCESSOR_PPC604, POWERPC_BASE_MASK | MASK_PPC_GFXOPT},
	 {"604e", PROCESSOR_PPC604e, POWERPC_BASE_MASK | MASK_PPC_GFXOPT},
	 {"620", PROCESSOR_PPC620,
	  POWERPC_BASE_MASK | MASK_PPC_GFXOPT | MASK_POWERPC64},
	 {"630", PROCESSOR_PPC630,
	  POWERPC_BASE_MASK | MASK_PPC_GFXOPT | MASK_POWERPC64},
	 {"740", PROCESSOR_PPC750, POWERPC_BASE_MASK | MASK_PPC_GFXOPT},
	 {"7400", PROCESSOR_PPC7400, POWERPC_7400_MASK},
	 {"7450", PROCESSOR_PPC7450, POWERPC_7400_MASK},
	 {"750", PROCESSOR_PPC750, POWERPC_BASE_MASK | MASK_PPC_GFXOPT},
	 {"801", PROCESSOR_MPCCORE, POWERPC_BASE_MASK | MASK_SOFT_FLOAT},
	 {"821", PROCESSOR_MPCCORE, POWERPC_BASE_MASK | MASK_SOFT_FLOAT},
	 {"823", PROCESSOR_MPCCORE, POWERPC_BASE_MASK | MASK_SOFT_FLOAT},
	 {"8540", PROCESSOR_PPC8540, POWERPC_BASE_MASK | MASK_PPC_GFXOPT},
	 {"860", PROCESSOR_MPCCORE, POWERPC_BASE_MASK | MASK_SOFT_FLOAT},
	 {"970", PROCESSOR_POWER4,
	  POWERPC_7400_MASK | MASK_PPC_GPOPT | MASK_MFCRF | MASK_POWERPC64},
	 {"common", PROCESSOR_COMMON, MASK_NEW_MNEMONICS},
	 {"ec603e", PROCESSOR_PPC603, POWERPC_BASE_MASK | MASK_SOFT_FLOAT},
	 {"G3", PROCESSOR_PPC750, POWERPC_BASE_MASK | MASK_PPC_GFXOPT},
	 {"G4",  PROCESSOR_PPC7450, POWERPC_7400_MASK},
	 {"G5", PROCESSOR_POWER4,
	  POWERPC_7400_MASK | MASK_PPC_GPOPT | MASK_MFCRF | MASK_POWERPC64},
	 {"power", PROCESSOR_POWER, MASK_POWER | MASK_MULTIPLE | MASK_STRING},
	 {"power2", PROCESSOR_POWER,
	  MASK_POWER | MASK_POWER2 | MASK_MULTIPLE | MASK_STRING},
	 {"power3", PROCESSOR_PPC630,
	  POWERPC_BASE_MASK | MASK_PPC_GFXOPT | MASK_POWERPC64},
	 {"power4", PROCESSOR_POWER4,
	  POWERPC_BASE_MASK | MASK_PPC_GFXOPT | MASK_MFCRF | MASK_POWERPC64},
	 {"power5", PROCESSOR_POWER5,
	  POWERPC_BASE_MASK | MASK_PPC_GFXOPT | MASK_MFCRF | MASK_POWERPC64},
	 {"powerpc", PROCESSOR_POWERPC, POWERPC_BASE_MASK},
	 {"powerpc64", PROCESSOR_POWERPC64,
	  POWERPC_BASE_MASK | MASK_POWERPC64},
	 {"rios", PROCESSOR_RIOS1, MASK_POWER | MASK_MULTIPLE | MASK_STRING},
	 {"rios1", PROCESSOR_RIOS1, MASK_POWER | MASK_MULTIPLE | MASK_STRING},
	 {"rios2", PROCESSOR_RIOS2,
	  MASK_POWER | MASK_POWER2 | MASK_MULTIPLE | MASK_STRING},
	 {"rsc", PROCESSOR_PPC601, MASK_POWER | MASK_MULTIPLE | MASK_STRING},
	 {"rsc1", PROCESSOR_PPC601, MASK_POWER | MASK_MULTIPLE | MASK_STRING},
	 {"rs64a", PROCESSOR_RS64A, POWERPC_BASE_MASK | MASK_POWERPC64},
      };

  const size_t ptt_size = ARRAY_SIZE (processor_target_table);

  /* Some OSs don't support saving the high part of 64-bit registers on
     context switch.  Other OSs don't support saving Altivec registers.
     On those OSs, we don't touch the MASK_POWERPC64 or MASK_ALTIVEC
     settings; if the user wants either, the user must explicitly specify
     them and we won't interfere with the user's specification.  */

  enum {
    POWER_MASKS = MASK_POWER | MASK_POWER2 | MASK_MULTIPLE | MASK_STRING,
    POWERPC_MASKS = (POWERPC_BASE_MASK | MASK_PPC_GPOPT 
		     | MASK_PPC_GFXOPT | MASK_POWERPC64 | MASK_ALTIVEC
		     | MASK_MFCRF)
  };
 set_masks = POWER_MASKS | POWERPC_MASKS | MASK_SOFT_FLOAT;
#ifdef OS_MISSING_POWERPC64
  if (OS_MISSING_POWERPC64)
    set_masks &= ~MASK_POWERPC64;
#endif
#ifdef OS_MISSING_ALTIVEC
  if (OS_MISSING_ALTIVEC)
    set_masks &= ~MASK_ALTIVEC;
#endif

  /* Don't override by the processor default if given explicitly.  */
  set_masks &= ~target_flags_explicit;

  /* Identify the processor type.  */
  rs6000_select[0].string = default_cpu;
  rs6000_cpu = TARGET_POWERPC64 ? PROCESSOR_DEFAULT64 : PROCESSOR_DEFAULT;

  for (i = 0; i < ARRAY_SIZE (rs6000_select); i++)
    {
      ptr = &rs6000_select[i];
      if (ptr->string != (char *)0 && ptr->string[0] != '\0')
	{
	  for (j = 0; j < ptt_size; j++)
	    if (! strcmp (ptr->string, processor_target_table[j].name))
	      {
		if (ptr->set_tune_p)
		  rs6000_cpu = processor_target_table[j].processor;

		if (ptr->set_arch_p)
		  {
		    target_flags &= ~set_masks;
		    target_flags |= (processor_target_table[j].target_enable
				     & set_masks);
		  }
		break;
	      }

	  if (j == ptt_size)
	    error ("bad value (%s) for %s switch", ptr->string, ptr->name);
	}
    }

  if (TARGET_E500)
    rs6000_isel = 1;

  /* If we are optimizing big endian systems for space, use the load/store
     multiple and string instructions.  */
  if (BYTES_BIG_ENDIAN && optimize_size)
    target_flags |= ~target_flags_explicit & (MASK_MULTIPLE | MASK_STRING);

  /* Don't allow -mmultiple or -mstring on little endian systems
     unless the cpu is a 750, because the hardware doesn't support the
     instructions used in little endian mode, and causes an alignment
     trap.  The 750 does not cause an alignment trap (except when the
     target is unaligned).  */

  if (!BYTES_BIG_ENDIAN && rs6000_cpu != PROCESSOR_PPC750)
    {
      if (TARGET_MULTIPLE)
	{
	  target_flags &= ~MASK_MULTIPLE;
	  if ((target_flags_explicit & MASK_MULTIPLE) != 0)
	    warning ("-mmultiple is not supported on little endian systems");
	}

      if (TARGET_STRING)
	{
	  target_flags &= ~MASK_STRING;
	  if ((target_flags_explicit & MASK_STRING) != 0)
	    warning ("-mstring is not supported on little endian systems");
	}
    }

  /* Set debug flags */
  if (rs6000_debug_name)
    {
      if (! strcmp (rs6000_debug_name, "all"))
	rs6000_debug_stack = rs6000_debug_arg = 1;
      else if (! strcmp (rs6000_debug_name, "stack"))
	rs6000_debug_stack = 1;
      else if (! strcmp (rs6000_debug_name, "arg"))
	rs6000_debug_arg = 1;
      else
	error ("unknown -mdebug-%s switch", rs6000_debug_name);
    }

  if (rs6000_traceback_name)
    {
      if (! strncmp (rs6000_traceback_name, "full", 4))
	rs6000_traceback = traceback_full;
      else if (! strncmp (rs6000_traceback_name, "part", 4))
	rs6000_traceback = traceback_part;
      else if (! strncmp (rs6000_traceback_name, "no", 2))
	rs6000_traceback = traceback_none;
      else
	error ("unknown -mtraceback arg `%s'; expecting `full', `partial' or `none'",
	       rs6000_traceback_name);
    }

  /* Set size of long double */
  rs6000_long_double_type_size = 64;
  if (rs6000_long_double_size_string)
    {
      char *tail;
      int size = strtol (rs6000_long_double_size_string, &tail, 10);
      if (*tail != '\0' || (size != 64 && size != 128))
	error ("Unknown switch -mlong-double-%s",
	       rs6000_long_double_size_string);
      else
	rs6000_long_double_type_size = size;
    }

  /* Set Altivec ABI as default for powerpc64 linux.  */
  if (TARGET_ELF && TARGET_64BIT)
    {
      rs6000_altivec_abi = 1;
      rs6000_altivec_vrsave = 1;
    }

  /* Handle -mabi= options.  */
  rs6000_parse_abi_options ();

  /* Handle -malign-XXXXX option.  */
  rs6000_parse_alignment_option ();

  /* Handle generic -mFOO=YES/NO options.  */
  rs6000_parse_yes_no_option ("vrsave", rs6000_altivec_vrsave_string,
			      &rs6000_altivec_vrsave);
  rs6000_parse_yes_no_option ("isel", rs6000_isel_string,
			      &rs6000_isel);
  rs6000_parse_yes_no_option ("spe", rs6000_spe_string, &rs6000_spe);
  rs6000_parse_yes_no_option ("float-gprs", rs6000_float_gprs_string,
			      &rs6000_float_gprs);

  /* Handle -mtls-size option.  */
  rs6000_parse_tls_size_option ();

#ifdef SUBTARGET_OVERRIDE_OPTIONS
  SUBTARGET_OVERRIDE_OPTIONS;
#endif
#ifdef SUBSUBTARGET_OVERRIDE_OPTIONS
  SUBSUBTARGET_OVERRIDE_OPTIONS;
#endif

  if (TARGET_E500)
    {
      if (TARGET_ALTIVEC)
      error ("AltiVec and E500 instructions cannot coexist");

      /* The e500 does not have string instructions, and we set
	 MASK_STRING above when optimizing for size.  */
      if ((target_flags & MASK_STRING) != 0)
	target_flags = target_flags & ~MASK_STRING;

      /* No SPE means 64-bit long doubles, even if an E500.  */
      if (rs6000_spe_string != 0
          && !strcmp (rs6000_spe_string, "no"))
	rs6000_long_double_type_size = 64;
    }
  else if (rs6000_select[1].string != NULL)
    {
      /* For the powerpc-eabispe configuration, we set all these by
	 default, so let's unset them if we manually set another
	 CPU that is not the E500.  */
      if (rs6000_abi_string == 0)
	rs6000_spe_abi = 0;
      if (rs6000_spe_string == 0)
	rs6000_spe = 0;
      if (rs6000_float_gprs_string == 0)
	rs6000_float_gprs = 0;
      if (rs6000_isel_string == 0)
	rs6000_isel = 0;
      if (rs6000_long_double_size_string == 0)
	rs6000_long_double_type_size = 64;
    }

  rs6000_always_hint = (rs6000_cpu != PROCESSOR_POWER4
			&& rs6000_cpu != PROCESSOR_POWER5);
  rs6000_sched_groups = (rs6000_cpu == PROCESSOR_POWER4
			 || rs6000_cpu == PROCESSOR_POWER5);

  /* Handle -m(no-)longcall option.  This is a bit of a cheap hack,
     using TARGET_OPTIONS to handle a toggle switch, but we're out of
     bits in target_flags so TARGET_SWITCHES cannot be used.
     Assumption here is that rs6000_longcall_switch points into the
     text of the complete option, rather than being a copy, so we can
     scan back for the presence or absence of the no- modifier.  */
  if (rs6000_longcall_switch)
    {
      const char *base = rs6000_longcall_switch;
      while (base[-1] != 'm') base--;

      if (*rs6000_longcall_switch != '\0')
	error ("invalid option `%s'", base);
      rs6000_default_long_calls = (base[0] != 'n');
    }

  /* Handle -m(no-)warn-altivec-long similarly.  */
  if (rs6000_warn_altivec_long_switch)
    {
      const char *base = rs6000_warn_altivec_long_switch;
      while (base[-1] != 'm') base--;

      if (*rs6000_warn_altivec_long_switch != '\0')
       error ("invalid option `%s'", base);
      rs6000_warn_altivec_long = (base[0] != 'n');
    }

  /* Handle -mprioritize-restricted-insns option.  */
  rs6000_sched_restricted_insns_priority
    = (rs6000_sched_groups ? 1 : 0);
  if (rs6000_sched_restricted_insns_priority_str)
    rs6000_sched_restricted_insns_priority =
      atoi (rs6000_sched_restricted_insns_priority_str);

  /* Handle -msched-costly-dep option.  */
  rs6000_sched_costly_dep
    = (rs6000_sched_groups ? store_to_load_dep_costly : no_dep_costly);
  if (rs6000_sched_costly_dep_str)
    {
      if (! strcmp (rs6000_sched_costly_dep_str, "no"))  
        rs6000_sched_costly_dep = no_dep_costly;
      else if (! strcmp (rs6000_sched_costly_dep_str, "all"))
        rs6000_sched_costly_dep = all_deps_costly;
      else if (! strcmp (rs6000_sched_costly_dep_str, "true_store_to_load"))
        rs6000_sched_costly_dep = true_store_to_load_dep_costly;
      else if (! strcmp (rs6000_sched_costly_dep_str, "store_to_load"))
        rs6000_sched_costly_dep = store_to_load_dep_costly;
      else 
        rs6000_sched_costly_dep = atoi (rs6000_sched_costly_dep_str);
    }

  /* Handle -minsert-sched-nops option.  */
  rs6000_sched_insert_nops
    = (rs6000_sched_groups ? sched_finish_regroup_exact : sched_finish_none);
  if (rs6000_sched_insert_nops_str)
    {
      if (! strcmp (rs6000_sched_insert_nops_str, "no"))
        rs6000_sched_insert_nops = sched_finish_none;
      else if (! strcmp (rs6000_sched_insert_nops_str, "pad"))
        rs6000_sched_insert_nops = sched_finish_pad_groups;
      else if (! strcmp (rs6000_sched_insert_nops_str, "regroup_exact"))
        rs6000_sched_insert_nops = sched_finish_regroup_exact;
      else
        rs6000_sched_insert_nops = atoi (rs6000_sched_insert_nops_str);
    }

#ifdef TARGET_REGNAMES
  /* If the user desires alternate register names, copy in the
     alternate names now.  */
  if (TARGET_REGNAMES)
    memcpy (rs6000_reg_names, alt_reg_names, sizeof (rs6000_reg_names));
#endif

  /* Set TARGET_AIX_STRUCT_RET last, after the ABI is determined.
     If -maix-struct-return or -msvr4-struct-return was explicitly
     used, don't override with the ABI default.  */
  if ((target_flags_explicit & MASK_AIX_STRUCT_RET) == 0)
    {
      if (DEFAULT_ABI == ABI_V4 && !DRAFT_V4_STRUCT_RET)
	target_flags = (target_flags & ~MASK_AIX_STRUCT_RET);
      else
	target_flags |= MASK_AIX_STRUCT_RET;
    }

  if (TARGET_LONG_DOUBLE_128
      && (DEFAULT_ABI == ABI_AIX || DEFAULT_ABI == ABI_DARWIN))
    REAL_MODE_FORMAT (TFmode) = &ibm_extended_format;

  /* Allocate an alias set for register saves & restores from stack.  */
  rs6000_sr_alias_set = new_alias_set ();

  if (TARGET_TOC) 
    ASM_GENERATE_INTERNAL_LABEL (toc_label_name, "LCTOC", 1);

  /* We can only guarantee the availability of DI pseudo-ops when
     assembling for 64-bit targets.  */
  if (!TARGET_64BIT)
    {
      targetm.asm_out.aligned_op.di = NULL;
      targetm.asm_out.unaligned_op.di = NULL;
    }

  /* Set maximum branch target alignment at two instructions, eight bytes.  */
  align_jumps_max_skip = 8;
  align_loops_max_skip = 8;

  /* Arrange to save and restore machine status around nested functions.  */
  init_machine_status = rs6000_init_machine_status;

  /* We should always be splitting complex arguments, but we can't break
     Linux and Darwin ABIs at the moment.  For now, only AIX is fixed.  */
  if (DEFAULT_ABI != ABI_AIX)
    targetm.calls.split_complex_arg = NULL;
}

/* Handle generic options of the form -mfoo=yes/no.
   NAME is the option name.
   VALUE is the option value.
   FLAG is the pointer to the flag where to store a 1 or 0, depending on
   whether the option value is 'yes' or 'no' respectively.  */
static void
rs6000_parse_yes_no_option (const char *name, const char *value, int *flag)
{
  if (value == 0)
    return;
  else if (!strcmp (value, "yes"))
    *flag = 1;
  else if (!strcmp (value, "no"))
    *flag = 0;
  else
    error ("unknown -m%s= option specified: '%s'", name, value);
}

/* Handle -mabi= options.  */
static void
rs6000_parse_abi_options (void)
{
  if (rs6000_abi_string == 0)
    return;
  else if (! strcmp (rs6000_abi_string, "altivec"))
    {
      rs6000_altivec_abi = 1;
      rs6000_spe_abi = 0;
    }
  else if (! strcmp (rs6000_abi_string, "no-altivec"))
    rs6000_altivec_abi = 0;
  else if (! strcmp (rs6000_abi_string, "spe"))
    {
      rs6000_spe_abi = 1;
      rs6000_altivec_abi = 0;
      if (!TARGET_SPE_ABI)
	error ("not configured for ABI: '%s'", rs6000_abi_string);
    }
  
  else if (! strcmp (rs6000_abi_string, "no-spe"))
    rs6000_spe_abi = 0;
  else
    error ("unknown ABI specified: '%s'", rs6000_abi_string);
}

/* Handle -malign-XXXXXX options.  */
static void
rs6000_parse_alignment_option (void)
{
  if (rs6000_alignment_string == 0)
    return;
  else if (! strcmp (rs6000_alignment_string, "power"))
    rs6000_alignment_flags = MASK_ALIGN_POWER;
  else if (! strcmp (rs6000_alignment_string, "natural"))
    rs6000_alignment_flags = MASK_ALIGN_NATURAL;
  else
    error ("unknown -malign-XXXXX option specified: '%s'",
	   rs6000_alignment_string);
}

/* Validate and record the size specified with the -mtls-size option.  */

static void
rs6000_parse_tls_size_option (void)
{
  if (rs6000_tls_size_string == 0)
    return;
  else if (strcmp (rs6000_tls_size_string, "16") == 0)
    rs6000_tls_size = 16;
  else if (strcmp (rs6000_tls_size_string, "32") == 0)
    rs6000_tls_size = 32;
  else if (strcmp (rs6000_tls_size_string, "64") == 0)
    rs6000_tls_size = 64;
  else
    error ("bad value `%s' for -mtls-size switch", rs6000_tls_size_string);
}

void
optimization_options (int level ATTRIBUTE_UNUSED, int size ATTRIBUTE_UNUSED)
{
}

/* Do anything needed at the start of the asm file.  */

static void
rs6000_file_start (void)
{
  size_t i;
  char buffer[80];
  const char *start = buffer;
  struct rs6000_cpu_select *ptr;
  const char *default_cpu = TARGET_CPU_DEFAULT;
  FILE *file = asm_out_file;

  default_file_start ();

#ifdef TARGET_BI_ARCH
  if ((TARGET_DEFAULT ^ target_flags) & MASK_64BIT)
    default_cpu = 0;
#endif

  if (flag_verbose_asm)
    {
      sprintf (buffer, "\n%s rs6000/powerpc options:", ASM_COMMENT_START);
      rs6000_select[0].string = default_cpu;

      for (i = 0; i < ARRAY_SIZE (rs6000_select); i++)
	{
	  ptr = &rs6000_select[i];
	  if (ptr->string != (char *)0 && ptr->string[0] != '\0')
	    {
	      fprintf (file, "%s %s%s", start, ptr->name, ptr->string);
	      start = "";
	    }
	}

#ifdef USING_ELFOS_H
      switch (rs6000_sdata)
	{
	case SDATA_NONE: fprintf (file, "%s -msdata=none", start); start = ""; break;
	case SDATA_DATA: fprintf (file, "%s -msdata=data", start); start = ""; break;
	case SDATA_SYSV: fprintf (file, "%s -msdata=sysv", start); start = ""; break;
	case SDATA_EABI: fprintf (file, "%s -msdata=eabi", start); start = ""; break;
	}

      if (rs6000_sdata && g_switch_value)
	{
	  fprintf (file, "%s -G " HOST_WIDE_INT_PRINT_UNSIGNED, start,
		   g_switch_value);
	  start = "";
	}
#endif

      if (*start == '\0')
	putc ('\n', file);
    }
}

/* Return nonzero if this function is known to have a null epilogue.  */

int
direct_return (void)
{
  if (reload_completed)
    {
      rs6000_stack_t *info = rs6000_stack_info ();

      if (info->first_gp_reg_save == 32
	  && info->first_fp_reg_save == 64
	  && info->first_altivec_reg_save == LAST_ALTIVEC_REGNO + 1
	  && ! info->lr_save_p
	  && ! info->cr_save_p
	  && info->vrsave_mask == 0
	  && ! info->push_p)
	return 1;
    }

  return 0;
}

/* Returns 1 always.  */

int
any_operand (rtx op ATTRIBUTE_UNUSED, 
	     enum machine_mode mode ATTRIBUTE_UNUSED)
{
  return 1;
}

/* Returns 1 if op is the count register.  */
int
count_register_operand (rtx op, enum machine_mode mode ATTRIBUTE_UNUSED)
{
  if (GET_CODE (op) != REG)
    return 0;

  if (REGNO (op) == COUNT_REGISTER_REGNUM)
    return 1;

  if (REGNO (op) > FIRST_PSEUDO_REGISTER)
    return 1;

  return 0;
}

/* Returns 1 if op is an altivec register.  */
int
altivec_register_operand (rtx op, enum machine_mode mode ATTRIBUTE_UNUSED)
{
  
  return (register_operand (op, mode)
	  && (GET_CODE (op) != REG
	      || REGNO (op) > FIRST_PSEUDO_REGISTER
	      || ALTIVEC_REGNO_P (REGNO (op))));
}

int
xer_operand (rtx op, enum machine_mode mode ATTRIBUTE_UNUSED)
{
  if (GET_CODE (op) != REG)
    return 0;

  if (XER_REGNO_P (REGNO (op)))
    return 1;

  return 0;
}

/* Return 1 if OP is a signed 8-bit constant.  Int multiplication
   by such constants completes more quickly.  */

int
s8bit_cint_operand (rtx op, enum machine_mode mode ATTRIBUTE_UNUSED)
{
  return ( GET_CODE (op) == CONST_INT
	  && (INTVAL (op) >= -128 && INTVAL (op) <= 127));
}

/* Return 1 if OP is a constant that can fit in a D field.  */

int
short_cint_operand (rtx op, enum machine_mode mode ATTRIBUTE_UNUSED)
{
  return (GET_CODE (op) == CONST_INT
	  && CONST_OK_FOR_LETTER_P (INTVAL (op), 'I'));
}

/* Similar for an unsigned D field.  */

int
u_short_cint_operand (rtx op, enum machine_mode mode ATTRIBUTE_UNUSED)
{
  return (GET_CODE (op) == CONST_INT
	  && CONST_OK_FOR_LETTER_P (INTVAL (op) & GET_MODE_MASK (mode), 'K'));
}

/* Return 1 if OP is a CONST_INT that cannot fit in a signed D field.  */

int
non_short_cint_operand (rtx op, enum machine_mode mode ATTRIBUTE_UNUSED)
{
  return (GET_CODE (op) == CONST_INT
	  && (unsigned HOST_WIDE_INT) (INTVAL (op) + 0x8000) >= 0x10000);
}

/* Returns 1 if OP is a CONST_INT that is a positive value
   and an exact power of 2.  */

int
exact_log2_cint_operand (rtx op, enum machine_mode mode ATTRIBUTE_UNUSED)
{
  return (GET_CODE (op) == CONST_INT
	  && INTVAL (op) > 0
	  && exact_log2 (INTVAL (op)) >= 0);
}

/* Returns 1 if OP is a register that is not special (i.e., not MQ,
   ctr, or lr).  */

int
gpc_reg_operand (rtx op, enum machine_mode mode)
{
  return (register_operand (op, mode)
	  && (GET_CODE (op) != REG
	      || (REGNO (op) >= ARG_POINTER_REGNUM 
		  && !XER_REGNO_P (REGNO (op)))
	      || REGNO (op) < MQ_REGNO));
}

/* Returns 1 if OP is either a pseudo-register or a register denoting a
   CR field.  */

int
cc_reg_operand (rtx op, enum machine_mode mode)
{
  return (register_operand (op, mode)
	  && (GET_CODE (op) != REG
	      || REGNO (op) >= FIRST_PSEUDO_REGISTER
	      || CR_REGNO_P (REGNO (op))));
}

/* Returns 1 if OP is either a pseudo-register or a register denoting a
   CR field that isn't CR0.  */

int
cc_reg_not_cr0_operand (rtx op, enum machine_mode mode)
{
  return (register_operand (op, mode)
	  && (GET_CODE (op) != REG
	      || REGNO (op) >= FIRST_PSEUDO_REGISTER
	      || CR_REGNO_NOT_CR0_P (REGNO (op))));
}

/* Returns 1 if OP is either a constant integer valid for a D-field or
   a non-special register.  If a register, it must be in the proper
   mode unless MODE is VOIDmode.  */

int
reg_or_short_operand (rtx op, enum machine_mode mode)
{
  return short_cint_operand (op, mode) || gpc_reg_operand (op, mode);
}

/* Similar, except check if the negation of the constant would be
   valid for a D-field.  Don't allow a constant zero, since all the
   patterns that call this predicate use "addic r1,r2,-constant" on
   a constant value to set a carry when r2 is greater or equal to
   "constant".  That doesn't work for zero.  */

int
reg_or_neg_short_operand (rtx op, enum machine_mode mode)
{
  if (GET_CODE (op) == CONST_INT)
    return CONST_OK_FOR_LETTER_P (INTVAL (op), 'P') && INTVAL (op) != 0;

  return gpc_reg_operand (op, mode);
}

/* Returns 1 if OP is either a constant integer valid for a DS-field or
   a non-special register.  If a register, it must be in the proper
   mode unless MODE is VOIDmode.  */

int
reg_or_aligned_short_operand (rtx op, enum machine_mode mode)
{
  if (gpc_reg_operand (op, mode))
    return 1;
  else if (short_cint_operand (op, mode) && !(INTVAL (op) & 3))
    return 1;

  return 0;
}


/* Return 1 if the operand is either a register or an integer whose
   high-order 16 bits are zero.  */

int
reg_or_u_short_operand (rtx op, enum machine_mode mode)
{
  return u_short_cint_operand (op, mode) || gpc_reg_operand (op, mode);
}

/* Return 1 is the operand is either a non-special register or ANY
   constant integer.  */

int
reg_or_cint_operand (rtx op, enum machine_mode mode)
{
  return (GET_CODE (op) == CONST_INT || gpc_reg_operand (op, mode));
}

/* Return 1 is the operand is either a non-special register or ANY
   32-bit signed constant integer.  */

int
reg_or_arith_cint_operand (rtx op, enum machine_mode mode)
{
  return (gpc_reg_operand (op, mode)
	  || (GET_CODE (op) == CONST_INT
#if HOST_BITS_PER_WIDE_INT != 32
	      && ((unsigned HOST_WIDE_INT) (INTVAL (op) + 0x80000000)
		  < (unsigned HOST_WIDE_INT) 0x100000000ll)
#endif
	      ));
}

/* Return 1 is the operand is either a non-special register or a 32-bit
   signed constant integer valid for 64-bit addition.  */

int
reg_or_add_cint64_operand (rtx op, enum machine_mode mode)
{
  return (gpc_reg_operand (op, mode)
	  || (GET_CODE (op) == CONST_INT
#if HOST_BITS_PER_WIDE_INT == 32
	      && INTVAL (op) < 0x7fff8000
#else
	      && ((unsigned HOST_WIDE_INT) (INTVAL (op) + 0x80008000)
		  < 0x100000000ll)
#endif
	      ));
}

/* Return 1 is the operand is either a non-special register or a 32-bit
   signed constant integer valid for 64-bit subtraction.  */

int
reg_or_sub_cint64_operand (rtx op, enum machine_mode mode)
{
  return (gpc_reg_operand (op, mode)
	  || (GET_CODE (op) == CONST_INT
#if HOST_BITS_PER_WIDE_INT == 32
	      && (- INTVAL (op)) < 0x7fff8000
#else
	      && ((unsigned HOST_WIDE_INT) ((- INTVAL (op)) + 0x80008000)
		  < 0x100000000ll)
#endif
	      ));
}

/* Return 1 is the operand is either a non-special register or ANY
   32-bit unsigned constant integer.  */

int
reg_or_logical_cint_operand (rtx op, enum machine_mode mode)
{
  if (GET_CODE (op) == CONST_INT)
    {
      if (GET_MODE_BITSIZE (mode) > HOST_BITS_PER_WIDE_INT)
	{
	  if (GET_MODE_BITSIZE (mode) <= 32)
	    abort ();

	  if (INTVAL (op) < 0)
	    return 0;
	}

      return ((INTVAL (op) & GET_MODE_MASK (mode)
	       & (~ (unsigned HOST_WIDE_INT) 0xffffffff)) == 0);
    }
  else if (GET_CODE (op) == CONST_DOUBLE)
    {
      if (GET_MODE_BITSIZE (mode) <= HOST_BITS_PER_WIDE_INT
	  || mode != DImode)
	abort ();

      return CONST_DOUBLE_HIGH (op) == 0;
    }
  else 
    return gpc_reg_operand (op, mode);
}

/* Return 1 if the operand is an operand that can be loaded via the GOT.  */

int
got_operand (rtx op, enum machine_mode mode ATTRIBUTE_UNUSED)
{
  return (GET_CODE (op) == SYMBOL_REF
	  || GET_CODE (op) == CONST
	  || GET_CODE (op) == LABEL_REF);
}

/* Return 1 if the operand is a simple references that can be loaded via
   the GOT (labels involving addition aren't allowed).  */

int
got_no_const_operand (rtx op, enum machine_mode mode ATTRIBUTE_UNUSED)
{
  return (GET_CODE (op) == SYMBOL_REF || GET_CODE (op) == LABEL_REF);
}

/* Return the number of instructions it takes to form a constant in an
   integer register.  */

static int
num_insns_constant_wide (HOST_WIDE_INT value)
{
  /* signed constant loadable with {cal|addi} */
  if (CONST_OK_FOR_LETTER_P (value, 'I'))
    return 1;

  /* constant loadable with {cau|addis} */
  else if (CONST_OK_FOR_LETTER_P (value, 'L'))
    return 1;

#if HOST_BITS_PER_WIDE_INT == 64
  else if (TARGET_POWERPC64)
    {
      HOST_WIDE_INT low  = ((value & 0xffffffff) ^ 0x80000000) - 0x80000000;
      HOST_WIDE_INT high = value >> 31;

      if (high == 0 || high == -1)
	return 2;

      high >>= 1;

      if (low == 0)
	return num_insns_constant_wide (high) + 1;
      else
	return (num_insns_constant_wide (high)
		+ num_insns_constant_wide (low) + 1);
    }
#endif

  else
    return 2;
}

int
num_insns_constant (rtx op, enum machine_mode mode)
{
  if (GET_CODE (op) == CONST_INT)
    {
#if HOST_BITS_PER_WIDE_INT == 64
      if ((INTVAL (op) >> 31) != 0 && (INTVAL (op) >> 31) != -1
	  && mask64_operand (op, mode))
	    return 2;
      else
#endif
	return num_insns_constant_wide (INTVAL (op));
    }

  else if (GET_CODE (op) == CONST_DOUBLE && mode == SFmode)
    {
      long l;
      REAL_VALUE_TYPE rv;

      REAL_VALUE_FROM_CONST_DOUBLE (rv, op);
      REAL_VALUE_TO_TARGET_SINGLE (rv, l);
      return num_insns_constant_wide ((HOST_WIDE_INT) l);
    }

  else if (GET_CODE (op) == CONST_DOUBLE)
    {
      HOST_WIDE_INT low;
      HOST_WIDE_INT high;
      long l[2];
      REAL_VALUE_TYPE rv;
      int endian = (WORDS_BIG_ENDIAN == 0);

      if (mode == VOIDmode || mode == DImode)
	{
	  high = CONST_DOUBLE_HIGH (op);
	  low  = CONST_DOUBLE_LOW (op);
	}
      else
	{
	  REAL_VALUE_FROM_CONST_DOUBLE (rv, op);
	  REAL_VALUE_TO_TARGET_DOUBLE (rv, l);
	  high = l[endian];
	  low  = l[1 - endian];
	}

      if (TARGET_32BIT)
	return (num_insns_constant_wide (low)
		+ num_insns_constant_wide (high));

      else
	{
	  if (high == 0 && low >= 0)
	    return num_insns_constant_wide (low);

	  else if (high == -1 && low < 0)
	    return num_insns_constant_wide (low);

	  else if (mask64_operand (op, mode))
	    return 2;

	  else if (low == 0)
	    return num_insns_constant_wide (high) + 1;

	  else
	    return (num_insns_constant_wide (high)
		    + num_insns_constant_wide (low) + 1);
	}
    }

  else
    abort ();
}

/* Return 1 if the operand is a CONST_DOUBLE and it can be put into a
   register with one instruction per word.  We only do this if we can
   safely read CONST_DOUBLE_{LOW,HIGH}.  */

int
easy_fp_constant (rtx op, enum machine_mode mode)
{
  if (GET_CODE (op) != CONST_DOUBLE
      || GET_MODE (op) != mode
      || (GET_MODE_CLASS (mode) != MODE_FLOAT && mode != DImode))
    return 0;

  /* Consider all constants with -msoft-float to be easy.  */
  if ((TARGET_SOFT_FLOAT || !TARGET_FPRS)
      && mode != DImode)
    return 1;

  /* If we are using V.4 style PIC, consider all constants to be hard.  */
  if (flag_pic && DEFAULT_ABI == ABI_V4)
    return 0;

#ifdef TARGET_RELOCATABLE
  /* Similarly if we are using -mrelocatable, consider all constants
     to be hard.  */
  if (TARGET_RELOCATABLE)
    return 0;
#endif

  if (mode == TFmode)
    {
      long k[4];
      REAL_VALUE_TYPE rv;

      REAL_VALUE_FROM_CONST_DOUBLE (rv, op);
      REAL_VALUE_TO_TARGET_LONG_DOUBLE (rv, k);

      return (num_insns_constant_wide ((HOST_WIDE_INT) k[0]) == 1
	      && num_insns_constant_wide ((HOST_WIDE_INT) k[1]) == 1
	      && num_insns_constant_wide ((HOST_WIDE_INT) k[2]) == 1
	      && num_insns_constant_wide ((HOST_WIDE_INT) k[3]) == 1);
    }

  else if (mode == DFmode)
    {
      long k[2];
      REAL_VALUE_TYPE rv;

      REAL_VALUE_FROM_CONST_DOUBLE (rv, op);
      REAL_VALUE_TO_TARGET_DOUBLE (rv, k);

      return (num_insns_constant_wide ((HOST_WIDE_INT) k[0]) == 1
	      && num_insns_constant_wide ((HOST_WIDE_INT) k[1]) == 1);
    }

  else if (mode == SFmode)
    {
      long l;
      REAL_VALUE_TYPE rv;

      REAL_VALUE_FROM_CONST_DOUBLE (rv, op);
      REAL_VALUE_TO_TARGET_SINGLE (rv, l);

      return num_insns_constant_wide (l) == 1;
    }

  else if (mode == DImode)
    return ((TARGET_POWERPC64
	     && GET_CODE (op) == CONST_DOUBLE && CONST_DOUBLE_LOW (op) == 0)
	    || (num_insns_constant (op, DImode) <= 2));

  else if (mode == SImode)
    return 1;
  else
    abort ();
}

/* Returns the constant for the splat instrunction, if exists.  */

static int
easy_vector_splat_const (int cst, enum machine_mode mode)
{
  switch (mode) 
    {
    case V4SImode:
      if (EASY_VECTOR_15 (cst) 
	  || EASY_VECTOR_15_ADD_SELF (cst)) 
	return cst;
      if ((cst & 0xffff) != ((cst >> 16) & 0xffff))
	break;
      cst = cst >> 16;
    case V8HImode:
      if (EASY_VECTOR_15 (cst) 
	  || EASY_VECTOR_15_ADD_SELF (cst)) 
	return cst;
      if ((cst & 0xff) != ((cst >> 8) & 0xff))
	break;
      cst = cst >> 8;
    case V16QImode:
	  if (EASY_VECTOR_15 (cst) 
	      || EASY_VECTOR_15_ADD_SELF (cst)) 
	    return cst;
    default: 
      break;
    }
  return 0;
}


/* Return nonzero if all elements of a vector have the same value.  */

static int
easy_vector_same (rtx op, enum machine_mode mode ATTRIBUTE_UNUSED)
{
  int units, i, cst;

  units = CONST_VECTOR_NUNITS (op);

  cst = INTVAL (CONST_VECTOR_ELT (op, 0));
  for (i = 1; i < units; ++i)
    if (INTVAL (CONST_VECTOR_ELT (op, i)) != cst)
      break;
  if (i == units && easy_vector_splat_const (cst, mode))
    return 1;
  return 0;
}

/* Return 1 if the operand is a CONST_INT and can be put into a
   register without using memory.  */

int
easy_vector_constant (rtx op, enum machine_mode mode)
{
  int cst, cst2;

  if (GET_CODE (op) != CONST_VECTOR
      || (!TARGET_ALTIVEC
	  && !TARGET_SPE))
    return 0;

  if (zero_constant (op, mode)
      && ((TARGET_ALTIVEC && ALTIVEC_VECTOR_MODE (mode))
	  || (TARGET_SPE && SPE_VECTOR_MODE (mode))))
    return 1;

  if (GET_MODE_CLASS (mode) != MODE_VECTOR_INT)
    return 0;

  if (TARGET_SPE && mode == V1DImode)
    return 0;

  cst  = INTVAL (CONST_VECTOR_ELT (op, 0));
  cst2 = INTVAL (CONST_VECTOR_ELT (op, 1));

  /* Limit SPE vectors to 15 bits signed.  These we can generate with:
       li r0, CONSTANT1
       evmergelo r0, r0, r0
       li r0, CONSTANT2

     I don't know how efficient it would be to allow bigger constants,
     considering we'll have an extra 'ori' for every 'li'.  I doubt 5
     instructions is better than a 64-bit memory load, but I don't
     have the e500 timing specs.  */
  if (TARGET_SPE && mode == V2SImode
      && cst  >= -0x7fff && cst <= 0x7fff
      && cst2 >= -0x7fff && cst2 <= 0x7fff)
    return 1;

  if (TARGET_ALTIVEC 
      && easy_vector_same (op, mode))
    {
      cst = easy_vector_splat_const (cst, mode);
      if (EASY_VECTOR_15_ADD_SELF (cst) 
	  || EASY_VECTOR_15 (cst))
	return 1;
    }  
  return 0;
}

/* Same as easy_vector_constant but only for EASY_VECTOR_15_ADD_SELF.  */

int
easy_vector_constant_add_self (rtx op, enum machine_mode mode)
{
  int cst;
  if (TARGET_ALTIVEC
      && GET_CODE (op) == CONST_VECTOR
      && easy_vector_same (op, mode))
    {
      cst = easy_vector_splat_const (INTVAL (CONST_VECTOR_ELT (op, 0)), mode);
      if (EASY_VECTOR_15_ADD_SELF (cst))
	return 1;  
    }
  return 0;
}

/* Generate easy_vector_constant out of a easy_vector_constant_add_self.  */

rtx 
gen_easy_vector_constant_add_self (rtx op)
{
  int i, units;
  rtvec v;
  units = GET_MODE_NUNITS (GET_MODE (op));
  v = rtvec_alloc (units);

  for (i = 0; i < units; i++)
    RTVEC_ELT (v, i) = 
      GEN_INT (INTVAL (CONST_VECTOR_ELT (op, i)) >> 1);
  return gen_rtx_raw_CONST_VECTOR (GET_MODE (op), v);
}

const char *
output_vec_const_move (rtx *operands)
{
  int cst, cst2;
  enum machine_mode mode;
  rtx dest, vec;

  dest = operands[0];
  vec = operands[1];

  cst = INTVAL (CONST_VECTOR_ELT (vec, 0));
  cst2 = INTVAL (CONST_VECTOR_ELT (vec, 1));
  mode = GET_MODE (dest);

  if (TARGET_ALTIVEC)
    {
      if (zero_constant (vec, mode))
	return "vxor %0,%0,%0";
      else if (easy_vector_constant (vec, mode))
	{
	  operands[1] = GEN_INT (cst);
	  switch (mode)
	    {
	    case V4SImode:
	      if (EASY_VECTOR_15 (cst))
		{
		  operands[1] = GEN_INT (cst);
		  return "vspltisw %0,%1";
		}
	      else if (EASY_VECTOR_15_ADD_SELF (cst))
		return "#";
	      cst = cst >> 16;
	    case V8HImode:
	      if (EASY_VECTOR_15 (cst))
		{
		  operands[1] = GEN_INT (cst);
		  return "vspltish %0,%1";
		}
	      else if (EASY_VECTOR_15_ADD_SELF (cst))
		return "#";
	      cst = cst >> 8;
	    case V16QImode:
	      if (EASY_VECTOR_15 (cst))
		{
		  operands[1] = GEN_INT (cst);
		  return "vspltisb %0,%1";
		}
	      else if (EASY_VECTOR_15_ADD_SELF (cst))
		return "#";
	    default:
	      abort ();
	    }
	}
      else
	abort ();
    }

  if (TARGET_SPE)
    {
      /* Vector constant 0 is handled as a splitter of V2SI, and in the
	 pattern of V1DI, V4HI, and V2SF.

	 FIXME: We should probably return # and add post reload
	 splitters for these, but this way is so easy ;-).
      */
      operands[1] = GEN_INT (cst);
      operands[2] = GEN_INT (cst2);
      if (cst == cst2)
	return "li %0,%1\n\tevmergelo %0,%0,%0";
      else
	return "li %0,%1\n\tevmergelo %0,%0,%0\n\tli %0,%2";
    }

  abort ();
}

/* Return 1 if the operand is the constant 0.  This works for scalars
   as well as vectors.  */
int
zero_constant (rtx op, enum machine_mode mode)
{
  return op == CONST0_RTX (mode);
}

/* Return 1 if the operand is 0.0.  */
int
zero_fp_constant (rtx op, enum machine_mode mode)
{
  return GET_MODE_CLASS (mode) == MODE_FLOAT && op == CONST0_RTX (mode);
}

/* Return 1 if the operand is in volatile memory.  Note that during
   the RTL generation phase, memory_operand does not return TRUE for
   volatile memory references.  So this function allows us to
   recognize volatile references where its safe.  */

int
volatile_mem_operand (rtx op, enum machine_mode mode)
{
  if (GET_CODE (op) != MEM)
    return 0;

  if (!MEM_VOLATILE_P (op))
    return 0;

  if (mode != GET_MODE (op))
    return 0;

  if (reload_completed)
    return memory_operand (op, mode);

  if (reload_in_progress)
    return strict_memory_address_p (mode, XEXP (op, 0));

  return memory_address_p (mode, XEXP (op, 0));
}

/* Return 1 if the operand is an offsettable memory operand.  */

int
offsettable_mem_operand (rtx op, enum machine_mode mode)
{
  return ((GET_CODE (op) == MEM)
	  && offsettable_address_p (reload_completed || reload_in_progress,
				    mode, XEXP (op, 0)));
}

/* Return 1 if the operand is either an easy FP constant (see above) or
   memory.  */

int
mem_or_easy_const_operand (rtx op, enum machine_mode mode)
{
  return memory_operand (op, mode) || easy_fp_constant (op, mode);
}

/* Return 1 if the operand is either a non-special register or an item
   that can be used as the operand of a `mode' add insn.  */

int
add_operand (rtx op, enum machine_mode mode)
{
  if (GET_CODE (op) == CONST_INT)
    return (CONST_OK_FOR_LETTER_P (INTVAL (op), 'I')
	    || CONST_OK_FOR_LETTER_P (INTVAL (op), 'L'));

  return gpc_reg_operand (op, mode);
}

/* Return 1 if OP is a constant but not a valid add_operand.  */

int
non_add_cint_operand (rtx op, enum machine_mode mode ATTRIBUTE_UNUSED)
{
  return (GET_CODE (op) == CONST_INT
	  && !CONST_OK_FOR_LETTER_P (INTVAL (op), 'I')
	  && !CONST_OK_FOR_LETTER_P (INTVAL (op), 'L'));
}

/* Return 1 if the operand is a non-special register or a constant that
   can be used as the operand of an OR or XOR insn on the RS/6000.  */

int
logical_operand (rtx op, enum machine_mode mode)
{
  HOST_WIDE_INT opl, oph;

  if (gpc_reg_operand (op, mode))
    return 1;

  if (GET_CODE (op) == CONST_INT)
    {
      opl = INTVAL (op) & GET_MODE_MASK (mode);

#if HOST_BITS_PER_WIDE_INT <= 32
      if (GET_MODE_BITSIZE (mode) > HOST_BITS_PER_WIDE_INT && opl < 0)
	return 0;
#endif
    }
  else if (GET_CODE (op) == CONST_DOUBLE)
    {
      if (GET_MODE_BITSIZE (mode) <= HOST_BITS_PER_WIDE_INT)
	abort ();

      opl = CONST_DOUBLE_LOW (op);
      oph = CONST_DOUBLE_HIGH (op);
      if (oph != 0)
	return 0;
    }
  else
    return 0;

  return ((opl & ~ (unsigned HOST_WIDE_INT) 0xffff) == 0
	  || (opl & ~ (unsigned HOST_WIDE_INT) 0xffff0000) == 0);
}

/* Return 1 if C is a constant that is not a logical operand (as
   above), but could be split into one.  */

int
non_logical_cint_operand (rtx op, enum machine_mode mode)
{
  return ((GET_CODE (op) == CONST_INT || GET_CODE (op) == CONST_DOUBLE)
	  && ! logical_operand (op, mode)
	  && reg_or_logical_cint_operand (op, mode));
}

/* Return 1 if C is a constant that can be encoded in a 32-bit mask on the
   RS/6000.  It is if there are no more than two 1->0 or 0->1 transitions.
   Reject all ones and all zeros, since these should have been optimized
   away and confuse the making of MB and ME.  */

int
mask_operand (rtx op, enum machine_mode mode ATTRIBUTE_UNUSED)
{
  HOST_WIDE_INT c, lsb;

  if (GET_CODE (op) != CONST_INT)
    return 0;

  c = INTVAL (op);

  /* Fail in 64-bit mode if the mask wraps around because the upper
     32-bits of the mask will all be 1s, contrary to GCC's internal view.  */
  if (TARGET_POWERPC64 && (c & 0x80000001) == 0x80000001)
    return 0;

  /* We don't change the number of transitions by inverting,
     so make sure we start with the LS bit zero.  */
  if (c & 1)
    c = ~c;

  /* Reject all zeros or all ones.  */
  if (c == 0)
    return 0;

  /* Find the first transition.  */
  lsb = c & -c;

  /* Invert to look for a second transition.  */
  c = ~c;

  /* Erase first transition.  */
  c &= -lsb;

  /* Find the second transition (if any).  */
  lsb = c & -c;

  /* Match if all the bits above are 1's (or c is zero).  */
  return c == -lsb;
}

/* Return 1 for the PowerPC64 rlwinm corner case.  */

int
mask_operand_wrap (rtx op, enum machine_mode mode ATTRIBUTE_UNUSED)
{
  HOST_WIDE_INT c, lsb;

  if (GET_CODE (op) != CONST_INT)
    return 0;

  c = INTVAL (op);

  if ((c & 0x80000001) != 0x80000001)
    return 0;

  c = ~c;
  if (c == 0)
    return 0;

  lsb = c & -c;
  c = ~c;
  c &= -lsb;
  lsb = c & -c;
  return c == -lsb;
}

/* Return 1 if the operand is a constant that is a PowerPC64 mask.
   It is if there are no more than one 1->0 or 0->1 transitions.
   Reject all zeros, since zero should have been optimized away and
   confuses the making of MB and ME.  */

int
mask64_operand (rtx op, enum machine_mode mode ATTRIBUTE_UNUSED)
{
  if (GET_CODE (op) == CONST_INT)
    {
      HOST_WIDE_INT c, lsb;

      c = INTVAL (op);

      /* Reject all zeros.  */
      if (c == 0)
	return 0;

      /* We don't change the number of transitions by inverting,
	 so make sure we start with the LS bit zero.  */
      if (c & 1)
	c = ~c;

      /* Find the transition, and check that all bits above are 1's.  */
      lsb = c & -c;

      /* Match if all the bits above are 1's (or c is zero).  */
      return c == -lsb;
    }
  return 0;
}

/* Like mask64_operand, but allow up to three transitions.  This
   predicate is used by insn patterns that generate two rldicl or
   rldicr machine insns.  */

int
mask64_2_operand (rtx op, enum machine_mode mode ATTRIBUTE_UNUSED)
{
  if (GET_CODE (op) == CONST_INT)
    {
      HOST_WIDE_INT c, lsb;

      c = INTVAL (op);

      /* Disallow all zeros.  */
      if (c == 0)
	return 0;

      /* We don't change the number of transitions by inverting,
	 so make sure we start with the LS bit zero.  */
      if (c & 1)
	c = ~c;

      /* Find the first transition.  */
      lsb = c & -c;

      /* Invert to look for a second transition.  */
      c = ~c;

      /* Erase first transition.  */
      c &= -lsb;

      /* Find the second transition.  */
      lsb = c & -c;

      /* Invert to look for a third transition.  */
      c = ~c;

      /* Erase second transition.  */
      c &= -lsb;

      /* Find the third transition (if any).  */
      lsb = c & -c;

      /* Match if all the bits above are 1's (or c is zero).  */
      return c == -lsb;
    }
  return 0;
}

/* Generates shifts and masks for a pair of rldicl or rldicr insns to
   implement ANDing by the mask IN.  */
void
build_mask64_2_operands (rtx in, rtx *out)
{
#if HOST_BITS_PER_WIDE_INT >= 64
  unsigned HOST_WIDE_INT c, lsb, m1, m2;
  int shift;

  if (GET_CODE (in) != CONST_INT)
    abort ();

  c = INTVAL (in);
  if (c & 1)
    {
      /* Assume c initially something like 0x00fff000000fffff.  The idea
	 is to rotate the word so that the middle ^^^^^^ group of zeros
	 is at the MS end and can be cleared with an rldicl mask.  We then
	 rotate back and clear off the MS    ^^ group of zeros with a
	 second rldicl.  */
      c = ~c;			/*   c == 0xff000ffffff00000 */
      lsb = c & -c;		/* lsb == 0x0000000000100000 */
      m1 = -lsb;		/*  m1 == 0xfffffffffff00000 */
      c = ~c;			/*   c == 0x00fff000000fffff */
      c &= -lsb;		/*   c == 0x00fff00000000000 */
      lsb = c & -c;		/* lsb == 0x0000100000000000 */
      c = ~c;			/*   c == 0xff000fffffffffff */
      c &= -lsb;		/*   c == 0xff00000000000000 */
      shift = 0;
      while ((lsb >>= 1) != 0)
	shift++;		/* shift == 44 on exit from loop */
      m1 <<= 64 - shift;	/*  m1 == 0xffffff0000000000 */
      m1 = ~m1;			/*  m1 == 0x000000ffffffffff */
      m2 = ~c;			/*  m2 == 0x00ffffffffffffff */
    }
  else
    {
      /* Assume c initially something like 0xff000f0000000000.  The idea
	 is to rotate the word so that the     ^^^  middle group of zeros
	 is at the LS end and can be cleared with an rldicr mask.  We then
	 rotate back and clear off the LS group of ^^^^^^^^^^ zeros with
	 a second rldicr.  */
      lsb = c & -c;		/* lsb == 0x0000010000000000 */
      m2 = -lsb;		/*  m2 == 0xffffff0000000000 */
      c = ~c;			/*   c == 0x00fff0ffffffffff */
      c &= -lsb;		/*   c == 0x00fff00000000000 */
      lsb = c & -c;		/* lsb == 0x0000100000000000 */
      c = ~c;			/*   c == 0xff000fffffffffff */
      c &= -lsb;		/*   c == 0xff00000000000000 */
      shift = 0;
      while ((lsb >>= 1) != 0)
	shift++;		/* shift == 44 on exit from loop */
      m1 = ~c;			/*  m1 == 0x00ffffffffffffff */
      m1 >>= shift;		/*  m1 == 0x0000000000000fff */
      m1 = ~m1;			/*  m1 == 0xfffffffffffff000 */
    }

  /* Note that when we only have two 0->1 and 1->0 transitions, one of the
     masks will be all 1's.  We are guaranteed more than one transition.  */
  out[0] = GEN_INT (64 - shift);
  out[1] = GEN_INT (m1);
  out[2] = GEN_INT (shift);
  out[3] = GEN_INT (m2);
#else
  (void)in;
  (void)out;
  abort ();
#endif
}

/* Return 1 if the operand is either a non-special register or a constant
   that can be used as the operand of a PowerPC64 logical AND insn.  */

int
and64_operand (rtx op, enum machine_mode mode)
{
  if (fixed_regs[CR0_REGNO])	/* CR0 not available, don't do andi./andis.  */
    return (gpc_reg_operand (op, mode) || mask64_operand (op, mode));

  return (logical_operand (op, mode) || mask64_operand (op, mode));
}

/* Like the above, but also match constants that can be implemented
   with two rldicl or rldicr insns.  */

int
and64_2_operand (rtx op, enum machine_mode mode)
{
  if (fixed_regs[CR0_REGNO])	/* CR0 not available, don't do andi./andis.  */
    return gpc_reg_operand (op, mode) || mask64_2_operand (op, mode);

  return logical_operand (op, mode) || mask64_2_operand (op, mode);
}

/* Return 1 if the operand is either a non-special register or a
   constant that can be used as the operand of an RS/6000 logical AND insn.  */

int
and_operand (rtx op, enum machine_mode mode)
{
  if (fixed_regs[CR0_REGNO])	/* CR0 not available, don't do andi./andis.  */
    return (gpc_reg_operand (op, mode) || mask_operand (op, mode));

  return (logical_operand (op, mode) || mask_operand (op, mode));
}

/* Return 1 if the operand is a general register or memory operand.  */

int
reg_or_mem_operand (rtx op, enum machine_mode mode)
{
  return (gpc_reg_operand (op, mode)
	  || memory_operand (op, mode)
	  || macho_lo_sum_memory_operand (op, mode)
	  || volatile_mem_operand (op, mode));
}

/* Return 1 if the operand is a general register or memory operand without
   pre_inc or pre_dec which produces invalid form of PowerPC lwa
   instruction.  */

int
lwa_operand (rtx op, enum machine_mode mode)
{
  rtx inner = op;

  if (reload_completed && GET_CODE (inner) == SUBREG)
    inner = SUBREG_REG (inner);
    
  return gpc_reg_operand (inner, mode)
    || (memory_operand (inner, mode)
	&& GET_CODE (XEXP (inner, 0)) != PRE_INC
	&& GET_CODE (XEXP (inner, 0)) != PRE_DEC
	&& (GET_CODE (XEXP (inner, 0)) != PLUS
	    || GET_CODE (XEXP (XEXP (inner, 0), 1)) != CONST_INT
	    || INTVAL (XEXP (XEXP (inner, 0), 1)) % 4 == 0));
}

/* Return 1 if the operand, used inside a MEM, is a SYMBOL_REF.  */

int
symbol_ref_operand (rtx op, enum machine_mode mode)
{
  if (mode != VOIDmode && GET_MODE (op) != mode)
    return 0;

  return (GET_CODE (op) == SYMBOL_REF
	  && (DEFAULT_ABI != ABI_AIX || SYMBOL_REF_FUNCTION_P (op)));
}

/* Return 1 if the operand, used inside a MEM, is a valid first argument
   to CALL.  This is a SYMBOL_REF, a pseudo-register, LR or CTR.  */

int
call_operand (rtx op, enum machine_mode mode)
{
  if (mode != VOIDmode && GET_MODE (op) != mode)
    return 0;

  return (GET_CODE (op) == SYMBOL_REF
	  || (GET_CODE (op) == REG
	      && (REGNO (op) == LINK_REGISTER_REGNUM
		  || REGNO (op) == COUNT_REGISTER_REGNUM
		  || REGNO (op) >= FIRST_PSEUDO_REGISTER)));
}

/* Return 1 if the operand is a SYMBOL_REF for a function known to be in
   this file.  */

int
current_file_function_operand (rtx op, 
                              enum machine_mode mode ATTRIBUTE_UNUSED)
{
  return (GET_CODE (op) == SYMBOL_REF
	  && (DEFAULT_ABI != ABI_AIX || SYMBOL_REF_FUNCTION_P (op))
	  && (SYMBOL_REF_LOCAL_P (op)
	      || (op == XEXP (DECL_RTL (current_function_decl), 0))));
}

/* Return 1 if this operand is a valid input for a move insn.  */

int
input_operand (rtx op, enum machine_mode mode)
{
  /* Memory is always valid.  */
  if (memory_operand (op, mode))
    return 1;

  /* Only a tiny bit of handling for CONSTANT_P_RTX is necessary.  */
  if (GET_CODE (op) == CONSTANT_P_RTX)
    return 1;

  /* For floating-point, easy constants are valid.  */
  if (GET_MODE_CLASS (mode) == MODE_FLOAT
      && CONSTANT_P (op)
      && easy_fp_constant (op, mode))
    return 1;

  /* Allow any integer constant.  */
  if (GET_MODE_CLASS (mode) == MODE_INT
      && (GET_CODE (op) == CONST_INT
	  || GET_CODE (op) == CONST_DOUBLE))
    return 1;

  /* Allow easy vector constants.  */
  if (GET_CODE (op) == CONST_VECTOR
      && easy_vector_constant (op, mode))
    return 1;

  /* For floating-point or multi-word mode, the only remaining valid type
     is a register.  */
  if (GET_MODE_CLASS (mode) == MODE_FLOAT
      || GET_MODE_SIZE (mode) > UNITS_PER_WORD)
    return register_operand (op, mode);

  /* The only cases left are integral modes one word or smaller (we
     do not get called for MODE_CC values).  These can be in any
     register.  */
  if (register_operand (op, mode))
    return 1;

  /* A SYMBOL_REF referring to the TOC is valid.  */
  if (legitimate_constant_pool_address_p (op))
    return 1;

  /* A constant pool expression (relative to the TOC) is valid */
  if (toc_relative_expr_p (op))
    return 1;

  /* V.4 allows SYMBOL_REFs and CONSTs that are in the small data region
     to be valid.  */
  if (DEFAULT_ABI == ABI_V4
      && (GET_CODE (op) == SYMBOL_REF || GET_CODE (op) == CONST)
      && small_data_operand (op, Pmode))
    return 1;

  return 0;
}


/* Darwin, AIX increases natural record alignment to doubleword if the first
   field is an FP double while the FP fields remain word aligned.  */

unsigned int
rs6000_special_round_type_align (tree type, int computed, int specified)
{
  tree field = TYPE_FIELDS (type);

  /* Skip all the static variables only if ABI is greater than
     1 or equal to 0.   */
  while (field != NULL && TREE_CODE (field) == VAR_DECL)
    field = TREE_CHAIN (field);

  if (field == NULL || field == type || DECL_MODE (field) != DFmode)
    return MAX (computed, specified);

  return MAX (MAX (computed, specified), 64);
}

/* Return 1 for an operand in small memory on V.4/eabi.  */

int
small_data_operand (rtx op ATTRIBUTE_UNUSED, 
		    enum machine_mode mode ATTRIBUTE_UNUSED)
{
#if TARGET_ELF
  rtx sym_ref;

  if (rs6000_sdata == SDATA_NONE || rs6000_sdata == SDATA_DATA)
    return 0;

  if (DEFAULT_ABI != ABI_V4)
    return 0;

  if (GET_CODE (op) == SYMBOL_REF)
    sym_ref = op;

  else if (GET_CODE (op) != CONST
	   || GET_CODE (XEXP (op, 0)) != PLUS
	   || GET_CODE (XEXP (XEXP (op, 0), 0)) != SYMBOL_REF
	   || GET_CODE (XEXP (XEXP (op, 0), 1)) != CONST_INT)
    return 0;

  else
    {
      rtx sum = XEXP (op, 0);
      HOST_WIDE_INT summand;

      /* We have to be careful here, because it is the referenced address
        that must be 32k from _SDA_BASE_, not just the symbol.  */
      summand = INTVAL (XEXP (sum, 1));
      if (summand < 0 || (unsigned HOST_WIDE_INT) summand > g_switch_value)
       return 0;

      sym_ref = XEXP (sum, 0);
    }

  return SYMBOL_REF_SMALL_P (sym_ref);
#else
  return 0;
#endif
}

/* Return true, if operand is a memory operand and has a
   displacement divisible by 4.  */

int
word_offset_memref_operand (rtx op, enum machine_mode mode ATTRIBUTE_UNUSED)
{
  rtx addr;
  int off = 0;

  if (!memory_operand (op, mode))
    return 0;

  addr = XEXP (op, 0);
  if (GET_CODE (addr) == PLUS
      && GET_CODE (XEXP (addr, 0)) == REG
      && GET_CODE (XEXP (addr, 1)) == CONST_INT)
    off = INTVAL (XEXP (addr, 1));

  return (off % 4) == 0;
}

/* Return true if either operand is a general purpose register.  */

bool
gpr_or_gpr_p (rtx op0, rtx op1)
{
  return ((REG_P (op0) && INT_REGNO_P (REGNO (op0)))
	  || (REG_P (op1) && INT_REGNO_P (REGNO (op1))));
}


/* Subroutines of rs6000_legitimize_address and rs6000_legitimate_address.  */

static int 
constant_pool_expr_1 (rtx op, int *have_sym, int *have_toc) 
{
  switch (GET_CODE(op)) 
    {
    case SYMBOL_REF:
      if (RS6000_SYMBOL_REF_TLS_P (op))
	return 0;
      else if (CONSTANT_POOL_ADDRESS_P (op))
	{
	  if (ASM_OUTPUT_SPECIAL_POOL_ENTRY_P (get_pool_constant (op), Pmode))
	    {
	      *have_sym = 1;
	      return 1;
	    }
	  else
	    return 0;
	}
      else if (! strcmp (XSTR (op, 0), toc_label_name))
	{
	  *have_toc = 1;
	  return 1;
	}
      else
	return 0;
    case PLUS:
    case MINUS:
      return (constant_pool_expr_1 (XEXP (op, 0), have_sym, have_toc)
	      && constant_pool_expr_1 (XEXP (op, 1), have_sym, have_toc));
    case CONST:
      return constant_pool_expr_1 (XEXP (op, 0), have_sym, have_toc);
    case CONST_INT:
      return 1;
    default:
      return 0;
    }
}

static bool
constant_pool_expr_p (rtx op)
{
  int have_sym = 0;
  int have_toc = 0;
  return constant_pool_expr_1 (op, &have_sym, &have_toc) && have_sym;
}

static bool
toc_relative_expr_p (rtx op)
{
  int have_sym = 0;
  int have_toc = 0;
  return constant_pool_expr_1 (op, &have_sym, &have_toc) && have_toc;
}

/* SPE offset addressing is limited to 5-bits worth of double words.  */
#define SPE_CONST_OFFSET_OK(x) (((x) & ~0xf8) == 0)

bool
legitimate_constant_pool_address_p (rtx x)
{
  return (TARGET_TOC
	  && GET_CODE (x) == PLUS
	  && GET_CODE (XEXP (x, 0)) == REG
	  && (TARGET_MINIMAL_TOC || REGNO (XEXP (x, 0)) == TOC_REGISTER)
	  && constant_pool_expr_p (XEXP (x, 1)));
}

static bool
legitimate_small_data_p (enum machine_mode mode, rtx x)
{
  return (DEFAULT_ABI == ABI_V4
	  && !flag_pic && !TARGET_TOC
	  && (GET_CODE (x) == SYMBOL_REF || GET_CODE (x) == CONST)
	  && small_data_operand (x, mode));
}

static bool
legitimate_offset_address_p (enum machine_mode mode, rtx x, int strict)
{
  unsigned HOST_WIDE_INT offset, extra;

  if (GET_CODE (x) != PLUS)
    return false;
  if (GET_CODE (XEXP (x, 0)) != REG)
    return false;
  if (!INT_REG_OK_FOR_BASE_P (XEXP (x, 0), strict))
    return false;
  if (GET_CODE (XEXP (x, 1)) != CONST_INT)
    return false;

  offset = INTVAL (XEXP (x, 1));
  extra = 0;
  switch (mode)
    {
    case V16QImode:
    case V8HImode:
    case V4SFmode:
    case V4SImode:
      /* AltiVec vector modes.  Only reg+reg addressing is valid here,
	 which leaves the only valid constant offset of zero, which by
	 canonicalization rules is also invalid.  */
      return false;

    case V4HImode:
    case V2SImode:
    case V1DImode:
    case V2SFmode:
      /* SPE vector modes.  */
      return SPE_CONST_OFFSET_OK (offset);

    case DFmode:
    case DImode:
      if (mode == DFmode || !TARGET_POWERPC64)
	extra = 4;
      else if (offset & 3)
	return false;
      break;

    case TFmode:
    case TImode:
      if (mode == TFmode || !TARGET_POWERPC64)
	extra = 12;
      else if (offset & 3)
	return false;
      else
	extra = 8;
      break;

    default:
      break;
    }

  offset += 0x8000;
  return (offset < 0x10000) && (offset + extra < 0x10000);
}

static bool
legitimate_indexed_address_p (rtx x, int strict)
{
  rtx op0, op1;

  if (GET_CODE (x) != PLUS)
    return false;
  op0 = XEXP (x, 0);
  op1 = XEXP (x, 1);

  if (!REG_P (op0) || !REG_P (op1))
    return false;

  return ((INT_REG_OK_FOR_BASE_P (op0, strict)
	   && INT_REG_OK_FOR_INDEX_P (op1, strict))
	  || (INT_REG_OK_FOR_BASE_P (op1, strict)
	      && INT_REG_OK_FOR_INDEX_P (op0, strict)));
}

static inline bool
legitimate_indirect_address_p (rtx x, int strict)
{
  return GET_CODE (x) == REG && INT_REG_OK_FOR_BASE_P (x, strict);
}

static bool
macho_lo_sum_memory_operand (rtx x, enum machine_mode mode)
{
    if (!TARGET_MACHO || !flag_pic
        || mode != SImode || GET_CODE(x) != MEM)
      return false;
    x = XEXP (x, 0);

  if (GET_CODE (x) != LO_SUM)
    return false;
  if (GET_CODE (XEXP (x, 0)) != REG)
    return false;
  if (!INT_REG_OK_FOR_BASE_P (XEXP (x, 0), 0))
    return false;
  x = XEXP (x, 1);

  return CONSTANT_P (x);
}

static bool
legitimate_lo_sum_address_p (enum machine_mode mode, rtx x, int strict)
{
  if (GET_CODE (x) != LO_SUM)
    return false;
  if (GET_CODE (XEXP (x, 0)) != REG)
    return false;
  if (!INT_REG_OK_FOR_BASE_P (XEXP (x, 0), strict))
    return false;
  x = XEXP (x, 1);

  if (TARGET_ELF || TARGET_MACHO)
    {
      if (DEFAULT_ABI != ABI_AIX && DEFAULT_ABI != ABI_DARWIN && flag_pic)
	return false;
      if (TARGET_TOC)
	return false;
      if (GET_MODE_NUNITS (mode) != 1)
	return false;
      if (GET_MODE_BITSIZE (mode) > 32
	  && !(TARGET_HARD_FLOAT && TARGET_FPRS && mode == DFmode))
	return false;

      return CONSTANT_P (x);
    }

  return false;
}


/* Try machine-dependent ways of modifying an illegitimate address
   to be legitimate.  If we find one, return the new, valid address.
   This is used from only one place: `memory_address' in explow.c.

   OLDX is the address as it was before break_out_memory_refs was
   called.  In some cases it is useful to look at this to decide what
   needs to be done.

   MODE is passed so that this function can use GO_IF_LEGITIMATE_ADDRESS.

   It is always safe for this function to do nothing.  It exists to
   recognize opportunities to optimize the output.

   On RS/6000, first check for the sum of a register with a constant
   integer that is out of range.  If so, generate code to add the
   constant with the low-order 16 bits masked to the register and force
   this result into another register (this can be done with `cau').
   Then generate an address of REG+(CONST&0xffff), allowing for the
   possibility of bit 16 being a one.

   Then check for the sum of a register and something not constant, try to
   load the other things into a register and return the sum.  */

rtx
rs6000_legitimize_address (rtx x, rtx oldx ATTRIBUTE_UNUSED,
			   enum machine_mode mode)
{
  if (GET_CODE (x) == SYMBOL_REF)
    {
      enum tls_model model = SYMBOL_REF_TLS_MODEL (x);
      if (model != 0)
	return rs6000_legitimize_tls_address (x, model);
    }

  if (GET_CODE (x) == PLUS 
      && GET_CODE (XEXP (x, 0)) == REG
      && GET_CODE (XEXP (x, 1)) == CONST_INT
      && (unsigned HOST_WIDE_INT) (INTVAL (XEXP (x, 1)) + 0x8000) >= 0x10000)
    { 
      HOST_WIDE_INT high_int, low_int;
      rtx sum;
      low_int = ((INTVAL (XEXP (x, 1)) & 0xffff) ^ 0x8000) - 0x8000;
      high_int = INTVAL (XEXP (x, 1)) - low_int;
      sum = force_operand (gen_rtx_PLUS (Pmode, XEXP (x, 0),
					 GEN_INT (high_int)), 0);
      return gen_rtx_PLUS (Pmode, sum, GEN_INT (low_int));
    }
  else if (GET_CODE (x) == PLUS 
	   && GET_CODE (XEXP (x, 0)) == REG
	   && GET_CODE (XEXP (x, 1)) != CONST_INT
	   && GET_MODE_NUNITS (mode) == 1
	   && ((TARGET_HARD_FLOAT && TARGET_FPRS)
	       || TARGET_POWERPC64
	       || (mode != DFmode && mode != TFmode))
	   && (TARGET_POWERPC64 || mode != DImode)
	   && mode != TImode)
    {
      return gen_rtx_PLUS (Pmode, XEXP (x, 0),
			   force_reg (Pmode, force_operand (XEXP (x, 1), 0)));
    }
  else if (ALTIVEC_VECTOR_MODE (mode))
    {
      rtx reg;

      /* Make sure both operands are registers.  */
      if (GET_CODE (x) == PLUS)
	return gen_rtx_PLUS (Pmode, force_reg (Pmode, XEXP (x, 0)),
			     force_reg (Pmode, XEXP (x, 1)));

      reg = force_reg (Pmode, x);
      return reg;
    }
  else if (SPE_VECTOR_MODE (mode))
    {
      /* We accept [reg + reg] and [reg + OFFSET].  */

      if (GET_CODE (x) == PLUS)
      {
        rtx op1 = XEXP (x, 0);
        rtx op2 = XEXP (x, 1);

        op1 = force_reg (Pmode, op1);

        if (GET_CODE (op2) != REG
            && (GET_CODE (op2) != CONST_INT
                || !SPE_CONST_OFFSET_OK (INTVAL (op2))))
          op2 = force_reg (Pmode, op2);

        return gen_rtx_PLUS (Pmode, op1, op2);
      }

      return force_reg (Pmode, x);
    }
  else if (TARGET_ELF
	   && TARGET_32BIT
	   && TARGET_NO_TOC
	   && ! flag_pic
	   && GET_CODE (x) != CONST_INT
	   && GET_CODE (x) != CONST_DOUBLE 
	   && CONSTANT_P (x)
	   && GET_MODE_NUNITS (mode) == 1
	   && (GET_MODE_BITSIZE (mode) <= 32
	       || ((TARGET_HARD_FLOAT && TARGET_FPRS) && mode == DFmode)))
    {
      rtx reg = gen_reg_rtx (Pmode);
      emit_insn (gen_elf_high (reg, x));
      return gen_rtx_LO_SUM (Pmode, reg, x);
    }
  else if (TARGET_MACHO && TARGET_32BIT && TARGET_NO_TOC
	   && ! flag_pic
#if TARGET_MACHO
	   && ! MACHO_DYNAMIC_NO_PIC_P
#endif
	   && GET_CODE (x) != CONST_INT
	   && GET_CODE (x) != CONST_DOUBLE 
	   && CONSTANT_P (x)
	   && ((TARGET_HARD_FLOAT && TARGET_FPRS) || mode != DFmode)
	   && mode != DImode 
	   && mode != TImode)
    {
      rtx reg = gen_reg_rtx (Pmode);
      emit_insn (gen_macho_high (reg, x));
      return gen_rtx_LO_SUM (Pmode, reg, x);
    }
  else if (TARGET_TOC 
	   && constant_pool_expr_p (x)
	   && ASM_OUTPUT_SPECIAL_POOL_ENTRY_P (get_pool_constant (x), Pmode))
    {
      return create_TOC_reference (x);
    }
  else
    return NULL_RTX;
}

/* This is called from dwarf2out.c via ASM_OUTPUT_DWARF_DTPREL.
   We need to emit DTP-relative relocations.  */

void
rs6000_output_dwarf_dtprel (FILE *file, int size, rtx x)
{
  switch (size)
    {
    case 4:
      fputs ("\t.long\t", file);
      break;
    case 8:
      fputs (DOUBLE_INT_ASM_OP, file);
      break;
    default:
      abort ();
    }
  output_addr_const (file, x);
  fputs ("@dtprel+0x8000", file);
}

/* Construct the SYMBOL_REF for the tls_get_addr function.  */

static GTY(()) rtx rs6000_tls_symbol;
static rtx
rs6000_tls_get_addr (void)
{
  if (!rs6000_tls_symbol)
    rs6000_tls_symbol = init_one_libfunc ("__tls_get_addr");

  return rs6000_tls_symbol;
}

/* Construct the SYMBOL_REF for TLS GOT references.  */

static GTY(()) rtx rs6000_got_symbol;
static rtx
rs6000_got_sym (void)
{
  if (!rs6000_got_symbol)
    {
      rs6000_got_symbol = gen_rtx_SYMBOL_REF (Pmode, "_GLOBAL_OFFSET_TABLE_");
      SYMBOL_REF_FLAGS (rs6000_got_symbol) |= SYMBOL_FLAG_LOCAL;
      SYMBOL_REF_FLAGS (rs6000_got_symbol) |= SYMBOL_FLAG_EXTERNAL;
    }     

  return rs6000_got_symbol;
}

/* ADDR contains a thread-local SYMBOL_REF.  Generate code to compute
   this (thread-local) address.  */

static rtx
rs6000_legitimize_tls_address (rtx addr, enum tls_model model)
{
  rtx dest, insn;

  dest = gen_reg_rtx (Pmode);
  if (model == TLS_MODEL_LOCAL_EXEC && rs6000_tls_size == 16)
    {
      rtx tlsreg;

      if (TARGET_64BIT)
	{
	  tlsreg = gen_rtx_REG (Pmode, 13);
	  insn = gen_tls_tprel_64 (dest, tlsreg, addr);
	}
      else
	{
	  tlsreg = gen_rtx_REG (Pmode, 2);
	  insn = gen_tls_tprel_32 (dest, tlsreg, addr);
	}
      emit_insn (insn);
    }
  else if (model == TLS_MODEL_LOCAL_EXEC && rs6000_tls_size == 32)
    {
      rtx tlsreg, tmp;

      tmp = gen_reg_rtx (Pmode);
      if (TARGET_64BIT)
	{
	  tlsreg = gen_rtx_REG (Pmode, 13);
	  insn = gen_tls_tprel_ha_64 (tmp, tlsreg, addr);
	}
      else
	{
	  tlsreg = gen_rtx_REG (Pmode, 2);
	  insn = gen_tls_tprel_ha_32 (tmp, tlsreg, addr);
	}
      emit_insn (insn);
      if (TARGET_64BIT)
	insn = gen_tls_tprel_lo_64 (dest, tmp, addr);
      else
	insn = gen_tls_tprel_lo_32 (dest, tmp, addr);
      emit_insn (insn);
    }
  else
    {
      rtx r3, got, tga, tmp1, tmp2, eqv;

      if (TARGET_64BIT)
	got = gen_rtx_REG (Pmode, 2);
      else
	{
	  if (flag_pic == 1)
	    got = gen_rtx_REG (Pmode, RS6000_PIC_OFFSET_TABLE_REGNUM);
	  else
	    {
	      rtx gsym = rs6000_got_sym ();
	      got = gen_reg_rtx (Pmode);
	      if (flag_pic == 0)
		rs6000_emit_move (got, gsym, Pmode);
	      else
		{
		  rtx tempLR, tmp3, mem;
		  rtx first, last;

		  tempLR = gen_reg_rtx (Pmode);
		  tmp1 = gen_reg_rtx (Pmode);
		  tmp2 = gen_reg_rtx (Pmode);
		  tmp3 = gen_reg_rtx (Pmode);
		  mem = gen_rtx_MEM (Pmode, tmp1);
		  RTX_UNCHANGING_P (mem) = 1;

		  first = emit_insn (gen_load_toc_v4_PIC_1b (tempLR, gsym));
		  emit_move_insn (tmp1, tempLR);
		  emit_move_insn (tmp2, mem);
		  emit_insn (gen_addsi3 (tmp3, tmp1, tmp2));
		  last = emit_move_insn (got, tmp3);
		  REG_NOTES (last) = gen_rtx_EXPR_LIST (REG_EQUAL, gsym,
							REG_NOTES (last));
		  REG_NOTES (first) = gen_rtx_INSN_LIST (REG_LIBCALL, last,
							 REG_NOTES (first));
		  REG_NOTES (last) = gen_rtx_INSN_LIST (REG_RETVAL, first,
							REG_NOTES (last));
		}
	    }
	}

      if (model == TLS_MODEL_GLOBAL_DYNAMIC)
	{
	  r3 = gen_rtx_REG (Pmode, 3);
	  if (TARGET_64BIT)
	    insn = gen_tls_gd_64 (r3, got, addr);
	  else
	    insn = gen_tls_gd_32 (r3, got, addr);
	  start_sequence ();
	  emit_insn (insn);
	  tga = gen_rtx_MEM (Pmode, rs6000_tls_get_addr ());
	  insn = gen_call_value (r3, tga, const0_rtx, const0_rtx);
	  insn = emit_call_insn (insn);
	  CONST_OR_PURE_CALL_P (insn) = 1;
	  use_reg (&CALL_INSN_FUNCTION_USAGE (insn), r3);
	  insn = get_insns ();
	  end_sequence ();
	  emit_libcall_block (insn, dest, r3, addr);
	}
      else if (model == TLS_MODEL_LOCAL_DYNAMIC)
	{
	  r3 = gen_rtx_REG (Pmode, 3);
	  if (TARGET_64BIT)
	    insn = gen_tls_ld_64 (r3, got);
	  else
	    insn = gen_tls_ld_32 (r3, got);
	  start_sequence ();
	  emit_insn (insn);
	  tga = gen_rtx_MEM (Pmode, rs6000_tls_get_addr ());
	  insn = gen_call_value (r3, tga, const0_rtx, const0_rtx);
	  insn = emit_call_insn (insn);
	  CONST_OR_PURE_CALL_P (insn) = 1;
	  use_reg (&CALL_INSN_FUNCTION_USAGE (insn), r3);
	  insn = get_insns ();
	  end_sequence ();
	  tmp1 = gen_reg_rtx (Pmode);
	  eqv = gen_rtx_UNSPEC (Pmode, gen_rtvec (1, const0_rtx),
				UNSPEC_TLSLD);
	  emit_libcall_block (insn, tmp1, r3, eqv);
	  if (rs6000_tls_size == 16)
	    {
	      if (TARGET_64BIT)
		insn = gen_tls_dtprel_64 (dest, tmp1, addr);
	      else
		insn = gen_tls_dtprel_32 (dest, tmp1, addr);
	    }
	  else if (rs6000_tls_size == 32)
	    {
	      tmp2 = gen_reg_rtx (Pmode);
	      if (TARGET_64BIT)
		insn = gen_tls_dtprel_ha_64 (tmp2, tmp1, addr);
	      else
		insn = gen_tls_dtprel_ha_32 (tmp2, tmp1, addr);
	      emit_insn (insn);
	      if (TARGET_64BIT)
		insn = gen_tls_dtprel_lo_64 (dest, tmp2, addr);
	      else
		insn = gen_tls_dtprel_lo_32 (dest, tmp2, addr);
	    }
	  else
	    {
	      tmp2 = gen_reg_rtx (Pmode);
	      if (TARGET_64BIT)
		insn = gen_tls_got_dtprel_64 (tmp2, got, addr);
	      else
		insn = gen_tls_got_dtprel_32 (tmp2, got, addr);
	      emit_insn (insn);
	      insn = gen_rtx_SET (Pmode, dest,
				  gen_rtx_PLUS (Pmode, tmp2, tmp1));
	    }
	  emit_insn (insn);
	}
      else
	{
	  /* IE, or 64 bit offset LE.  */
	  tmp2 = gen_reg_rtx (Pmode);
	  if (TARGET_64BIT)
	    insn = gen_tls_got_tprel_64 (tmp2, got, addr);
	  else
	    insn = gen_tls_got_tprel_32 (tmp2, got, addr);
	  emit_insn (insn);
	  if (TARGET_64BIT)
	    insn = gen_tls_tls_64 (dest, tmp2, addr);
	  else
	    insn = gen_tls_tls_32 (dest, tmp2, addr);
	  emit_insn (insn);
	}
    }

  return dest;
}

/* Return 1 if X is a SYMBOL_REF for a TLS symbol.  This is used in
   instruction definitions.  */

int
rs6000_tls_symbol_ref (rtx x, enum machine_mode mode ATTRIBUTE_UNUSED)
{
  return RS6000_SYMBOL_REF_TLS_P (x);
}

/* Return 1 if X contains a thread-local symbol.  */

bool
rs6000_tls_referenced_p (rtx x)
{
  if (! TARGET_HAVE_TLS)
    return false;

  return for_each_rtx (&x, &rs6000_tls_symbol_ref_1, 0);
}

/* Return 1 if *X is a thread-local symbol.  This is the same as
   rs6000_tls_symbol_ref except for the type of the unused argument.  */

static inline int
rs6000_tls_symbol_ref_1 (rtx *x, void *data ATTRIBUTE_UNUSED)
{
  return RS6000_SYMBOL_REF_TLS_P (*x);
}

/* The convention appears to be to define this wherever it is used.
   With legitimize_reload_address now defined here, REG_MODE_OK_FOR_BASE_P
   is now used here.  */
#ifndef REG_MODE_OK_FOR_BASE_P
#define REG_MODE_OK_FOR_BASE_P(REGNO, MODE) REG_OK_FOR_BASE_P (REGNO)
#endif

/* Our implementation of LEGITIMIZE_RELOAD_ADDRESS.  Returns a value to
   replace the input X, or the original X if no replacement is called for.
   The output parameter *WIN is 1 if the calling macro should goto WIN,
   0 if it should not.

   For RS/6000, we wish to handle large displacements off a base
   register by splitting the addend across an addiu/addis and the mem insn.
   This cuts number of extra insns needed from 3 to 1.

   On Darwin, we use this to generate code for floating point constants.
   A movsf_low is generated so we wind up with 2 instructions rather than 3.
   The Darwin code is inside #if TARGET_MACHO because only then is
   machopic_function_base_name() defined.  */
rtx
rs6000_legitimize_reload_address (rtx x, enum machine_mode mode, 
	int opnum, int type, int ind_levels ATTRIBUTE_UNUSED, int *win)
{
  /* We must recognize output that we have already generated ourselves.  */ 
  if (GET_CODE (x) == PLUS
      && GET_CODE (XEXP (x, 0)) == PLUS
      && GET_CODE (XEXP (XEXP (x, 0), 0)) == REG
      && GET_CODE (XEXP (XEXP (x, 0), 1)) == CONST_INT
      && GET_CODE (XEXP (x, 1)) == CONST_INT)
    {
      push_reload (XEXP (x, 0), NULL_RTX, &XEXP (x, 0), NULL,
                   BASE_REG_CLASS, GET_MODE (x), VOIDmode, 0, 0,
                   opnum, (enum reload_type)type);
      *win = 1;
      return x;
    }

#if TARGET_MACHO
  if (DEFAULT_ABI == ABI_DARWIN && flag_pic
      && GET_CODE (x) == LO_SUM
      && GET_CODE (XEXP (x, 0)) == PLUS
      && XEXP (XEXP (x, 0), 0) == pic_offset_table_rtx
      && GET_CODE (XEXP (XEXP (x, 0), 1)) == HIGH
      && GET_CODE (XEXP (XEXP (XEXP (x, 0), 1), 0)) == CONST
      && XEXP (XEXP (XEXP (x, 0), 1), 0) == XEXP (x, 1)
      && GET_CODE (XEXP (XEXP (x, 1), 0)) == MINUS
      && GET_CODE (XEXP (XEXP (XEXP (x, 1), 0), 0)) == SYMBOL_REF
      && GET_CODE (XEXP (XEXP (XEXP (x, 1), 0), 1)) == SYMBOL_REF)
    {
      /* Result of previous invocation of this function on Darwin
	 floating point constant.  */
      push_reload (XEXP (x, 0), NULL_RTX, &XEXP (x, 0), NULL,
		BASE_REG_CLASS, Pmode, VOIDmode, 0, 0,
		opnum, (enum reload_type)type);
      *win = 1;
      return x;
    }
#endif

  /* Force ld/std non-word aligned offset into base register by wrapping
     in offset 0.  */
  if (GET_CODE (x) == PLUS
      && GET_CODE (XEXP (x, 0)) == REG
      && REGNO (XEXP (x, 0)) < 32
      && REG_MODE_OK_FOR_BASE_P (XEXP (x, 0), mode)
      && GET_CODE (XEXP (x, 1)) == CONST_INT
      && (INTVAL (XEXP (x, 1)) & 3) != 0
      && GET_MODE_SIZE (mode) >= UNITS_PER_WORD
      && TARGET_POWERPC64)
    {
      x = gen_rtx_PLUS (GET_MODE (x), x, GEN_INT (0));
      push_reload (XEXP (x, 0), NULL_RTX, &XEXP (x, 0), NULL,
		   BASE_REG_CLASS, GET_MODE (x), VOIDmode, 0, 0,
		   opnum, (enum reload_type) type);
      *win = 1;
      return x;
    }

  if (GET_CODE (x) == PLUS
      && GET_CODE (XEXP (x, 0)) == REG
      && REGNO (XEXP (x, 0)) < FIRST_PSEUDO_REGISTER
      && REG_MODE_OK_FOR_BASE_P (XEXP (x, 0), mode)
      && GET_CODE (XEXP (x, 1)) == CONST_INT
      && !SPE_VECTOR_MODE (mode)
      && !ALTIVEC_VECTOR_MODE (mode))
    {
      HOST_WIDE_INT val = INTVAL (XEXP (x, 1));
      HOST_WIDE_INT low = ((val & 0xffff) ^ 0x8000) - 0x8000;
      HOST_WIDE_INT high
        = (((val - low) & 0xffffffff) ^ 0x80000000) - 0x80000000;

      /* Check for 32-bit overflow.  */
      if (high + low != val)
        {
	  *win = 0;
	  return x;
	}

      /* Reload the high part into a base reg; leave the low part
         in the mem directly.  */

      x = gen_rtx_PLUS (GET_MODE (x),
                        gen_rtx_PLUS (GET_MODE (x), XEXP (x, 0),
                                      GEN_INT (high)),
                        GEN_INT (low));

      push_reload (XEXP (x, 0), NULL_RTX, &XEXP (x, 0), NULL,
                   BASE_REG_CLASS, GET_MODE (x), VOIDmode, 0, 0,
                   opnum, (enum reload_type)type);
      *win = 1;
      return x;
    }

#if TARGET_MACHO
  if (GET_CODE (x) == SYMBOL_REF
      && DEFAULT_ABI == ABI_DARWIN
      && !ALTIVEC_VECTOR_MODE (mode)
      && (flag_pic || MACHO_DYNAMIC_NO_PIC_P)
      /* Don't do this for TFmode, since the result isn't offsettable.  */
      && mode != TFmode)
    {
      if (flag_pic)
	{
	  rtx offset = gen_rtx_CONST (Pmode,
			 gen_rtx_MINUS (Pmode, x,
			   gen_rtx_SYMBOL_REF (Pmode,
			     machopic_function_base_name ())));
	  x = gen_rtx_LO_SUM (GET_MODE (x),
		gen_rtx_PLUS (Pmode, pic_offset_table_rtx,
		  gen_rtx_HIGH (Pmode, offset)), offset);
	}
      else
	x = gen_rtx_LO_SUM (GET_MODE (x),
              gen_rtx_HIGH (Pmode, x), x);

      push_reload (XEXP (x, 0), NULL_RTX, &XEXP (x, 0), NULL,
		   BASE_REG_CLASS, Pmode, VOIDmode, 0, 0,
		   opnum, (enum reload_type)type);
      *win = 1;
      return x;
    }
#endif

  if (TARGET_TOC
      && constant_pool_expr_p (x)
      && ASM_OUTPUT_SPECIAL_POOL_ENTRY_P (get_pool_constant (x), mode))
    {
      (x) = create_TOC_reference (x);
      *win = 1;
      return x;
    }
  *win = 0;
  return x;
}    

/* GO_IF_LEGITIMATE_ADDRESS recognizes an RTL expression
   that is a valid memory address for an instruction.
   The MODE argument is the machine mode for the MEM expression
   that wants to use this address.

   On the RS/6000, there are four valid address: a SYMBOL_REF that
   refers to a constant pool entry of an address (or the sum of it
   plus a constant), a short (16-bit signed) constant plus a register,
   the sum of two registers, or a register indirect, possibly with an
   auto-increment.  For DFmode and DImode with a constant plus register,
   we must ensure that both words are addressable or PowerPC64 with offset
   word aligned.

   For modes spanning multiple registers (DFmode in 32-bit GPRs,
   32-bit DImode, TImode), indexed addressing cannot be used because
   adjacent memory cells are accessed by adding word-sized offsets
   during assembly output.  */
int
rs6000_legitimate_address (enum machine_mode mode, rtx x, int reg_ok_strict)
{
  if (RS6000_SYMBOL_REF_TLS_P (x))
    return 0;
  if (legitimate_indirect_address_p (x, reg_ok_strict))
    return 1;
  if ((GET_CODE (x) == PRE_INC || GET_CODE (x) == PRE_DEC)
      && !ALTIVEC_VECTOR_MODE (mode)
      && !SPE_VECTOR_MODE (mode)
      && TARGET_UPDATE
      && legitimate_indirect_address_p (XEXP (x, 0), reg_ok_strict))
    return 1;
  if (legitimate_small_data_p (mode, x))
    return 1;
  if (legitimate_constant_pool_address_p (x))
    return 1;
  /* If not REG_OK_STRICT (before reload) let pass any stack offset.  */
  if (! reg_ok_strict
      && GET_CODE (x) == PLUS
      && GET_CODE (XEXP (x, 0)) == REG
      && (XEXP (x, 0) == virtual_stack_vars_rtx
	  || XEXP (x, 0) == arg_pointer_rtx)
      && GET_CODE (XEXP (x, 1)) == CONST_INT)
    return 1;
  if (legitimate_offset_address_p (mode, x, reg_ok_strict))
    return 1;
  if (mode != TImode
      && ((TARGET_HARD_FLOAT && TARGET_FPRS)
	  || TARGET_POWERPC64
	  || (mode != DFmode && mode != TFmode))
      && (TARGET_POWERPC64 || mode != DImode)
      && legitimate_indexed_address_p (x, reg_ok_strict))
    return 1;
  if (legitimate_lo_sum_address_p (mode, x, reg_ok_strict))
    return 1;
  return 0;
}

/* Go to LABEL if ADDR (a legitimate address expression)
   has an effect that depends on the machine mode it is used for.

   On the RS/6000 this is true of all integral offsets (since AltiVec
   modes don't allow them) or is a pre-increment or decrement.

   ??? Except that due to conceptual problems in offsettable_address_p
   we can't really report the problems of integral offsets.  So leave
   this assuming that the adjustable offset must be valid for the 
   sub-words of a TFmode operand, which is what we had before.  */

bool
rs6000_mode_dependent_address (rtx addr)
{
  switch (GET_CODE (addr))
    {
    case PLUS:
      if (GET_CODE (XEXP (addr, 1)) == CONST_INT)
	{
	  unsigned HOST_WIDE_INT val = INTVAL (XEXP (addr, 1));
	  return val + 12 + 0x8000 >= 0x10000;
	}
      break;

    case LO_SUM:
      return true;

    case PRE_INC:
    case PRE_DEC:
      return TARGET_UPDATE;

    default:
      break;
    }

  return false;
}

/* Try to output insns to set TARGET equal to the constant C if it can
   be done in less than N insns.  Do all computations in MODE.
   Returns the place where the output has been placed if it can be
   done and the insns have been emitted.  If it would take more than N
   insns, zero is returned and no insns and emitted.  */

rtx
rs6000_emit_set_const (rtx dest, enum machine_mode mode, 
		       rtx source, int n ATTRIBUTE_UNUSED)
{
  rtx result, insn, set;
  HOST_WIDE_INT c0, c1;

  if (mode == QImode || mode == HImode)
    {
      if (dest == NULL)
        dest = gen_reg_rtx (mode);
      emit_insn (gen_rtx_SET (VOIDmode, dest, source));
      return dest;
    }
  else if (mode == SImode)
    {
      result = no_new_pseudos ? dest : gen_reg_rtx (SImode);

      emit_insn (gen_rtx_SET (VOIDmode, result,
			      GEN_INT (INTVAL (source)
				       & (~ (HOST_WIDE_INT) 0xffff))));
      emit_insn (gen_rtx_SET (VOIDmode, dest,
			      gen_rtx_IOR (SImode, result,
					   GEN_INT (INTVAL (source) & 0xffff))));
      result = dest;
    }
  else if (mode == DImode)
    {
      if (GET_CODE (source) == CONST_INT)
	{
	  c0 = INTVAL (source);
	  c1 = -(c0 < 0);
	}
      else if (GET_CODE (source) == CONST_DOUBLE)
	{
#if HOST_BITS_PER_WIDE_INT >= 64
	  c0 = CONST_DOUBLE_LOW (source);
	  c1 = -(c0 < 0);
#else
	  c0 = CONST_DOUBLE_LOW (source);
	  c1 = CONST_DOUBLE_HIGH (source);
#endif
	}
      else
	abort ();

      result = rs6000_emit_set_long_const (dest, c0, c1);
    }
  else
    abort ();

  insn = get_last_insn ();
  set = single_set (insn);
  if (! CONSTANT_P (SET_SRC (set)))
    set_unique_reg_note (insn, REG_EQUAL, source);

  return result;
}

/* Having failed to find a 3 insn sequence in rs6000_emit_set_const,
   fall back to a straight forward decomposition.  We do this to avoid
   exponential run times encountered when looking for longer sequences
   with rs6000_emit_set_const.  */
static rtx
rs6000_emit_set_long_const (rtx dest, HOST_WIDE_INT c1, HOST_WIDE_INT c2)
{
  if (!TARGET_POWERPC64)
    {
      rtx operand1, operand2;

      operand1 = operand_subword_force (dest, WORDS_BIG_ENDIAN == 0,
					DImode);
      operand2 = operand_subword_force (dest, WORDS_BIG_ENDIAN != 0,
					DImode);
      emit_move_insn (operand1, GEN_INT (c1));
      emit_move_insn (operand2, GEN_INT (c2));
    }
  else
    {
      HOST_WIDE_INT ud1, ud2, ud3, ud4;

      ud1 = c1 & 0xffff;
      ud2 = (c1 & 0xffff0000) >> 16;
#if HOST_BITS_PER_WIDE_INT >= 64
      c2 = c1 >> 32;
#endif
      ud3 = c2 & 0xffff;
      ud4 = (c2 & 0xffff0000) >> 16;

      if ((ud4 == 0xffff && ud3 == 0xffff && ud2 == 0xffff && (ud1 & 0x8000)) 
	  || (ud4 == 0 && ud3 == 0 && ud2 == 0 && ! (ud1 & 0x8000)))
	{
	  if (ud1 & 0x8000)
	    emit_move_insn (dest, GEN_INT (((ud1 ^ 0x8000) -  0x8000)));
	  else
	    emit_move_insn (dest, GEN_INT (ud1));
	}

      else if ((ud4 == 0xffff && ud3 == 0xffff && (ud2 & 0x8000)) 
	       || (ud4 == 0 && ud3 == 0 && ! (ud2 & 0x8000)))
	{
	  if (ud2 & 0x8000)
	    emit_move_insn (dest, GEN_INT (((ud2 << 16) ^ 0x80000000) 
					   - 0x80000000));
	  else
	    emit_move_insn (dest, GEN_INT (ud2 << 16));
	  if (ud1 != 0)
	    emit_move_insn (dest, gen_rtx_IOR (DImode, dest, GEN_INT (ud1)));
	}
      else if ((ud4 == 0xffff && (ud3 & 0x8000)) 
	       || (ud4 == 0 && ! (ud3 & 0x8000)))
	{
	  if (ud3 & 0x8000)
	    emit_move_insn (dest, GEN_INT (((ud3 << 16) ^ 0x80000000) 
					   - 0x80000000));
	  else
	    emit_move_insn (dest, GEN_INT (ud3 << 16));

	  if (ud2 != 0)
	    emit_move_insn (dest, gen_rtx_IOR (DImode, dest, GEN_INT (ud2)));
	  emit_move_insn (dest, gen_rtx_ASHIFT (DImode, dest, GEN_INT (16)));
	  if (ud1 != 0)
	    emit_move_insn (dest, gen_rtx_IOR (DImode, dest, GEN_INT (ud1)));
	}
      else 
	{
	  if (ud4 & 0x8000)
	    emit_move_insn (dest, GEN_INT (((ud4 << 16) ^ 0x80000000) 
					   - 0x80000000));
	  else
	    emit_move_insn (dest, GEN_INT (ud4 << 16));

	  if (ud3 != 0)
	    emit_move_insn (dest, gen_rtx_IOR (DImode, dest, GEN_INT (ud3)));

	  emit_move_insn (dest, gen_rtx_ASHIFT (DImode, dest, GEN_INT (32)));
	  if (ud2 != 0)
	    emit_move_insn (dest, gen_rtx_IOR (DImode, dest, 
					       GEN_INT (ud2 << 16)));	
	  if (ud1 != 0)
	    emit_move_insn (dest, gen_rtx_IOR (DImode, dest, GEN_INT (ud1)));
	}
    }
  return dest;
}

/* Emit a move from SOURCE to DEST in mode MODE.  */
void
rs6000_emit_move (rtx dest, rtx source, enum machine_mode mode)
{
  rtx operands[2];
  operands[0] = dest;
  operands[1] = source;
  
  /* Sanity checks.  Check that we get CONST_DOUBLE only when we should.  */
  if (GET_CODE (operands[1]) == CONST_DOUBLE
      && ! FLOAT_MODE_P (mode)
      && GET_MODE_BITSIZE (mode) <= HOST_BITS_PER_WIDE_INT)
    {
      /* FIXME.  This should never happen.  */
      /* Since it seems that it does, do the safe thing and convert
	 to a CONST_INT.  */
      operands[1] = gen_int_mode (CONST_DOUBLE_LOW (operands[1]), mode);
    }
  if (GET_CODE (operands[1]) == CONST_DOUBLE
      && ! FLOAT_MODE_P (mode)
      && ((CONST_DOUBLE_HIGH (operands[1]) == 0
	   && CONST_DOUBLE_LOW (operands[1]) >= 0)
	  || (CONST_DOUBLE_HIGH (operands[1]) == -1
	      && CONST_DOUBLE_LOW (operands[1]) < 0)))
    abort ();

  /* Check if GCC is setting up a block move that will end up using FP
     registers as temporaries.  We must make sure this is acceptable.  */
  if (GET_CODE (operands[0]) == MEM
      && GET_CODE (operands[1]) == MEM
      && mode == DImode
      && (SLOW_UNALIGNED_ACCESS (DImode, MEM_ALIGN (operands[0]))
	  || SLOW_UNALIGNED_ACCESS (DImode, MEM_ALIGN (operands[1])))
      && ! (SLOW_UNALIGNED_ACCESS (SImode, (MEM_ALIGN (operands[0]) > 32
					    ? 32 : MEM_ALIGN (operands[0])))
	    || SLOW_UNALIGNED_ACCESS (SImode, (MEM_ALIGN (operands[1]) > 32
					       ? 32 
					       : MEM_ALIGN (operands[1]))))
      && ! MEM_VOLATILE_P (operands [0])
      && ! MEM_VOLATILE_P (operands [1]))
    {
      emit_move_insn (adjust_address (operands[0], SImode, 0),
		      adjust_address (operands[1], SImode, 0));
      emit_move_insn (adjust_address (operands[0], SImode, 4),
		      adjust_address (operands[1], SImode, 4));
      return;
    }
  
  if (!no_new_pseudos)
    {
      if (GET_CODE (operands[1]) == MEM && optimize > 0
	  && (mode == QImode || mode == HImode || mode == SImode)
	  && GET_MODE_SIZE (mode) < GET_MODE_SIZE (word_mode))
	{
	  rtx reg = gen_reg_rtx (word_mode);

	  emit_insn (gen_rtx_SET (word_mode, reg,
				  gen_rtx_ZERO_EXTEND (word_mode,
						       operands[1])));
	  operands[1] = gen_lowpart (mode, reg);
	}
      if (GET_CODE (operands[0]) != REG)
	operands[1] = force_reg (mode, operands[1]);
    }

  if (mode == SFmode && ! TARGET_POWERPC
      && TARGET_HARD_FLOAT && TARGET_FPRS
      && GET_CODE (operands[0]) == MEM)
    {
      int regnum;

      if (reload_in_progress || reload_completed)
	regnum = true_regnum (operands[1]);
      else if (GET_CODE (operands[1]) == REG)
	regnum = REGNO (operands[1]);
      else
	regnum = -1;
      
      /* If operands[1] is a register, on POWER it may have
	 double-precision data in it, so truncate it to single
	 precision.  */
      if (FP_REGNO_P (regnum) || regnum >= FIRST_PSEUDO_REGISTER)
	{
	  rtx newreg;
	  newreg = (no_new_pseudos ? operands[1] : gen_reg_rtx (mode));
	  emit_insn (gen_aux_truncdfsf2 (newreg, operands[1]));
	  operands[1] = newreg;
	}
    }

  /* Recognize the case where operand[1] is a reference to thread-local
     data and load its address to a register.  */
  if (GET_CODE (operands[1]) == SYMBOL_REF)
    {
      enum tls_model model = SYMBOL_REF_TLS_MODEL (operands[1]);
      if (model != 0)
	operands[1] = rs6000_legitimize_tls_address (operands[1], model);
    }

  /* Handle the case where reload calls us with an invalid address.  */
  if (reload_in_progress && mode == Pmode
      && (! general_operand (operands[1], mode)
	  || ! nonimmediate_operand (operands[0], mode)))
    goto emit_set;

  /* Handle the case of CONSTANT_P_RTX.  */
  if (GET_CODE (operands[1]) == CONSTANT_P_RTX)
    goto emit_set;

  /* 128-bit constant floating-point values on Darwin should really be
     loaded as two parts.  */
  if ((DEFAULT_ABI == ABI_AIX || DEFAULT_ABI == ABI_DARWIN)
      && TARGET_HARD_FLOAT && TARGET_FPRS && TARGET_LONG_DOUBLE_128
      && mode == TFmode && GET_CODE (operands[1]) == CONST_DOUBLE)
    {
      /* DImode is used, not DFmode, because simplify_gen_subreg doesn't
	 know how to get a DFmode SUBREG of a TFmode.  */
      rs6000_emit_move (simplify_gen_subreg (DImode, operands[0], mode, 0),
			simplify_gen_subreg (DImode, operands[1], mode, 0),
			DImode);
      rs6000_emit_move (simplify_gen_subreg (DImode, operands[0], mode,
					     GET_MODE_SIZE (DImode)),
			simplify_gen_subreg (DImode, operands[1], mode,
					     GET_MODE_SIZE (DImode)),
			DImode);
      return;
    }

  /* FIXME:  In the long term, this switch statement should go away
     and be replaced by a sequence of tests based on things like
     mode == Pmode.  */
  switch (mode)
    {
    case HImode:
    case QImode:
      if (CONSTANT_P (operands[1])
	  && GET_CODE (operands[1]) != CONST_INT)
	operands[1] = force_const_mem (mode, operands[1]);
      break;

    case TFmode:
    case DFmode:
    case SFmode:
      if (CONSTANT_P (operands[1]) 
	  && ! easy_fp_constant (operands[1], mode))
	operands[1] = force_const_mem (mode, operands[1]);
      break;
      
    case V16QImode:
    case V8HImode:
    case V4SFmode:
    case V4SImode:
    case V4HImode:
    case V2SFmode:
    case V2SImode:
    case V1DImode:
      if (CONSTANT_P (operands[1])
	  && !easy_vector_constant (operands[1], mode))
	operands[1] = force_const_mem (mode, operands[1]);
      break;
      
    case SImode:
    case DImode:
      /* Use default pattern for address of ELF small data */
      if (TARGET_ELF
	  && mode == Pmode
	  && DEFAULT_ABI == ABI_V4
	  && (GET_CODE (operands[1]) == SYMBOL_REF 
	      || GET_CODE (operands[1]) == CONST)
	  && small_data_operand (operands[1], mode))
	{
	  emit_insn (gen_rtx_SET (VOIDmode, operands[0], operands[1]));
	  return;
	}

      if (DEFAULT_ABI == ABI_V4
	  && mode == Pmode && mode == SImode
	  && flag_pic == 1 && got_operand (operands[1], mode))
	{
	  emit_insn (gen_movsi_got (operands[0], operands[1]));
	  return;
	}

      if ((TARGET_ELF || DEFAULT_ABI == ABI_DARWIN)
	  && TARGET_NO_TOC
	  && ! flag_pic
	  && mode == Pmode
	  && CONSTANT_P (operands[1])
	  && GET_CODE (operands[1]) != HIGH
	  && GET_CODE (operands[1]) != CONST_INT)
	{
	  rtx target = (no_new_pseudos ? operands[0] : gen_reg_rtx (mode));

	  /* If this is a function address on -mcall-aixdesc,
	     convert it to the address of the descriptor.  */
	  if (DEFAULT_ABI == ABI_AIX
	      && GET_CODE (operands[1]) == SYMBOL_REF
	      && XSTR (operands[1], 0)[0] == '.')
	    {
	      const char *name = XSTR (operands[1], 0);
	      rtx new_ref;
	      while (*name == '.')
		name++;
	      new_ref = gen_rtx_SYMBOL_REF (Pmode, name);
	      CONSTANT_POOL_ADDRESS_P (new_ref)
		= CONSTANT_POOL_ADDRESS_P (operands[1]);
	      SYMBOL_REF_FLAGS (new_ref) = SYMBOL_REF_FLAGS (operands[1]);
	      SYMBOL_REF_USED (new_ref) = SYMBOL_REF_USED (operands[1]);
	      SYMBOL_REF_DECL (new_ref) = SYMBOL_REF_DECL (operands[1]);
	      operands[1] = new_ref;
	    }

	  if (DEFAULT_ABI == ABI_DARWIN)
	    {
#if TARGET_MACHO
	      if (MACHO_DYNAMIC_NO_PIC_P)
		{
		  /* Take care of any required data indirection.  */
		  operands[1] = rs6000_machopic_legitimize_pic_address (
				  operands[1], mode, operands[0]);
		  if (operands[0] != operands[1])
		    emit_insn (gen_rtx_SET (VOIDmode,
				            operands[0], operands[1]));
		  return;
		}
#endif
	      emit_insn (gen_macho_high (target, operands[1]));
	      emit_insn (gen_macho_low (operands[0], target, operands[1]));
	      return;
	    }

	  emit_insn (gen_elf_high (target, operands[1]));
	  emit_insn (gen_elf_low (operands[0], target, operands[1]));
	  return;
	}

      /* If this is a SYMBOL_REF that refers to a constant pool entry,
	 and we have put it in the TOC, we just need to make a TOC-relative
	 reference to it.  */
      if (TARGET_TOC
	  && GET_CODE (operands[1]) == SYMBOL_REF
	  && constant_pool_expr_p (operands[1])
	  && ASM_OUTPUT_SPECIAL_POOL_ENTRY_P (get_pool_constant (operands[1]),
					      get_pool_mode (operands[1])))
	{
	  operands[1] = create_TOC_reference (operands[1]);
	}
      else if (mode == Pmode
	       && CONSTANT_P (operands[1])
	       && ((GET_CODE (operands[1]) != CONST_INT
		    && ! easy_fp_constant (operands[1], mode))
		   || (GET_CODE (operands[1]) == CONST_INT
		       && num_insns_constant (operands[1], mode) > 2)
		   || (GET_CODE (operands[0]) == REG
		       && FP_REGNO_P (REGNO (operands[0]))))
	       && GET_CODE (operands[1]) != HIGH
	       && ! legitimate_constant_pool_address_p (operands[1])
	       && ! toc_relative_expr_p (operands[1]))
	{
	  /* Emit a USE operation so that the constant isn't deleted if
	     expensive optimizations are turned on because nobody
	     references it.  This should only be done for operands that
	     contain SYMBOL_REFs with CONSTANT_POOL_ADDRESS_P set.
	     This should not be done for operands that contain LABEL_REFs.
	     For now, we just handle the obvious case.  */
	  if (GET_CODE (operands[1]) != LABEL_REF)
	    emit_insn (gen_rtx_USE (VOIDmode, operands[1]));

#if TARGET_MACHO
	  /* Darwin uses a special PIC legitimizer.  */
	  if (DEFAULT_ABI == ABI_DARWIN && MACHOPIC_INDIRECT)
	    {
	      operands[1] =
		rs6000_machopic_legitimize_pic_address (operands[1], mode,
							operands[0]);
	      if (operands[0] != operands[1])
		emit_insn (gen_rtx_SET (VOIDmode, operands[0], operands[1]));
	      return;
	    }
#endif

	  /* If we are to limit the number of things we put in the TOC and
	     this is a symbol plus a constant we can add in one insn,
	     just put the symbol in the TOC and add the constant.  Don't do
	     this if reload is in progress.  */
	  if (GET_CODE (operands[1]) == CONST
	      && TARGET_NO_SUM_IN_TOC && ! reload_in_progress
	      && GET_CODE (XEXP (operands[1], 0)) == PLUS
	      && add_operand (XEXP (XEXP (operands[1], 0), 1), mode)
	      && (GET_CODE (XEXP (XEXP (operands[1], 0), 0)) == LABEL_REF
		  || GET_CODE (XEXP (XEXP (operands[1], 0), 0)) == SYMBOL_REF)
	      && ! side_effects_p (operands[0]))
	    {
	      rtx sym =
		force_const_mem (mode, XEXP (XEXP (operands[1], 0), 0));
	      rtx other = XEXP (XEXP (operands[1], 0), 1);

	      sym = force_reg (mode, sym);
	      if (mode == SImode)
		emit_insn (gen_addsi3 (operands[0], sym, other));
	      else
		emit_insn (gen_adddi3 (operands[0], sym, other));
	      return;
	    }

	  operands[1] = force_const_mem (mode, operands[1]);

	  if (TARGET_TOC 
	      && constant_pool_expr_p (XEXP (operands[1], 0))
	      && ASM_OUTPUT_SPECIAL_POOL_ENTRY_P (
			get_pool_constant (XEXP (operands[1], 0)),
			get_pool_mode (XEXP (operands[1], 0))))
	    {
	      operands[1]
		= gen_rtx_MEM (mode,
			       create_TOC_reference (XEXP (operands[1], 0)));
	      set_mem_alias_set (operands[1], get_TOC_alias_set ());
	      RTX_UNCHANGING_P (operands[1]) = 1;
	    }
	}
      break;

    case TImode:
      if (GET_CODE (operands[0]) == MEM
	  && GET_CODE (XEXP (operands[0], 0)) != REG
	  && ! reload_in_progress)
	operands[0]
	  = replace_equiv_address (operands[0],
				   copy_addr_to_reg (XEXP (operands[0], 0)));

      if (GET_CODE (operands[1]) == MEM
	  && GET_CODE (XEXP (operands[1], 0)) != REG
	  && ! reload_in_progress)
	operands[1]
	  = replace_equiv_address (operands[1],
				   copy_addr_to_reg (XEXP (operands[1], 0)));
      if (TARGET_POWER)
	{
	  emit_insn (gen_rtx_PARALLEL (VOIDmode,
		       gen_rtvec (2,
				  gen_rtx_SET (VOIDmode,
					       operands[0], operands[1]),
				  gen_rtx_CLOBBER (VOIDmode,
						   gen_rtx_SCRATCH (SImode)))));
	  return;
	}
      break;

    default:
      abort ();
    }

  /* Above, we may have called force_const_mem which may have returned
     an invalid address.  If we can, fix this up; otherwise, reload will
     have to deal with it.  */
  if (GET_CODE (operands[1]) == MEM && ! reload_in_progress)
    operands[1] = validize_mem (operands[1]);

 emit_set:
  emit_insn (gen_rtx_SET (VOIDmode, operands[0], operands[1]));
}

/* Nonzero if we can use a floating-point register to pass this arg.  */
#define USE_FP_FOR_ARG_P(CUM,MODE,TYPE)		\
  (GET_MODE_CLASS (MODE) == MODE_FLOAT		\
   && (CUM)->fregno <= FP_ARG_MAX_REG		\
   && TARGET_HARD_FLOAT && TARGET_FPRS)

/* Nonzero if we can use an AltiVec register to pass this arg.  */
#define USE_ALTIVEC_FOR_ARG_P(CUM,MODE,TYPE,NAMED)	\
  (ALTIVEC_VECTOR_MODE (MODE)				\
   && (CUM)->vregno <= ALTIVEC_ARG_MAX_REG		\
   && TARGET_ALTIVEC_ABI				\
   && (NAMED))

/* Return a nonzero value to say to return the function value in
   memory, just as large structures are always returned.  TYPE will be
   the data type of the value, and FNTYPE will be the type of the
   function doing the returning, or @code{NULL} for libcalls.

   The AIX ABI for the RS/6000 specifies that all structures are
   returned in memory.  The Darwin ABI does the same.  The SVR4 ABI
   specifies that structures <= 8 bytes are returned in r3/r4, but a
   draft put them in memory, and GCC used to implement the draft
   instead of the final standard.  Therefore, TARGET_AIX_STRUCT_RET
   controls this instead of DEFAULT_ABI; V.4 targets needing backward
   compatibility can change DRAFT_V4_STRUCT_RET to override the
   default, and -m switches get the final word.  See
   rs6000_override_options for more details.

   The PPC32 SVR4 ABI uses IEEE double extended for long double, if 128-bit
   long double support is enabled.  These values are returned in memory.

   int_size_in_bytes returns -1 for variable size objects, which go in
   memory always.  The cast to unsigned makes -1 > 8.  */

static bool
rs6000_return_in_memory (tree type, tree fntype ATTRIBUTE_UNUSED)
{
  if (AGGREGATE_TYPE_P (type)
      && (TARGET_AIX_STRUCT_RET
	  || (unsigned HOST_WIDE_INT) int_size_in_bytes (type) > 8))
    return true;
  if (DEFAULT_ABI == ABI_V4 && TYPE_MODE (type) == TFmode)
    return true;
  return false;
}

/* Initialize a variable CUM of type CUMULATIVE_ARGS
   for a call to a function whose data type is FNTYPE.
   For a library call, FNTYPE is 0.

   For incoming args we set the number of arguments in the prototype large
   so we never return a PARALLEL.  */

void
init_cumulative_args (CUMULATIVE_ARGS *cum, tree fntype, 
		      rtx libname ATTRIBUTE_UNUSED, int incoming,
		      int libcall, int n_named_args)
{
  static CUMULATIVE_ARGS zero_cumulative;

  *cum = zero_cumulative;
  cum->words = 0;
  cum->fregno = FP_ARG_MIN_REG;
  cum->vregno = ALTIVEC_ARG_MIN_REG;
  cum->prototype = (fntype && TYPE_ARG_TYPES (fntype));
  cum->call_cookie = ((DEFAULT_ABI == ABI_V4 && libcall)
		      ? CALL_LIBCALL : CALL_NORMAL);
  cum->sysv_gregno = GP_ARG_MIN_REG;
  cum->stdarg = fntype
    && (TYPE_ARG_TYPES (fntype) != 0
	&& (TREE_VALUE (tree_last  (TYPE_ARG_TYPES (fntype)))
	    != void_type_node));

  cum->nargs_prototype = 0;
  if (incoming || cum->prototype)
    cum->nargs_prototype = n_named_args;

  /* Check for a longcall attribute.  */
  if ((!fntype && rs6000_default_long_calls)
      || (fntype
	  && lookup_attribute ("longcall", TYPE_ATTRIBUTES (fntype))
	  && !lookup_attribute ("shortcall", TYPE_ATTRIBUTES (fntype))))
    cum->call_cookie |= CALL_LONG;

  if (TARGET_DEBUG_ARG)
    {
      fprintf (stderr, "\ninit_cumulative_args:");
      if (fntype)
	{
	  tree ret_type = TREE_TYPE (fntype);
	  fprintf (stderr, " ret code = %s,",
		   tree_code_name[ (int)TREE_CODE (ret_type) ]);
	}

      if (cum->call_cookie & CALL_LONG)
	fprintf (stderr, " longcall,");

      fprintf (stderr, " proto = %d, nargs = %d\n",
	       cum->prototype, cum->nargs_prototype);
    }
  
    if (fntype 
	&& !TARGET_ALTIVEC 
	&& TARGET_ALTIVEC_ABI
        && ALTIVEC_VECTOR_MODE (TYPE_MODE (TREE_TYPE (fntype))))
      {
	error ("Cannot return value in vector register because"
	       " altivec instructions are disabled, use -maltivec"
	       " to enable them.");
      }
}

/* If defined, a C expression which determines whether, and in which
   direction, to pad out an argument with extra space.  The value
   should be of type `enum direction': either `upward' to pad above
   the argument, `downward' to pad below, or `none' to inhibit
   padding.

   For the AIX ABI structs are always stored left shifted in their
   argument slot.  */

enum direction
function_arg_padding (enum machine_mode mode, tree type)
{
#ifndef AGGREGATE_PADDING_FIXED
#define AGGREGATE_PADDING_FIXED 0
#endif
#ifndef AGGREGATES_PAD_UPWARD_ALWAYS
#define AGGREGATES_PAD_UPWARD_ALWAYS 0
#endif

  if (!AGGREGATE_PADDING_FIXED)
    {
      /* GCC used to pass structures of the same size as integer types as
	 if they were in fact integers, ignoring FUNCTION_ARG_PADDING.
	 ie. Structures of size 1 or 2 (or 4 when TARGET_64BIT) were
	 passed padded downward, except that -mstrict-align further
	 muddied the water in that multi-component structures of 2 and 4
	 bytes in size were passed padded upward.

	 The following arranges for best compatibility with previous
	 versions of gcc, but removes the -mstrict-align dependency.  */
      if (BYTES_BIG_ENDIAN)
	{
	  HOST_WIDE_INT size = 0;

	  if (mode == BLKmode)
	    {
	      if (type && TREE_CODE (TYPE_SIZE (type)) == INTEGER_CST)
		size = int_size_in_bytes (type);
	    }
	  else
	    size = GET_MODE_SIZE (mode);

	  if (size == 1 || size == 2 || size == 4)
	    return downward;
	}
      return upward;
    }

  if (AGGREGATES_PAD_UPWARD_ALWAYS)
    {
      if (type != 0 && AGGREGATE_TYPE_P (type))
	return upward;
    }

  /* Fall back to the default.  */
  return DEFAULT_FUNCTION_ARG_PADDING (mode, type);
}

/* If defined, a C expression that gives the alignment boundary, in bits,
   of an argument with the specified mode and type.  If it is not defined, 
   PARM_BOUNDARY is used for all arguments.
   
   V.4 wants long longs to be double word aligned.  */

int
function_arg_boundary (enum machine_mode mode, tree type ATTRIBUTE_UNUSED)
{
  if (DEFAULT_ABI == ABI_V4 && GET_MODE_SIZE (mode) == 8)
    return 64;
  else if (SPE_VECTOR_MODE (mode))
    return 64;
  else if (ALTIVEC_VECTOR_MODE (mode))
    return 128;
  else
    return PARM_BOUNDARY;
}

/* Compute the size (in words) of a function argument.  */

static unsigned long
rs6000_arg_size (enum machine_mode mode, tree type)
{
  unsigned long size;

  if (mode != BLKmode)
    size = GET_MODE_SIZE (mode);
  else
    size = int_size_in_bytes (type);

  if (TARGET_32BIT)
    return (size + 3) >> 2;
  else
    return (size + 7) >> 3;
}

/* Update the data in CUM to advance over an argument
   of mode MODE and data type TYPE.
   (TYPE is null for libcalls where that information may not be available.)

   Note that for args passed by reference, function_arg will be called
   with MODE and TYPE set to that of the pointer to the arg, not the arg
   itself.  */

void
function_arg_advance (CUMULATIVE_ARGS *cum, enum machine_mode mode, 
		      tree type, int named)
{
  cum->nargs_prototype--;

  if (TARGET_ALTIVEC_ABI && ALTIVEC_VECTOR_MODE (mode))
    {
      bool stack = false;

      if (USE_ALTIVEC_FOR_ARG_P (cum, mode, type, named))
        {
	  cum->vregno++;
	  if (!TARGET_ALTIVEC)
	    error ("Cannot pass argument in vector register because"
		   " altivec instructions are disabled, use -maltivec"
		   " to enable them.");

	  /* PowerPC64 Linux and AIX allocate GPRs for a vector argument
	     even if it is going to be passed in a vector register.  
	     Darwin does the same for variable-argument functions.  */
	  if ((DEFAULT_ABI == ABI_AIX && TARGET_64BIT)
	      || (cum->stdarg && DEFAULT_ABI != ABI_V4))
	    stack = true;
	}
      else
	stack = true;

      if (stack)
        {
	  int align;
	  
	  /* Vector parameters must be 16-byte aligned.  This places
	     them at 2 mod 4 in terms of words in 32-bit mode, since
	     the parameter save area starts at offset 24 from the
	     stack.  In 64-bit mode, they just have to start on an
	     even word, since the parameter save area is 16-byte
	     aligned.  Space for GPRs is reserved even if the argument
	     will be passed in memory.  */
	  if (TARGET_32BIT)
	    align = (2 - cum->words) & 3;
	  else
	    align = cum->words & 1;
	  cum->words += align + rs6000_arg_size (mode, type);
	  
	  if (TARGET_DEBUG_ARG)
	    {
	      fprintf (stderr, "function_adv: words = %2d, align=%d, ", 
		       cum->words, align);
	      fprintf (stderr, "nargs = %4d, proto = %d, mode = %4s\n",
		       cum->nargs_prototype, cum->prototype, 
		       GET_MODE_NAME (mode));
	    }
	}
    }
  else if (TARGET_SPE_ABI && TARGET_SPE && SPE_VECTOR_MODE (mode)
	   && !cum->stdarg
	   && cum->sysv_gregno <= GP_ARG_MAX_REG)
    cum->sysv_gregno++;
  else if (DEFAULT_ABI == ABI_V4)
    {
      if (TARGET_HARD_FLOAT && TARGET_FPRS
	  && (mode == SFmode || mode == DFmode))
	{
	  if (cum->fregno <= FP_ARG_V4_MAX_REG)
	    cum->fregno++;
	  else
	    {
	      if (mode == DFmode)
	        cum->words += cum->words & 1;
	      cum->words += rs6000_arg_size (mode, type);
	    }
	}
      else
	{
	  int n_words = rs6000_arg_size (mode, type);
	  int gregno = cum->sysv_gregno;

	  /* Long long and SPE vectors are put in (r3,r4), (r5,r6),
	     (r7,r8) or (r9,r10).  As does any other 2 word item such
	     as complex int due to a historical mistake.  */
	  if (n_words == 2)
	    gregno += (1 - gregno) & 1;

	  /* Multi-reg args are not split between registers and stack.  */
	  if (gregno + n_words - 1 > GP_ARG_MAX_REG)
	    {
	      /* Long long and SPE vectors are aligned on the stack.
		 So are other 2 word items such as complex int due to
		 a historical mistake.  */
	      if (n_words == 2)
		cum->words += cum->words & 1;
	      cum->words += n_words;
	    }

	  /* Note: continuing to accumulate gregno past when we've started
	     spilling to the stack indicates the fact that we've started
	     spilling to the stack to expand_builtin_saveregs.  */
	  cum->sysv_gregno = gregno + n_words;
	}

      if (TARGET_DEBUG_ARG)
	{
	  fprintf (stderr, "function_adv: words = %2d, fregno = %2d, ",
		   cum->words, cum->fregno);
	  fprintf (stderr, "gregno = %2d, nargs = %4d, proto = %d, ",
		   cum->sysv_gregno, cum->nargs_prototype, cum->prototype);
	  fprintf (stderr, "mode = %4s, named = %d\n",
		   GET_MODE_NAME (mode), named);
	}
    }
  else
    {
      int n_words = rs6000_arg_size (mode, type);
      int align = function_arg_boundary (mode, type) / PARM_BOUNDARY - 1;

      /* The simple alignment calculation here works because
	 function_arg_boundary / PARM_BOUNDARY will only be 1 or 2.
	 If we ever want to handle alignments larger than 8 bytes for
	 32-bit or 16 bytes for 64-bit, then we'll need to take into
	 account the offset to the start of the parm save area.  */
      align &= cum->words;
      cum->words += align + n_words;

      if (GET_MODE_CLASS (mode) == MODE_FLOAT
	  && TARGET_HARD_FLOAT && TARGET_FPRS)
	cum->fregno += (GET_MODE_SIZE (mode) + 7) >> 3;

      if (TARGET_DEBUG_ARG)
	{
	  fprintf (stderr, "function_adv: words = %2d, fregno = %2d, ",
		   cum->words, cum->fregno);
	  fprintf (stderr, "nargs = %4d, proto = %d, mode = %4s, ",
		   cum->nargs_prototype, cum->prototype, GET_MODE_NAME (mode));
	  fprintf (stderr, "named = %d, align = %d\n", named, align);
	}
    }
}

/* Determine where to put a SIMD argument on the SPE.  */

static rtx
rs6000_spe_function_arg (CUMULATIVE_ARGS *cum, enum machine_mode mode, 
			 tree type)
{
  if (cum->stdarg)
    {
      int gregno = cum->sysv_gregno;
      int n_words = rs6000_arg_size (mode, type);

      /* SPE vectors are put in odd registers.  */
      if (n_words == 2 && (gregno & 1) == 0)
	gregno += 1;

      if (gregno + n_words - 1 <= GP_ARG_MAX_REG)
	{
	  rtx r1, r2;
	  enum machine_mode m = SImode;

	  r1 = gen_rtx_REG (m, gregno);
	  r1 = gen_rtx_EXPR_LIST (m, r1, const0_rtx);
	  r2 = gen_rtx_REG (m, gregno + 1);
	  r2 = gen_rtx_EXPR_LIST (m, r2, GEN_INT (4));
	  return gen_rtx_PARALLEL (mode, gen_rtvec (2, r1, r2));
	}
      else
	return NULL_RTX;
    }
  else
    {
      if (cum->sysv_gregno <= GP_ARG_MAX_REG)
	return gen_rtx_REG (mode, cum->sysv_gregno);
      else
	return NULL_RTX;
    }
}

/* Determine where to place an argument in 64-bit mode with 32-bit ABI.  */

static rtx
rs6000_mixed_function_arg (enum machine_mode mode, tree type, int align_words)
{
  int n_units;
  int i, k;
  rtx rvec[GP_ARG_NUM_REG + 1];

  if (align_words >= GP_ARG_NUM_REG)
    return NULL_RTX;

  n_units = rs6000_arg_size (mode, type);

  /* Optimize the simple case where the arg fits in one gpr, except in
     the case of BLKmode due to assign_parms assuming that registers are
     BITS_PER_WORD wide.  */
  if (n_units == 0
      || (n_units == 1 && mode != BLKmode))
    return gen_rtx_REG (mode, GP_ARG_MIN_REG + align_words);

  k = 0;
  if (align_words + n_units > GP_ARG_NUM_REG)
    /* Not all of the arg fits in gprs.  Say that it goes in memory too,
       using a magic NULL_RTX component.
       FIXME: This is not strictly correct.  Only some of the arg
       belongs in memory, not all of it.  However, there isn't any way
       to do this currently, apart from building rtx descriptions for
       the pieces of memory we want stored.  Due to bugs in the generic
       code we can't use the normal function_arg_partial_nregs scheme
       with the PARALLEL arg description we emit here.
       In any case, the code to store the whole arg to memory is often
       more efficient than code to store pieces, and we know that space
       is available in the right place for the whole arg.  */
    rvec[k++] = gen_rtx_EXPR_LIST (VOIDmode, NULL_RTX, const0_rtx);

  i = 0;
  do
    {
      rtx r = gen_rtx_REG (SImode, GP_ARG_MIN_REG + align_words);
      rtx off = GEN_INT (i++ * 4);
      rvec[k++] = gen_rtx_EXPR_LIST (VOIDmode, r, off);
    }
  while (++align_words < GP_ARG_NUM_REG && --n_units != 0);

  return gen_rtx_PARALLEL (mode, gen_rtvec_v (k, rvec));
}

/* Determine where to put an argument to a function.
   Value is zero to push the argument on the stack,
   or a hard register in which to store the argument.

   MODE is the argument's machine mode.
   TYPE is the data type of the argument (as a tree).
    This is null for libcalls where that information may
    not be available.
   CUM is a variable of type CUMULATIVE_ARGS which gives info about
    the preceding args and about the function being called.
   NAMED is nonzero if this argument is a named parameter
    (otherwise it is an extra parameter matching an ellipsis).

   On RS/6000 the first eight words of non-FP are normally in registers
   and the rest are pushed.  Under AIX, the first 13 FP args are in registers.
   Under V.4, the first 8 FP args are in registers.

   If this is floating-point and no prototype is specified, we use
   both an FP and integer register (or possibly FP reg and stack).  Library
   functions (when CALL_LIBCALL is set) always have the proper types for args,
   so we can pass the FP value just in one register.  emit_library_function
   doesn't support PARALLEL anyway.

   Note that for args passed by reference, function_arg will be called
   with MODE and TYPE set to that of the pointer to the arg, not the arg
   itself.  */

struct rtx_def *
function_arg (CUMULATIVE_ARGS *cum, enum machine_mode mode, 
	      tree type, int named)
{
  enum rs6000_abi abi = DEFAULT_ABI;

  /* Return a marker to indicate whether CR1 needs to set or clear the
     bit that V.4 uses to say fp args were passed in registers.
     Assume that we don't need the marker for software floating point,
     or compiler generated library calls.  */
  if (mode == VOIDmode)
    {
      if (abi == ABI_V4
	  && (cum->call_cookie & CALL_LIBCALL) == 0
	  && (cum->stdarg
	      || (cum->nargs_prototype < 0
		  && (cum->prototype || TARGET_NO_PROTOTYPE))))
	{
	  /* For the SPE, we need to crxor CR6 always.  */
	  if (TARGET_SPE_ABI)
	    return GEN_INT (cum->call_cookie | CALL_V4_SET_FP_ARGS);
	  else if (TARGET_HARD_FLOAT && TARGET_FPRS)
	    return GEN_INT (cum->call_cookie
			    | ((cum->fregno == FP_ARG_MIN_REG)
			       ? CALL_V4_SET_FP_ARGS
			       : CALL_V4_CLEAR_FP_ARGS));
	}

      return GEN_INT (cum->call_cookie);
    }

  if (USE_ALTIVEC_FOR_ARG_P (cum, mode, type, named))
    if (TARGET_64BIT && ! cum->prototype)
      {
       /* Vector parameters get passed in vector register
          and also in GPRs or memory, in absence of prototype.  */
       int align_words;
       rtx slot;
       align_words = (cum->words + 1) & ~1;

       if (align_words >= GP_ARG_NUM_REG)
         {
           slot = NULL_RTX;
         }
       else
         {
           slot = gen_rtx_REG (mode, GP_ARG_MIN_REG + align_words);
         }
       return gen_rtx_PARALLEL (mode,
                gen_rtvec (2,
                           gen_rtx_EXPR_LIST (VOIDmode,
                                              slot, const0_rtx),
                           gen_rtx_EXPR_LIST (VOIDmode,
                                              gen_rtx_REG (mode, cum->vregno),
                                              const0_rtx)));
      }
    else
      return gen_rtx_REG (mode, cum->vregno);
  else if (TARGET_ALTIVEC_ABI && ALTIVEC_VECTOR_MODE (mode))
    {
      if (named || abi == ABI_V4)
	return NULL_RTX;
      else
	{
	  /* Vector parameters to varargs functions under AIX or Darwin
	     get passed in memory and possibly also in GPRs.  */
	  int align, align_words, n_words;
	  enum machine_mode part_mode;

	  /* Vector parameters must be 16-byte aligned.  This places them at
	     2 mod 4 in terms of words in 32-bit mode, since the parameter
	     save area starts at offset 24 from the stack.  In 64-bit mode,
	     they just have to start on an even word, since the parameter
	     save area is 16-byte aligned.  */
	  if (TARGET_32BIT)
	    align = (2 - cum->words) & 3;
	  else
	    align = cum->words & 1;
	  align_words = cum->words + align;

	  /* Out of registers?  Memory, then.  */
	  if (align_words >= GP_ARG_NUM_REG)
	    return NULL_RTX;

	  if (TARGET_32BIT && TARGET_POWERPC64)
	    return rs6000_mixed_function_arg (mode, type, align_words);

	  /* The vector value goes in GPRs.  Only the part of the
	     value in GPRs is reported here.  */
	  part_mode = mode;
	  n_words = rs6000_arg_size (mode, type);
	  if (align_words + n_words > GP_ARG_NUM_REG)
	    /* Fortunately, there are only two possibilities, the value
	       is either wholly in GPRs or half in GPRs and half not.  */
	    part_mode = DImode;

	  return gen_rtx_REG (part_mode, GP_ARG_MIN_REG + align_words);
	}
    }
  else if (TARGET_SPE_ABI && TARGET_SPE && SPE_VECTOR_MODE (mode))
    return rs6000_spe_function_arg (cum, mode, type);
  else if (abi == ABI_V4)
    {
      if (TARGET_HARD_FLOAT && TARGET_FPRS
	  && (mode == SFmode || mode == DFmode))
	{
	  if (cum->fregno <= FP_ARG_V4_MAX_REG)
	    return gen_rtx_REG (mode, cum->fregno);
	  else
	    return NULL_RTX;
	}
      else
	{
	  int n_words = rs6000_arg_size (mode, type);
	  int gregno = cum->sysv_gregno;

	  /* Long long and SPE vectors are put in (r3,r4), (r5,r6),
	     (r7,r8) or (r9,r10).  As does any other 2 word item such
	     as complex int due to a historical mistake.  */
	  if (n_words == 2)
	    gregno += (1 - gregno) & 1;

	  /* Multi-reg args are not split between registers and stack.  */
	  if (gregno + n_words - 1 > GP_ARG_MAX_REG)
	    return NULL_RTX;

	  if (TARGET_32BIT && TARGET_POWERPC64)
	    return rs6000_mixed_function_arg (mode, type,
					      gregno - GP_ARG_MIN_REG);
	  return gen_rtx_REG (mode, gregno);
	}
    }
  else
    {
      int align = function_arg_boundary (mode, type) / PARM_BOUNDARY - 1;
      int align_words = cum->words + (cum->words & align);

      if (USE_FP_FOR_ARG_P (cum, mode, type))
	{
	  rtx rvec[GP_ARG_NUM_REG + 1];
	  rtx r;
	  int k;
	  bool needs_psave;
	  enum machine_mode fmode = mode;
	  unsigned long n_fpreg = (GET_MODE_SIZE (mode) + 7) >> 3;

	  if (cum->fregno + n_fpreg > FP_ARG_MAX_REG + 1)
	    {
	      /* Currently, we only ever need one reg here because complex
		 doubles are split.  */
	      if (cum->fregno != FP_ARG_MAX_REG || fmode != TFmode)
		abort ();

	      /* Long double split over regs and memory.  */
	      fmode = DFmode;
	    }

	  /* Do we also need to pass this arg in the parameter save
	     area?  */
	  needs_psave = (type
			 && (cum->nargs_prototype <= 0
			     || (DEFAULT_ABI == ABI_AIX
				 && TARGET_XL_COMPAT
				 && align_words >= GP_ARG_NUM_REG)));

	  if (!needs_psave && mode == fmode)
	    return gen_rtx_REG (fmode, cum->fregno);

	  k = 0;
	  if (needs_psave)
	    {
	      /* Describe the part that goes in gprs or the stack.
		 This piece must come first, before the fprs.  */
	      if (align_words < GP_ARG_NUM_REG)
		{
		  unsigned long n_words = rs6000_arg_size (mode, type);

		  if (align_words + n_words > GP_ARG_NUM_REG
		      || (TARGET_32BIT && TARGET_POWERPC64))
		    {
		      /* If this is partially on the stack, then we only
			 include the portion actually in registers here.  */
		      enum machine_mode rmode = TARGET_32BIT ? SImode : DImode;
		      rtx off;
		      do
			{
			  r = gen_rtx_REG (rmode,
					   GP_ARG_MIN_REG + align_words);
			  off = GEN_INT (k * GET_MODE_SIZE (rmode));
			  rvec[k++] = gen_rtx_EXPR_LIST (VOIDmode, r, off);
			}
		      while (++align_words < GP_ARG_NUM_REG && --n_words != 0);
		    }
		  else
		    {
		      /* The whole arg fits in gprs.  */
		      r = gen_rtx_REG (mode, GP_ARG_MIN_REG + align_words);
		      rvec[k++] = gen_rtx_EXPR_LIST (VOIDmode, r, const0_rtx);
		    }
		}
	      else
		/* It's entirely in memory.  */
		rvec[k++] = gen_rtx_EXPR_LIST (VOIDmode, NULL_RTX, const0_rtx);
	    }

	  /* Describe where this piece goes in the fprs.  */
	  r = gen_rtx_REG (fmode, cum->fregno);
	  rvec[k++] = gen_rtx_EXPR_LIST (VOIDmode, r, const0_rtx);

	  return gen_rtx_PARALLEL (mode, gen_rtvec_v (k, rvec));
	}
      else if (align_words < GP_ARG_NUM_REG)
	{
	  if (TARGET_32BIT && TARGET_POWERPC64)
	    return rs6000_mixed_function_arg (mode, type, align_words);

	  return gen_rtx_REG (mode, GP_ARG_MIN_REG + align_words);
	}
      else
	return NULL_RTX;
    }
}

/* For an arg passed partly in registers and partly in memory, this is
   the number of registers used.  For args passed entirely in registers
   or entirely in memory, zero.  When an arg is described by a PARALLEL,
   perhaps using more than one register type, this function returns the
   number of registers used by the first element of the PARALLEL.  */

int
function_arg_partial_nregs (CUMULATIVE_ARGS *cum, enum machine_mode mode, 
			    tree type, int named)
{
  int ret = 0;
  int align;
  int parm_offset;
  int align_words;

  if (DEFAULT_ABI == ABI_V4)
    return 0;

  if (USE_ALTIVEC_FOR_ARG_P (cum, mode, type, named)
      && cum->nargs_prototype >= 0)
    return 0;

  align = function_arg_boundary (mode, type) / PARM_BOUNDARY - 1;
  parm_offset = TARGET_32BIT ? 2 : 0;
  align_words = cum->words + ((parm_offset - cum->words) & align);

  if (USE_FP_FOR_ARG_P (cum, mode, type)
      /* If we are passing this arg in gprs as well, then this function
	 should return the number of gprs (or memory) partially passed,
	 *not* the number of fprs.  */
      && !(type
	   && (cum->nargs_prototype <= 0
	       || (DEFAULT_ABI == ABI_AIX
		   && TARGET_XL_COMPAT
		   && align_words >= GP_ARG_NUM_REG))))
    {
      if (cum->fregno + ((GET_MODE_SIZE (mode) + 7) >> 3) > FP_ARG_MAX_REG + 1)
	ret = FP_ARG_MAX_REG + 1 - cum->fregno;
      else if (cum->nargs_prototype >= 0)
	return 0;
    }

  if (align_words < GP_ARG_NUM_REG
      && GP_ARG_NUM_REG < align_words + rs6000_arg_size (mode, type))
    ret = GP_ARG_NUM_REG - align_words;

  if (ret != 0 && TARGET_DEBUG_ARG)
    fprintf (stderr, "function_arg_partial_nregs: %d\n", ret);

  return ret;
}

/* A C expression that indicates when an argument must be passed by
   reference.  If nonzero for an argument, a copy of that argument is
   made in memory and a pointer to the argument is passed instead of
   the argument itself.  The pointer is passed in whatever way is
   appropriate for passing a pointer to that type.

   Under V.4, aggregates and long double are passed by reference.

   As an extension to all 32-bit ABIs, AltiVec vectors are passed by
   reference unless the AltiVec vector extension ABI is in force.

   As an extension to all ABIs, variable sized types are passed by
   reference.  */

int
function_arg_pass_by_reference (CUMULATIVE_ARGS *cum ATTRIBUTE_UNUSED, 
				enum machine_mode mode ATTRIBUTE_UNUSED, 
				tree type, int named ATTRIBUTE_UNUSED)
{
  if ((DEFAULT_ABI == ABI_V4
       && ((type && AGGREGATE_TYPE_P (type))
	   || mode == TFmode))
      || (TARGET_32BIT && !TARGET_ALTIVEC_ABI && ALTIVEC_VECTOR_MODE (mode))
      || (type && int_size_in_bytes (type) < 0))
    {
      if (TARGET_DEBUG_ARG)
	fprintf (stderr, "function_arg_pass_by_reference\n");

      return 1;
    }
  return 0;
}

static void
rs6000_move_block_from_reg (int regno, rtx x, int nregs)
{
  int i;
  enum machine_mode reg_mode = TARGET_32BIT ? SImode : DImode;

  if (nregs == 0)
    return;

    for (i = 0; i < nregs; i++)
    {
      rtx tem = adjust_address_nv (x, reg_mode, i*GET_MODE_SIZE(reg_mode));
      if (reload_completed)
      {
	if (! strict_memory_address_p (reg_mode, XEXP (tem, 0)))
	  tem = NULL_RTX;
	else
	  tem = simplify_gen_subreg (reg_mode, x, BLKmode, 
				     i * GET_MODE_SIZE(reg_mode));
      }
      else
	tem = replace_equiv_address (tem, XEXP (tem, 0));

      if (tem == NULL_RTX)
        abort ();

      emit_move_insn (tem, gen_rtx_REG (reg_mode, regno + i));
    }
}


/* Perform any needed actions needed for a function that is receiving a
   variable number of arguments. 

   CUM is as above.

   MODE and TYPE are the mode and type of the current parameter.

   PRETEND_SIZE is a variable that should be set to the amount of stack
   that must be pushed by the prolog to pretend that our caller pushed
   it.

   Normally, this macro will push all remaining incoming registers on the
   stack and set PRETEND_SIZE to the length of the registers pushed.  */

static void
setup_incoming_varargs (CUMULATIVE_ARGS *cum, enum machine_mode mode, 
		tree type, int *pretend_size ATTRIBUTE_UNUSED, int no_rtl)
{
  CUMULATIVE_ARGS next_cum;
  int reg_size = TARGET_32BIT ? 4 : 8;
  rtx save_area = NULL_RTX, mem;
  int first_reg_offset, set;

  /* Skip the last named argument.  */
  next_cum = *cum;
  function_arg_advance (&next_cum, mode, type, 1);

  if (DEFAULT_ABI == ABI_V4)
    {
      /* Indicate to allocate space on the stack for varargs save area.  */
      cfun->machine->sysv_varargs_p = 1;
      if (! no_rtl)
	save_area = plus_constant (virtual_stack_vars_rtx,
				   - RS6000_VARARGS_SIZE);

      first_reg_offset = next_cum.sysv_gregno - GP_ARG_MIN_REG;
    }
  else
    {
      first_reg_offset = next_cum.words;
      save_area = virtual_incoming_args_rtx;
      cfun->machine->sysv_varargs_p = 0;

      if (MUST_PASS_IN_STACK (mode, type))
	first_reg_offset += rs6000_arg_size (TYPE_MODE (type), type);
    }

  set = get_varargs_alias_set ();
  if (! no_rtl && first_reg_offset < GP_ARG_NUM_REG)
    {
      mem = gen_rtx_MEM (BLKmode,
		         plus_constant (save_area,
					first_reg_offset * reg_size)),
      set_mem_alias_set (mem, set);
      set_mem_align (mem, BITS_PER_WORD);

      rs6000_move_block_from_reg (GP_ARG_MIN_REG + first_reg_offset, mem, 
			          GP_ARG_NUM_REG - first_reg_offset);
    }

  /* Save FP registers if needed.  */
  if (DEFAULT_ABI == ABI_V4
      && TARGET_HARD_FLOAT && TARGET_FPRS
      && ! no_rtl
      && next_cum.fregno <= FP_ARG_V4_MAX_REG)
    {
      int fregno = next_cum.fregno;
      rtx cr1 = gen_rtx_REG (CCmode, CR1_REGNO);
      rtx lab = gen_label_rtx ();
      int off = (GP_ARG_NUM_REG * reg_size) + ((fregno - FP_ARG_MIN_REG) * 8);

      emit_jump_insn (gen_rtx_SET (VOIDmode,
				   pc_rtx,
				   gen_rtx_IF_THEN_ELSE (VOIDmode,
					    gen_rtx_NE (VOIDmode, cr1,
						        const0_rtx),
					    gen_rtx_LABEL_REF (VOIDmode, lab),
					    pc_rtx)));

      while (fregno <= FP_ARG_V4_MAX_REG)
	{
	  mem = gen_rtx_MEM (DFmode, plus_constant (save_area, off));
          set_mem_alias_set (mem, set);
	  set_mem_align (mem, GET_MODE_ALIGNMENT (DFmode));
	  emit_move_insn (mem, gen_rtx_REG (DFmode, fregno));
	  fregno++;
	  off += 8;
	}

      emit_label (lab);
    }
}

/* Create the va_list data type.  */

static tree
rs6000_build_builtin_va_list (void)
{
  tree f_gpr, f_fpr, f_res, f_ovf, f_sav, record, type_decl;

  /* For AIX, prefer 'char *' because that's what the system
     header files like.  */
  if (DEFAULT_ABI != ABI_V4)
    return build_pointer_type (char_type_node);

  record = (*lang_hooks.types.make_type) (RECORD_TYPE);
  type_decl = build_decl (TYPE_DECL, get_identifier ("__va_list_tag"), record);

  f_gpr = build_decl (FIELD_DECL, get_identifier ("gpr"), 
		      unsigned_char_type_node);
  f_fpr = build_decl (FIELD_DECL, get_identifier ("fpr"), 
		      unsigned_char_type_node);
  /* Give the two bytes of padding a name, so that -Wpadded won't warn on
     every user file.  */
  f_res = build_decl (FIELD_DECL, get_identifier ("reserved"),
		      short_unsigned_type_node);
  f_ovf = build_decl (FIELD_DECL, get_identifier ("overflow_arg_area"),
		      ptr_type_node);
  f_sav = build_decl (FIELD_DECL, get_identifier ("reg_save_area"),
		      ptr_type_node);

  DECL_FIELD_CONTEXT (f_gpr) = record;
  DECL_FIELD_CONTEXT (f_fpr) = record;
  DECL_FIELD_CONTEXT (f_res) = record;
  DECL_FIELD_CONTEXT (f_ovf) = record;
  DECL_FIELD_CONTEXT (f_sav) = record;

  TREE_CHAIN (record) = type_decl;
  TYPE_NAME (record) = type_decl;
  TYPE_FIELDS (record) = f_gpr;
  TREE_CHAIN (f_gpr) = f_fpr;
  TREE_CHAIN (f_fpr) = f_res;
  TREE_CHAIN (f_res) = f_ovf;
  TREE_CHAIN (f_ovf) = f_sav;

  layout_type (record);

  /* The correct type is an array type of one element.  */
  return build_array_type (record, build_index_type (size_zero_node));
}

/* Implement va_start.  */

void
rs6000_va_start (tree valist, rtx nextarg)
{
  HOST_WIDE_INT words, n_gpr, n_fpr;
  tree f_gpr, f_fpr, f_res, f_ovf, f_sav;
  tree gpr, fpr, ovf, sav, t;

  /* Only SVR4 needs something special.  */
  if (DEFAULT_ABI != ABI_V4)
    {
      std_expand_builtin_va_start (valist, nextarg);
      return;
    }

  f_gpr = TYPE_FIELDS (TREE_TYPE (va_list_type_node));
  f_fpr = TREE_CHAIN (f_gpr);
  f_res = TREE_CHAIN (f_fpr);
  f_ovf = TREE_CHAIN (f_res);
  f_sav = TREE_CHAIN (f_ovf);

  valist = build1 (INDIRECT_REF, TREE_TYPE (TREE_TYPE (valist)), valist);
  gpr = build (COMPONENT_REF, TREE_TYPE (f_gpr), valist, f_gpr);
  fpr = build (COMPONENT_REF, TREE_TYPE (f_fpr), valist, f_fpr);
  ovf = build (COMPONENT_REF, TREE_TYPE (f_ovf), valist, f_ovf);
  sav = build (COMPONENT_REF, TREE_TYPE (f_sav), valist, f_sav);

  /* Count number of gp and fp argument registers used.  */
  words = current_function_args_info.words;
  n_gpr = current_function_args_info.sysv_gregno - GP_ARG_MIN_REG;
  n_fpr = current_function_args_info.fregno - FP_ARG_MIN_REG;

  if (TARGET_DEBUG_ARG)
    fprintf (stderr, "va_start: words = "HOST_WIDE_INT_PRINT_DEC", n_gpr = "
	     HOST_WIDE_INT_PRINT_DEC", n_fpr = "HOST_WIDE_INT_PRINT_DEC"\n",
	     words, n_gpr, n_fpr);

  t = build (MODIFY_EXPR, TREE_TYPE (gpr), gpr, build_int_2 (n_gpr, 0));
  TREE_SIDE_EFFECTS (t) = 1;
  expand_expr (t, const0_rtx, VOIDmode, EXPAND_NORMAL);

  t = build (MODIFY_EXPR, TREE_TYPE (fpr), fpr, build_int_2 (n_fpr, 0));
  TREE_SIDE_EFFECTS (t) = 1;
  expand_expr (t, const0_rtx, VOIDmode, EXPAND_NORMAL);

  /* Find the overflow area.  */
  t = make_tree (TREE_TYPE (ovf), virtual_incoming_args_rtx);
  if (words != 0)
    t = build (PLUS_EXPR, TREE_TYPE (ovf), t,
	       build_int_2 (words * UNITS_PER_WORD, 0));
  t = build (MODIFY_EXPR, TREE_TYPE (ovf), ovf, t);
  TREE_SIDE_EFFECTS (t) = 1;
  expand_expr (t, const0_rtx, VOIDmode, EXPAND_NORMAL);

  /* Find the register save area.  */
  t = make_tree (TREE_TYPE (sav), virtual_stack_vars_rtx);
  t = build (PLUS_EXPR, TREE_TYPE (sav), t,
	     build_int_2 (-RS6000_VARARGS_SIZE, -1));
  t = build (MODIFY_EXPR, TREE_TYPE (sav), sav, t);
  TREE_SIDE_EFFECTS (t) = 1;
  expand_expr (t, const0_rtx, VOIDmode, EXPAND_NORMAL);
}

/* Implement va_arg.  */

rtx
rs6000_va_arg (tree valist, tree type)
{
  tree f_gpr, f_fpr, f_res, f_ovf, f_sav;
  tree gpr, fpr, ovf, sav, reg, t, u;
  int indirect_p, size, rsize, n_reg, sav_ofs, sav_scale;
  rtx lab_false, lab_over, addr_rtx, r;
  int align;

  if (DEFAULT_ABI != ABI_V4)
    {
      /* Variable sized types are passed by reference, as are AltiVec
	 vectors when 32-bit and not using the AltiVec ABI extension.  */
      if (int_size_in_bytes (type) < 0
	  || (TARGET_32BIT
	      && !TARGET_ALTIVEC_ABI
	      && ALTIVEC_VECTOR_MODE (TYPE_MODE (type))))
	{
	  u = build_pointer_type (type);

	  /* Args grow upward.  */
	  t = build (POSTINCREMENT_EXPR, TREE_TYPE (valist), valist,
		     build_int_2 (POINTER_SIZE / BITS_PER_UNIT, 0));
	  TREE_SIDE_EFFECTS (t) = 1;

	  t = build1 (NOP_EXPR, build_pointer_type (u), t);
	  TREE_SIDE_EFFECTS (t) = 1;

	  t = build1 (INDIRECT_REF, u, t);
	  TREE_SIDE_EFFECTS (t) = 1;

	  return expand_expr (t, NULL_RTX, VOIDmode, EXPAND_NORMAL);
	}
      if (targetm.calls.split_complex_arg
	  && TREE_CODE (type) == COMPLEX_TYPE)
	{
	  tree elem_type = TREE_TYPE (type);
	  enum machine_mode elem_mode = TYPE_MODE (elem_type);
	  int elem_size = GET_MODE_SIZE (elem_mode);

	  if (elem_size < UNITS_PER_WORD)
	    {
	      rtx real_part, imag_part, dest_real, rr;

	      real_part = rs6000_va_arg (valist, elem_type);
	      imag_part = rs6000_va_arg (valist, elem_type);

	      /* We're not returning the value here, but the address.
		 real_part and imag_part are not contiguous, and we know
		 there is space available to pack real_part next to
		 imag_part.  float _Complex is not promoted to
		 double _Complex by the default promotion rules that
		 promote float to double.  */
	      if (2 * elem_size > UNITS_PER_WORD)
		abort ();

	      real_part = gen_rtx_MEM (elem_mode, real_part);
	      imag_part = gen_rtx_MEM (elem_mode, imag_part);

	      dest_real = adjust_address (imag_part, elem_mode, -elem_size);
	      rr = gen_reg_rtx (elem_mode);
	      emit_move_insn (rr, real_part);
	      emit_move_insn (dest_real, rr);

	      return XEXP (dest_real, 0);
	    }
	}

      return std_expand_builtin_va_arg (valist, type);
    }

  f_gpr = TYPE_FIELDS (TREE_TYPE (va_list_type_node));
  f_fpr = TREE_CHAIN (f_gpr);
  f_res = TREE_CHAIN (f_fpr);
  f_ovf = TREE_CHAIN (f_res);
  f_sav = TREE_CHAIN (f_ovf);

  valist = build1 (INDIRECT_REF, TREE_TYPE (TREE_TYPE (valist)), valist);
  gpr = build (COMPONENT_REF, TREE_TYPE (f_gpr), valist, f_gpr);
  fpr = build (COMPONENT_REF, TREE_TYPE (f_fpr), valist, f_fpr);
  ovf = build (COMPONENT_REF, TREE_TYPE (f_ovf), valist, f_ovf);
  sav = build (COMPONENT_REF, TREE_TYPE (f_sav), valist, f_sav);

  size = int_size_in_bytes (type);
  rsize = (size + 3) / 4;
  align = 1;

  if (AGGREGATE_TYPE_P (type)
      || TYPE_MODE (type) == TFmode
      || (!TARGET_ALTIVEC_ABI && ALTIVEC_VECTOR_MODE (TYPE_MODE (type))))
    {
      /* Aggregates, long doubles, and AltiVec vectors are passed by
	 reference.  */
      indirect_p = 1;
      reg = gpr;
      n_reg = 1;
      sav_ofs = 0;
      sav_scale = 4;
      size = 4;
      rsize = 1;
    }
  else if (TARGET_HARD_FLOAT && TARGET_FPRS
	   && (TYPE_MODE (type) == SFmode || TYPE_MODE (type) == DFmode))
    {
      /* FP args go in FP registers, if present.  */
      indirect_p = 0;
      reg = fpr;
      n_reg = 1;
      sav_ofs = 8*4;
      sav_scale = 8;
      if (TYPE_MODE (type) == DFmode)
	align = 8;
    }
  else
    {
      /* Otherwise into GP registers.  */
      indirect_p = 0;
      reg = gpr;
      n_reg = rsize;
      sav_ofs = 0;
      sav_scale = 4;
      if (n_reg == 2)
	align = 8;
    }

  /* Pull the value out of the saved registers....  */

  lab_over = NULL_RTX;
  addr_rtx = gen_reg_rtx (Pmode);

  /*  AltiVec vectors never go in registers when -mabi=altivec.  */
  if (TARGET_ALTIVEC_ABI && ALTIVEC_VECTOR_MODE (TYPE_MODE (type)))
    align = 16;
  else
    {
      lab_false = gen_label_rtx ();
      lab_over = gen_label_rtx ();

      /* Long long and SPE vectors are aligned in the registers.
	 As are any other 2 gpr item such as complex int due to a
	 historical mistake.  */
      u = reg;
      if (n_reg == 2)
	{
	  u = build (BIT_AND_EXPR, TREE_TYPE (reg), reg,
		     build_int_2 (n_reg - 1, 0));
	  u = build (POSTINCREMENT_EXPR, TREE_TYPE (reg), reg, u);
	  TREE_SIDE_EFFECTS (u) = 1;
	}

      emit_cmp_and_jump_insns
	(expand_expr (u, NULL_RTX, QImode, EXPAND_NORMAL),
	 GEN_INT (8 - n_reg + 1), GE, const1_rtx, QImode, 1,
	 lab_false);

      t = sav;
      if (sav_ofs)
	t = build (PLUS_EXPR, ptr_type_node, sav, build_int_2 (sav_ofs, 0));

      u = build (POSTINCREMENT_EXPR, TREE_TYPE (reg), reg,
		 build_int_2 (n_reg, 0));
      TREE_SIDE_EFFECTS (u) = 1;

      u = build1 (CONVERT_EXPR, integer_type_node, u);
      TREE_SIDE_EFFECTS (u) = 1;

      u = build (MULT_EXPR, integer_type_node, u, build_int_2 (sav_scale, 0));
      TREE_SIDE_EFFECTS (u) = 1;

      t = build (PLUS_EXPR, ptr_type_node, t, u);
      TREE_SIDE_EFFECTS (t) = 1;

      r = expand_expr (t, addr_rtx, Pmode, EXPAND_NORMAL);
      if (r != addr_rtx)
	emit_move_insn (addr_rtx, r);

      emit_jump_insn (gen_jump (lab_over));
      emit_barrier ();

      emit_label (lab_false);
      if (n_reg > 2)
	{
	  /* Ensure that we don't find any more args in regs.
	     Alignment has taken care of the n_reg == 2 case.  */
	  t = build (MODIFY_EXPR, TREE_TYPE (reg), reg, build_int_2 (8, 0));
	  TREE_SIDE_EFFECTS (t) = 1;
	  expand_expr (t, const0_rtx, VOIDmode, EXPAND_NORMAL);
	}
    }

  /* ... otherwise out of the overflow area.  */

  /* Care for on-stack alignment if needed.  */
  t = ovf;
  if (align != 1)
    {
      t = build (PLUS_EXPR, TREE_TYPE (t), t, build_int_2 (align - 1, 0));
      t = build (BIT_AND_EXPR, TREE_TYPE (t), t, build_int_2 (-align, -1));
    }
  t = save_expr (t);

  r = expand_expr (t, addr_rtx, Pmode, EXPAND_NORMAL);
  if (r != addr_rtx)
    emit_move_insn (addr_rtx, r);

  t = build (PLUS_EXPR, TREE_TYPE (t), t, build_int_2 (size, 0));
  t = build (MODIFY_EXPR, TREE_TYPE (ovf), ovf, t);
  TREE_SIDE_EFFECTS (t) = 1;
  expand_expr (t, const0_rtx, VOIDmode, EXPAND_NORMAL);

  if (lab_over)
    emit_label (lab_over);

  if (indirect_p)
    {
      r = gen_rtx_MEM (Pmode, addr_rtx);
      set_mem_alias_set (r, get_varargs_alias_set ());
      emit_move_insn (addr_rtx, r);
    }

  return addr_rtx;
}

/* Builtins.  */

#define def_builtin(MASK, NAME, TYPE, CODE)			\
do {								\
  if ((MASK) & target_flags)					\
    builtin_function ((NAME), (TYPE), (CODE), BUILT_IN_MD,	\
		      NULL, NULL_TREE);				\
} while (0)

/* Simple ternary operations: VECd = foo (VECa, VECb, VECc).  */

static const struct builtin_description bdesc_3arg[] =
{
  { MASK_ALTIVEC, CODE_FOR_altivec_vmaddfp, "__builtin_altivec_vmaddfp", ALTIVEC_BUILTIN_VMADDFP },
  { MASK_ALTIVEC, CODE_FOR_altivec_vmhaddshs, "__builtin_altivec_vmhaddshs", ALTIVEC_BUILTIN_VMHADDSHS },
  { MASK_ALTIVEC, CODE_FOR_altivec_vmhraddshs, "__builtin_altivec_vmhraddshs", ALTIVEC_BUILTIN_VMHRADDSHS },
  { MASK_ALTIVEC, CODE_FOR_altivec_vmladduhm, "__builtin_altivec_vmladduhm", ALTIVEC_BUILTIN_VMLADDUHM},
  { MASK_ALTIVEC, CODE_FOR_altivec_vmsumubm, "__builtin_altivec_vmsumubm", ALTIVEC_BUILTIN_VMSUMUBM },
  { MASK_ALTIVEC, CODE_FOR_altivec_vmsummbm, "__builtin_altivec_vmsummbm", ALTIVEC_BUILTIN_VMSUMMBM },
  { MASK_ALTIVEC, CODE_FOR_altivec_vmsumuhm, "__builtin_altivec_vmsumuhm", ALTIVEC_BUILTIN_VMSUMUHM },
  { MASK_ALTIVEC, CODE_FOR_altivec_vmsumshm, "__builtin_altivec_vmsumshm", ALTIVEC_BUILTIN_VMSUMSHM },
  { MASK_ALTIVEC, CODE_FOR_altivec_vmsumuhs, "__builtin_altivec_vmsumuhs", ALTIVEC_BUILTIN_VMSUMUHS },
  { MASK_ALTIVEC, CODE_FOR_altivec_vmsumshs, "__builtin_altivec_vmsumshs", ALTIVEC_BUILTIN_VMSUMSHS },
  { MASK_ALTIVEC, CODE_FOR_altivec_vnmsubfp, "__builtin_altivec_vnmsubfp", ALTIVEC_BUILTIN_VNMSUBFP }, 
  { MASK_ALTIVEC, CODE_FOR_altivec_vperm_4sf, "__builtin_altivec_vperm_4sf", ALTIVEC_BUILTIN_VPERM_4SF },
  { MASK_ALTIVEC, CODE_FOR_altivec_vperm_4si, "__builtin_altivec_vperm_4si", ALTIVEC_BUILTIN_VPERM_4SI },
  { MASK_ALTIVEC, CODE_FOR_altivec_vperm_8hi, "__builtin_altivec_vperm_8hi", ALTIVEC_BUILTIN_VPERM_8HI },
  { MASK_ALTIVEC, CODE_FOR_altivec_vperm_16qi, "__builtin_altivec_vperm_16qi", ALTIVEC_BUILTIN_VPERM_16QI },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsel_4sf, "__builtin_altivec_vsel_4sf", ALTIVEC_BUILTIN_VSEL_4SF },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsel_4si, "__builtin_altivec_vsel_4si", ALTIVEC_BUILTIN_VSEL_4SI },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsel_8hi, "__builtin_altivec_vsel_8hi", ALTIVEC_BUILTIN_VSEL_8HI },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsel_16qi, "__builtin_altivec_vsel_16qi", ALTIVEC_BUILTIN_VSEL_16QI },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsldoi_16qi, "__builtin_altivec_vsldoi_16qi", ALTIVEC_BUILTIN_VSLDOI_16QI },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsldoi_8hi, "__builtin_altivec_vsldoi_8hi", ALTIVEC_BUILTIN_VSLDOI_8HI },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsldoi_4si, "__builtin_altivec_vsldoi_4si", ALTIVEC_BUILTIN_VSLDOI_4SI },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsldoi_4sf, "__builtin_altivec_vsldoi_4sf", ALTIVEC_BUILTIN_VSLDOI_4SF },
};

/* DST operations: void foo (void *, const int, const char).  */

static const struct builtin_description bdesc_dst[] =
{
  { MASK_ALTIVEC, CODE_FOR_altivec_dst, "__builtin_altivec_dst", ALTIVEC_BUILTIN_DST },
  { MASK_ALTIVEC, CODE_FOR_altivec_dstt, "__builtin_altivec_dstt", ALTIVEC_BUILTIN_DSTT },
  { MASK_ALTIVEC, CODE_FOR_altivec_dstst, "__builtin_altivec_dstst", ALTIVEC_BUILTIN_DSTST },
  { MASK_ALTIVEC, CODE_FOR_altivec_dststt, "__builtin_altivec_dststt", ALTIVEC_BUILTIN_DSTSTT }
};

/* Simple binary operations: VECc = foo (VECa, VECb).  */

static struct builtin_description bdesc_2arg[] =
{
  { MASK_ALTIVEC, CODE_FOR_addv16qi3, "__builtin_altivec_vaddubm", ALTIVEC_BUILTIN_VADDUBM },
  { MASK_ALTIVEC, CODE_FOR_addv8hi3, "__builtin_altivec_vadduhm", ALTIVEC_BUILTIN_VADDUHM },
  { MASK_ALTIVEC, CODE_FOR_addv4si3, "__builtin_altivec_vadduwm", ALTIVEC_BUILTIN_VADDUWM },
  { MASK_ALTIVEC, CODE_FOR_addv4sf3, "__builtin_altivec_vaddfp", ALTIVEC_BUILTIN_VADDFP },
  { MASK_ALTIVEC, CODE_FOR_altivec_vaddcuw, "__builtin_altivec_vaddcuw", ALTIVEC_BUILTIN_VADDCUW },
  { MASK_ALTIVEC, CODE_FOR_altivec_vaddubs, "__builtin_altivec_vaddubs", ALTIVEC_BUILTIN_VADDUBS },
  { MASK_ALTIVEC, CODE_FOR_altivec_vaddsbs, "__builtin_altivec_vaddsbs", ALTIVEC_BUILTIN_VADDSBS },
  { MASK_ALTIVEC, CODE_FOR_altivec_vadduhs, "__builtin_altivec_vadduhs", ALTIVEC_BUILTIN_VADDUHS },
  { MASK_ALTIVEC, CODE_FOR_altivec_vaddshs, "__builtin_altivec_vaddshs", ALTIVEC_BUILTIN_VADDSHS },
  { MASK_ALTIVEC, CODE_FOR_altivec_vadduws, "__builtin_altivec_vadduws", ALTIVEC_BUILTIN_VADDUWS },
  { MASK_ALTIVEC, CODE_FOR_altivec_vaddsws, "__builtin_altivec_vaddsws", ALTIVEC_BUILTIN_VADDSWS },
  { MASK_ALTIVEC, CODE_FOR_andv4si3, "__builtin_altivec_vand", ALTIVEC_BUILTIN_VAND },
  { MASK_ALTIVEC, CODE_FOR_altivec_vandc, "__builtin_altivec_vandc", ALTIVEC_BUILTIN_VANDC },
  { MASK_ALTIVEC, CODE_FOR_altivec_vavgub, "__builtin_altivec_vavgub", ALTIVEC_BUILTIN_VAVGUB },
  { MASK_ALTIVEC, CODE_FOR_altivec_vavgsb, "__builtin_altivec_vavgsb", ALTIVEC_BUILTIN_VAVGSB },
  { MASK_ALTIVEC, CODE_FOR_altivec_vavguh, "__builtin_altivec_vavguh", ALTIVEC_BUILTIN_VAVGUH },
  { MASK_ALTIVEC, CODE_FOR_altivec_vavgsh, "__builtin_altivec_vavgsh", ALTIVEC_BUILTIN_VAVGSH },
  { MASK_ALTIVEC, CODE_FOR_altivec_vavguw, "__builtin_altivec_vavguw", ALTIVEC_BUILTIN_VAVGUW },
  { MASK_ALTIVEC, CODE_FOR_altivec_vavgsw, "__builtin_altivec_vavgsw", ALTIVEC_BUILTIN_VAVGSW },
  { MASK_ALTIVEC, CODE_FOR_altivec_vcfux, "__builtin_altivec_vcfux", ALTIVEC_BUILTIN_VCFUX },
  { MASK_ALTIVEC, CODE_FOR_altivec_vcfsx, "__builtin_altivec_vcfsx", ALTIVEC_BUILTIN_VCFSX },
  { MASK_ALTIVEC, CODE_FOR_altivec_vcmpbfp, "__builtin_altivec_vcmpbfp", ALTIVEC_BUILTIN_VCMPBFP },
  { MASK_ALTIVEC, CODE_FOR_altivec_vcmpequb, "__builtin_altivec_vcmpequb", ALTIVEC_BUILTIN_VCMPEQUB },
  { MASK_ALTIVEC, CODE_FOR_altivec_vcmpequh, "__builtin_altivec_vcmpequh", ALTIVEC_BUILTIN_VCMPEQUH },
  { MASK_ALTIVEC, CODE_FOR_altivec_vcmpequw, "__builtin_altivec_vcmpequw", ALTIVEC_BUILTIN_VCMPEQUW },
  { MASK_ALTIVEC, CODE_FOR_altivec_vcmpeqfp, "__builtin_altivec_vcmpeqfp", ALTIVEC_BUILTIN_VCMPEQFP },
  { MASK_ALTIVEC, CODE_FOR_altivec_vcmpgefp, "__builtin_altivec_vcmpgefp", ALTIVEC_BUILTIN_VCMPGEFP },
  { MASK_ALTIVEC, CODE_FOR_altivec_vcmpgtub, "__builtin_altivec_vcmpgtub", ALTIVEC_BUILTIN_VCMPGTUB },
  { MASK_ALTIVEC, CODE_FOR_altivec_vcmpgtsb, "__builtin_altivec_vcmpgtsb", ALTIVEC_BUILTIN_VCMPGTSB },
  { MASK_ALTIVEC, CODE_FOR_altivec_vcmpgtuh, "__builtin_altivec_vcmpgtuh", ALTIVEC_BUILTIN_VCMPGTUH },
  { MASK_ALTIVEC, CODE_FOR_altivec_vcmpgtsh, "__builtin_altivec_vcmpgtsh", ALTIVEC_BUILTIN_VCMPGTSH },
  { MASK_ALTIVEC, CODE_FOR_altivec_vcmpgtuw, "__builtin_altivec_vcmpgtuw", ALTIVEC_BUILTIN_VCMPGTUW },
  { MASK_ALTIVEC, CODE_FOR_altivec_vcmpgtsw, "__builtin_altivec_vcmpgtsw", ALTIVEC_BUILTIN_VCMPGTSW },
  { MASK_ALTIVEC, CODE_FOR_altivec_vcmpgtfp, "__builtin_altivec_vcmpgtfp", ALTIVEC_BUILTIN_VCMPGTFP },
  { MASK_ALTIVEC, CODE_FOR_altivec_vctsxs, "__builtin_altivec_vctsxs", ALTIVEC_BUILTIN_VCTSXS },
  { MASK_ALTIVEC, CODE_FOR_altivec_vctuxs, "__builtin_altivec_vctuxs", ALTIVEC_BUILTIN_VCTUXS },
  { MASK_ALTIVEC, CODE_FOR_umaxv16qi3, "__builtin_altivec_vmaxub", ALTIVEC_BUILTIN_VMAXUB },
  { MASK_ALTIVEC, CODE_FOR_smaxv16qi3, "__builtin_altivec_vmaxsb", ALTIVEC_BUILTIN_VMAXSB },
  { MASK_ALTIVEC, CODE_FOR_umaxv8hi3, "__builtin_altivec_vmaxuh", ALTIVEC_BUILTIN_VMAXUH },
  { MASK_ALTIVEC, CODE_FOR_smaxv8hi3, "__builtin_altivec_vmaxsh", ALTIVEC_BUILTIN_VMAXSH },
  { MASK_ALTIVEC, CODE_FOR_umaxv4si3, "__builtin_altivec_vmaxuw", ALTIVEC_BUILTIN_VMAXUW },
  { MASK_ALTIVEC, CODE_FOR_smaxv4si3, "__builtin_altivec_vmaxsw", ALTIVEC_BUILTIN_VMAXSW },
  { MASK_ALTIVEC, CODE_FOR_smaxv4sf3, "__builtin_altivec_vmaxfp", ALTIVEC_BUILTIN_VMAXFP },
  { MASK_ALTIVEC, CODE_FOR_altivec_vmrghb, "__builtin_altivec_vmrghb", ALTIVEC_BUILTIN_VMRGHB },
  { MASK_ALTIVEC, CODE_FOR_altivec_vmrghh, "__builtin_altivec_vmrghh", ALTIVEC_BUILTIN_VMRGHH },
  { MASK_ALTIVEC, CODE_FOR_altivec_vmrghw, "__builtin_altivec_vmrghw", ALTIVEC_BUILTIN_VMRGHW },
  { MASK_ALTIVEC, CODE_FOR_altivec_vmrglb, "__builtin_altivec_vmrglb", ALTIVEC_BUILTIN_VMRGLB },
  { MASK_ALTIVEC, CODE_FOR_altivec_vmrglh, "__builtin_altivec_vmrglh", ALTIVEC_BUILTIN_VMRGLH },
  { MASK_ALTIVEC, CODE_FOR_altivec_vmrglw, "__builtin_altivec_vmrglw", ALTIVEC_BUILTIN_VMRGLW },
  { MASK_ALTIVEC, CODE_FOR_uminv16qi3, "__builtin_altivec_vminub", ALTIVEC_BUILTIN_VMINUB },
  { MASK_ALTIVEC, CODE_FOR_sminv16qi3, "__builtin_altivec_vminsb", ALTIVEC_BUILTIN_VMINSB },
  { MASK_ALTIVEC, CODE_FOR_uminv8hi3, "__builtin_altivec_vminuh", ALTIVEC_BUILTIN_VMINUH },
  { MASK_ALTIVEC, CODE_FOR_sminv8hi3, "__builtin_altivec_vminsh", ALTIVEC_BUILTIN_VMINSH },
  { MASK_ALTIVEC, CODE_FOR_uminv4si3, "__builtin_altivec_vminuw", ALTIVEC_BUILTIN_VMINUW },
  { MASK_ALTIVEC, CODE_FOR_sminv4si3, "__builtin_altivec_vminsw", ALTIVEC_BUILTIN_VMINSW },
  { MASK_ALTIVEC, CODE_FOR_sminv4sf3, "__builtin_altivec_vminfp", ALTIVEC_BUILTIN_VMINFP },
  { MASK_ALTIVEC, CODE_FOR_altivec_vmuleub, "__builtin_altivec_vmuleub", ALTIVEC_BUILTIN_VMULEUB },
  { MASK_ALTIVEC, CODE_FOR_altivec_vmulesb, "__builtin_altivec_vmulesb", ALTIVEC_BUILTIN_VMULESB },
  { MASK_ALTIVEC, CODE_FOR_altivec_vmuleuh, "__builtin_altivec_vmuleuh", ALTIVEC_BUILTIN_VMULEUH },
  { MASK_ALTIVEC, CODE_FOR_altivec_vmulesh, "__builtin_altivec_vmulesh", ALTIVEC_BUILTIN_VMULESH },
  { MASK_ALTIVEC, CODE_FOR_altivec_vmuloub, "__builtin_altivec_vmuloub", ALTIVEC_BUILTIN_VMULOUB },
  { MASK_ALTIVEC, CODE_FOR_altivec_vmulosb, "__builtin_altivec_vmulosb", ALTIVEC_BUILTIN_VMULOSB },
  { MASK_ALTIVEC, CODE_FOR_altivec_vmulouh, "__builtin_altivec_vmulouh", ALTIVEC_BUILTIN_VMULOUH },
  { MASK_ALTIVEC, CODE_FOR_altivec_vmulosh, "__builtin_altivec_vmulosh", ALTIVEC_BUILTIN_VMULOSH },
  { MASK_ALTIVEC, CODE_FOR_altivec_vnor, "__builtin_altivec_vnor", ALTIVEC_BUILTIN_VNOR },
  { MASK_ALTIVEC, CODE_FOR_iorv4si3, "__builtin_altivec_vor", ALTIVEC_BUILTIN_VOR },
  { MASK_ALTIVEC, CODE_FOR_altivec_vpkuhum, "__builtin_altivec_vpkuhum", ALTIVEC_BUILTIN_VPKUHUM },
  { MASK_ALTIVEC, CODE_FOR_altivec_vpkuwum, "__builtin_altivec_vpkuwum", ALTIVEC_BUILTIN_VPKUWUM },
  { MASK_ALTIVEC, CODE_FOR_altivec_vpkpx, "__builtin_altivec_vpkpx", ALTIVEC_BUILTIN_VPKPX },
  { MASK_ALTIVEC, CODE_FOR_altivec_vpkuhss, "__builtin_altivec_vpkuhss", ALTIVEC_BUILTIN_VPKUHSS },
  { MASK_ALTIVEC, CODE_FOR_altivec_vpkshss, "__builtin_altivec_vpkshss", ALTIVEC_BUILTIN_VPKSHSS },
  { MASK_ALTIVEC, CODE_FOR_altivec_vpkuwss, "__builtin_altivec_vpkuwss", ALTIVEC_BUILTIN_VPKUWSS },
  { MASK_ALTIVEC, CODE_FOR_altivec_vpkswss, "__builtin_altivec_vpkswss", ALTIVEC_BUILTIN_VPKSWSS },
  { MASK_ALTIVEC, CODE_FOR_altivec_vpkuhus, "__builtin_altivec_vpkuhus", ALTIVEC_BUILTIN_VPKUHUS },
  { MASK_ALTIVEC, CODE_FOR_altivec_vpkshus, "__builtin_altivec_vpkshus", ALTIVEC_BUILTIN_VPKSHUS },
  { MASK_ALTIVEC, CODE_FOR_altivec_vpkuwus, "__builtin_altivec_vpkuwus", ALTIVEC_BUILTIN_VPKUWUS },
  { MASK_ALTIVEC, CODE_FOR_altivec_vpkswus, "__builtin_altivec_vpkswus", ALTIVEC_BUILTIN_VPKSWUS },
  { MASK_ALTIVEC, CODE_FOR_altivec_vrlb, "__builtin_altivec_vrlb", ALTIVEC_BUILTIN_VRLB },
  { MASK_ALTIVEC, CODE_FOR_altivec_vrlh, "__builtin_altivec_vrlh", ALTIVEC_BUILTIN_VRLH },
  { MASK_ALTIVEC, CODE_FOR_altivec_vrlw, "__builtin_altivec_vrlw", ALTIVEC_BUILTIN_VRLW },
  { MASK_ALTIVEC, CODE_FOR_altivec_vslb, "__builtin_altivec_vslb", ALTIVEC_BUILTIN_VSLB },
  { MASK_ALTIVEC, CODE_FOR_altivec_vslh, "__builtin_altivec_vslh", ALTIVEC_BUILTIN_VSLH },
  { MASK_ALTIVEC, CODE_FOR_altivec_vslw, "__builtin_altivec_vslw", ALTIVEC_BUILTIN_VSLW },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsl, "__builtin_altivec_vsl", ALTIVEC_BUILTIN_VSL },
  { MASK_ALTIVEC, CODE_FOR_altivec_vslo, "__builtin_altivec_vslo", ALTIVEC_BUILTIN_VSLO },
  { MASK_ALTIVEC, CODE_FOR_altivec_vspltb, "__builtin_altivec_vspltb", ALTIVEC_BUILTIN_VSPLTB },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsplth, "__builtin_altivec_vsplth", ALTIVEC_BUILTIN_VSPLTH },
  { MASK_ALTIVEC, CODE_FOR_altivec_vspltw, "__builtin_altivec_vspltw", ALTIVEC_BUILTIN_VSPLTW },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsrb, "__builtin_altivec_vsrb", ALTIVEC_BUILTIN_VSRB },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsrh, "__builtin_altivec_vsrh", ALTIVEC_BUILTIN_VSRH },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsrw, "__builtin_altivec_vsrw", ALTIVEC_BUILTIN_VSRW },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsrab, "__builtin_altivec_vsrab", ALTIVEC_BUILTIN_VSRAB },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsrah, "__builtin_altivec_vsrah", ALTIVEC_BUILTIN_VSRAH },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsraw, "__builtin_altivec_vsraw", ALTIVEC_BUILTIN_VSRAW },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsr, "__builtin_altivec_vsr", ALTIVEC_BUILTIN_VSR },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsro, "__builtin_altivec_vsro", ALTIVEC_BUILTIN_VSRO },
  { MASK_ALTIVEC, CODE_FOR_subv16qi3, "__builtin_altivec_vsububm", ALTIVEC_BUILTIN_VSUBUBM },
  { MASK_ALTIVEC, CODE_FOR_subv8hi3, "__builtin_altivec_vsubuhm", ALTIVEC_BUILTIN_VSUBUHM },
  { MASK_ALTIVEC, CODE_FOR_subv4si3, "__builtin_altivec_vsubuwm", ALTIVEC_BUILTIN_VSUBUWM },
  { MASK_ALTIVEC, CODE_FOR_subv4sf3, "__builtin_altivec_vsubfp", ALTIVEC_BUILTIN_VSUBFP },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsubcuw, "__builtin_altivec_vsubcuw", ALTIVEC_BUILTIN_VSUBCUW },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsububs, "__builtin_altivec_vsububs", ALTIVEC_BUILTIN_VSUBUBS },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsubsbs, "__builtin_altivec_vsubsbs", ALTIVEC_BUILTIN_VSUBSBS },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsubuhs, "__builtin_altivec_vsubuhs", ALTIVEC_BUILTIN_VSUBUHS },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsubshs, "__builtin_altivec_vsubshs", ALTIVEC_BUILTIN_VSUBSHS },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsubuws, "__builtin_altivec_vsubuws", ALTIVEC_BUILTIN_VSUBUWS },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsubsws, "__builtin_altivec_vsubsws", ALTIVEC_BUILTIN_VSUBSWS },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsum4ubs, "__builtin_altivec_vsum4ubs", ALTIVEC_BUILTIN_VSUM4UBS },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsum4sbs, "__builtin_altivec_vsum4sbs", ALTIVEC_BUILTIN_VSUM4SBS },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsum4shs, "__builtin_altivec_vsum4shs", ALTIVEC_BUILTIN_VSUM4SHS },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsum2sws, "__builtin_altivec_vsum2sws", ALTIVEC_BUILTIN_VSUM2SWS },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsumsws, "__builtin_altivec_vsumsws", ALTIVEC_BUILTIN_VSUMSWS },
  { MASK_ALTIVEC, CODE_FOR_xorv4si3, "__builtin_altivec_vxor", ALTIVEC_BUILTIN_VXOR },

  /* Place holder, leave as first spe builtin.  */
  { 0, CODE_FOR_spe_evaddw, "__builtin_spe_evaddw", SPE_BUILTIN_EVADDW },
  { 0, CODE_FOR_spe_evand, "__builtin_spe_evand", SPE_BUILTIN_EVAND },
  { 0, CODE_FOR_spe_evandc, "__builtin_spe_evandc", SPE_BUILTIN_EVANDC },
  { 0, CODE_FOR_spe_evdivws, "__builtin_spe_evdivws", SPE_BUILTIN_EVDIVWS },
  { 0, CODE_FOR_spe_evdivwu, "__builtin_spe_evdivwu", SPE_BUILTIN_EVDIVWU },
  { 0, CODE_FOR_spe_eveqv, "__builtin_spe_eveqv", SPE_BUILTIN_EVEQV },
  { 0, CODE_FOR_spe_evfsadd, "__builtin_spe_evfsadd", SPE_BUILTIN_EVFSADD },
  { 0, CODE_FOR_spe_evfsdiv, "__builtin_spe_evfsdiv", SPE_BUILTIN_EVFSDIV },
  { 0, CODE_FOR_spe_evfsmul, "__builtin_spe_evfsmul", SPE_BUILTIN_EVFSMUL },
  { 0, CODE_FOR_spe_evfssub, "__builtin_spe_evfssub", SPE_BUILTIN_EVFSSUB },
  { 0, CODE_FOR_spe_evmergehi, "__builtin_spe_evmergehi", SPE_BUILTIN_EVMERGEHI },
  { 0, CODE_FOR_spe_evmergehilo, "__builtin_spe_evmergehilo", SPE_BUILTIN_EVMERGEHILO },
  { 0, CODE_FOR_spe_evmergelo, "__builtin_spe_evmergelo", SPE_BUILTIN_EVMERGELO },
  { 0, CODE_FOR_spe_evmergelohi, "__builtin_spe_evmergelohi", SPE_BUILTIN_EVMERGELOHI },
  { 0, CODE_FOR_spe_evmhegsmfaa, "__builtin_spe_evmhegsmfaa", SPE_BUILTIN_EVMHEGSMFAA },
  { 0, CODE_FOR_spe_evmhegsmfan, "__builtin_spe_evmhegsmfan", SPE_BUILTIN_EVMHEGSMFAN },
  { 0, CODE_FOR_spe_evmhegsmiaa, "__builtin_spe_evmhegsmiaa", SPE_BUILTIN_EVMHEGSMIAA },
  { 0, CODE_FOR_spe_evmhegsmian, "__builtin_spe_evmhegsmian", SPE_BUILTIN_EVMHEGSMIAN },
  { 0, CODE_FOR_spe_evmhegumiaa, "__builtin_spe_evmhegumiaa", SPE_BUILTIN_EVMHEGUMIAA },
  { 0, CODE_FOR_spe_evmhegumian, "__builtin_spe_evmhegumian", SPE_BUILTIN_EVMHEGUMIAN },
  { 0, CODE_FOR_spe_evmhesmf, "__builtin_spe_evmhesmf", SPE_BUILTIN_EVMHESMF },
  { 0, CODE_FOR_spe_evmhesmfa, "__builtin_spe_evmhesmfa", SPE_BUILTIN_EVMHESMFA },
  { 0, CODE_FOR_spe_evmhesmfaaw, "__builtin_spe_evmhesmfaaw", SPE_BUILTIN_EVMHESMFAAW },
  { 0, CODE_FOR_spe_evmhesmfanw, "__builtin_spe_evmhesmfanw", SPE_BUILTIN_EVMHESMFANW },
  { 0, CODE_FOR_spe_evmhesmi, "__builtin_spe_evmhesmi", SPE_BUILTIN_EVMHESMI },
  { 0, CODE_FOR_spe_evmhesmia, "__builtin_spe_evmhesmia", SPE_BUILTIN_EVMHESMIA },
  { 0, CODE_FOR_spe_evmhesmiaaw, "__builtin_spe_evmhesmiaaw", SPE_BUILTIN_EVMHESMIAAW },
  { 0, CODE_FOR_spe_evmhesmianw, "__builtin_spe_evmhesmianw", SPE_BUILTIN_EVMHESMIANW },
  { 0, CODE_FOR_spe_evmhessf, "__builtin_spe_evmhessf", SPE_BUILTIN_EVMHESSF },
  { 0, CODE_FOR_spe_evmhessfa, "__builtin_spe_evmhessfa", SPE_BUILTIN_EVMHESSFA },
  { 0, CODE_FOR_spe_evmhessfaaw, "__builtin_spe_evmhessfaaw", SPE_BUILTIN_EVMHESSFAAW },
  { 0, CODE_FOR_spe_evmhessfanw, "__builtin_spe_evmhessfanw", SPE_BUILTIN_EVMHESSFANW },
  { 0, CODE_FOR_spe_evmhessiaaw, "__builtin_spe_evmhessiaaw", SPE_BUILTIN_EVMHESSIAAW },
  { 0, CODE_FOR_spe_evmhessianw, "__builtin_spe_evmhessianw", SPE_BUILTIN_EVMHESSIANW },
  { 0, CODE_FOR_spe_evmheumi, "__builtin_spe_evmheumi", SPE_BUILTIN_EVMHEUMI },
  { 0, CODE_FOR_spe_evmheumia, "__builtin_spe_evmheumia", SPE_BUILTIN_EVMHEUMIA },
  { 0, CODE_FOR_spe_evmheumiaaw, "__builtin_spe_evmheumiaaw", SPE_BUILTIN_EVMHEUMIAAW },
  { 0, CODE_FOR_spe_evmheumianw, "__builtin_spe_evmheumianw", SPE_BUILTIN_EVMHEUMIANW },
  { 0, CODE_FOR_spe_evmheusiaaw, "__builtin_spe_evmheusiaaw", SPE_BUILTIN_EVMHEUSIAAW },
  { 0, CODE_FOR_spe_evmheusianw, "__builtin_spe_evmheusianw", SPE_BUILTIN_EVMHEUSIANW },
  { 0, CODE_FOR_spe_evmhogsmfaa, "__builtin_spe_evmhogsmfaa", SPE_BUILTIN_EVMHOGSMFAA },
  { 0, CODE_FOR_spe_evmhogsmfan, "__builtin_spe_evmhogsmfan", SPE_BUILTIN_EVMHOGSMFAN },
  { 0, CODE_FOR_spe_evmhogsmiaa, "__builtin_spe_evmhogsmiaa", SPE_BUILTIN_EVMHOGSMIAA },
  { 0, CODE_FOR_spe_evmhogsmian, "__builtin_spe_evmhogsmian", SPE_BUILTIN_EVMHOGSMIAN },
  { 0, CODE_FOR_spe_evmhogumiaa, "__builtin_spe_evmhogumiaa", SPE_BUILTIN_EVMHOGUMIAA },
  { 0, CODE_FOR_spe_evmhogumian, "__builtin_spe_evmhogumian", SPE_BUILTIN_EVMHOGUMIAN },
  { 0, CODE_FOR_spe_evmhosmf, "__builtin_spe_evmhosmf", SPE_BUILTIN_EVMHOSMF },
  { 0, CODE_FOR_spe_evmhosmfa, "__builtin_spe_evmhosmfa", SPE_BUILTIN_EVMHOSMFA },
  { 0, CODE_FOR_spe_evmhosmfaaw, "__builtin_spe_evmhosmfaaw", SPE_BUILTIN_EVMHOSMFAAW },
  { 0, CODE_FOR_spe_evmhosmfanw, "__builtin_spe_evmhosmfanw", SPE_BUILTIN_EVMHOSMFANW },
  { 0, CODE_FOR_spe_evmhosmi, "__builtin_spe_evmhosmi", SPE_BUILTIN_EVMHOSMI },
  { 0, CODE_FOR_spe_evmhosmia, "__builtin_spe_evmhosmia", SPE_BUILTIN_EVMHOSMIA },
  { 0, CODE_FOR_spe_evmhosmiaaw, "__builtin_spe_evmhosmiaaw", SPE_BUILTIN_EVMHOSMIAAW },
  { 0, CODE_FOR_spe_evmhosmianw, "__builtin_spe_evmhosmianw", SPE_BUILTIN_EVMHOSMIANW },
  { 0, CODE_FOR_spe_evmhossf, "__builtin_spe_evmhossf", SPE_BUILTIN_EVMHOSSF },
  { 0, CODE_FOR_spe_evmhossfa, "__builtin_spe_evmhossfa", SPE_BUILTIN_EVMHOSSFA },
  { 0, CODE_FOR_spe_evmhossfaaw, "__builtin_spe_evmhossfaaw", SPE_BUILTIN_EVMHOSSFAAW },
  { 0, CODE_FOR_spe_evmhossfanw, "__builtin_spe_evmhossfanw", SPE_BUILTIN_EVMHOSSFANW },
  { 0, CODE_FOR_spe_evmhossiaaw, "__builtin_spe_evmhossiaaw", SPE_BUILTIN_EVMHOSSIAAW },
  { 0, CODE_FOR_spe_evmhossianw, "__builtin_spe_evmhossianw", SPE_BUILTIN_EVMHOSSIANW },
  { 0, CODE_FOR_spe_evmhoumi, "__builtin_spe_evmhoumi", SPE_BUILTIN_EVMHOUMI },
  { 0, CODE_FOR_spe_evmhoumia, "__builtin_spe_evmhoumia", SPE_BUILTIN_EVMHOUMIA },
  { 0, CODE_FOR_spe_evmhoumiaaw, "__builtin_spe_evmhoumiaaw", SPE_BUILTIN_EVMHOUMIAAW },
  { 0, CODE_FOR_spe_evmhoumianw, "__builtin_spe_evmhoumianw", SPE_BUILTIN_EVMHOUMIANW },
  { 0, CODE_FOR_spe_evmhousiaaw, "__builtin_spe_evmhousiaaw", SPE_BUILTIN_EVMHOUSIAAW },
  { 0, CODE_FOR_spe_evmhousianw, "__builtin_spe_evmhousianw", SPE_BUILTIN_EVMHOUSIANW },
  { 0, CODE_FOR_spe_evmwhsmf, "__builtin_spe_evmwhsmf", SPE_BUILTIN_EVMWHSMF },
  { 0, CODE_FOR_spe_evmwhsmfa, "__builtin_spe_evmwhsmfa", SPE_BUILTIN_EVMWHSMFA },
  { 0, CODE_FOR_spe_evmwhsmi, "__builtin_spe_evmwhsmi", SPE_BUILTIN_EVMWHSMI },
  { 0, CODE_FOR_spe_evmwhsmia, "__builtin_spe_evmwhsmia", SPE_BUILTIN_EVMWHSMIA },
  { 0, CODE_FOR_spe_evmwhssf, "__builtin_spe_evmwhssf", SPE_BUILTIN_EVMWHSSF },
  { 0, CODE_FOR_spe_evmwhssfa, "__builtin_spe_evmwhssfa", SPE_BUILTIN_EVMWHSSFA },
  { 0, CODE_FOR_spe_evmwhumi, "__builtin_spe_evmwhumi", SPE_BUILTIN_EVMWHUMI },
  { 0, CODE_FOR_spe_evmwhumia, "__builtin_spe_evmwhumia", SPE_BUILTIN_EVMWHUMIA },
  { 0, CODE_FOR_spe_evmwlsmiaaw, "__builtin_spe_evmwlsmiaaw", SPE_BUILTIN_EVMWLSMIAAW },
  { 0, CODE_FOR_spe_evmwlsmianw, "__builtin_spe_evmwlsmianw", SPE_BUILTIN_EVMWLSMIANW },
  { 0, CODE_FOR_spe_evmwlssiaaw, "__builtin_spe_evmwlssiaaw", SPE_BUILTIN_EVMWLSSIAAW },
  { 0, CODE_FOR_spe_evmwlssianw, "__builtin_spe_evmwlssianw", SPE_BUILTIN_EVMWLSSIANW },
  { 0, CODE_FOR_spe_evmwlumi, "__builtin_spe_evmwlumi", SPE_BUILTIN_EVMWLUMI },
  { 0, CODE_FOR_spe_evmwlumia, "__builtin_spe_evmwlumia", SPE_BUILTIN_EVMWLUMIA },
  { 0, CODE_FOR_spe_evmwlumiaaw, "__builtin_spe_evmwlumiaaw", SPE_BUILTIN_EVMWLUMIAAW },
  { 0, CODE_FOR_spe_evmwlumianw, "__builtin_spe_evmwlumianw", SPE_BUILTIN_EVMWLUMIANW },
  { 0, CODE_FOR_spe_evmwlusiaaw, "__builtin_spe_evmwlusiaaw", SPE_BUILTIN_EVMWLUSIAAW },
  { 0, CODE_FOR_spe_evmwlusianw, "__builtin_spe_evmwlusianw", SPE_BUILTIN_EVMWLUSIANW },
  { 0, CODE_FOR_spe_evmwsmf, "__builtin_spe_evmwsmf", SPE_BUILTIN_EVMWSMF },
  { 0, CODE_FOR_spe_evmwsmfa, "__builtin_spe_evmwsmfa", SPE_BUILTIN_EVMWSMFA },
  { 0, CODE_FOR_spe_evmwsmfaa, "__builtin_spe_evmwsmfaa", SPE_BUILTIN_EVMWSMFAA },
  { 0, CODE_FOR_spe_evmwsmfan, "__builtin_spe_evmwsmfan", SPE_BUILTIN_EVMWSMFAN },
  { 0, CODE_FOR_spe_evmwsmi, "__builtin_spe_evmwsmi", SPE_BUILTIN_EVMWSMI },
  { 0, CODE_FOR_spe_evmwsmia, "__builtin_spe_evmwsmia", SPE_BUILTIN_EVMWSMIA },
  { 0, CODE_FOR_spe_evmwsmiaa, "__builtin_spe_evmwsmiaa", SPE_BUILTIN_EVMWSMIAA },
  { 0, CODE_FOR_spe_evmwsmian, "__builtin_spe_evmwsmian", SPE_BUILTIN_EVMWSMIAN },
  { 0, CODE_FOR_spe_evmwssf, "__builtin_spe_evmwssf", SPE_BUILTIN_EVMWSSF },
  { 0, CODE_FOR_spe_evmwssfa, "__builtin_spe_evmwssfa", SPE_BUILTIN_EVMWSSFA },
  { 0, CODE_FOR_spe_evmwssfaa, "__builtin_spe_evmwssfaa", SPE_BUILTIN_EVMWSSFAA },
  { 0, CODE_FOR_spe_evmwssfan, "__builtin_spe_evmwssfan", SPE_BUILTIN_EVMWSSFAN },
  { 0, CODE_FOR_spe_evmwumi, "__builtin_spe_evmwumi", SPE_BUILTIN_EVMWUMI },
  { 0, CODE_FOR_spe_evmwumia, "__builtin_spe_evmwumia", SPE_BUILTIN_EVMWUMIA },
  { 0, CODE_FOR_spe_evmwumiaa, "__builtin_spe_evmwumiaa", SPE_BUILTIN_EVMWUMIAA },
  { 0, CODE_FOR_spe_evmwumian, "__builtin_spe_evmwumian", SPE_BUILTIN_EVMWUMIAN },
  { 0, CODE_FOR_spe_evnand, "__builtin_spe_evnand", SPE_BUILTIN_EVNAND },
  { 0, CODE_FOR_spe_evnor, "__builtin_spe_evnor", SPE_BUILTIN_EVNOR },
  { 0, CODE_FOR_spe_evor, "__builtin_spe_evor", SPE_BUILTIN_EVOR },
  { 0, CODE_FOR_spe_evorc, "__builtin_spe_evorc", SPE_BUILTIN_EVORC },
  { 0, CODE_FOR_spe_evrlw, "__builtin_spe_evrlw", SPE_BUILTIN_EVRLW },
  { 0, CODE_FOR_spe_evslw, "__builtin_spe_evslw", SPE_BUILTIN_EVSLW },
  { 0, CODE_FOR_spe_evsrws, "__builtin_spe_evsrws", SPE_BUILTIN_EVSRWS },
  { 0, CODE_FOR_spe_evsrwu, "__builtin_spe_evsrwu", SPE_BUILTIN_EVSRWU },
  { 0, CODE_FOR_spe_evsubfw, "__builtin_spe_evsubfw", SPE_BUILTIN_EVSUBFW },

  /* SPE binary operations expecting a 5-bit unsigned literal.  */
  { 0, CODE_FOR_spe_evaddiw, "__builtin_spe_evaddiw", SPE_BUILTIN_EVADDIW },

  { 0, CODE_FOR_spe_evrlwi, "__builtin_spe_evrlwi", SPE_BUILTIN_EVRLWI },
  { 0, CODE_FOR_spe_evslwi, "__builtin_spe_evslwi", SPE_BUILTIN_EVSLWI },
  { 0, CODE_FOR_spe_evsrwis, "__builtin_spe_evsrwis", SPE_BUILTIN_EVSRWIS },
  { 0, CODE_FOR_spe_evsrwiu, "__builtin_spe_evsrwiu", SPE_BUILTIN_EVSRWIU },
  { 0, CODE_FOR_spe_evsubifw, "__builtin_spe_evsubifw", SPE_BUILTIN_EVSUBIFW },
  { 0, CODE_FOR_spe_evmwhssfaa, "__builtin_spe_evmwhssfaa", SPE_BUILTIN_EVMWHSSFAA },
  { 0, CODE_FOR_spe_evmwhssmaa, "__builtin_spe_evmwhssmaa", SPE_BUILTIN_EVMWHSSMAA },
  { 0, CODE_FOR_spe_evmwhsmfaa, "__builtin_spe_evmwhsmfaa", SPE_BUILTIN_EVMWHSMFAA },
  { 0, CODE_FOR_spe_evmwhsmiaa, "__builtin_spe_evmwhsmiaa", SPE_BUILTIN_EVMWHSMIAA },
  { 0, CODE_FOR_spe_evmwhusiaa, "__builtin_spe_evmwhusiaa", SPE_BUILTIN_EVMWHUSIAA },
  { 0, CODE_FOR_spe_evmwhumiaa, "__builtin_spe_evmwhumiaa", SPE_BUILTIN_EVMWHUMIAA },
  { 0, CODE_FOR_spe_evmwhssfan, "__builtin_spe_evmwhssfan", SPE_BUILTIN_EVMWHSSFAN },
  { 0, CODE_FOR_spe_evmwhssian, "__builtin_spe_evmwhssian", SPE_BUILTIN_EVMWHSSIAN },
  { 0, CODE_FOR_spe_evmwhsmfan, "__builtin_spe_evmwhsmfan", SPE_BUILTIN_EVMWHSMFAN },
  { 0, CODE_FOR_spe_evmwhsmian, "__builtin_spe_evmwhsmian", SPE_BUILTIN_EVMWHSMIAN },
  { 0, CODE_FOR_spe_evmwhusian, "__builtin_spe_evmwhusian", SPE_BUILTIN_EVMWHUSIAN },
  { 0, CODE_FOR_spe_evmwhumian, "__builtin_spe_evmwhumian", SPE_BUILTIN_EVMWHUMIAN },
  { 0, CODE_FOR_spe_evmwhgssfaa, "__builtin_spe_evmwhgssfaa", SPE_BUILTIN_EVMWHGSSFAA },
  { 0, CODE_FOR_spe_evmwhgsmfaa, "__builtin_spe_evmwhgsmfaa", SPE_BUILTIN_EVMWHGSMFAA },
  { 0, CODE_FOR_spe_evmwhgsmiaa, "__builtin_spe_evmwhgsmiaa", SPE_BUILTIN_EVMWHGSMIAA },
  { 0, CODE_FOR_spe_evmwhgumiaa, "__builtin_spe_evmwhgumiaa", SPE_BUILTIN_EVMWHGUMIAA },
  { 0, CODE_FOR_spe_evmwhgssfan, "__builtin_spe_evmwhgssfan", SPE_BUILTIN_EVMWHGSSFAN },
  { 0, CODE_FOR_spe_evmwhgsmfan, "__builtin_spe_evmwhgsmfan", SPE_BUILTIN_EVMWHGSMFAN },
  { 0, CODE_FOR_spe_evmwhgsmian, "__builtin_spe_evmwhgsmian", SPE_BUILTIN_EVMWHGSMIAN },
  { 0, CODE_FOR_spe_evmwhgumian, "__builtin_spe_evmwhgumian", SPE_BUILTIN_EVMWHGUMIAN },
  { 0, CODE_FOR_spe_brinc, "__builtin_spe_brinc", SPE_BUILTIN_BRINC },

  /* Place-holder.  Leave as last binary SPE builtin.  */
  { 0, CODE_FOR_xorv2si3, "__builtin_spe_evxor", SPE_BUILTIN_EVXOR },
};

/* AltiVec predicates.  */

struct builtin_description_predicates
{
  const unsigned int mask;
  const enum insn_code icode;
  const char *opcode;
  const char *const name;
  const enum rs6000_builtins code;
};

static const struct builtin_description_predicates bdesc_altivec_preds[] =
{
  { MASK_ALTIVEC, CODE_FOR_altivec_predicate_v4sf, "*vcmpbfp.", "__builtin_altivec_vcmpbfp_p", ALTIVEC_BUILTIN_VCMPBFP_P },
  { MASK_ALTIVEC, CODE_FOR_altivec_predicate_v4sf, "*vcmpeqfp.", "__builtin_altivec_vcmpeqfp_p", ALTIVEC_BUILTIN_VCMPEQFP_P },
  { MASK_ALTIVEC, CODE_FOR_altivec_predicate_v4sf, "*vcmpgefp.", "__builtin_altivec_vcmpgefp_p", ALTIVEC_BUILTIN_VCMPGEFP_P },
  { MASK_ALTIVEC, CODE_FOR_altivec_predicate_v4sf, "*vcmpgtfp.", "__builtin_altivec_vcmpgtfp_p", ALTIVEC_BUILTIN_VCMPGTFP_P },
  { MASK_ALTIVEC, CODE_FOR_altivec_predicate_v4si, "*vcmpequw.", "__builtin_altivec_vcmpequw_p", ALTIVEC_BUILTIN_VCMPEQUW_P },
  { MASK_ALTIVEC, CODE_FOR_altivec_predicate_v4si, "*vcmpgtsw.", "__builtin_altivec_vcmpgtsw_p", ALTIVEC_BUILTIN_VCMPGTSW_P },
  { MASK_ALTIVEC, CODE_FOR_altivec_predicate_v4si, "*vcmpgtuw.", "__builtin_altivec_vcmpgtuw_p", ALTIVEC_BUILTIN_VCMPGTUW_P },
  { MASK_ALTIVEC, CODE_FOR_altivec_predicate_v8hi, "*vcmpgtuh.", "__builtin_altivec_vcmpgtuh_p", ALTIVEC_BUILTIN_VCMPGTUH_P },
  { MASK_ALTIVEC, CODE_FOR_altivec_predicate_v8hi, "*vcmpgtsh.", "__builtin_altivec_vcmpgtsh_p", ALTIVEC_BUILTIN_VCMPGTSH_P },
  { MASK_ALTIVEC, CODE_FOR_altivec_predicate_v8hi, "*vcmpequh.", "__builtin_altivec_vcmpequh_p", ALTIVEC_BUILTIN_VCMPEQUH_P },
  { MASK_ALTIVEC, CODE_FOR_altivec_predicate_v16qi, "*vcmpequb.", "__builtin_altivec_vcmpequb_p", ALTIVEC_BUILTIN_VCMPEQUB_P },
  { MASK_ALTIVEC, CODE_FOR_altivec_predicate_v16qi, "*vcmpgtsb.", "__builtin_altivec_vcmpgtsb_p", ALTIVEC_BUILTIN_VCMPGTSB_P },
  { MASK_ALTIVEC, CODE_FOR_altivec_predicate_v16qi, "*vcmpgtub.", "__builtin_altivec_vcmpgtub_p", ALTIVEC_BUILTIN_VCMPGTUB_P }
};

/* SPE predicates.  */
static struct builtin_description bdesc_spe_predicates[] =
{
  /* Place-holder.  Leave as first.  */
  { 0, CODE_FOR_spe_evcmpeq, "__builtin_spe_evcmpeq", SPE_BUILTIN_EVCMPEQ },
  { 0, CODE_FOR_spe_evcmpgts, "__builtin_spe_evcmpgts", SPE_BUILTIN_EVCMPGTS },
  { 0, CODE_FOR_spe_evcmpgtu, "__builtin_spe_evcmpgtu", SPE_BUILTIN_EVCMPGTU },
  { 0, CODE_FOR_spe_evcmplts, "__builtin_spe_evcmplts", SPE_BUILTIN_EVCMPLTS },
  { 0, CODE_FOR_spe_evcmpltu, "__builtin_spe_evcmpltu", SPE_BUILTIN_EVCMPLTU },
  { 0, CODE_FOR_spe_evfscmpeq, "__builtin_spe_evfscmpeq", SPE_BUILTIN_EVFSCMPEQ },
  { 0, CODE_FOR_spe_evfscmpgt, "__builtin_spe_evfscmpgt", SPE_BUILTIN_EVFSCMPGT },
  { 0, CODE_FOR_spe_evfscmplt, "__builtin_spe_evfscmplt", SPE_BUILTIN_EVFSCMPLT },
  { 0, CODE_FOR_spe_evfststeq, "__builtin_spe_evfststeq", SPE_BUILTIN_EVFSTSTEQ },
  { 0, CODE_FOR_spe_evfststgt, "__builtin_spe_evfststgt", SPE_BUILTIN_EVFSTSTGT },
  /* Place-holder.  Leave as last.  */
  { 0, CODE_FOR_spe_evfststlt, "__builtin_spe_evfststlt", SPE_BUILTIN_EVFSTSTLT },
};

/* SPE evsel predicates.  */
static struct builtin_description bdesc_spe_evsel[] =
{
  /* Place-holder.  Leave as first.  */
  { 0, CODE_FOR_spe_evcmpgts, "__builtin_spe_evsel_gts", SPE_BUILTIN_EVSEL_CMPGTS },
  { 0, CODE_FOR_spe_evcmpgtu, "__builtin_spe_evsel_gtu", SPE_BUILTIN_EVSEL_CMPGTU },
  { 0, CODE_FOR_spe_evcmplts, "__builtin_spe_evsel_lts", SPE_BUILTIN_EVSEL_CMPLTS },
  { 0, CODE_FOR_spe_evcmpltu, "__builtin_spe_evsel_ltu", SPE_BUILTIN_EVSEL_CMPLTU },
  { 0, CODE_FOR_spe_evcmpeq, "__builtin_spe_evsel_eq", SPE_BUILTIN_EVSEL_CMPEQ },
  { 0, CODE_FOR_spe_evfscmpgt, "__builtin_spe_evsel_fsgt", SPE_BUILTIN_EVSEL_FSCMPGT },
  { 0, CODE_FOR_spe_evfscmplt, "__builtin_spe_evsel_fslt", SPE_BUILTIN_EVSEL_FSCMPLT },
  { 0, CODE_FOR_spe_evfscmpeq, "__builtin_spe_evsel_fseq", SPE_BUILTIN_EVSEL_FSCMPEQ },
  { 0, CODE_FOR_spe_evfststgt, "__builtin_spe_evsel_fststgt", SPE_BUILTIN_EVSEL_FSTSTGT },
  { 0, CODE_FOR_spe_evfststlt, "__builtin_spe_evsel_fststlt", SPE_BUILTIN_EVSEL_FSTSTLT },
  /* Place-holder.  Leave as last.  */
  { 0, CODE_FOR_spe_evfststeq, "__builtin_spe_evsel_fststeq", SPE_BUILTIN_EVSEL_FSTSTEQ },
};

/* ABS* operations.  */

static const struct builtin_description bdesc_abs[] =
{
  { MASK_ALTIVEC, CODE_FOR_absv4si2, "__builtin_altivec_abs_v4si", ALTIVEC_BUILTIN_ABS_V4SI },
  { MASK_ALTIVEC, CODE_FOR_absv8hi2, "__builtin_altivec_abs_v8hi", ALTIVEC_BUILTIN_ABS_V8HI },
  { MASK_ALTIVEC, CODE_FOR_absv4sf2, "__builtin_altivec_abs_v4sf", ALTIVEC_BUILTIN_ABS_V4SF },
  { MASK_ALTIVEC, CODE_FOR_absv16qi2, "__builtin_altivec_abs_v16qi", ALTIVEC_BUILTIN_ABS_V16QI },
  { MASK_ALTIVEC, CODE_FOR_altivec_abss_v4si, "__builtin_altivec_abss_v4si", ALTIVEC_BUILTIN_ABSS_V4SI },
  { MASK_ALTIVEC, CODE_FOR_altivec_abss_v8hi, "__builtin_altivec_abss_v8hi", ALTIVEC_BUILTIN_ABSS_V8HI },
  { MASK_ALTIVEC, CODE_FOR_altivec_abss_v16qi, "__builtin_altivec_abss_v16qi", ALTIVEC_BUILTIN_ABSS_V16QI }
};

/* Simple unary operations: VECb = foo (unsigned literal) or VECb =
   foo (VECa).  */

static struct builtin_description bdesc_1arg[] =
{
  { MASK_ALTIVEC, CODE_FOR_altivec_vexptefp, "__builtin_altivec_vexptefp", ALTIVEC_BUILTIN_VEXPTEFP },
  { MASK_ALTIVEC, CODE_FOR_altivec_vlogefp, "__builtin_altivec_vlogefp", ALTIVEC_BUILTIN_VLOGEFP },
  { MASK_ALTIVEC, CODE_FOR_altivec_vrefp, "__builtin_altivec_vrefp", ALTIVEC_BUILTIN_VREFP },
  { MASK_ALTIVEC, CODE_FOR_altivec_vrfim, "__builtin_altivec_vrfim", ALTIVEC_BUILTIN_VRFIM },
  { MASK_ALTIVEC, CODE_FOR_altivec_vrfin, "__builtin_altivec_vrfin", ALTIVEC_BUILTIN_VRFIN },
  { MASK_ALTIVEC, CODE_FOR_altivec_vrfip, "__builtin_altivec_vrfip", ALTIVEC_BUILTIN_VRFIP },
  { MASK_ALTIVEC, CODE_FOR_ftruncv4sf2, "__builtin_altivec_vrfiz", ALTIVEC_BUILTIN_VRFIZ },
  { MASK_ALTIVEC, CODE_FOR_altivec_vrsqrtefp, "__builtin_altivec_vrsqrtefp", ALTIVEC_BUILTIN_VRSQRTEFP },
  { MASK_ALTIVEC, CODE_FOR_altivec_vspltisb, "__builtin_altivec_vspltisb", ALTIVEC_BUILTIN_VSPLTISB },
  { MASK_ALTIVEC, CODE_FOR_altivec_vspltish, "__builtin_altivec_vspltish", ALTIVEC_BUILTIN_VSPLTISH },
  { MASK_ALTIVEC, CODE_FOR_altivec_vspltisw, "__builtin_altivec_vspltisw", ALTIVEC_BUILTIN_VSPLTISW },
  { MASK_ALTIVEC, CODE_FOR_altivec_vupkhsb, "__builtin_altivec_vupkhsb", ALTIVEC_BUILTIN_VUPKHSB },
  { MASK_ALTIVEC, CODE_FOR_altivec_vupkhpx, "__builtin_altivec_vupkhpx", ALTIVEC_BUILTIN_VUPKHPX },
  { MASK_ALTIVEC, CODE_FOR_altivec_vupkhsh, "__builtin_altivec_vupkhsh", ALTIVEC_BUILTIN_VUPKHSH },
  { MASK_ALTIVEC, CODE_FOR_altivec_vupklsb, "__builtin_altivec_vupklsb", ALTIVEC_BUILTIN_VUPKLSB },
  { MASK_ALTIVEC, CODE_FOR_altivec_vupklpx, "__builtin_altivec_vupklpx", ALTIVEC_BUILTIN_VUPKLPX },
  { MASK_ALTIVEC, CODE_FOR_altivec_vupklsh, "__builtin_altivec_vupklsh", ALTIVEC_BUILTIN_VUPKLSH },

  /* The SPE unary builtins must start with SPE_BUILTIN_EVABS and
     end with SPE_BUILTIN_EVSUBFUSIAAW.  */
  { 0, CODE_FOR_spe_evabs, "__builtin_spe_evabs", SPE_BUILTIN_EVABS },
  { 0, CODE_FOR_spe_evaddsmiaaw, "__builtin_spe_evaddsmiaaw", SPE_BUILTIN_EVADDSMIAAW },
  { 0, CODE_FOR_spe_evaddssiaaw, "__builtin_spe_evaddssiaaw", SPE_BUILTIN_EVADDSSIAAW },
  { 0, CODE_FOR_spe_evaddumiaaw, "__builtin_spe_evaddumiaaw", SPE_BUILTIN_EVADDUMIAAW },
  { 0, CODE_FOR_spe_evaddusiaaw, "__builtin_spe_evaddusiaaw", SPE_BUILTIN_EVADDUSIAAW },
  { 0, CODE_FOR_spe_evcntlsw, "__builtin_spe_evcntlsw", SPE_BUILTIN_EVCNTLSW },
  { 0, CODE_FOR_spe_evcntlzw, "__builtin_spe_evcntlzw", SPE_BUILTIN_EVCNTLZW },
  { 0, CODE_FOR_spe_evextsb, "__builtin_spe_evextsb", SPE_BUILTIN_EVEXTSB },
  { 0, CODE_FOR_spe_evextsh, "__builtin_spe_evextsh", SPE_BUILTIN_EVEXTSH },
  { 0, CODE_FOR_spe_evfsabs, "__builtin_spe_evfsabs", SPE_BUILTIN_EVFSABS },
  { 0, CODE_FOR_spe_evfscfsf, "__builtin_spe_evfscfsf", SPE_BUILTIN_EVFSCFSF },
  { 0, CODE_FOR_spe_evfscfsi, "__builtin_spe_evfscfsi", SPE_BUILTIN_EVFSCFSI },
  { 0, CODE_FOR_spe_evfscfuf, "__builtin_spe_evfscfuf", SPE_BUILTIN_EVFSCFUF },
  { 0, CODE_FOR_spe_evfscfui, "__builtin_spe_evfscfui", SPE_BUILTIN_EVFSCFUI },
  { 0, CODE_FOR_spe_evfsctsf, "__builtin_spe_evfsctsf", SPE_BUILTIN_EVFSCTSF },
  { 0, CODE_FOR_spe_evfsctsi, "__builtin_spe_evfsctsi", SPE_BUILTIN_EVFSCTSI },
  { 0, CODE_FOR_spe_evfsctsiz, "__builtin_spe_evfsctsiz", SPE_BUILTIN_EVFSCTSIZ },
  { 0, CODE_FOR_spe_evfsctuf, "__builtin_spe_evfsctuf", SPE_BUILTIN_EVFSCTUF },
  { 0, CODE_FOR_spe_evfsctui, "__builtin_spe_evfsctui", SPE_BUILTIN_EVFSCTUI },
  { 0, CODE_FOR_spe_evfsctuiz, "__builtin_spe_evfsctuiz", SPE_BUILTIN_EVFSCTUIZ },
  { 0, CODE_FOR_spe_evfsnabs, "__builtin_spe_evfsnabs", SPE_BUILTIN_EVFSNABS },
  { 0, CODE_FOR_spe_evfsneg, "__builtin_spe_evfsneg", SPE_BUILTIN_EVFSNEG },
  { 0, CODE_FOR_spe_evmra, "__builtin_spe_evmra", SPE_BUILTIN_EVMRA },
  { 0, CODE_FOR_negv2si2, "__builtin_spe_evneg", SPE_BUILTIN_EVNEG },
  { 0, CODE_FOR_spe_evrndw, "__builtin_spe_evrndw", SPE_BUILTIN_EVRNDW },
  { 0, CODE_FOR_spe_evsubfsmiaaw, "__builtin_spe_evsubfsmiaaw", SPE_BUILTIN_EVSUBFSMIAAW },
  { 0, CODE_FOR_spe_evsubfssiaaw, "__builtin_spe_evsubfssiaaw", SPE_BUILTIN_EVSUBFSSIAAW },
  { 0, CODE_FOR_spe_evsubfumiaaw, "__builtin_spe_evsubfumiaaw", SPE_BUILTIN_EVSUBFUMIAAW },

  /* Place-holder.  Leave as last unary SPE builtin.  */
  { 0, CODE_FOR_spe_evsubfusiaaw, "__builtin_spe_evsubfusiaaw", SPE_BUILTIN_EVSUBFUSIAAW },
};

static rtx
rs6000_expand_unop_builtin (enum insn_code icode, tree arglist, rtx target)
{
  rtx pat;
  tree arg0 = TREE_VALUE (arglist);
  rtx op0 = expand_expr (arg0, NULL_RTX, VOIDmode, 0);
  enum machine_mode tmode = insn_data[icode].operand[0].mode;
  enum machine_mode mode0 = insn_data[icode].operand[1].mode;

  if (icode == CODE_FOR_nothing)
    /* Builtin not supported on this processor.  */
    return 0;

  /* If we got invalid arguments bail out before generating bad rtl.  */
  if (arg0 == error_mark_node)
    return const0_rtx;

  if (icode == CODE_FOR_altivec_vspltisb
      || icode == CODE_FOR_altivec_vspltish
      || icode == CODE_FOR_altivec_vspltisw
      || icode == CODE_FOR_spe_evsplatfi
      || icode == CODE_FOR_spe_evsplati)
    {
      /* Only allow 5-bit *signed* literals.  */
      if (GET_CODE (op0) != CONST_INT
	  || INTVAL (op0) > 0x1f
	  || INTVAL (op0) < -0x1f)
	{
	  error ("argument 1 must be a 5-bit signed literal");
	  return const0_rtx;
	}
    }

  if (target == 0
      || GET_MODE (target) != tmode
      || ! (*insn_data[icode].operand[0].predicate) (target, tmode))
    target = gen_reg_rtx (tmode);

  if (! (*insn_data[icode].operand[1].predicate) (op0, mode0))
    op0 = copy_to_mode_reg (mode0, op0);

  pat = GEN_FCN (icode) (target, op0);
  if (! pat)
    return 0;
  emit_insn (pat);

  return target;
}

static rtx
altivec_expand_abs_builtin (enum insn_code icode, tree arglist, rtx target)
{
  rtx pat, scratch1, scratch2;
  tree arg0 = TREE_VALUE (arglist);
  rtx op0 = expand_expr (arg0, NULL_RTX, VOIDmode, 0);
  enum machine_mode tmode = insn_data[icode].operand[0].mode;
  enum machine_mode mode0 = insn_data[icode].operand[1].mode;

  /* If we have invalid arguments, bail out before generating bad rtl.  */
  if (arg0 == error_mark_node)
    return const0_rtx;

  if (target == 0
      || GET_MODE (target) != tmode
      || ! (*insn_data[icode].operand[0].predicate) (target, tmode))
    target = gen_reg_rtx (tmode);

  if (! (*insn_data[icode].operand[1].predicate) (op0, mode0))
    op0 = copy_to_mode_reg (mode0, op0);

  scratch1 = gen_reg_rtx (mode0);
  scratch2 = gen_reg_rtx (mode0);

  pat = GEN_FCN (icode) (target, op0, scratch1, scratch2);
  if (! pat)
    return 0;
  emit_insn (pat);

  return target;
}

static rtx
rs6000_expand_binop_builtin (enum insn_code icode, tree arglist, rtx target)
{
  rtx pat;
  tree arg0 = TREE_VALUE (arglist);
  tree arg1 = TREE_VALUE (TREE_CHAIN (arglist));
  rtx op0 = expand_expr (arg0, NULL_RTX, VOIDmode, 0);
  rtx op1 = expand_expr (arg1, NULL_RTX, VOIDmode, 0);
  enum machine_mode tmode = insn_data[icode].operand[0].mode;
  enum machine_mode mode0 = insn_data[icode].operand[1].mode;
  enum machine_mode mode1 = insn_data[icode].operand[2].mode;

  if (icode == CODE_FOR_nothing)
    /* Builtin not supported on this processor.  */
    return 0;

  /* If we got invalid arguments bail out before generating bad rtl.  */
  if (arg0 == error_mark_node || arg1 == error_mark_node)
    return const0_rtx;

  if (icode == CODE_FOR_altivec_vcfux
      || icode == CODE_FOR_altivec_vcfsx
      || icode == CODE_FOR_altivec_vctsxs
      || icode == CODE_FOR_altivec_vctuxs
      || icode == CODE_FOR_altivec_vspltb
      || icode == CODE_FOR_altivec_vsplth
      || icode == CODE_FOR_altivec_vspltw
      || icode == CODE_FOR_spe_evaddiw
      || icode == CODE_FOR_spe_evldd
      || icode == CODE_FOR_spe_evldh
      || icode == CODE_FOR_spe_evldw
      || icode == CODE_FOR_spe_evlhhesplat
      || icode == CODE_FOR_spe_evlhhossplat
      || icode == CODE_FOR_spe_evlhhousplat
      || icode == CODE_FOR_spe_evlwhe
      || icode == CODE_FOR_spe_evlwhos
      || icode == CODE_FOR_spe_evlwhou
      || icode == CODE_FOR_spe_evlwhsplat
      || icode == CODE_FOR_spe_evlwwsplat
      || icode == CODE_FOR_spe_evrlwi
      || icode == CODE_FOR_spe_evslwi
      || icode == CODE_FOR_spe_evsrwis
      || icode == CODE_FOR_spe_evsubifw
      || icode == CODE_FOR_spe_evsrwiu)
    {
      /* Only allow 5-bit unsigned literals.  */
      STRIP_NOPS (arg1);
      if (TREE_CODE (arg1) != INTEGER_CST
	  || TREE_INT_CST_LOW (arg1) & ~0x1f)
	{
	  error ("argument 2 must be a 5-bit unsigned literal");
	  return const0_rtx;
	}
    }

  if (target == 0
      || GET_MODE (target) != tmode
      || ! (*insn_data[icode].operand[0].predicate) (target, tmode))
    target = gen_reg_rtx (tmode);

  if (! (*insn_data[icode].operand[1].predicate) (op0, mode0))
    op0 = copy_to_mode_reg (mode0, op0);
  if (! (*insn_data[icode].operand[2].predicate) (op1, mode1))
    op1 = copy_to_mode_reg (mode1, op1);

  pat = GEN_FCN (icode) (target, op0, op1);
  if (! pat)
    return 0;
  emit_insn (pat);

  return target;
}

static rtx
altivec_expand_predicate_builtin (enum insn_code icode, const char *opcode, 
				  tree arglist, rtx target)
{
  rtx pat, scratch;
  tree cr6_form = TREE_VALUE (arglist);
  tree arg0 = TREE_VALUE (TREE_CHAIN (arglist));
  tree arg1 = TREE_VALUE (TREE_CHAIN (TREE_CHAIN (arglist)));
  rtx op0 = expand_expr (arg0, NULL_RTX, VOIDmode, 0);
  rtx op1 = expand_expr (arg1, NULL_RTX, VOIDmode, 0);
  enum machine_mode tmode = SImode;
  enum machine_mode mode0 = insn_data[icode].operand[1].mode;
  enum machine_mode mode1 = insn_data[icode].operand[2].mode;
  int cr6_form_int;

  if (TREE_CODE (cr6_form) != INTEGER_CST)
    {
      error ("argument 1 of __builtin_altivec_predicate must be a constant");
      return const0_rtx;
    }
  else
    cr6_form_int = TREE_INT_CST_LOW (cr6_form);

  if (mode0 != mode1)
    abort ();

  /* If we have invalid arguments, bail out before generating bad rtl.  */
  if (arg0 == error_mark_node || arg1 == error_mark_node)
    return const0_rtx;

  if (target == 0
      || GET_MODE (target) != tmode
      || ! (*insn_data[icode].operand[0].predicate) (target, tmode))
    target = gen_reg_rtx (tmode);

  if (! (*insn_data[icode].operand[1].predicate) (op0, mode0))
    op0 = copy_to_mode_reg (mode0, op0);
  if (! (*insn_data[icode].operand[2].predicate) (op1, mode1))
    op1 = copy_to_mode_reg (mode1, op1);

  scratch = gen_reg_rtx (mode0);

  pat = GEN_FCN (icode) (scratch, op0, op1,
			 gen_rtx (SYMBOL_REF, Pmode, opcode));
  if (! pat)
    return 0;
  emit_insn (pat);

  /* The vec_any* and vec_all* predicates use the same opcodes for two
     different operations, but the bits in CR6 will be different
     depending on what information we want.  So we have to play tricks
     with CR6 to get the right bits out.

     If you think this is disgusting, look at the specs for the
     AltiVec predicates.  */

     switch (cr6_form_int)
       {
       case 0:
	 emit_insn (gen_cr6_test_for_zero (target));
	 break;
       case 1:
	 emit_insn (gen_cr6_test_for_zero_reverse (target));
	 break;
       case 2:
	 emit_insn (gen_cr6_test_for_lt (target));
	 break;
       case 3:
	 emit_insn (gen_cr6_test_for_lt_reverse (target));
	 break;
       default:
	 error ("argument 1 of __builtin_altivec_predicate is out of range");
	 break;
       }

  return target;
}

static rtx
altivec_expand_lv_builtin (enum insn_code icode, tree arglist, rtx target)
{
  rtx pat, addr;
  tree arg0 = TREE_VALUE (arglist);
  tree arg1 = TREE_VALUE (TREE_CHAIN (arglist));
  enum machine_mode tmode = insn_data[icode].operand[0].mode;
  enum machine_mode mode0 = Pmode;
  enum machine_mode mode1 = Pmode;
  rtx op0 = expand_expr (arg0, NULL_RTX, VOIDmode, 0);
  rtx op1 = expand_expr (arg1, NULL_RTX, VOIDmode, 0);

  if (icode == CODE_FOR_nothing)
    /* Builtin not supported on this processor.  */
    return 0;

  /* If we got invalid arguments bail out before generating bad rtl.  */
  if (arg0 == error_mark_node || arg1 == error_mark_node)
    return const0_rtx;

  if (target == 0
      || GET_MODE (target) != tmode
      || ! (*insn_data[icode].operand[0].predicate) (target, tmode))
    target = gen_reg_rtx (tmode);

  op1 = copy_to_mode_reg (mode1, op1); 

  if (op0 == const0_rtx)
    {
      addr = gen_rtx_MEM (tmode, op1);
    }
  else
    {
      op0 = copy_to_mode_reg (mode0, op0);
      addr = gen_rtx_MEM (tmode, gen_rtx_PLUS (Pmode, op0, op1));
    }

  pat = GEN_FCN (icode) (target, addr);

  if (! pat)
    return 0;
  emit_insn (pat);

  return target;
}

static rtx
spe_expand_stv_builtin (enum insn_code icode, tree arglist)
{
  tree arg0 = TREE_VALUE (arglist);
  tree arg1 = TREE_VALUE (TREE_CHAIN (arglist));
  tree arg2 = TREE_VALUE (TREE_CHAIN (TREE_CHAIN (arglist)));
  rtx op0 = expand_expr (arg0, NULL_RTX, VOIDmode, 0);
  rtx op1 = expand_expr (arg1, NULL_RTX, VOIDmode, 0);
  rtx op2 = expand_expr (arg2, NULL_RTX, VOIDmode, 0);
  rtx pat;
  enum machine_mode mode0 = insn_data[icode].operand[0].mode;
  enum machine_mode mode1 = insn_data[icode].operand[1].mode;
  enum machine_mode mode2 = insn_data[icode].operand[2].mode;

  /* Invalid arguments.  Bail before doing anything stoopid!  */
  if (arg0 == error_mark_node
      || arg1 == error_mark_node
      || arg2 == error_mark_node)
    return const0_rtx;

  if (! (*insn_data[icode].operand[2].predicate) (op0, mode2))
    op0 = copy_to_mode_reg (mode2, op0);
  if (! (*insn_data[icode].operand[0].predicate) (op1, mode0))
    op1 = copy_to_mode_reg (mode0, op1);
  if (! (*insn_data[icode].operand[1].predicate) (op2, mode1))
    op2 = copy_to_mode_reg (mode1, op2);

  pat = GEN_FCN (icode) (op1, op2, op0);
  if (pat)
    emit_insn (pat);
  return NULL_RTX;
}

static rtx
altivec_expand_stv_builtin (enum insn_code icode, tree arglist)
{
  tree arg0 = TREE_VALUE (arglist);
  tree arg1 = TREE_VALUE (TREE_CHAIN (arglist));
  tree arg2 = TREE_VALUE (TREE_CHAIN (TREE_CHAIN (arglist)));
  rtx op0 = expand_expr (arg0, NULL_RTX, VOIDmode, 0);
  rtx op1 = expand_expr (arg1, NULL_RTX, VOIDmode, 0);
  rtx op2 = expand_expr (arg2, NULL_RTX, VOIDmode, 0);
  rtx pat, addr;
  enum machine_mode tmode = insn_data[icode].operand[0].mode;
  enum machine_mode mode1 = Pmode;
  enum machine_mode mode2 = Pmode;

  /* Invalid arguments.  Bail before doing anything stoopid!  */
  if (arg0 == error_mark_node
      || arg1 == error_mark_node
      || arg2 == error_mark_node)
    return const0_rtx;

  if (! (*insn_data[icode].operand[1].predicate) (op0, tmode))
    op0 = copy_to_mode_reg (tmode, op0);

  op2 = copy_to_mode_reg (mode2, op2); 

  if (op1 == const0_rtx)
    {
      addr = gen_rtx_MEM (tmode, op2);
    }
  else
    {
      op1 = copy_to_mode_reg (mode1, op1);
      addr = gen_rtx_MEM (tmode, gen_rtx_PLUS (Pmode, op1, op2));
    }

  pat = GEN_FCN (icode) (addr, op0);
  if (pat)
    emit_insn (pat);
  return NULL_RTX;
}

static rtx
rs6000_expand_ternop_builtin (enum insn_code icode, tree arglist, rtx target)
{
  rtx pat;
  tree arg0 = TREE_VALUE (arglist);
  tree arg1 = TREE_VALUE (TREE_CHAIN (arglist));
  tree arg2 = TREE_VALUE (TREE_CHAIN (TREE_CHAIN (arglist)));
  rtx op0 = expand_expr (arg0, NULL_RTX, VOIDmode, 0);
  rtx op1 = expand_expr (arg1, NULL_RTX, VOIDmode, 0);
  rtx op2 = expand_expr (arg2, NULL_RTX, VOIDmode, 0);
  enum machine_mode tmode = insn_data[icode].operand[0].mode;
  enum machine_mode mode0 = insn_data[icode].operand[1].mode;
  enum machine_mode mode1 = insn_data[icode].operand[2].mode;
  enum machine_mode mode2 = insn_data[icode].operand[3].mode;

  if (icode == CODE_FOR_nothing)
    /* Builtin not supported on this processor.  */
    return 0;

  /* If we got invalid arguments bail out before generating bad rtl.  */
  if (arg0 == error_mark_node
      || arg1 == error_mark_node
      || arg2 == error_mark_node)
    return const0_rtx;

  if (icode == CODE_FOR_altivec_vsldoi_4sf
      || icode == CODE_FOR_altivec_vsldoi_4si
      || icode == CODE_FOR_altivec_vsldoi_8hi
      || icode == CODE_FOR_altivec_vsldoi_16qi)
    {
      /* Only allow 4-bit unsigned literals.  */
      if (TREE_CODE (arg2) != INTEGER_CST
	  || TREE_INT_CST_LOW (arg2) & ~0xf)
	{
	  error ("argument 3 must be a 4-bit unsigned literal");
	  return const0_rtx;
	}
    }

  if (target == 0
      || GET_MODE (target) != tmode
      || ! (*insn_data[icode].operand[0].predicate) (target, tmode))
    target = gen_reg_rtx (tmode);

  if (! (*insn_data[icode].operand[1].predicate) (op0, mode0))
    op0 = copy_to_mode_reg (mode0, op0);
  if (! (*insn_data[icode].operand[2].predicate) (op1, mode1))
    op1 = copy_to_mode_reg (mode1, op1);
  if (! (*insn_data[icode].operand[3].predicate) (op2, mode2))
    op2 = copy_to_mode_reg (mode2, op2);

  pat = GEN_FCN (icode) (target, op0, op1, op2);
  if (! pat)
    return 0;
  emit_insn (pat);

  return target;
}

/* Expand the lvx builtins.  */
static rtx
altivec_expand_ld_builtin (tree exp, rtx target, bool *expandedp)
{
  tree fndecl = TREE_OPERAND (TREE_OPERAND (exp, 0), 0);
  tree arglist = TREE_OPERAND (exp, 1);
  unsigned int fcode = DECL_FUNCTION_CODE (fndecl);
  tree arg0;
  enum machine_mode tmode, mode0;
  rtx pat, op0;
  enum insn_code icode;

  switch (fcode)
    {
    case ALTIVEC_BUILTIN_LD_INTERNAL_16qi:
      icode = CODE_FOR_altivec_lvx_16qi;
      break;
    case ALTIVEC_BUILTIN_LD_INTERNAL_8hi:
      icode = CODE_FOR_altivec_lvx_8hi;
      break;
    case ALTIVEC_BUILTIN_LD_INTERNAL_4si:
      icode = CODE_FOR_altivec_lvx_4si;
      break;
    case ALTIVEC_BUILTIN_LD_INTERNAL_4sf:
      icode = CODE_FOR_altivec_lvx_4sf;
      break;
    default:
      *expandedp = false;
      return NULL_RTX;
    }

  *expandedp = true;

  arg0 = TREE_VALUE (arglist);
  op0 = expand_expr (arg0, NULL_RTX, VOIDmode, 0);
  tmode = insn_data[icode].operand[0].mode;
  mode0 = insn_data[icode].operand[1].mode;

  if (target == 0
      || GET_MODE (target) != tmode
      || ! (*insn_data[icode].operand[0].predicate) (target, tmode))
    target = gen_reg_rtx (tmode);

  if (! (*insn_data[icode].operand[1].predicate) (op0, mode0))
    op0 = gen_rtx_MEM (mode0, copy_to_mode_reg (Pmode, op0));

  pat = GEN_FCN (icode) (target, op0);
  if (! pat)
    return 0;
  emit_insn (pat);
  return target;
}

/* Expand the stvx builtins.  */
static rtx
altivec_expand_st_builtin (tree exp, rtx target ATTRIBUTE_UNUSED, 
			   bool *expandedp)
{
  tree fndecl = TREE_OPERAND (TREE_OPERAND (exp, 0), 0);
  tree arglist = TREE_OPERAND (exp, 1);
  unsigned int fcode = DECL_FUNCTION_CODE (fndecl);
  tree arg0, arg1;
  enum machine_mode mode0, mode1;
  rtx pat, op0, op1;
  enum insn_code icode;

  switch (fcode)
    {
    case ALTIVEC_BUILTIN_ST_INTERNAL_16qi:
      icode = CODE_FOR_altivec_stvx_16qi;
      break;
    case ALTIVEC_BUILTIN_ST_INTERNAL_8hi:
      icode = CODE_FOR_altivec_stvx_8hi;
      break;
    case ALTIVEC_BUILTIN_ST_INTERNAL_4si:
      icode = CODE_FOR_altivec_stvx_4si;
      break;
    case ALTIVEC_BUILTIN_ST_INTERNAL_4sf:
      icode = CODE_FOR_altivec_stvx_4sf;
      break;
    default:
      *expandedp = false;
      return NULL_RTX;
    }

  arg0 = TREE_VALUE (arglist);
  arg1 = TREE_VALUE (TREE_CHAIN (arglist));
  op0 = expand_expr (arg0, NULL_RTX, VOIDmode, 0);
  op1 = expand_expr (arg1, NULL_RTX, VOIDmode, 0);
  mode0 = insn_data[icode].operand[0].mode;
  mode1 = insn_data[icode].operand[1].mode;

  if (! (*insn_data[icode].operand[0].predicate) (op0, mode0))
    op0 = gen_rtx_MEM (mode0, copy_to_mode_reg (Pmode, op0));
  if (! (*insn_data[icode].operand[1].predicate) (op1, mode1))
    op1 = copy_to_mode_reg (mode1, op1);

  pat = GEN_FCN (icode) (op0, op1);
  if (pat)
    emit_insn (pat);

  *expandedp = true;
  return NULL_RTX;
}

/* Expand the dst builtins.  */
static rtx
altivec_expand_dst_builtin (tree exp, rtx target ATTRIBUTE_UNUSED, 
			    bool *expandedp)
{
  tree fndecl = TREE_OPERAND (TREE_OPERAND (exp, 0), 0);
  tree arglist = TREE_OPERAND (exp, 1);
  unsigned int fcode = DECL_FUNCTION_CODE (fndecl);
  tree arg0, arg1, arg2;
  enum machine_mode mode0, mode1, mode2;
  rtx pat, op0, op1, op2;
  struct builtin_description *d;
  size_t i;

  *expandedp = false;

  /* Handle DST variants.  */
  d = (struct builtin_description *) bdesc_dst;
  for (i = 0; i < ARRAY_SIZE (bdesc_dst); i++, d++)
    if (d->code == fcode)
      {
	arg0 = TREE_VALUE (arglist);
	arg1 = TREE_VALUE (TREE_CHAIN (arglist));
	arg2 = TREE_VALUE (TREE_CHAIN (TREE_CHAIN (arglist)));
	op0 = expand_expr (arg0, NULL_RTX, VOIDmode, 0);
	op1 = expand_expr (arg1, NULL_RTX, VOIDmode, 0);
	op2 = expand_expr (arg2, NULL_RTX, VOIDmode, 0);
	mode0 = insn_data[d->icode].operand[0].mode;
	mode1 = insn_data[d->icode].operand[1].mode;
	mode2 = insn_data[d->icode].operand[2].mode;

	/* Invalid arguments, bail out before generating bad rtl.  */
	if (arg0 == error_mark_node
	    || arg1 == error_mark_node
	    || arg2 == error_mark_node)
	  return const0_rtx;

	*expandedp = true;
	STRIP_NOPS (arg2);
	if (TREE_CODE (arg2) != INTEGER_CST
	    || TREE_INT_CST_LOW (arg2) & ~0x3)
	  {
	    error ("argument to `%s' must be a 2-bit unsigned literal", d->name);
	    return const0_rtx;
	  }

	if (! (*insn_data[d->icode].operand[0].predicate) (op0, mode0))
	  op0 = gen_rtx_MEM (mode0, copy_to_mode_reg (Pmode, op0));
	if (! (*insn_data[d->icode].operand[1].predicate) (op1, mode1))
	  op1 = copy_to_mode_reg (mode1, op1);

	pat = GEN_FCN (d->icode) (op0, op1, op2);
	if (pat != 0)
	  emit_insn (pat);

	return NULL_RTX;
      }

  return NULL_RTX;
}

/* Expand the builtin in EXP and store the result in TARGET.  Store
   true in *EXPANDEDP if we found a builtin to expand.  */
static rtx
altivec_expand_builtin (tree exp, rtx target, bool *expandedp)
{
  struct builtin_description *d;
  struct builtin_description_predicates *dp;
  size_t i;
  enum insn_code icode;
  tree fndecl = TREE_OPERAND (TREE_OPERAND (exp, 0), 0);
  tree arglist = TREE_OPERAND (exp, 1);
  tree arg0;
  rtx op0, pat;
  enum machine_mode tmode, mode0;
  unsigned int fcode = DECL_FUNCTION_CODE (fndecl);

  target = altivec_expand_ld_builtin (exp, target, expandedp);
  if (*expandedp)
    return target;

  target = altivec_expand_st_builtin (exp, target, expandedp);
  if (*expandedp)
    return target;

  target = altivec_expand_dst_builtin (exp, target, expandedp);
  if (*expandedp)
    return target;

  *expandedp = true;

  switch (fcode)
    {
    case ALTIVEC_BUILTIN_STVX:
      return altivec_expand_stv_builtin (CODE_FOR_altivec_stvx, arglist);
    case ALTIVEC_BUILTIN_STVEBX:
      return altivec_expand_stv_builtin (CODE_FOR_altivec_stvebx, arglist);
    case ALTIVEC_BUILTIN_STVEHX:
      return altivec_expand_stv_builtin (CODE_FOR_altivec_stvehx, arglist);
    case ALTIVEC_BUILTIN_STVEWX:
      return altivec_expand_stv_builtin (CODE_FOR_altivec_stvewx, arglist);
    case ALTIVEC_BUILTIN_STVXL:
      return altivec_expand_stv_builtin (CODE_FOR_altivec_stvxl, arglist);

    case ALTIVEC_BUILTIN_MFVSCR:
      icode = CODE_FOR_altivec_mfvscr;
      tmode = insn_data[icode].operand[0].mode;

      if (target == 0
	  || GET_MODE (target) != tmode
	  || ! (*insn_data[icode].operand[0].predicate) (target, tmode))
	target = gen_reg_rtx (tmode);
      
      pat = GEN_FCN (icode) (target);
      if (! pat)
	return 0;
      emit_insn (pat);
      return target;

    case ALTIVEC_BUILTIN_MTVSCR:
      icode = CODE_FOR_altivec_mtvscr;
      arg0 = TREE_VALUE (arglist);
      op0 = expand_expr (arg0, NULL_RTX, VOIDmode, 0);
      mode0 = insn_data[icode].operand[0].mode;

      /* If we got invalid arguments bail out before generating bad rtl.  */
      if (arg0 == error_mark_node)
	return const0_rtx;

      if (! (*insn_data[icode].operand[0].predicate) (op0, mode0))
	op0 = copy_to_mode_reg (mode0, op0);

      pat = GEN_FCN (icode) (op0);
      if (pat)
	emit_insn (pat);
      return NULL_RTX;

    case ALTIVEC_BUILTIN_DSSALL:
      emit_insn (gen_altivec_dssall ());
      return NULL_RTX;

    case ALTIVEC_BUILTIN_DSS:
      icode = CODE_FOR_altivec_dss;
      arg0 = TREE_VALUE (arglist);
      STRIP_NOPS (arg0);
      op0 = expand_expr (arg0, NULL_RTX, VOIDmode, 0);
      mode0 = insn_data[icode].operand[0].mode;

      /* If we got invalid arguments bail out before generating bad rtl.  */
      if (arg0 == error_mark_node)
	return const0_rtx;

      if (TREE_CODE (arg0) != INTEGER_CST
	  || TREE_INT_CST_LOW (arg0) & ~0x3)
	{
	  error ("argument to dss must be a 2-bit unsigned literal");
	  return const0_rtx;
	}

      if (! (*insn_data[icode].operand[0].predicate) (op0, mode0))
	op0 = copy_to_mode_reg (mode0, op0);

      emit_insn (gen_altivec_dss (op0));
      return NULL_RTX;

    case ALTIVEC_BUILTIN_COMPILETIME_ERROR:
      arg0 = TREE_VALUE (arglist);
      while (TREE_CODE (arg0) == NOP_EXPR || TREE_CODE (arg0) == ADDR_EXPR)
	arg0 = TREE_OPERAND (arg0, 0);
      error ("invalid parameter combination for `%s' AltiVec intrinsic",
	     TREE_STRING_POINTER (arg0));

      return const0_rtx;
    }

  /* Expand abs* operations.  */
  d = (struct builtin_description *) bdesc_abs;
  for (i = 0; i < ARRAY_SIZE (bdesc_abs); i++, d++)
    if (d->code == fcode)
      return altivec_expand_abs_builtin (d->icode, arglist, target);

  /* Expand the AltiVec predicates.  */
  dp = (struct builtin_description_predicates *) bdesc_altivec_preds;
  for (i = 0; i < ARRAY_SIZE (bdesc_altivec_preds); i++, dp++)
    if (dp->code == fcode)
      return altivec_expand_predicate_builtin (dp->icode, dp->opcode, arglist, target);

  /* LV* are funky.  We initialized them differently.  */
  switch (fcode)
    {
    case ALTIVEC_BUILTIN_LVSL:
      return altivec_expand_lv_builtin (CODE_FOR_altivec_lvsl,
					   arglist, target);
    case ALTIVEC_BUILTIN_LVSR:
      return altivec_expand_lv_builtin (CODE_FOR_altivec_lvsr,
					  arglist, target);
    case ALTIVEC_BUILTIN_LVEBX:
      return altivec_expand_lv_builtin (CODE_FOR_altivec_lvebx,
					  arglist, target);
    case ALTIVEC_BUILTIN_LVEHX:
      return altivec_expand_lv_builtin (CODE_FOR_altivec_lvehx,
					  arglist, target);
    case ALTIVEC_BUILTIN_LVEWX:
      return altivec_expand_lv_builtin (CODE_FOR_altivec_lvewx,
					  arglist, target);
    case ALTIVEC_BUILTIN_LVXL:
      return altivec_expand_lv_builtin (CODE_FOR_altivec_lvxl,
					  arglist, target);
    case ALTIVEC_BUILTIN_LVX:
      return altivec_expand_lv_builtin (CODE_FOR_altivec_lvx,
					  arglist, target);
    default:
      break;
      /* Fall through.  */
    }

  *expandedp = false;
  return NULL_RTX;
}

/* Binops that need to be initialized manually, but can be expanded
   automagically by rs6000_expand_binop_builtin.  */
static struct builtin_description bdesc_2arg_spe[] =
{
  { 0, CODE_FOR_spe_evlddx, "__builtin_spe_evlddx", SPE_BUILTIN_EVLDDX },
  { 0, CODE_FOR_spe_evldwx, "__builtin_spe_evldwx", SPE_BUILTIN_EVLDWX },
  { 0, CODE_FOR_spe_evldhx, "__builtin_spe_evldhx", SPE_BUILTIN_EVLDHX },
  { 0, CODE_FOR_spe_evlwhex, "__builtin_spe_evlwhex", SPE_BUILTIN_EVLWHEX },
  { 0, CODE_FOR_spe_evlwhoux, "__builtin_spe_evlwhoux", SPE_BUILTIN_EVLWHOUX },
  { 0, CODE_FOR_spe_evlwhosx, "__builtin_spe_evlwhosx", SPE_BUILTIN_EVLWHOSX },
  { 0, CODE_FOR_spe_evlwwsplatx, "__builtin_spe_evlwwsplatx", SPE_BUILTIN_EVLWWSPLATX },
  { 0, CODE_FOR_spe_evlwhsplatx, "__builtin_spe_evlwhsplatx", SPE_BUILTIN_EVLWHSPLATX },
  { 0, CODE_FOR_spe_evlhhesplatx, "__builtin_spe_evlhhesplatx", SPE_BUILTIN_EVLHHESPLATX },
  { 0, CODE_FOR_spe_evlhhousplatx, "__builtin_spe_evlhhousplatx", SPE_BUILTIN_EVLHHOUSPLATX },
  { 0, CODE_FOR_spe_evlhhossplatx, "__builtin_spe_evlhhossplatx", SPE_BUILTIN_EVLHHOSSPLATX },
  { 0, CODE_FOR_spe_evldd, "__builtin_spe_evldd", SPE_BUILTIN_EVLDD },
  { 0, CODE_FOR_spe_evldw, "__builtin_spe_evldw", SPE_BUILTIN_EVLDW },
  { 0, CODE_FOR_spe_evldh, "__builtin_spe_evldh", SPE_BUILTIN_EVLDH },
  { 0, CODE_FOR_spe_evlwhe, "__builtin_spe_evlwhe", SPE_BUILTIN_EVLWHE },
  { 0, CODE_FOR_spe_evlwhou, "__builtin_spe_evlwhou", SPE_BUILTIN_EVLWHOU },
  { 0, CODE_FOR_spe_evlwhos, "__builtin_spe_evlwhos", SPE_BUILTIN_EVLWHOS },
  { 0, CODE_FOR_spe_evlwwsplat, "__builtin_spe_evlwwsplat", SPE_BUILTIN_EVLWWSPLAT },
  { 0, CODE_FOR_spe_evlwhsplat, "__builtin_spe_evlwhsplat", SPE_BUILTIN_EVLWHSPLAT },
  { 0, CODE_FOR_spe_evlhhesplat, "__builtin_spe_evlhhesplat", SPE_BUILTIN_EVLHHESPLAT },
  { 0, CODE_FOR_spe_evlhhousplat, "__builtin_spe_evlhhousplat", SPE_BUILTIN_EVLHHOUSPLAT },
  { 0, CODE_FOR_spe_evlhhossplat, "__builtin_spe_evlhhossplat", SPE_BUILTIN_EVLHHOSSPLAT }
};

/* Expand the builtin in EXP and store the result in TARGET.  Store
   true in *EXPANDEDP if we found a builtin to expand.

   This expands the SPE builtins that are not simple unary and binary
   operations.  */
static rtx
spe_expand_builtin (tree exp, rtx target, bool *expandedp)
{
  tree fndecl = TREE_OPERAND (TREE_OPERAND (exp, 0), 0);
  tree arglist = TREE_OPERAND (exp, 1);
  tree arg1, arg0;
  unsigned int fcode = DECL_FUNCTION_CODE (fndecl);
  enum insn_code icode;
  enum machine_mode tmode, mode0;
  rtx pat, op0;
  struct builtin_description *d;
  size_t i;

  *expandedp = true;

  /* Syntax check for a 5-bit unsigned immediate.  */
  switch (fcode)
    {
    case SPE_BUILTIN_EVSTDD:
    case SPE_BUILTIN_EVSTDH:
    case SPE_BUILTIN_EVSTDW:
    case SPE_BUILTIN_EVSTWHE:
    case SPE_BUILTIN_EVSTWHO:
    case SPE_BUILTIN_EVSTWWE:
    case SPE_BUILTIN_EVSTWWO:
      arg1 = TREE_VALUE (TREE_CHAIN (TREE_CHAIN (arglist)));
      if (TREE_CODE (arg1) != INTEGER_CST
	  || TREE_INT_CST_LOW (arg1) & ~0x1f)
	{
	  error ("argument 2 must be a 5-bit unsigned literal");
	  return const0_rtx;
	}
      break;
    default:
      break;
    }

  /* The evsplat*i instructions are not quite generic.  */
  switch (fcode)
    {
    case SPE_BUILTIN_EVSPLATFI:
      return rs6000_expand_unop_builtin (CODE_FOR_spe_evsplatfi,
					 arglist, target);
    case SPE_BUILTIN_EVSPLATI:
      return rs6000_expand_unop_builtin (CODE_FOR_spe_evsplati,
					 arglist, target);
    default:
      break;
    }

  d = (struct builtin_description *) bdesc_2arg_spe;
  for (i = 0; i < ARRAY_SIZE (bdesc_2arg_spe); ++i, ++d)
    if (d->code == fcode)
      return rs6000_expand_binop_builtin (d->icode, arglist, target);

  d = (struct builtin_description *) bdesc_spe_predicates;
  for (i = 0; i < ARRAY_SIZE (bdesc_spe_predicates); ++i, ++d)
    if (d->code == fcode)
      return spe_expand_predicate_builtin (d->icode, arglist, target);

  d = (struct builtin_description *) bdesc_spe_evsel;
  for (i = 0; i < ARRAY_SIZE (bdesc_spe_evsel); ++i, ++d)
    if (d->code == fcode)
      return spe_expand_evsel_builtin (d->icode, arglist, target);

  switch (fcode)
    {
    case SPE_BUILTIN_EVSTDDX:
      return spe_expand_stv_builtin (CODE_FOR_spe_evstddx, arglist);
    case SPE_BUILTIN_EVSTDHX:
      return spe_expand_stv_builtin (CODE_FOR_spe_evstdhx, arglist);
    case SPE_BUILTIN_EVSTDWX:
      return spe_expand_stv_builtin (CODE_FOR_spe_evstdwx, arglist);
    case SPE_BUILTIN_EVSTWHEX:
      return spe_expand_stv_builtin (CODE_FOR_spe_evstwhex, arglist);
    case SPE_BUILTIN_EVSTWHOX:
      return spe_expand_stv_builtin (CODE_FOR_spe_evstwhox, arglist);
    case SPE_BUILTIN_EVSTWWEX:
      return spe_expand_stv_builtin (CODE_FOR_spe_evstwwex, arglist);
    case SPE_BUILTIN_EVSTWWOX:
      return spe_expand_stv_builtin (CODE_FOR_spe_evstwwox, arglist);
    case SPE_BUILTIN_EVSTDD:
      return spe_expand_stv_builtin (CODE_FOR_spe_evstdd, arglist);
    case SPE_BUILTIN_EVSTDH:
      return spe_expand_stv_builtin (CODE_FOR_spe_evstdh, arglist);
    case SPE_BUILTIN_EVSTDW:
      return spe_expand_stv_builtin (CODE_FOR_spe_evstdw, arglist);
    case SPE_BUILTIN_EVSTWHE:
      return spe_expand_stv_builtin (CODE_FOR_spe_evstwhe, arglist);
    case SPE_BUILTIN_EVSTWHO:
      return spe_expand_stv_builtin (CODE_FOR_spe_evstwho, arglist);
    case SPE_BUILTIN_EVSTWWE:
      return spe_expand_stv_builtin (CODE_FOR_spe_evstwwe, arglist);
    case SPE_BUILTIN_EVSTWWO:
      return spe_expand_stv_builtin (CODE_FOR_spe_evstwwo, arglist);
    case SPE_BUILTIN_MFSPEFSCR:
      icode = CODE_FOR_spe_mfspefscr;
      tmode = insn_data[icode].operand[0].mode;

      if (target == 0
	  || GET_MODE (target) != tmode
	  || ! (*insn_data[icode].operand[0].predicate) (target, tmode))
	target = gen_reg_rtx (tmode);
      
      pat = GEN_FCN (icode) (target);
      if (! pat)
	return 0;
      emit_insn (pat);
      return target;
    case SPE_BUILTIN_MTSPEFSCR:
      icode = CODE_FOR_spe_mtspefscr;
      arg0 = TREE_VALUE (arglist);
      op0 = expand_expr (arg0, NULL_RTX, VOIDmode, 0);
      mode0 = insn_data[icode].operand[0].mode;

      if (arg0 == error_mark_node)
	return const0_rtx;

      if (! (*insn_data[icode].operand[0].predicate) (op0, mode0))
	op0 = copy_to_mode_reg (mode0, op0);

      pat = GEN_FCN (icode) (op0);
      if (pat)
	emit_insn (pat);
      return NULL_RTX;
    default:
      break;
    }

  *expandedp = false;
  return NULL_RTX;
}

static rtx
spe_expand_predicate_builtin (enum insn_code icode, tree arglist, rtx target)
{
  rtx pat, scratch, tmp;
  tree form = TREE_VALUE (arglist);
  tree arg0 = TREE_VALUE (TREE_CHAIN (arglist));
  tree arg1 = TREE_VALUE (TREE_CHAIN (TREE_CHAIN (arglist)));
  rtx op0 = expand_expr (arg0, NULL_RTX, VOIDmode, 0);
  rtx op1 = expand_expr (arg1, NULL_RTX, VOIDmode, 0);
  enum machine_mode mode0 = insn_data[icode].operand[1].mode;
  enum machine_mode mode1 = insn_data[icode].operand[2].mode;
  int form_int;
  enum rtx_code code;

  if (TREE_CODE (form) != INTEGER_CST)
    {
      error ("argument 1 of __builtin_spe_predicate must be a constant");
      return const0_rtx;
    }
  else
    form_int = TREE_INT_CST_LOW (form);

  if (mode0 != mode1)
    abort ();

  if (arg0 == error_mark_node || arg1 == error_mark_node)
    return const0_rtx;

  if (target == 0
      || GET_MODE (target) != SImode
      || ! (*insn_data[icode].operand[0].predicate) (target, SImode))
    target = gen_reg_rtx (SImode);

  if (! (*insn_data[icode].operand[1].predicate) (op0, mode0))
    op0 = copy_to_mode_reg (mode0, op0);
  if (! (*insn_data[icode].operand[2].predicate) (op1, mode1))
    op1 = copy_to_mode_reg (mode1, op1);

  scratch = gen_reg_rtx (CCmode);

  pat = GEN_FCN (icode) (scratch, op0, op1);
  if (! pat)
    return const0_rtx;
  emit_insn (pat);

  /* There are 4 variants for each predicate: _any_, _all_, _upper_,
     _lower_.  We use one compare, but look in different bits of the
     CR for each variant.

     There are 2 elements in each SPE simd type (upper/lower).  The CR
     bits are set as follows:

     BIT0  | BIT 1  | BIT 2   | BIT 3
     U     |   L    | (U | L) | (U & L)

     So, for an "all" relationship, BIT 3 would be set.
     For an "any" relationship, BIT 2 would be set.  Etc.

     Following traditional nomenclature, these bits map to:

     BIT0  | BIT 1  | BIT 2   | BIT 3
     LT    | GT     | EQ      | OV

     Later, we will generate rtl to look in the LT/EQ/EQ/OV bits.
  */

  switch (form_int)
    {
      /* All variant.  OV bit.  */
    case 0:
      /* We need to get to the OV bit, which is the ORDERED bit.  We
	 could generate (ordered:SI (reg:CC xx) (const_int 0)), but
	 that's ugly and will trigger a validate_condition_mode abort.
	 So let's just use another pattern.  */
      emit_insn (gen_move_from_CR_ov_bit (target, scratch));
      return target;
      /* Any variant.  EQ bit.  */
    case 1:
      code = EQ;
      break;
      /* Upper variant.  LT bit.  */
    case 2:
      code = LT;
      break;
      /* Lower variant.  GT bit.  */
    case 3:
      code = GT;
      break;
    default:
      error ("argument 1 of __builtin_spe_predicate is out of range");
      return const0_rtx;
    }

  tmp = gen_rtx_fmt_ee (code, SImode, scratch, const0_rtx);
  emit_move_insn (target, tmp);

  return target;
}

/* The evsel builtins look like this:

     e = __builtin_spe_evsel_OP (a, b, c, d);

   and work like this:

     e[upper] = a[upper] *OP* b[upper] ? c[upper] : d[upper];
     e[lower] = a[lower] *OP* b[lower] ? c[lower] : d[lower];
*/

static rtx
spe_expand_evsel_builtin (enum insn_code icode, tree arglist, rtx target)
{
  rtx pat, scratch;
  tree arg0 = TREE_VALUE (arglist);
  tree arg1 = TREE_VALUE (TREE_CHAIN (arglist));
  tree arg2 = TREE_VALUE (TREE_CHAIN (TREE_CHAIN (arglist)));
  tree arg3 = TREE_VALUE (TREE_CHAIN (TREE_CHAIN (TREE_CHAIN (arglist))));
  rtx op0 = expand_expr (arg0, NULL_RTX, VOIDmode, 0);
  rtx op1 = expand_expr (arg1, NULL_RTX, VOIDmode, 0);
  rtx op2 = expand_expr (arg2, NULL_RTX, VOIDmode, 0);
  rtx op3 = expand_expr (arg3, NULL_RTX, VOIDmode, 0);
  enum machine_mode mode0 = insn_data[icode].operand[1].mode;
  enum machine_mode mode1 = insn_data[icode].operand[2].mode;

  if (mode0 != mode1)
    abort ();

  if (arg0 == error_mark_node || arg1 == error_mark_node
      || arg2 == error_mark_node || arg3 == error_mark_node)
    return const0_rtx;

  if (target == 0
      || GET_MODE (target) != mode0
      || ! (*insn_data[icode].operand[0].predicate) (target, mode0))
    target = gen_reg_rtx (mode0);

  if (! (*insn_data[icode].operand[1].predicate) (op0, mode0))
    op0 = copy_to_mode_reg (mode0, op0);
  if (! (*insn_data[icode].operand[1].predicate) (op1, mode1))
    op1 = copy_to_mode_reg (mode0, op1);
  if (! (*insn_data[icode].operand[1].predicate) (op2, mode1))
    op2 = copy_to_mode_reg (mode0, op2);
  if (! (*insn_data[icode].operand[1].predicate) (op3, mode1))
    op3 = copy_to_mode_reg (mode0, op3);

  /* Generate the compare.  */
  scratch = gen_reg_rtx (CCmode);
  pat = GEN_FCN (icode) (scratch, op0, op1);
  if (! pat)
    return const0_rtx;
  emit_insn (pat);

  if (mode0 == V2SImode)
    emit_insn (gen_spe_evsel (target, op2, op3, scratch));
  else
    emit_insn (gen_spe_evsel_fs (target, op2, op3, scratch));

  return target;
}

/* Expand an expression EXP that calls a built-in function,
   with result going to TARGET if that's convenient
   (and in mode MODE if that's convenient).
   SUBTARGET may be used as the target for computing one of EXP's operands.
   IGNORE is nonzero if the value is to be ignored.  */

static rtx
rs6000_expand_builtin (tree exp, rtx target, rtx subtarget ATTRIBUTE_UNUSED,
		      enum machine_mode mode ATTRIBUTE_UNUSED, 
		      int ignore ATTRIBUTE_UNUSED)
{
  tree fndecl = TREE_OPERAND (TREE_OPERAND (exp, 0), 0);
  tree arglist = TREE_OPERAND (exp, 1);
  unsigned int fcode = DECL_FUNCTION_CODE (fndecl);
  struct builtin_description *d;
  size_t i;
  rtx ret;
  bool success;
  
  if (TARGET_ALTIVEC)
    {
      ret = altivec_expand_builtin (exp, target, &success);

      if (success)
	return ret;
    }
  if (TARGET_SPE)
    {
      ret = spe_expand_builtin (exp, target, &success);

      if (success)
	return ret;
    }

  if (TARGET_ALTIVEC || TARGET_SPE)
    {
      /* Handle simple unary operations.  */
      d = (struct builtin_description *) bdesc_1arg;
      for (i = 0; i < ARRAY_SIZE (bdesc_1arg); i++, d++)
	if (d->code == fcode)
	  return rs6000_expand_unop_builtin (d->icode, arglist, target);

      /* Handle simple binary operations.  */
      d = (struct builtin_description *) bdesc_2arg;
      for (i = 0; i < ARRAY_SIZE (bdesc_2arg); i++, d++)
	if (d->code == fcode)
	  return rs6000_expand_binop_builtin (d->icode, arglist, target);

      /* Handle simple ternary operations.  */
      d = (struct builtin_description *) bdesc_3arg;
      for (i = 0; i < ARRAY_SIZE  (bdesc_3arg); i++, d++)
	if (d->code == fcode)
	  return rs6000_expand_ternop_builtin (d->icode, arglist, target);
    }

  abort ();
  return NULL_RTX;
}

static void
rs6000_init_builtins (void)
{
  opaque_V2SI_type_node = copy_node (V2SI_type_node);
  opaque_V2SF_type_node = copy_node (V2SF_type_node);
  opaque_p_V2SI_type_node = build_pointer_type (opaque_V2SI_type_node);

  /* The 'vector bool ...' types must be kept distinct from 'vector unsigned ...'
     types, especially in C++ land.  Similarly, 'vector pixel' is distinct from+     'vector unsigned short'.  */

  bool_char_type_node = copy_node (unsigned_intQI_type_node);
  TYPE_MAIN_VARIANT (bool_char_type_node) = bool_char_type_node;
  bool_short_type_node = copy_node (unsigned_intHI_type_node);
  TYPE_MAIN_VARIANT (bool_short_type_node) = bool_short_type_node;
  bool_int_type_node = copy_node (unsigned_intSI_type_node);
  TYPE_MAIN_VARIANT (bool_int_type_node) = bool_int_type_node;
  pixel_type_node = copy_node (unsigned_intHI_type_node);
  TYPE_MAIN_VARIANT (pixel_type_node) = pixel_type_node;

  (*lang_hooks.decls.pushdecl) (build_decl (TYPE_DECL,
					    get_identifier ("__bool char"),
					    bool_char_type_node));
  (*lang_hooks.decls.pushdecl) (build_decl (TYPE_DECL,
					    get_identifier ("__bool short"),
					    bool_short_type_node));
  (*lang_hooks.decls.pushdecl) (build_decl (TYPE_DECL,
					    get_identifier ("__bool int"),
					    bool_int_type_node));
  (*lang_hooks.decls.pushdecl) (build_decl (TYPE_DECL,
					    get_identifier ("__pixel"),
					    pixel_type_node));

  bool_V16QI_type_node = make_vector (V16QImode, bool_char_type_node, 1);
  bool_V8HI_type_node = make_vector (V8HImode, bool_short_type_node, 1);
  bool_V4SI_type_node = make_vector (V4SImode, bool_int_type_node, 1);
  pixel_V8HI_type_node = make_vector (V8HImode, pixel_type_node, 1);

  (*lang_hooks.decls.pushdecl) (build_decl (TYPE_DECL,
					    get_identifier ("__vector unsigned char"),
					    unsigned_V16QI_type_node));
  (*lang_hooks.decls.pushdecl) (build_decl (TYPE_DECL,
					    get_identifier ("__vector signed char"),
					    V16QI_type_node));
  (*lang_hooks.decls.pushdecl) (build_decl (TYPE_DECL,
					    get_identifier ("__vector __bool char"),
					    bool_V16QI_type_node));

  (*lang_hooks.decls.pushdecl) (build_decl (TYPE_DECL,
					    get_identifier ("__vector unsigned short"),
					    unsigned_V8HI_type_node));
  (*lang_hooks.decls.pushdecl) (build_decl (TYPE_DECL,
					    get_identifier ("__vector signed short"),
					    V8HI_type_node));
  (*lang_hooks.decls.pushdecl) (build_decl (TYPE_DECL,
					    get_identifier ("__vector __bool short"),
					    bool_V8HI_type_node));

  (*lang_hooks.decls.pushdecl) (build_decl (TYPE_DECL,
					    get_identifier ("__vector unsigned int"),
					    unsigned_V4SI_type_node));
  (*lang_hooks.decls.pushdecl) (build_decl (TYPE_DECL,
					    get_identifier ("__vector signed int"),
					    V4SI_type_node));
  (*lang_hooks.decls.pushdecl) (build_decl (TYPE_DECL,
					    get_identifier ("__vector __bool int"),
					    bool_V4SI_type_node));

  (*lang_hooks.decls.pushdecl) (build_decl (TYPE_DECL,
					    get_identifier ("__vector float"),
					    V4SF_type_node));
  (*lang_hooks.decls.pushdecl) (build_decl (TYPE_DECL,
					    get_identifier ("__vector __pixel"),
					    pixel_V8HI_type_node));

  if (TARGET_SPE)
    spe_init_builtins ();
  if (TARGET_ALTIVEC)
    altivec_init_builtins ();
  if (TARGET_ALTIVEC || TARGET_SPE)
    rs6000_common_init_builtins ();
}

/* Search through a set of builtins and enable the mask bits.
   DESC is an array of builtins.
   SIZE is the total number of builtins.
   START is the builtin enum at which to start.
   END is the builtin enum at which to end.  */
static void
enable_mask_for_builtins (struct builtin_description *desc, int size,
			  enum rs6000_builtins start, 
			  enum rs6000_builtins end)
{
  int i;

  for (i = 0; i < size; ++i)
    if (desc[i].code == start)
      break;

  if (i == size)
    return;

  for (; i < size; ++i)
    {
      /* Flip all the bits on.  */
      desc[i].mask = target_flags;
      if (desc[i].code == end)
	break;
    }
}

static void
spe_init_builtins (void)
{
  tree endlink = void_list_node;
  tree puint_type_node = build_pointer_type (unsigned_type_node);
  tree pushort_type_node = build_pointer_type (short_unsigned_type_node);
  struct builtin_description *d;
  size_t i;

  tree v2si_ftype_4_v2si
    = build_function_type
    (opaque_V2SI_type_node,
     tree_cons (NULL_TREE, opaque_V2SI_type_node,
		tree_cons (NULL_TREE, opaque_V2SI_type_node,
			   tree_cons (NULL_TREE, opaque_V2SI_type_node,
				      tree_cons (NULL_TREE, opaque_V2SI_type_node,
						 endlink)))));

  tree v2sf_ftype_4_v2sf
    = build_function_type
    (opaque_V2SF_type_node,
     tree_cons (NULL_TREE, opaque_V2SF_type_node,
		tree_cons (NULL_TREE, opaque_V2SF_type_node,
			   tree_cons (NULL_TREE, opaque_V2SF_type_node,
				      tree_cons (NULL_TREE, opaque_V2SF_type_node,
						 endlink)))));

  tree int_ftype_int_v2si_v2si
    = build_function_type
    (integer_type_node,
     tree_cons (NULL_TREE, integer_type_node,
		tree_cons (NULL_TREE, opaque_V2SI_type_node,
			   tree_cons (NULL_TREE, opaque_V2SI_type_node,
				      endlink))));

  tree int_ftype_int_v2sf_v2sf
    = build_function_type
    (integer_type_node,
     tree_cons (NULL_TREE, integer_type_node,
		tree_cons (NULL_TREE, opaque_V2SF_type_node,
			   tree_cons (NULL_TREE, opaque_V2SF_type_node,
				      endlink))));

  tree void_ftype_v2si_puint_int
    = build_function_type (void_type_node,
			   tree_cons (NULL_TREE, opaque_V2SI_type_node,
				      tree_cons (NULL_TREE, puint_type_node,
						 tree_cons (NULL_TREE,
							    integer_type_node,
							    endlink))));

  tree void_ftype_v2si_puint_char
    = build_function_type (void_type_node,
			   tree_cons (NULL_TREE, opaque_V2SI_type_node,
				      tree_cons (NULL_TREE, puint_type_node,
						 tree_cons (NULL_TREE,
							    char_type_node,
							    endlink))));

  tree void_ftype_v2si_pv2si_int
    = build_function_type (void_type_node,
			   tree_cons (NULL_TREE, opaque_V2SI_type_node,
				      tree_cons (NULL_TREE, opaque_p_V2SI_type_node,
						 tree_cons (NULL_TREE,
							    integer_type_node,
							    endlink))));

  tree void_ftype_v2si_pv2si_char
    = build_function_type (void_type_node,
			   tree_cons (NULL_TREE, opaque_V2SI_type_node,
				      tree_cons (NULL_TREE, opaque_p_V2SI_type_node,
						 tree_cons (NULL_TREE,
							    char_type_node,
							    endlink))));

  tree void_ftype_int
    = build_function_type (void_type_node,
			   tree_cons (NULL_TREE, integer_type_node, endlink));

  tree int_ftype_void
    = build_function_type (integer_type_node, endlink);

  tree v2si_ftype_pv2si_int
    = build_function_type (opaque_V2SI_type_node,
			   tree_cons (NULL_TREE, opaque_p_V2SI_type_node,
				      tree_cons (NULL_TREE, integer_type_node,
						 endlink)));

  tree v2si_ftype_puint_int
    = build_function_type (opaque_V2SI_type_node,
			   tree_cons (NULL_TREE, puint_type_node,
				      tree_cons (NULL_TREE, integer_type_node,
						 endlink)));

  tree v2si_ftype_pushort_int
    = build_function_type (opaque_V2SI_type_node,
			   tree_cons (NULL_TREE, pushort_type_node,
				      tree_cons (NULL_TREE, integer_type_node,
						 endlink)));

  tree v2si_ftype_signed_char
    = build_function_type (opaque_V2SI_type_node,
			   tree_cons (NULL_TREE, signed_char_type_node,
				      endlink));

  /* The initialization of the simple binary and unary builtins is
     done in rs6000_common_init_builtins, but we have to enable the
     mask bits here manually because we have run out of `target_flags'
     bits.  We really need to redesign this mask business.  */

  enable_mask_for_builtins ((struct builtin_description *) bdesc_2arg,
			    ARRAY_SIZE (bdesc_2arg),
			    SPE_BUILTIN_EVADDW,
			    SPE_BUILTIN_EVXOR);
  enable_mask_for_builtins ((struct builtin_description *) bdesc_1arg,
			    ARRAY_SIZE (bdesc_1arg),
			    SPE_BUILTIN_EVABS,
			    SPE_BUILTIN_EVSUBFUSIAAW);
  enable_mask_for_builtins ((struct builtin_description *) bdesc_spe_predicates,
			    ARRAY_SIZE (bdesc_spe_predicates),
			    SPE_BUILTIN_EVCMPEQ,
			    SPE_BUILTIN_EVFSTSTLT);
  enable_mask_for_builtins ((struct builtin_description *) bdesc_spe_evsel,
			    ARRAY_SIZE (bdesc_spe_evsel),
			    SPE_BUILTIN_EVSEL_CMPGTS,
			    SPE_BUILTIN_EVSEL_FSTSTEQ);

  (*lang_hooks.decls.pushdecl)
    (build_decl (TYPE_DECL, get_identifier ("__ev64_opaque__"),
		 opaque_V2SI_type_node));

  /* Initialize irregular SPE builtins.  */
  
  def_builtin (target_flags, "__builtin_spe_mtspefscr", void_ftype_int, SPE_BUILTIN_MTSPEFSCR);
  def_builtin (target_flags, "__builtin_spe_mfspefscr", int_ftype_void, SPE_BUILTIN_MFSPEFSCR);
  def_builtin (target_flags, "__builtin_spe_evstddx", void_ftype_v2si_pv2si_int, SPE_BUILTIN_EVSTDDX);
  def_builtin (target_flags, "__builtin_spe_evstdhx", void_ftype_v2si_pv2si_int, SPE_BUILTIN_EVSTDHX);
  def_builtin (target_flags, "__builtin_spe_evstdwx", void_ftype_v2si_pv2si_int, SPE_BUILTIN_EVSTDWX);
  def_builtin (target_flags, "__builtin_spe_evstwhex", void_ftype_v2si_puint_int, SPE_BUILTIN_EVSTWHEX);
  def_builtin (target_flags, "__builtin_spe_evstwhox", void_ftype_v2si_puint_int, SPE_BUILTIN_EVSTWHOX);
  def_builtin (target_flags, "__builtin_spe_evstwwex", void_ftype_v2si_puint_int, SPE_BUILTIN_EVSTWWEX);
  def_builtin (target_flags, "__builtin_spe_evstwwox", void_ftype_v2si_puint_int, SPE_BUILTIN_EVSTWWOX);
  def_builtin (target_flags, "__builtin_spe_evstdd", void_ftype_v2si_pv2si_char, SPE_BUILTIN_EVSTDD);
  def_builtin (target_flags, "__builtin_spe_evstdh", void_ftype_v2si_pv2si_char, SPE_BUILTIN_EVSTDH);
  def_builtin (target_flags, "__builtin_spe_evstdw", void_ftype_v2si_pv2si_char, SPE_BUILTIN_EVSTDW);
  def_builtin (target_flags, "__builtin_spe_evstwhe", void_ftype_v2si_puint_char, SPE_BUILTIN_EVSTWHE);
  def_builtin (target_flags, "__builtin_spe_evstwho", void_ftype_v2si_puint_char, SPE_BUILTIN_EVSTWHO);
  def_builtin (target_flags, "__builtin_spe_evstwwe", void_ftype_v2si_puint_char, SPE_BUILTIN_EVSTWWE);
  def_builtin (target_flags, "__builtin_spe_evstwwo", void_ftype_v2si_puint_char, SPE_BUILTIN_EVSTWWO);
  def_builtin (target_flags, "__builtin_spe_evsplatfi", v2si_ftype_signed_char, SPE_BUILTIN_EVSPLATFI);
  def_builtin (target_flags, "__builtin_spe_evsplati", v2si_ftype_signed_char, SPE_BUILTIN_EVSPLATI);

  /* Loads.  */
  def_builtin (target_flags, "__builtin_spe_evlddx", v2si_ftype_pv2si_int, SPE_BUILTIN_EVLDDX);
  def_builtin (target_flags, "__builtin_spe_evldwx", v2si_ftype_pv2si_int, SPE_BUILTIN_EVLDWX);
  def_builtin (target_flags, "__builtin_spe_evldhx", v2si_ftype_pv2si_int, SPE_BUILTIN_EVLDHX);
  def_builtin (target_flags, "__builtin_spe_evlwhex", v2si_ftype_puint_int, SPE_BUILTIN_EVLWHEX);
  def_builtin (target_flags, "__builtin_spe_evlwhoux", v2si_ftype_puint_int, SPE_BUILTIN_EVLWHOUX);
  def_builtin (target_flags, "__builtin_spe_evlwhosx", v2si_ftype_puint_int, SPE_BUILTIN_EVLWHOSX);
  def_builtin (target_flags, "__builtin_spe_evlwwsplatx", v2si_ftype_puint_int, SPE_BUILTIN_EVLWWSPLATX);
  def_builtin (target_flags, "__builtin_spe_evlwhsplatx", v2si_ftype_puint_int, SPE_BUILTIN_EVLWHSPLATX);
  def_builtin (target_flags, "__builtin_spe_evlhhesplatx", v2si_ftype_pushort_int, SPE_BUILTIN_EVLHHESPLATX);
  def_builtin (target_flags, "__builtin_spe_evlhhousplatx", v2si_ftype_pushort_int, SPE_BUILTIN_EVLHHOUSPLATX);
  def_builtin (target_flags, "__builtin_spe_evlhhossplatx", v2si_ftype_pushort_int, SPE_BUILTIN_EVLHHOSSPLATX);
  def_builtin (target_flags, "__builtin_spe_evldd", v2si_ftype_pv2si_int, SPE_BUILTIN_EVLDD);
  def_builtin (target_flags, "__builtin_spe_evldw", v2si_ftype_pv2si_int, SPE_BUILTIN_EVLDW);
  def_builtin (target_flags, "__builtin_spe_evldh", v2si_ftype_pv2si_int, SPE_BUILTIN_EVLDH);
  def_builtin (target_flags, "__builtin_spe_evlhhesplat", v2si_ftype_pushort_int, SPE_BUILTIN_EVLHHESPLAT);
  def_builtin (target_flags, "__builtin_spe_evlhhossplat", v2si_ftype_pushort_int, SPE_BUILTIN_EVLHHOSSPLAT);
  def_builtin (target_flags, "__builtin_spe_evlhhousplat", v2si_ftype_pushort_int, SPE_BUILTIN_EVLHHOUSPLAT);
  def_builtin (target_flags, "__builtin_spe_evlwhe", v2si_ftype_puint_int, SPE_BUILTIN_EVLWHE);
  def_builtin (target_flags, "__builtin_spe_evlwhos", v2si_ftype_puint_int, SPE_BUILTIN_EVLWHOS);
  def_builtin (target_flags, "__builtin_spe_evlwhou", v2si_ftype_puint_int, SPE_BUILTIN_EVLWHOU);
  def_builtin (target_flags, "__builtin_spe_evlwhsplat", v2si_ftype_puint_int, SPE_BUILTIN_EVLWHSPLAT);
  def_builtin (target_flags, "__builtin_spe_evlwwsplat", v2si_ftype_puint_int, SPE_BUILTIN_EVLWWSPLAT);

  /* Predicates.  */
  d = (struct builtin_description *) bdesc_spe_predicates;
  for (i = 0; i < ARRAY_SIZE (bdesc_spe_predicates); ++i, d++)
    {
      tree type;

      switch (insn_data[d->icode].operand[1].mode)
	{
	case V2SImode:
	  type = int_ftype_int_v2si_v2si;
	  break;
	case V2SFmode:
	  type = int_ftype_int_v2sf_v2sf;
	  break;
	default:
	  abort ();
	}

      def_builtin (d->mask, d->name, type, d->code);
    }

  /* Evsel predicates.  */
  d = (struct builtin_description *) bdesc_spe_evsel;
  for (i = 0; i < ARRAY_SIZE (bdesc_spe_evsel); ++i, d++)
    {
      tree type;

      switch (insn_data[d->icode].operand[1].mode)
	{
	case V2SImode:
	  type = v2si_ftype_4_v2si;
	  break;
	case V2SFmode:
	  type = v2sf_ftype_4_v2sf;
	  break;
	default:
	  abort ();
	}

      def_builtin (d->mask, d->name, type, d->code);
    }
}

static void
altivec_init_builtins (void)
{
  struct builtin_description *d;
  struct builtin_description_predicates *dp;
  size_t i;
  tree pfloat_type_node = build_pointer_type (float_type_node);
  tree pint_type_node = build_pointer_type (integer_type_node);
  tree pshort_type_node = build_pointer_type (short_integer_type_node);
  tree pchar_type_node = build_pointer_type (char_type_node);

  tree pvoid_type_node = build_pointer_type (void_type_node);

  tree pcfloat_type_node = build_pointer_type (build_qualified_type (float_type_node, TYPE_QUAL_CONST));
  tree pcint_type_node = build_pointer_type (build_qualified_type (integer_type_node, TYPE_QUAL_CONST));
  tree pcshort_type_node = build_pointer_type (build_qualified_type (short_integer_type_node, TYPE_QUAL_CONST));
  tree pcchar_type_node = build_pointer_type (build_qualified_type (char_type_node, TYPE_QUAL_CONST));

  tree pcvoid_type_node = build_pointer_type (build_qualified_type (void_type_node, TYPE_QUAL_CONST));

  tree int_ftype_int_v4si_v4si
    = build_function_type_list (integer_type_node,
				integer_type_node, V4SI_type_node,
				V4SI_type_node, NULL_TREE);
  tree v4sf_ftype_pcfloat
    = build_function_type_list (V4SF_type_node, pcfloat_type_node, NULL_TREE);
  tree void_ftype_pfloat_v4sf
    = build_function_type_list (void_type_node,
				pfloat_type_node, V4SF_type_node, NULL_TREE);
  tree v4si_ftype_pcint
    = build_function_type_list (V4SI_type_node, pcint_type_node, NULL_TREE);
  tree void_ftype_pint_v4si
    = build_function_type_list (void_type_node,
				pint_type_node, V4SI_type_node, NULL_TREE);
  tree v8hi_ftype_pcshort
    = build_function_type_list (V8HI_type_node, pcshort_type_node, NULL_TREE);
  tree void_ftype_pshort_v8hi
    = build_function_type_list (void_type_node,
				pshort_type_node, V8HI_type_node, NULL_TREE);
  tree v16qi_ftype_pcchar
    = build_function_type_list (V16QI_type_node, pcchar_type_node, NULL_TREE);
  tree void_ftype_pchar_v16qi
    = build_function_type_list (void_type_node,
				pchar_type_node, V16QI_type_node, NULL_TREE);
  tree void_ftype_v4si
    = build_function_type_list (void_type_node, V4SI_type_node, NULL_TREE);
  tree v8hi_ftype_void
    = build_function_type (V8HI_type_node, void_list_node);
  tree void_ftype_void
    = build_function_type (void_type_node, void_list_node);
  tree void_ftype_int
    = build_function_type_list (void_type_node, integer_type_node, NULL_TREE);

  tree v16qi_ftype_long_pcvoid
    = build_function_type_list (V16QI_type_node,
				long_integer_type_node, pcvoid_type_node, NULL_TREE);
  tree v8hi_ftype_long_pcvoid
    = build_function_type_list (V8HI_type_node,
				long_integer_type_node, pcvoid_type_node, NULL_TREE);
  tree v4si_ftype_long_pcvoid
    = build_function_type_list (V4SI_type_node,
				long_integer_type_node, pcvoid_type_node, NULL_TREE);

  tree void_ftype_v4si_long_pvoid
    = build_function_type_list (void_type_node,
				V4SI_type_node, long_integer_type_node,
				pvoid_type_node, NULL_TREE);
  tree void_ftype_v16qi_long_pvoid
    = build_function_type_list (void_type_node,
				V16QI_type_node, long_integer_type_node,
				pvoid_type_node, NULL_TREE);
  tree void_ftype_v8hi_long_pvoid
    = build_function_type_list (void_type_node,
				V8HI_type_node, long_integer_type_node,
				pvoid_type_node, NULL_TREE);
  tree int_ftype_int_v8hi_v8hi
    = build_function_type_list (integer_type_node,
				integer_type_node, V8HI_type_node,
				V8HI_type_node, NULL_TREE);
  tree int_ftype_int_v16qi_v16qi
    = build_function_type_list (integer_type_node,
				integer_type_node, V16QI_type_node,
				V16QI_type_node, NULL_TREE);
  tree int_ftype_int_v4sf_v4sf
    = build_function_type_list (integer_type_node,
				integer_type_node, V4SF_type_node,
				V4SF_type_node, NULL_TREE);
  tree v4si_ftype_v4si
    = build_function_type_list (V4SI_type_node, V4SI_type_node, NULL_TREE);
  tree v8hi_ftype_v8hi
    = build_function_type_list (V8HI_type_node, V8HI_type_node, NULL_TREE);
  tree v16qi_ftype_v16qi
    = build_function_type_list (V16QI_type_node, V16QI_type_node, NULL_TREE);
  tree v4sf_ftype_v4sf
    = build_function_type_list (V4SF_type_node, V4SF_type_node, NULL_TREE);
  tree void_ftype_pcvoid_int_int
    = build_function_type_list (void_type_node,
				pcvoid_type_node, integer_type_node,
				integer_type_node, NULL_TREE);
  tree int_ftype_pcchar
    = build_function_type_list (integer_type_node,
				pcchar_type_node, NULL_TREE);
  
  def_builtin (MASK_ALTIVEC, "__builtin_altivec_ld_internal_4sf", v4sf_ftype_pcfloat,
	       ALTIVEC_BUILTIN_LD_INTERNAL_4sf);
  def_builtin (MASK_ALTIVEC, "__builtin_altivec_st_internal_4sf", void_ftype_pfloat_v4sf,
	       ALTIVEC_BUILTIN_ST_INTERNAL_4sf);
  def_builtin (MASK_ALTIVEC, "__builtin_altivec_ld_internal_4si", v4si_ftype_pcint,
	       ALTIVEC_BUILTIN_LD_INTERNAL_4si);
  def_builtin (MASK_ALTIVEC, "__builtin_altivec_st_internal_4si", void_ftype_pint_v4si,
	       ALTIVEC_BUILTIN_ST_INTERNAL_4si);
  def_builtin (MASK_ALTIVEC, "__builtin_altivec_ld_internal_8hi", v8hi_ftype_pcshort,
	       ALTIVEC_BUILTIN_LD_INTERNAL_8hi);
  def_builtin (MASK_ALTIVEC, "__builtin_altivec_st_internal_8hi", void_ftype_pshort_v8hi,
	       ALTIVEC_BUILTIN_ST_INTERNAL_8hi);
  def_builtin (MASK_ALTIVEC, "__builtin_altivec_ld_internal_16qi", v16qi_ftype_pcchar,
	       ALTIVEC_BUILTIN_LD_INTERNAL_16qi);
  def_builtin (MASK_ALTIVEC, "__builtin_altivec_st_internal_16qi", void_ftype_pchar_v16qi,
	       ALTIVEC_BUILTIN_ST_INTERNAL_16qi);
  def_builtin (MASK_ALTIVEC, "__builtin_altivec_mtvscr", void_ftype_v4si, ALTIVEC_BUILTIN_MTVSCR);
  def_builtin (MASK_ALTIVEC, "__builtin_altivec_mfvscr", v8hi_ftype_void, ALTIVEC_BUILTIN_MFVSCR);
  def_builtin (MASK_ALTIVEC, "__builtin_altivec_dssall", void_ftype_void, ALTIVEC_BUILTIN_DSSALL);
  def_builtin (MASK_ALTIVEC, "__builtin_altivec_dss", void_ftype_int, ALTIVEC_BUILTIN_DSS);
  def_builtin (MASK_ALTIVEC, "__builtin_altivec_lvsl", v16qi_ftype_long_pcvoid, ALTIVEC_BUILTIN_LVSL);
  def_builtin (MASK_ALTIVEC, "__builtin_altivec_lvsr", v16qi_ftype_long_pcvoid, ALTIVEC_BUILTIN_LVSR);
  def_builtin (MASK_ALTIVEC, "__builtin_altivec_lvebx", v16qi_ftype_long_pcvoid, ALTIVEC_BUILTIN_LVEBX);
  def_builtin (MASK_ALTIVEC, "__builtin_altivec_lvehx", v8hi_ftype_long_pcvoid, ALTIVEC_BUILTIN_LVEHX);
  def_builtin (MASK_ALTIVEC, "__builtin_altivec_lvewx", v4si_ftype_long_pcvoid, ALTIVEC_BUILTIN_LVEWX);
  def_builtin (MASK_ALTIVEC, "__builtin_altivec_lvxl", v4si_ftype_long_pcvoid, ALTIVEC_BUILTIN_LVXL);
  def_builtin (MASK_ALTIVEC, "__builtin_altivec_lvx", v4si_ftype_long_pcvoid, ALTIVEC_BUILTIN_LVX);
  def_builtin (MASK_ALTIVEC, "__builtin_altivec_stvx", void_ftype_v4si_long_pvoid, ALTIVEC_BUILTIN_STVX);
  def_builtin (MASK_ALTIVEC, "__builtin_altivec_stvewx", void_ftype_v4si_long_pvoid, ALTIVEC_BUILTIN_STVEWX);
  def_builtin (MASK_ALTIVEC, "__builtin_altivec_stvxl", void_ftype_v4si_long_pvoid, ALTIVEC_BUILTIN_STVXL);
  def_builtin (MASK_ALTIVEC, "__builtin_altivec_stvebx", void_ftype_v16qi_long_pvoid, ALTIVEC_BUILTIN_STVEBX);
  def_builtin (MASK_ALTIVEC, "__builtin_altivec_stvehx", void_ftype_v8hi_long_pvoid, ALTIVEC_BUILTIN_STVEHX);

  /* See altivec.h for usage of "__builtin_altivec_compiletime_error".  */
  def_builtin (MASK_ALTIVEC, "__builtin_altivec_compiletime_error", int_ftype_pcchar,
	       ALTIVEC_BUILTIN_COMPILETIME_ERROR);

  /* Add the DST variants.  */
  d = (struct builtin_description *) bdesc_dst;
  for (i = 0; i < ARRAY_SIZE (bdesc_dst); i++, d++)
    def_builtin (d->mask, d->name, void_ftype_pcvoid_int_int, d->code);

  /* Initialize the predicates.  */
  dp = (struct builtin_description_predicates *) bdesc_altivec_preds;
  for (i = 0; i < ARRAY_SIZE (bdesc_altivec_preds); i++, dp++)
    {
      enum machine_mode mode1;
      tree type;

      mode1 = insn_data[dp->icode].operand[1].mode;

      switch (mode1)
	{
	case V4SImode:
	  type = int_ftype_int_v4si_v4si;
	  break;
	case V8HImode:
	  type = int_ftype_int_v8hi_v8hi;
	  break;
	case V16QImode:
	  type = int_ftype_int_v16qi_v16qi;
	  break;
	case V4SFmode:
	  type = int_ftype_int_v4sf_v4sf;
	  break;
	default:
	  abort ();
	}
      
      def_builtin (dp->mask, dp->name, type, dp->code);
    }

  /* Initialize the abs* operators.  */
  d = (struct builtin_description *) bdesc_abs;
  for (i = 0; i < ARRAY_SIZE (bdesc_abs); i++, d++)
    {
      enum machine_mode mode0;
      tree type;

      mode0 = insn_data[d->icode].operand[0].mode;

      switch (mode0)
	{
	case V4SImode:
	  type = v4si_ftype_v4si;
	  break;
	case V8HImode:
	  type = v8hi_ftype_v8hi;
	  break;
	case V16QImode:
	  type = v16qi_ftype_v16qi;
	  break;
	case V4SFmode:
	  type = v4sf_ftype_v4sf;
	  break;
	default:
	  abort ();
	}
      
      def_builtin (d->mask, d->name, type, d->code);
    }
}

static void
rs6000_common_init_builtins (void)
{
  struct builtin_description *d;
  size_t i;

  tree v4sf_ftype_v4sf_v4sf_v16qi
    = build_function_type_list (V4SF_type_node,
				V4SF_type_node, V4SF_type_node,
				V16QI_type_node, NULL_TREE);
  tree v4si_ftype_v4si_v4si_v16qi
    = build_function_type_list (V4SI_type_node,
				V4SI_type_node, V4SI_type_node,
				V16QI_type_node, NULL_TREE);
  tree v8hi_ftype_v8hi_v8hi_v16qi
    = build_function_type_list (V8HI_type_node,
				V8HI_type_node, V8HI_type_node,
				V16QI_type_node, NULL_TREE);
  tree v16qi_ftype_v16qi_v16qi_v16qi
    = build_function_type_list (V16QI_type_node,
				V16QI_type_node, V16QI_type_node,
				V16QI_type_node, NULL_TREE);
  tree v4si_ftype_int
    = build_function_type_list (V4SI_type_node, integer_type_node, NULL_TREE);
  tree v8hi_ftype_int
    = build_function_type_list (V8HI_type_node, integer_type_node, NULL_TREE);
  tree v16qi_ftype_int
    = build_function_type_list (V16QI_type_node, integer_type_node, NULL_TREE);
  tree v8hi_ftype_v16qi
    = build_function_type_list (V8HI_type_node, V16QI_type_node, NULL_TREE);
  tree v4sf_ftype_v4sf
    = build_function_type_list (V4SF_type_node, V4SF_type_node, NULL_TREE);

  tree v2si_ftype_v2si_v2si
    = build_function_type_list (opaque_V2SI_type_node,
				opaque_V2SI_type_node,
				opaque_V2SI_type_node, NULL_TREE);

  tree v2sf_ftype_v2sf_v2sf
    = build_function_type_list (opaque_V2SF_type_node,
				opaque_V2SF_type_node,
				opaque_V2SF_type_node, NULL_TREE);

  tree v2si_ftype_int_int
    = build_function_type_list (opaque_V2SI_type_node,
				integer_type_node, integer_type_node,
				NULL_TREE);

  tree v2si_ftype_v2si
    = build_function_type_list (opaque_V2SI_type_node,
				opaque_V2SI_type_node, NULL_TREE);

  tree v2sf_ftype_v2sf
    = build_function_type_list (opaque_V2SF_type_node,
				opaque_V2SF_type_node, NULL_TREE);
  
  tree v2sf_ftype_v2si
    = build_function_type_list (opaque_V2SF_type_node,
				opaque_V2SI_type_node, NULL_TREE);

  tree v2si_ftype_v2sf
    = build_function_type_list (opaque_V2SI_type_node,
				opaque_V2SF_type_node, NULL_TREE);

  tree v2si_ftype_v2si_char
    = build_function_type_list (opaque_V2SI_type_node,
				opaque_V2SI_type_node,
				char_type_node, NULL_TREE);

  tree v2si_ftype_int_char
    = build_function_type_list (opaque_V2SI_type_node,
				integer_type_node, char_type_node, NULL_TREE);

  tree v2si_ftype_char
    = build_function_type_list (opaque_V2SI_type_node,
				char_type_node, NULL_TREE);

  tree int_ftype_int_int
    = build_function_type_list (integer_type_node,
				integer_type_node, integer_type_node,
				NULL_TREE);

  tree v4si_ftype_v4si_v4si
    = build_function_type_list (V4SI_type_node,
				V4SI_type_node, V4SI_type_node, NULL_TREE);
  tree v4sf_ftype_v4si_int
    = build_function_type_list (V4SF_type_node,
				V4SI_type_node, integer_type_node, NULL_TREE);
  tree v4si_ftype_v4sf_int
    = build_function_type_list (V4SI_type_node,
				V4SF_type_node, integer_type_node, NULL_TREE);
  tree v4si_ftype_v4si_int
    = build_function_type_list (V4SI_type_node,
				V4SI_type_node, integer_type_node, NULL_TREE);
  tree v8hi_ftype_v8hi_int
    = build_function_type_list (V8HI_type_node,
				V8HI_type_node, integer_type_node, NULL_TREE);
  tree v16qi_ftype_v16qi_int
    = build_function_type_list (V16QI_type_node,
				V16QI_type_node, integer_type_node, NULL_TREE);
  tree v16qi_ftype_v16qi_v16qi_int
    = build_function_type_list (V16QI_type_node,
				V16QI_type_node, V16QI_type_node,
				integer_type_node, NULL_TREE);
  tree v8hi_ftype_v8hi_v8hi_int
    = build_function_type_list (V8HI_type_node,
				V8HI_type_node, V8HI_type_node,
				integer_type_node, NULL_TREE);
  tree v4si_ftype_v4si_v4si_int
    = build_function_type_list (V4SI_type_node,
				V4SI_type_node, V4SI_type_node,
				integer_type_node, NULL_TREE);
  tree v4sf_ftype_v4sf_v4sf_int
    = build_function_type_list (V4SF_type_node,
				V4SF_type_node, V4SF_type_node,
				integer_type_node, NULL_TREE);
  tree v4sf_ftype_v4sf_v4sf
    = build_function_type_list (V4SF_type_node,
				V4SF_type_node, V4SF_type_node, NULL_TREE);
  tree v4sf_ftype_v4sf_v4sf_v4si
    = build_function_type_list (V4SF_type_node,
				V4SF_type_node, V4SF_type_node,
				V4SI_type_node, NULL_TREE);
  tree v4sf_ftype_v4sf_v4sf_v4sf
    = build_function_type_list (V4SF_type_node,
				V4SF_type_node, V4SF_type_node,
				V4SF_type_node, NULL_TREE);
  tree v4si_ftype_v4si_v4si_v4si 
    = build_function_type_list (V4SI_type_node,
				V4SI_type_node, V4SI_type_node,
				V4SI_type_node, NULL_TREE);
  tree v8hi_ftype_v8hi_v8hi
    = build_function_type_list (V8HI_type_node,
				V8HI_type_node, V8HI_type_node, NULL_TREE);
  tree v8hi_ftype_v8hi_v8hi_v8hi
    = build_function_type_list (V8HI_type_node,
				V8HI_type_node, V8HI_type_node,
				V8HI_type_node, NULL_TREE);
 tree v4si_ftype_v8hi_v8hi_v4si
    = build_function_type_list (V4SI_type_node,
				V8HI_type_node, V8HI_type_node,
				V4SI_type_node, NULL_TREE);
 tree v4si_ftype_v16qi_v16qi_v4si
    = build_function_type_list (V4SI_type_node,
				V16QI_type_node, V16QI_type_node,
				V4SI_type_node, NULL_TREE);
  tree v16qi_ftype_v16qi_v16qi
    = build_function_type_list (V16QI_type_node,
				V16QI_type_node, V16QI_type_node, NULL_TREE);
  tree v4si_ftype_v4sf_v4sf
    = build_function_type_list (V4SI_type_node,
				V4SF_type_node, V4SF_type_node, NULL_TREE);
  tree v8hi_ftype_v16qi_v16qi
    = build_function_type_list (V8HI_type_node,
				V16QI_type_node, V16QI_type_node, NULL_TREE);
  tree v4si_ftype_v8hi_v8hi
    = build_function_type_list (V4SI_type_node,
				V8HI_type_node, V8HI_type_node, NULL_TREE);
  tree v8hi_ftype_v4si_v4si
    = build_function_type_list (V8HI_type_node,
				V4SI_type_node, V4SI_type_node, NULL_TREE);
  tree v16qi_ftype_v8hi_v8hi
    = build_function_type_list (V16QI_type_node,
				V8HI_type_node, V8HI_type_node, NULL_TREE);
  tree v4si_ftype_v16qi_v4si
    = build_function_type_list (V4SI_type_node,
				V16QI_type_node, V4SI_type_node, NULL_TREE);
  tree v4si_ftype_v16qi_v16qi
    = build_function_type_list (V4SI_type_node,
				V16QI_type_node, V16QI_type_node, NULL_TREE);
  tree v4si_ftype_v8hi_v4si
    = build_function_type_list (V4SI_type_node,
				V8HI_type_node, V4SI_type_node, NULL_TREE);
  tree v4si_ftype_v8hi
    = build_function_type_list (V4SI_type_node, V8HI_type_node, NULL_TREE);
  tree int_ftype_v4si_v4si
    = build_function_type_list (integer_type_node,
				V4SI_type_node, V4SI_type_node, NULL_TREE);
  tree int_ftype_v4sf_v4sf
    = build_function_type_list (integer_type_node,
				V4SF_type_node, V4SF_type_node, NULL_TREE);
  tree int_ftype_v16qi_v16qi
    = build_function_type_list (integer_type_node,
				V16QI_type_node, V16QI_type_node, NULL_TREE);
  tree int_ftype_v8hi_v8hi
    = build_function_type_list (integer_type_node,
				V8HI_type_node, V8HI_type_node, NULL_TREE);

  /* Add the simple ternary operators.  */
  d = (struct builtin_description *) bdesc_3arg;
  for (i = 0; i < ARRAY_SIZE (bdesc_3arg); i++, d++)
    {
      
      enum machine_mode mode0, mode1, mode2, mode3;
      tree type;

      if (d->name == 0 || d->icode == CODE_FOR_nothing)
	continue;
      
      mode0 = insn_data[d->icode].operand[0].mode;
      mode1 = insn_data[d->icode].operand[1].mode;
      mode2 = insn_data[d->icode].operand[2].mode;
      mode3 = insn_data[d->icode].operand[3].mode;
      
      /* When all four are of the same mode.  */
      if (mode0 == mode1 && mode1 == mode2 && mode2 == mode3)
	{
	  switch (mode0)
	    {
	    case V4SImode:
	      type = v4si_ftype_v4si_v4si_v4si;
	      break;
	    case V4SFmode:
	      type = v4sf_ftype_v4sf_v4sf_v4sf;
	      break;
	    case V8HImode:
	      type = v8hi_ftype_v8hi_v8hi_v8hi;
	      break;	      
	    case V16QImode:
	      type = v16qi_ftype_v16qi_v16qi_v16qi;
	      break;	      
	    default:
	      abort();	      
	    }
	}
      else if (mode0 == mode1 && mode1 == mode2 && mode3 == V16QImode)
        {
	  switch (mode0)
	    {
	    case V4SImode:
	      type = v4si_ftype_v4si_v4si_v16qi;
	      break;
	    case V4SFmode:
	      type = v4sf_ftype_v4sf_v4sf_v16qi;
	      break;
	    case V8HImode:
	      type = v8hi_ftype_v8hi_v8hi_v16qi;
	      break;	      
	    case V16QImode:
	      type = v16qi_ftype_v16qi_v16qi_v16qi;
	      break;	      
	    default:
	      abort();	      
	    }
	}
      else if (mode0 == V4SImode && mode1 == V16QImode && mode2 == V16QImode 
	       && mode3 == V4SImode)
	type = v4si_ftype_v16qi_v16qi_v4si;
      else if (mode0 == V4SImode && mode1 == V8HImode && mode2 == V8HImode 
	       && mode3 == V4SImode)
	type = v4si_ftype_v8hi_v8hi_v4si;
      else if (mode0 == V4SFmode && mode1 == V4SFmode && mode2 == V4SFmode 
	       && mode3 == V4SImode)
	type = v4sf_ftype_v4sf_v4sf_v4si;

      /* vchar, vchar, vchar, 4 bit literal.  */
      else if (mode0 == V16QImode && mode1 == mode0 && mode2 == mode0
	       && mode3 == QImode)
	type = v16qi_ftype_v16qi_v16qi_int;

      /* vshort, vshort, vshort, 4 bit literal.  */
      else if (mode0 == V8HImode && mode1 == mode0 && mode2 == mode0
	       && mode3 == QImode)
	type = v8hi_ftype_v8hi_v8hi_int;

      /* vint, vint, vint, 4 bit literal.  */
      else if (mode0 == V4SImode && mode1 == mode0 && mode2 == mode0
	       && mode3 == QImode)
	type = v4si_ftype_v4si_v4si_int;

      /* vfloat, vfloat, vfloat, 4 bit literal.  */
      else if (mode0 == V4SFmode && mode1 == mode0 && mode2 == mode0
	       && mode3 == QImode)
	type = v4sf_ftype_v4sf_v4sf_int;

      else
	abort ();

      def_builtin (d->mask, d->name, type, d->code);
    }

  /* Add the simple binary operators.  */
  d = (struct builtin_description *) bdesc_2arg;
  for (i = 0; i < ARRAY_SIZE (bdesc_2arg); i++, d++)
    {
      enum machine_mode mode0, mode1, mode2;
      tree type;

      if (d->name == 0 || d->icode == CODE_FOR_nothing)
	continue;
      
      mode0 = insn_data[d->icode].operand[0].mode;
      mode1 = insn_data[d->icode].operand[1].mode;
      mode2 = insn_data[d->icode].operand[2].mode;

      /* When all three operands are of the same mode.  */
      if (mode0 == mode1 && mode1 == mode2)
	{
	  switch (mode0)
	    {
	    case V4SFmode:
	      type = v4sf_ftype_v4sf_v4sf;
	      break;
	    case V4SImode:
	      type = v4si_ftype_v4si_v4si;
	      break;
	    case V16QImode:
	      type = v16qi_ftype_v16qi_v16qi;
	      break;
	    case V8HImode:
	      type = v8hi_ftype_v8hi_v8hi;
	      break;
	    case V2SImode:
	      type = v2si_ftype_v2si_v2si;
	      break;
	    case V2SFmode:
	      type = v2sf_ftype_v2sf_v2sf;
	      break;
	    case SImode:
	      type = int_ftype_int_int;
	      break;
	    default:
	      abort ();
	    }
	}

      /* A few other combos we really don't want to do manually.  */

      /* vint, vfloat, vfloat.  */
      else if (mode0 == V4SImode && mode1 == V4SFmode && mode2 == V4SFmode)
	type = v4si_ftype_v4sf_v4sf;

      /* vshort, vchar, vchar.  */
      else if (mode0 == V8HImode && mode1 == V16QImode && mode2 == V16QImode)
	type = v8hi_ftype_v16qi_v16qi;

      /* vint, vshort, vshort.  */
      else if (mode0 == V4SImode && mode1 == V8HImode && mode2 == V8HImode)
	type = v4si_ftype_v8hi_v8hi;

      /* vshort, vint, vint.  */
      else if (mode0 == V8HImode && mode1 == V4SImode && mode2 == V4SImode)
	type = v8hi_ftype_v4si_v4si;

      /* vchar, vshort, vshort.  */
      else if (mode0 == V16QImode && mode1 == V8HImode && mode2 == V8HImode)
	type = v16qi_ftype_v8hi_v8hi;

      /* vint, vchar, vint.  */
      else if (mode0 == V4SImode && mode1 == V16QImode && mode2 == V4SImode)
	type = v4si_ftype_v16qi_v4si;

      /* vint, vchar, vchar.  */
      else if (mode0 == V4SImode && mode1 == V16QImode && mode2 == V16QImode)
	type = v4si_ftype_v16qi_v16qi;

      /* vint, vshort, vint.  */
      else if (mode0 == V4SImode && mode1 == V8HImode && mode2 == V4SImode)
	type = v4si_ftype_v8hi_v4si;
      
      /* vint, vint, 5 bit literal.  */
      else if (mode0 == V4SImode && mode1 == V4SImode && mode2 == QImode)
	type = v4si_ftype_v4si_int;
      
      /* vshort, vshort, 5 bit literal.  */
      else if (mode0 == V8HImode && mode1 == V8HImode && mode2 == QImode)
	type = v8hi_ftype_v8hi_int;
      
      /* vchar, vchar, 5 bit literal.  */
      else if (mode0 == V16QImode && mode1 == V16QImode && mode2 == QImode)
	type = v16qi_ftype_v16qi_int;

      /* vfloat, vint, 5 bit literal.  */
      else if (mode0 == V4SFmode && mode1 == V4SImode && mode2 == QImode)
	type = v4sf_ftype_v4si_int;
      
      /* vint, vfloat, 5 bit literal.  */
      else if (mode0 == V4SImode && mode1 == V4SFmode && mode2 == QImode)
	type = v4si_ftype_v4sf_int;

      else if (mode0 == V2SImode && mode1 == SImode && mode2 == SImode)
	type = v2si_ftype_int_int;

      else if (mode0 == V2SImode && mode1 == V2SImode && mode2 == QImode)
	type = v2si_ftype_v2si_char;

      else if (mode0 == V2SImode && mode1 == SImode && mode2 == QImode)
	type = v2si_ftype_int_char;

      /* int, x, x.  */
      else if (mode0 == SImode)
	{
	  switch (mode1)
	    {
	    case V4SImode:
	      type = int_ftype_v4si_v4si;
	      break;
	    case V4SFmode:
	      type = int_ftype_v4sf_v4sf;
	      break;
	    case V16QImode:
	      type = int_ftype_v16qi_v16qi;
	      break;
	    case V8HImode:
	      type = int_ftype_v8hi_v8hi;
	      break;
	    default:
	      abort ();
	    }
	}

      else
	abort ();

      def_builtin (d->mask, d->name, type, d->code);
    }

  /* Add the simple unary operators.  */
  d = (struct builtin_description *) bdesc_1arg;
  for (i = 0; i < ARRAY_SIZE (bdesc_1arg); i++, d++)
    {
      enum machine_mode mode0, mode1;
      tree type;

      if (d->name == 0 || d->icode == CODE_FOR_nothing)
	continue;
      
      mode0 = insn_data[d->icode].operand[0].mode;
      mode1 = insn_data[d->icode].operand[1].mode;

      if (mode0 == V4SImode && mode1 == QImode)
        type = v4si_ftype_int;
      else if (mode0 == V8HImode && mode1 == QImode)
        type = v8hi_ftype_int;
      else if (mode0 == V16QImode && mode1 == QImode)
        type = v16qi_ftype_int;
      else if (mode0 == V4SFmode && mode1 == V4SFmode)
	type = v4sf_ftype_v4sf;
      else if (mode0 == V8HImode && mode1 == V16QImode)
	type = v8hi_ftype_v16qi;
      else if (mode0 == V4SImode && mode1 == V8HImode)
	type = v4si_ftype_v8hi;
      else if (mode0 == V2SImode && mode1 == V2SImode)
	type = v2si_ftype_v2si;
      else if (mode0 == V2SFmode && mode1 == V2SFmode)
	type = v2sf_ftype_v2sf;
      else if (mode0 == V2SFmode && mode1 == V2SImode)
	type = v2sf_ftype_v2si;
      else if (mode0 == V2SImode && mode1 == V2SFmode)
	type = v2si_ftype_v2sf;
      else if (mode0 == V2SImode && mode1 == QImode)
	type = v2si_ftype_char;
      else
	abort ();

      def_builtin (d->mask, d->name, type, d->code);
    }
}

static void
rs6000_init_libfuncs (void)
{
  if (!TARGET_HARD_FLOAT)
    return;

  if (DEFAULT_ABI != ABI_V4)
    {
      if (TARGET_XCOFF && ! TARGET_POWER2 && ! TARGET_POWERPC)
	{
	  /* AIX library routines for float->int conversion.  */
	  set_conv_libfunc (sfix_optab, SImode, DFmode, "__itrunc");
	  set_conv_libfunc (ufix_optab, SImode, DFmode, "__uitrunc");
	  set_conv_libfunc (sfix_optab, SImode, TFmode, "_qitrunc");
	  set_conv_libfunc (ufix_optab, SImode, TFmode, "_quitrunc");
	}

      /* AIX/Darwin/64-bit Linux quad floating point routines.  */
      if (!TARGET_XL_COMPAT)
	{
	  set_optab_libfunc (add_optab, TFmode, "__gcc_qadd");
	  set_optab_libfunc (sub_optab, TFmode, "__gcc_qsub");
	  set_optab_libfunc (smul_optab, TFmode, "__gcc_qmul");
	  set_optab_libfunc (sdiv_optab, TFmode, "__gcc_qdiv");
	}
      else
	{
	  set_optab_libfunc (add_optab, TFmode, "_xlqadd");
	  set_optab_libfunc (sub_optab, TFmode, "_xlqsub");
	  set_optab_libfunc (smul_optab, TFmode, "_xlqmul");
	  set_optab_libfunc (sdiv_optab, TFmode, "_xlqdiv");
	}
    }
  else
    {
      /* 32-bit SVR4 quad floating point routines.  */

      set_optab_libfunc (add_optab, TFmode, "_q_add");
      set_optab_libfunc (sub_optab, TFmode, "_q_sub");
      set_optab_libfunc (neg_optab, TFmode, "_q_neg");
      set_optab_libfunc (smul_optab, TFmode, "_q_mul");
      set_optab_libfunc (sdiv_optab, TFmode, "_q_div");
      if (TARGET_PPC_GPOPT || TARGET_POWER2)
	set_optab_libfunc (sqrt_optab, TFmode, "_q_sqrt");

      set_optab_libfunc (eq_optab, TFmode, "_q_feq");
      set_optab_libfunc (ne_optab, TFmode, "_q_fne");
      set_optab_libfunc (gt_optab, TFmode, "_q_fgt");
      set_optab_libfunc (ge_optab, TFmode, "_q_fge");
      set_optab_libfunc (lt_optab, TFmode, "_q_flt");
      set_optab_libfunc (le_optab, TFmode, "_q_fle");

      set_conv_libfunc (sext_optab, TFmode, SFmode, "_q_stoq");
      set_conv_libfunc (sext_optab, TFmode, DFmode, "_q_dtoq");
      set_conv_libfunc (trunc_optab, SFmode, TFmode, "_q_qtos");
      set_conv_libfunc (trunc_optab, DFmode, TFmode, "_q_qtod");
      set_conv_libfunc (sfix_optab, SImode, TFmode, "_q_qtoi");
      set_conv_libfunc (ufix_optab, SImode, TFmode, "_q_qtou");
      set_conv_libfunc (sfloat_optab, TFmode, SImode, "_q_itoq");
    }
}

/* Expand a block move operation, and return 1 if successful.  Return 0
   if we should let the compiler generate normal code.

   operands[0] is the destination
   operands[1] is the source
   operands[2] is the length
   operands[3] is the alignment */

#define MAX_MOVE_REG 4

int
expand_block_move (rtx operands[])
{
  rtx orig_dest = operands[0];
  rtx orig_src	= operands[1];
  rtx bytes_rtx	= operands[2];
  rtx align_rtx = operands[3];
  int constp	= (GET_CODE (bytes_rtx) == CONST_INT);
  int align;
  int bytes;
  int offset;
  int move_bytes;
  rtx stores[MAX_MOVE_REG];
  int num_reg = 0;

  /* If this is not a fixed size move, just call memcpy */
  if (! constp)
    return 0;

  /* If this is not a fixed size alignment, abort */
  if (GET_CODE (align_rtx) != CONST_INT)
    abort ();
  align = INTVAL (align_rtx);

  /* Anything to move? */
  bytes = INTVAL (bytes_rtx);
  if (bytes <= 0)
    return 1;

  /* store_one_arg depends on expand_block_move to handle at least the size of
     reg_parm_stack_space.  */	
  if (bytes > (TARGET_POWERPC64 ? 64 : 32))
    return 0;

  for (offset = 0; bytes > 0; offset += move_bytes, bytes -= move_bytes)
    {
      union {
	rtx (*movstrsi) (rtx, rtx, rtx, rtx);
	rtx (*mov) (rtx, rtx);
      } gen_func;
      enum machine_mode mode = BLKmode;
      rtx src, dest;
      
      if (TARGET_STRING
	  && bytes > 24		/* move up to 32 bytes at a time */
	  && ! fixed_regs[5]
	  && ! fixed_regs[6]
	  && ! fixed_regs[7]
	  && ! fixed_regs[8]
	  && ! fixed_regs[9]
	  && ! fixed_regs[10]
	  && ! fixed_regs[11]
	  && ! fixed_regs[12])
	{
	  move_bytes = (bytes > 32) ? 32 : bytes;
	  gen_func.movstrsi = gen_movstrsi_8reg;
	}
      else if (TARGET_STRING
	       && bytes > 16	/* move up to 24 bytes at a time */
	       && ! fixed_regs[5]
	       && ! fixed_regs[6]
	       && ! fixed_regs[7]
	       && ! fixed_regs[8]
	       && ! fixed_regs[9]
	       && ! fixed_regs[10])
	{
	  move_bytes = (bytes > 24) ? 24 : bytes;
	  gen_func.movstrsi = gen_movstrsi_6reg;
	}
      else if (TARGET_STRING
	       && bytes > 8	/* move up to 16 bytes at a time */
	       && ! fixed_regs[5]
	       && ! fixed_regs[6]
	       && ! fixed_regs[7]
	       && ! fixed_regs[8])
	{
	  move_bytes = (bytes > 16) ? 16 : bytes;
	  gen_func.movstrsi = gen_movstrsi_4reg;
	}
      else if (bytes >= 8 && TARGET_POWERPC64
	       /* 64-bit loads and stores require word-aligned
		  displacements.  */
	       && (align >= 8 || (! STRICT_ALIGNMENT && align >= 4)))
	{
	  move_bytes = 8;
	  mode = DImode;
	  gen_func.mov = gen_movdi;
	}
      else if (TARGET_STRING && bytes > 4 && !TARGET_POWERPC64)
	{			/* move up to 8 bytes at a time */
	  move_bytes = (bytes > 8) ? 8 : bytes;
	  gen_func.movstrsi = gen_movstrsi_2reg;
	}
      else if (bytes >= 4 && (align >= 4 || ! STRICT_ALIGNMENT))
	{			/* move 4 bytes */
	  move_bytes = 4;
	  mode = SImode;
	  gen_func.mov = gen_movsi;
	}
      else if (bytes >= 2 && (align >= 2 || ! STRICT_ALIGNMENT))
	{			/* move 2 bytes */
	  move_bytes = 2;
	  mode = HImode;
	  gen_func.mov = gen_movhi;
	}
      else if (TARGET_STRING && bytes > 1)
	{			/* move up to 4 bytes at a time */
	  move_bytes = (bytes > 4) ? 4 : bytes;
	  gen_func.movstrsi = gen_movstrsi_1reg;
	}
      else /* move 1 byte at a time */
	{
	  move_bytes = 1;
	  mode = QImode;
	  gen_func.mov = gen_movqi;
	}
      
      src = adjust_address (orig_src, mode, offset);
      dest = adjust_address (orig_dest, mode, offset);
      
      if (mode != BLKmode) 
	{
	  rtx tmp_reg = gen_reg_rtx (mode);
	  
	  emit_insn ((*gen_func.mov) (tmp_reg, src));
	  stores[num_reg++] = (*gen_func.mov) (dest, tmp_reg);
	}

      if (mode == BLKmode || num_reg >= MAX_MOVE_REG || bytes == move_bytes)
	{
	  int i;
	  for (i = 0; i < num_reg; i++)
	    emit_insn (stores[i]);
	  num_reg = 0;
	}

      if (mode == BLKmode)
	{
	  /* Move the address into scratch registers.  The movstrsi
	     patterns require zero offset.  */
	  if (!REG_P (XEXP (src, 0)))
	    {
	      rtx src_reg = copy_addr_to_reg (XEXP (src, 0));
	      src = replace_equiv_address (src, src_reg);
	    }
	  set_mem_size (src, GEN_INT (move_bytes));
	  
	  if (!REG_P (XEXP (dest, 0)))
	    {
	      rtx dest_reg = copy_addr_to_reg (XEXP (dest, 0));
	      dest = replace_equiv_address (dest, dest_reg);
	    }
	  set_mem_size (dest, GEN_INT (move_bytes));
	  
	  emit_insn ((*gen_func.movstrsi) (dest, src,
					   GEN_INT (move_bytes & 31),
					   align_rtx));
	}
    }

  return 1;
}


/* Return 1 if OP is a load multiple operation.  It is known to be a
   PARALLEL and the first section will be tested.  */

int
load_multiple_operation (rtx op, enum machine_mode mode ATTRIBUTE_UNUSED)
{
  int count = XVECLEN (op, 0);
  unsigned int dest_regno;
  rtx src_addr;
  int i;

  /* Perform a quick check so we don't blow up below.  */
  if (count <= 1
      || GET_CODE (XVECEXP (op, 0, 0)) != SET
      || GET_CODE (SET_DEST (XVECEXP (op, 0, 0))) != REG
      || GET_CODE (SET_SRC (XVECEXP (op, 0, 0))) != MEM)
    return 0;

  dest_regno = REGNO (SET_DEST (XVECEXP (op, 0, 0)));
  src_addr = XEXP (SET_SRC (XVECEXP (op, 0, 0)), 0);

  for (i = 1; i < count; i++)
    {
      rtx elt = XVECEXP (op, 0, i);

      if (GET_CODE (elt) != SET
	  || GET_CODE (SET_DEST (elt)) != REG
	  || GET_MODE (SET_DEST (elt)) != SImode
	  || REGNO (SET_DEST (elt)) != dest_regno + i
	  || GET_CODE (SET_SRC (elt)) != MEM
	  || GET_MODE (SET_SRC (elt)) != SImode
	  || GET_CODE (XEXP (SET_SRC (elt), 0)) != PLUS
	  || ! rtx_equal_p (XEXP (XEXP (SET_SRC (elt), 0), 0), src_addr)
	  || GET_CODE (XEXP (XEXP (SET_SRC (elt), 0), 1)) != CONST_INT
	  || INTVAL (XEXP (XEXP (SET_SRC (elt), 0), 1)) != i * 4)
	return 0;
    }

  return 1;
}

/* Similar, but tests for store multiple.  Here, the second vector element
   is a CLOBBER.  It will be tested later.  */

int
store_multiple_operation (rtx op, enum machine_mode mode ATTRIBUTE_UNUSED)
{
  int count = XVECLEN (op, 0) - 1;
  unsigned int src_regno;
  rtx dest_addr;
  int i;

  /* Perform a quick check so we don't blow up below.  */
  if (count <= 1
      || GET_CODE (XVECEXP (op, 0, 0)) != SET
      || GET_CODE (SET_DEST (XVECEXP (op, 0, 0))) != MEM
      || GET_CODE (SET_SRC (XVECEXP (op, 0, 0))) != REG)
    return 0;

  src_regno = REGNO (SET_SRC (XVECEXP (op, 0, 0)));
  dest_addr = XEXP (SET_DEST (XVECEXP (op, 0, 0)), 0);

  for (i = 1; i < count; i++)
    {
      rtx elt = XVECEXP (op, 0, i + 1);

      if (GET_CODE (elt) != SET
	  || GET_CODE (SET_SRC (elt)) != REG
	  || GET_MODE (SET_SRC (elt)) != SImode
	  || REGNO (SET_SRC (elt)) != src_regno + i
	  || GET_CODE (SET_DEST (elt)) != MEM
	  || GET_MODE (SET_DEST (elt)) != SImode
	  || GET_CODE (XEXP (SET_DEST (elt), 0)) != PLUS
	  || ! rtx_equal_p (XEXP (XEXP (SET_DEST (elt), 0), 0), dest_addr)
	  || GET_CODE (XEXP (XEXP (SET_DEST (elt), 0), 1)) != CONST_INT
	  || INTVAL (XEXP (XEXP (SET_DEST (elt), 0), 1)) != i * 4)
	return 0;
    }

  return 1;
}

/* Return a string to perform a load_multiple operation.
   operands[0] is the vector.
   operands[1] is the source address.
   operands[2] is the first destination register.  */

const char *
rs6000_output_load_multiple (rtx operands[3])
{
  /* We have to handle the case where the pseudo used to contain the address
     is assigned to one of the output registers.  */
  int i, j;
  int words = XVECLEN (operands[0], 0);
  rtx xop[10];

  if (XVECLEN (operands[0], 0) == 1)
    return "{l|lwz} %2,0(%1)";

  for (i = 0; i < words; i++)
    if (refers_to_regno_p (REGNO (operands[2]) + i,
			   REGNO (operands[2]) + i + 1, operands[1], 0))
      {
	if (i == words-1)
	  {
	    xop[0] = GEN_INT (4 * (words-1));
	    xop[1] = operands[1];
	    xop[2] = operands[2];
	    output_asm_insn ("{lsi|lswi} %2,%1,%0\n\t{l|lwz} %1,%0(%1)", xop);
	    return "";
	  }
	else if (i == 0)
	  {
	    xop[0] = GEN_INT (4 * (words-1));
	    xop[1] = operands[1];
	    xop[2] = gen_rtx_REG (SImode, REGNO (operands[2]) + 1);
	    output_asm_insn ("{cal %1,4(%1)|addi %1,%1,4}\n\t{lsi|lswi} %2,%1,%0\n\t{l|lwz} %1,-4(%1)", xop);
	    return "";
	  }
	else
	  {
	    for (j = 0; j < words; j++)
	      if (j != i)
		{
		  xop[0] = GEN_INT (j * 4);
		  xop[1] = operands[1];
		  xop[2] = gen_rtx_REG (SImode, REGNO (operands[2]) + j);
		  output_asm_insn ("{l|lwz} %2,%0(%1)", xop);
		}
	    xop[0] = GEN_INT (i * 4);
	    xop[1] = operands[1];
	    output_asm_insn ("{l|lwz} %1,%0(%1)", xop);
	    return "";
	  }
      }

  return "{lsi|lswi} %2,%1,%N0";
}

/* Return 1 for a parallel vrsave operation.  */

int
vrsave_operation (rtx op, enum machine_mode mode ATTRIBUTE_UNUSED)
{
  int count = XVECLEN (op, 0);
  unsigned int dest_regno, src_regno;
  int i;

  if (count <= 1
      || GET_CODE (XVECEXP (op, 0, 0)) != SET
      || GET_CODE (SET_DEST (XVECEXP (op, 0, 0))) != REG
      || GET_CODE (SET_SRC (XVECEXP (op, 0, 0))) != UNSPEC_VOLATILE)
    return 0;

  dest_regno = REGNO (SET_DEST (XVECEXP (op, 0, 0)));
  src_regno  = REGNO (SET_SRC (XVECEXP (op, 0, 0)));

  if (dest_regno != VRSAVE_REGNO
      && src_regno != VRSAVE_REGNO)
    return 0;

  for (i = 1; i < count; i++)
    {
      rtx elt = XVECEXP (op, 0, i);

      if (GET_CODE (elt) != CLOBBER
	  && GET_CODE (elt) != SET)
	return 0;
    }

  return 1;
}

/* Return 1 for an PARALLEL suitable for mfcr.  */

int
mfcr_operation (rtx op, enum machine_mode mode ATTRIBUTE_UNUSED)
{
  int count = XVECLEN (op, 0);
  int i;

  /* Perform a quick check so we don't blow up below.  */
  if (count < 1
      || GET_CODE (XVECEXP (op, 0, 0)) != SET
      || GET_CODE (SET_SRC (XVECEXP (op, 0, 0))) != UNSPEC
      || XVECLEN (SET_SRC (XVECEXP (op, 0, 0)), 0) != 2)
    return 0;

  for (i = 0; i < count; i++)
    {
      rtx exp = XVECEXP (op, 0, i);
      rtx unspec;
      int maskval;
      rtx src_reg;

      src_reg = XVECEXP (SET_SRC (exp), 0, 0);

      if (GET_CODE (src_reg) != REG
	  || GET_MODE (src_reg) != CCmode
	  || ! CR_REGNO_P (REGNO (src_reg)))
	return 0;

      if (GET_CODE (exp) != SET
	  || GET_CODE (SET_DEST (exp)) != REG
	  || GET_MODE (SET_DEST (exp)) != SImode
	  || ! INT_REGNO_P (REGNO (SET_DEST (exp))))
	return 0;
      unspec = SET_SRC (exp);
      maskval = 1 << (MAX_CR_REGNO - REGNO (src_reg));

      if (GET_CODE (unspec) != UNSPEC
	  || XINT (unspec, 1) != UNSPEC_MOVESI_FROM_CR
	  || XVECLEN (unspec, 0) != 2
	  || XVECEXP (unspec, 0, 0) != src_reg
	  || GET_CODE (XVECEXP (unspec, 0, 1)) != CONST_INT
	  || INTVAL (XVECEXP (unspec, 0, 1)) != maskval)
	return 0;
    }
  return 1;
}

/* Return 1 for an PARALLEL suitable for mtcrf.  */

int
mtcrf_operation (rtx op, enum machine_mode mode ATTRIBUTE_UNUSED)
{
  int count = XVECLEN (op, 0);
  int i;
  rtx src_reg;

  /* Perform a quick check so we don't blow up below.  */
  if (count < 1
      || GET_CODE (XVECEXP (op, 0, 0)) != SET
      || GET_CODE (SET_SRC (XVECEXP (op, 0, 0))) != UNSPEC
      || XVECLEN (SET_SRC (XVECEXP (op, 0, 0)), 0) != 2)
    return 0;
  src_reg = XVECEXP (SET_SRC (XVECEXP (op, 0, 0)), 0, 0);
  
  if (GET_CODE (src_reg) != REG
      || GET_MODE (src_reg) != SImode
      || ! INT_REGNO_P (REGNO (src_reg)))
    return 0;

  for (i = 0; i < count; i++)
    {
      rtx exp = XVECEXP (op, 0, i);
      rtx unspec;
      int maskval;
      
      if (GET_CODE (exp) != SET
	  || GET_CODE (SET_DEST (exp)) != REG
	  || GET_MODE (SET_DEST (exp)) != CCmode
	  || ! CR_REGNO_P (REGNO (SET_DEST (exp))))
	return 0;
      unspec = SET_SRC (exp);
      maskval = 1 << (MAX_CR_REGNO - REGNO (SET_DEST (exp)));
      
      if (GET_CODE (unspec) != UNSPEC
	  || XINT (unspec, 1) != UNSPEC_MOVESI_TO_CR
	  || XVECLEN (unspec, 0) != 2
	  || XVECEXP (unspec, 0, 0) != src_reg
	  || GET_CODE (XVECEXP (unspec, 0, 1)) != CONST_INT
	  || INTVAL (XVECEXP (unspec, 0, 1)) != maskval)
	return 0;
    }
  return 1;
}

/* Return 1 for an PARALLEL suitable for lmw.  */

int
lmw_operation (rtx op, enum machine_mode mode ATTRIBUTE_UNUSED)
{
  int count = XVECLEN (op, 0);
  unsigned int dest_regno;
  rtx src_addr;
  unsigned int base_regno;
  HOST_WIDE_INT offset;
  int i;

  /* Perform a quick check so we don't blow up below.  */
  if (count <= 1
      || GET_CODE (XVECEXP (op, 0, 0)) != SET
      || GET_CODE (SET_DEST (XVECEXP (op, 0, 0))) != REG
      || GET_CODE (SET_SRC (XVECEXP (op, 0, 0))) != MEM)
    return 0;

  dest_regno = REGNO (SET_DEST (XVECEXP (op, 0, 0)));
  src_addr = XEXP (SET_SRC (XVECEXP (op, 0, 0)), 0);

  if (dest_regno > 31
      || count != 32 - (int) dest_regno)
    return 0;

  if (legitimate_indirect_address_p (src_addr, 0))
    {
      offset = 0;
      base_regno = REGNO (src_addr);
      if (base_regno == 0)
	return 0;
    }
  else if (legitimate_offset_address_p (SImode, src_addr, 0))
    {
      offset = INTVAL (XEXP (src_addr, 1));
      base_regno = REGNO (XEXP (src_addr, 0));
    }
  else
    return 0;

  for (i = 0; i < count; i++)
    {
      rtx elt = XVECEXP (op, 0, i);
      rtx newaddr;
      rtx addr_reg;
      HOST_WIDE_INT newoffset;

      if (GET_CODE (elt) != SET
	  || GET_CODE (SET_DEST (elt)) != REG
	  || GET_MODE (SET_DEST (elt)) != SImode
	  || REGNO (SET_DEST (elt)) != dest_regno + i
	  || GET_CODE (SET_SRC (elt)) != MEM
	  || GET_MODE (SET_SRC (elt)) != SImode)
	return 0;
      newaddr = XEXP (SET_SRC (elt), 0);
      if (legitimate_indirect_address_p (newaddr, 0))
	{
	  newoffset = 0;
	  addr_reg = newaddr;
	}
      else if (legitimate_offset_address_p (SImode, newaddr, 0))
	{
	  addr_reg = XEXP (newaddr, 0);
	  newoffset = INTVAL (XEXP (newaddr, 1));
	}
      else
	return 0;
      if (REGNO (addr_reg) != base_regno
	  || newoffset != offset + 4 * i)
	return 0;
    }

  return 1;
}

/* Return 1 for an PARALLEL suitable for stmw.  */

int
stmw_operation (rtx op, enum machine_mode mode ATTRIBUTE_UNUSED)
{
  int count = XVECLEN (op, 0);
  unsigned int src_regno;
  rtx dest_addr;
  unsigned int base_regno;
  HOST_WIDE_INT offset;
  int i;

  /* Perform a quick check so we don't blow up below.  */
  if (count <= 1
      || GET_CODE (XVECEXP (op, 0, 0)) != SET
      || GET_CODE (SET_DEST (XVECEXP (op, 0, 0))) != MEM
      || GET_CODE (SET_SRC (XVECEXP (op, 0, 0))) != REG)
    return 0;

  src_regno = REGNO (SET_SRC (XVECEXP (op, 0, 0)));
  dest_addr = XEXP (SET_DEST (XVECEXP (op, 0, 0)), 0);

  if (src_regno > 31
      || count != 32 - (int) src_regno)
    return 0;

  if (legitimate_indirect_address_p (dest_addr, 0))
    {
      offset = 0;
      base_regno = REGNO (dest_addr);
      if (base_regno == 0)
	return 0;
    }
  else if (legitimate_offset_address_p (SImode, dest_addr, 0))
    {
      offset = INTVAL (XEXP (dest_addr, 1));
      base_regno = REGNO (XEXP (dest_addr, 0));
    }
  else
    return 0;

  for (i = 0; i < count; i++)
    {
      rtx elt = XVECEXP (op, 0, i);
      rtx newaddr;
      rtx addr_reg;
      HOST_WIDE_INT newoffset;

      if (GET_CODE (elt) != SET
	  || GET_CODE (SET_SRC (elt)) != REG
	  || GET_MODE (SET_SRC (elt)) != SImode
	  || REGNO (SET_SRC (elt)) != src_regno + i
	  || GET_CODE (SET_DEST (elt)) != MEM
	  || GET_MODE (SET_DEST (elt)) != SImode)
	return 0;
      newaddr = XEXP (SET_DEST (elt), 0);
      if (legitimate_indirect_address_p (newaddr, 0))
	{
	  newoffset = 0;
	  addr_reg = newaddr;
	}
      else if (legitimate_offset_address_p (SImode, newaddr, 0))
	{
	  addr_reg = XEXP (newaddr, 0);
	  newoffset = INTVAL (XEXP (newaddr, 1));
	}
      else
	return 0;
      if (REGNO (addr_reg) != base_regno
	  || newoffset != offset + 4 * i)
	return 0;
    }

  return 1;
}

/* A validation routine: say whether CODE, a condition code, and MODE
   match.  The other alternatives either don't make sense or should
   never be generated.  */

static void
validate_condition_mode (enum rtx_code code, enum machine_mode mode)
{
  if (GET_RTX_CLASS (code) != '<' 
      || GET_MODE_CLASS (mode) != MODE_CC)
    abort ();

  /* These don't make sense.  */
  if ((code == GT || code == LT || code == GE || code == LE)
      && mode == CCUNSmode)
    abort ();

  if ((code == GTU || code == LTU || code == GEU || code == LEU)
      && mode != CCUNSmode)
    abort ();

  if (mode != CCFPmode
      && (code == ORDERED || code == UNORDERED
	  || code == UNEQ || code == LTGT
	  || code == UNGT || code == UNLT
	  || code == UNGE || code == UNLE))
    abort ();
  
  /* These should never be generated except for 
     flag_finite_math_only.  */
  if (mode == CCFPmode
      && ! flag_finite_math_only
      && (code == LE || code == GE
	  || code == UNEQ || code == LTGT
	  || code == UNGT || code == UNLT))
    abort ();

  /* These are invalid; the information is not there.  */
  if (mode == CCEQmode 
      && code != EQ && code != NE)
    abort ();
}

/* Return 1 if OP is a comparison operation that is valid for a branch insn.
   We only check the opcode against the mode of the CC value here.  */

int
branch_comparison_operator (rtx op, enum machine_mode mode ATTRIBUTE_UNUSED)
{
  enum rtx_code code = GET_CODE (op);
  enum machine_mode cc_mode;

  if (GET_RTX_CLASS (code) != '<')
    return 0;

  cc_mode = GET_MODE (XEXP (op, 0));
  if (GET_MODE_CLASS (cc_mode) != MODE_CC)
    return 0;

  validate_condition_mode (code, cc_mode);

  return 1;
}

/* Return 1 if OP is a comparison operation that is valid for a branch
   insn and which is true if the corresponding bit in the CC register
   is set.  */

int
branch_positive_comparison_operator (rtx op, enum machine_mode mode)
{
  enum rtx_code code;

  if (! branch_comparison_operator (op, mode))
    return 0;

  code = GET_CODE (op);
  return (code == EQ || code == LT || code == GT
	  || code == LTU || code == GTU
	  || code == UNORDERED);
}

/* Return 1 if OP is a comparison operation that is valid for an scc
   insn: it must be a positive comparison.  */

int
scc_comparison_operator (rtx op, enum machine_mode mode)
{
  return branch_positive_comparison_operator (op, mode);
}

int
trap_comparison_operator (rtx op, enum machine_mode mode)
{
  if (mode != VOIDmode && mode != GET_MODE (op))
    return 0;
  return GET_RTX_CLASS (GET_CODE (op)) == '<';
}

int
boolean_operator (rtx op, enum machine_mode mode ATTRIBUTE_UNUSED)
{
  enum rtx_code code = GET_CODE (op);
  return (code == AND || code == IOR || code == XOR);
}

int
boolean_or_operator (rtx op, enum machine_mode mode ATTRIBUTE_UNUSED)
{
  enum rtx_code code = GET_CODE (op);
  return (code == IOR || code == XOR);
}

int
min_max_operator (rtx op, enum machine_mode mode ATTRIBUTE_UNUSED)
{
  enum rtx_code code = GET_CODE (op);
  return (code == SMIN || code == SMAX || code == UMIN || code == UMAX);
}

/* Return 1 if ANDOP is a mask that has no bits on that are not in the
   mask required to convert the result of a rotate insn into a shift
   left insn of SHIFTOP bits.  Both are known to be SImode CONST_INT.  */

int
includes_lshift_p (rtx shiftop, rtx andop)
{
  unsigned HOST_WIDE_INT shift_mask = ~(unsigned HOST_WIDE_INT) 0;

  shift_mask <<= INTVAL (shiftop);

  return (INTVAL (andop) & 0xffffffff & ~shift_mask) == 0;
}

/* Similar, but for right shift.  */

int
includes_rshift_p (rtx shiftop, rtx andop)
{
  unsigned HOST_WIDE_INT shift_mask = ~(unsigned HOST_WIDE_INT) 0;

  shift_mask >>= INTVAL (shiftop);

  return (INTVAL (andop) & 0xffffffff & ~shift_mask) == 0;
}

/* Return 1 if ANDOP is a mask suitable for use with an rldic insn
   to perform a left shift.  It must have exactly SHIFTOP least
   significant 0's, then one or more 1's, then zero or more 0's.  */

int
includes_rldic_lshift_p (rtx shiftop, rtx andop)
{
  if (GET_CODE (andop) == CONST_INT)
    {
      HOST_WIDE_INT c, lsb, shift_mask;

      c = INTVAL (andop);
      if (c == 0 || c == ~0)
	return 0;

      shift_mask = ~0;
      shift_mask <<= INTVAL (shiftop);

      /* Find the least significant one bit.  */
      lsb = c & -c;

      /* It must coincide with the LSB of the shift mask.  */
      if (-lsb != shift_mask)
	return 0;

      /* Invert to look for the next transition (if any).  */
      c = ~c;

      /* Remove the low group of ones (originally low group of zeros).  */
      c &= -lsb;

      /* Again find the lsb, and check we have all 1's above.  */
      lsb = c & -c;
      return c == -lsb;
    }
  else if (GET_CODE (andop) == CONST_DOUBLE
	   && (GET_MODE (andop) == VOIDmode || GET_MODE (andop) == DImode))
    {
      HOST_WIDE_INT low, high, lsb;
      HOST_WIDE_INT shift_mask_low, shift_mask_high;

      low = CONST_DOUBLE_LOW (andop);
      if (HOST_BITS_PER_WIDE_INT < 64)
	high = CONST_DOUBLE_HIGH (andop);

      if ((low == 0 && (HOST_BITS_PER_WIDE_INT >= 64 || high == 0))
	  || (low == ~0 && (HOST_BITS_PER_WIDE_INT >= 64 || high == ~0)))
	return 0;

      if (HOST_BITS_PER_WIDE_INT < 64 && low == 0)
	{
	  shift_mask_high = ~0;
	  if (INTVAL (shiftop) > 32)
	    shift_mask_high <<= INTVAL (shiftop) - 32;

	  lsb = high & -high;

	  if (-lsb != shift_mask_high || INTVAL (shiftop) < 32)
	    return 0;

	  high = ~high;
	  high &= -lsb;

	  lsb = high & -high;
	  return high == -lsb;
	}

      shift_mask_low = ~0;
      shift_mask_low <<= INTVAL (shiftop);

      lsb = low & -low;

      if (-lsb != shift_mask_low)
	return 0;

      if (HOST_BITS_PER_WIDE_INT < 64)
	high = ~high;
      low = ~low;
      low &= -lsb;

      if (HOST_BITS_PER_WIDE_INT < 64 && low == 0)
	{
	  lsb = high & -high;
	  return high == -lsb;
	}

      lsb = low & -low;
      return low == -lsb && (HOST_BITS_PER_WIDE_INT >= 64 || high == ~0);
    }
  else
    return 0;
}

/* Return 1 if ANDOP is a mask suitable for use with an rldicr insn
   to perform a left shift.  It must have SHIFTOP or more least
   significant 0's, with the remainder of the word 1's.  */

int
includes_rldicr_lshift_p (rtx shiftop, rtx andop)
{
  if (GET_CODE (andop) == CONST_INT)
    {
      HOST_WIDE_INT c, lsb, shift_mask;

      shift_mask = ~0;
      shift_mask <<= INTVAL (shiftop);
      c = INTVAL (andop);

      /* Find the least significant one bit.  */
      lsb = c & -c;

      /* It must be covered by the shift mask.
	 This test also rejects c == 0.  */
      if ((lsb & shift_mask) == 0)
	return 0;

      /* Check we have all 1's above the transition, and reject all 1's.  */
      return c == -lsb && lsb != 1;
    }
  else if (GET_CODE (andop) == CONST_DOUBLE
	   && (GET_MODE (andop) == VOIDmode || GET_MODE (andop) == DImode))
    {
      HOST_WIDE_INT low, lsb, shift_mask_low;

      low = CONST_DOUBLE_LOW (andop);

      if (HOST_BITS_PER_WIDE_INT < 64)
	{
	  HOST_WIDE_INT high, shift_mask_high;

	  high = CONST_DOUBLE_HIGH (andop);

	  if (low == 0)
	    {
	      shift_mask_high = ~0;
	      if (INTVAL (shiftop) > 32)
		shift_mask_high <<= INTVAL (shiftop) - 32;

	      lsb = high & -high;

	      if ((lsb & shift_mask_high) == 0)
		return 0;

	      return high == -lsb;
	    }
	  if (high != ~0)
	    return 0;
	}

      shift_mask_low = ~0;
      shift_mask_low <<= INTVAL (shiftop);

      lsb = low & -low;

      if ((lsb & shift_mask_low) == 0)
	return 0;

      return low == -lsb && lsb != 1;
    }
  else
    return 0;
}

/* Return 1 if REGNO (reg1) == REGNO (reg2) - 1 making them candidates
   for lfq and stfq insns.

   Note reg1 and reg2 *must* be hard registers.  To be sure we will
   abort if we are passed pseudo registers.  */

int
registers_ok_for_quad_peep (rtx reg1, rtx reg2)
{
  /* We might have been passed a SUBREG.  */
  if (GET_CODE (reg1) != REG || GET_CODE (reg2) != REG) 
    return 0;

  return (REGNO (reg1) == REGNO (reg2) - 1);
}

/* Return 1 if addr1 and addr2 are suitable for lfq or stfq insn.
   addr1 and addr2 must be in consecutive memory locations
   (addr2 == addr1 + 8).  */

int
addrs_ok_for_quad_peep (rtx addr1, rtx addr2)
{
  unsigned int reg1;
  int offset1;

  /* Extract an offset (if used) from the first addr.  */
  if (GET_CODE (addr1) == PLUS)
    {
      /* If not a REG, return zero.  */
      if (GET_CODE (XEXP (addr1, 0)) != REG)
	return 0;
      else
	{
          reg1 = REGNO (XEXP (addr1, 0));
	  /* The offset must be constant!  */
	  if (GET_CODE (XEXP (addr1, 1)) != CONST_INT)
            return 0;
          offset1 = INTVAL (XEXP (addr1, 1));
	}
    }
  else if (GET_CODE (addr1) != REG)
    return 0;
  else
    {
      reg1 = REGNO (addr1);
      /* This was a simple (mem (reg)) expression.  Offset is 0.  */
      offset1 = 0;
    }

  /* Make sure the second address is a (mem (plus (reg) (const_int)))
      or if it is (mem (reg)) then make sure that offset1 is -8 and the same 
      register as addr1.  */
  if (offset1 == -8 && GET_CODE (addr2) == REG && reg1 == REGNO (addr2))
   return 1;
  if (GET_CODE (addr2) != PLUS)
    return 0;

  if (GET_CODE (XEXP (addr2, 0)) != REG
      || GET_CODE (XEXP (addr2, 1)) != CONST_INT)
    return 0;

  if (reg1 != REGNO (XEXP (addr2, 0)))
    return 0;

  /* The offset for the second addr must be 8 more than the first addr.  */
  if (INTVAL (XEXP (addr2, 1)) != offset1 + 8)
    return 0;

  /* All the tests passed.  addr1 and addr2 are valid for lfq or stfq
     instructions.  */
  return 1;
}

/* Return the register class of a scratch register needed to copy IN into
   or out of a register in CLASS in MODE.  If it can be done directly,
   NO_REGS is returned.  */

enum reg_class
secondary_reload_class (enum reg_class class,
			enum machine_mode mode,
			rtx in)
{
  int regno;

  if (TARGET_ELF || (DEFAULT_ABI == ABI_DARWIN
#if TARGET_MACHO
                    && MACHOPIC_INDIRECT
#endif
                    ))
    {
      /* We cannot copy a symbolic operand directly into anything
         other than BASE_REGS for TARGET_ELF.  So indicate that a
         register from BASE_REGS is needed as an intermediate
         register.
         
	 On Darwin, pic addresses require a load from memory, which
	 needs a base register.  */
      if (class != BASE_REGS
          && (GET_CODE (in) == SYMBOL_REF
              || GET_CODE (in) == HIGH
              || GET_CODE (in) == LABEL_REF
              || GET_CODE (in) == CONST))
        return BASE_REGS;
    }

  if (GET_CODE (in) == REG)
    {
      regno = REGNO (in);
      if (regno >= FIRST_PSEUDO_REGISTER)
	{
	  regno = true_regnum (in);
	  if (regno >= FIRST_PSEUDO_REGISTER)
	    regno = -1;
	}
    }
  else if (GET_CODE (in) == SUBREG)
    {
      regno = true_regnum (in);
      if (regno >= FIRST_PSEUDO_REGISTER)
	regno = -1;
    }
  else
    regno = -1;

  /* We can place anything into GENERAL_REGS and can put GENERAL_REGS
     into anything.  */
  if (class == GENERAL_REGS || class == BASE_REGS
      || (regno >= 0 && INT_REGNO_P (regno)))
    return NO_REGS;

  /* Constants, memory, and FP registers can go into FP registers.  */
  if ((regno == -1 || FP_REGNO_P (regno))
      && (class == FLOAT_REGS || class == NON_SPECIAL_REGS))
    return NO_REGS;

  /* Memory, and AltiVec registers can go into AltiVec registers.  */
  if ((regno == -1 || ALTIVEC_REGNO_P (regno))
      && class == ALTIVEC_REGS)
    return NO_REGS;

  /* We can copy among the CR registers.  */
  if ((class == CR_REGS || class == CR0_REGS)
      && regno >= 0 && CR_REGNO_P (regno))
    return NO_REGS;

  /* Otherwise, we need GENERAL_REGS.  */
  return GENERAL_REGS;
}

/* Given a comparison operation, return the bit number in CCR to test.  We
   know this is a valid comparison.  

   SCC_P is 1 if this is for an scc.  That means that %D will have been
   used instead of %C, so the bits will be in different places.

   Return -1 if OP isn't a valid comparison for some reason.  */

int
ccr_bit (rtx op, int scc_p)
{
  enum rtx_code code = GET_CODE (op);
  enum machine_mode cc_mode;
  int cc_regnum;
  int base_bit;
  rtx reg;

  if (GET_RTX_CLASS (code) != '<')
    return -1;

  reg = XEXP (op, 0);

  if (GET_CODE (reg) != REG
      || ! CR_REGNO_P (REGNO (reg)))
    abort ();

  cc_mode = GET_MODE (reg);
  cc_regnum = REGNO (reg);
  base_bit = 4 * (cc_regnum - CR0_REGNO);

  validate_condition_mode (code, cc_mode);

  /* When generating a sCOND operation, only positive conditions are
     allowed.  */
  if (scc_p && code != EQ && code != GT && code != LT && code != UNORDERED
      && code != GTU && code != LTU)
    abort ();
  
  switch (code)
    {
    case NE:
      return scc_p ? base_bit + 3 : base_bit + 2;
    case EQ:
      return base_bit + 2;
    case GT:  case GTU:  case UNLE:
      return base_bit + 1;
    case LT:  case LTU:  case UNGE:
      return base_bit;
    case ORDERED:  case UNORDERED:
      return base_bit + 3;

    case GE:  case GEU:
      /* If scc, we will have done a cror to put the bit in the
	 unordered position.  So test that bit.  For integer, this is ! LT
	 unless this is an scc insn.  */
      return scc_p ? base_bit + 3 : base_bit;

    case LE:  case LEU:
      return scc_p ? base_bit + 3 : base_bit + 1;

    default:
      abort ();
    }
}

/* Return the GOT register.  */

struct rtx_def *
rs6000_got_register (rtx value ATTRIBUTE_UNUSED)
{
  /* The second flow pass currently (June 1999) can't update
     regs_ever_live without disturbing other parts of the compiler, so
     update it here to make the prolog/epilogue code happy.  */
  if (no_new_pseudos && ! regs_ever_live[RS6000_PIC_OFFSET_TABLE_REGNUM])
    regs_ever_live[RS6000_PIC_OFFSET_TABLE_REGNUM] = 1;

  current_function_uses_pic_offset_table = 1;

  return pic_offset_table_rtx;
}

/* Function to init struct machine_function.
   This will be called, via a pointer variable,
   from push_function_context.  */

static struct machine_function *
rs6000_init_machine_status (void)
{
  return ggc_alloc_cleared (sizeof (machine_function));
}

/* These macros test for integers and extract the low-order bits.  */
#define INT_P(X)  \
((GET_CODE (X) == CONST_INT || GET_CODE (X) == CONST_DOUBLE)	\
 && GET_MODE (X) == VOIDmode)

#define INT_LOWPART(X) \
  (GET_CODE (X) == CONST_INT ? INTVAL (X) : CONST_DOUBLE_LOW (X))

int
extract_MB (rtx op)
{
  int i;
  unsigned long val = INT_LOWPART (op);

  /* If the high bit is zero, the value is the first 1 bit we find
     from the left.  */
  if ((val & 0x80000000) == 0)
    {
      if ((val & 0xffffffff) == 0)
	abort ();

      i = 1;
      while (((val <<= 1) & 0x80000000) == 0)
	++i;
      return i;
    }

  /* If the high bit is set and the low bit is not, or the mask is all
     1's, the value is zero.  */
  if ((val & 1) == 0 || (val & 0xffffffff) == 0xffffffff)
    return 0;

  /* Otherwise we have a wrap-around mask.  Look for the first 0 bit
     from the right.  */
  i = 31;
  while (((val >>= 1) & 1) != 0)
    --i;

  return i;
}

int
extract_ME (rtx op)
{
  int i;
  unsigned long val = INT_LOWPART (op);

  /* If the low bit is zero, the value is the first 1 bit we find from
     the right.  */
  if ((val & 1) == 0)
    {
      if ((val & 0xffffffff) == 0)
	abort ();

      i = 30;
      while (((val >>= 1) & 1) == 0)
	--i;

      return i;
    }

  /* If the low bit is set and the high bit is not, or the mask is all
     1's, the value is 31.  */
  if ((val & 0x80000000) == 0 || (val & 0xffffffff) == 0xffffffff)
    return 31;

  /* Otherwise we have a wrap-around mask.  Look for the first 0 bit
     from the left.  */
  i = 0;
  while (((val <<= 1) & 0x80000000) != 0)
    ++i;

  return i;
}

/* Locate some local-dynamic symbol still in use by this function
   so that we can print its name in some tls_ld pattern.  */

static const char *
rs6000_get_some_local_dynamic_name (void)
{
  rtx insn;

  if (cfun->machine->some_ld_name)
    return cfun->machine->some_ld_name;

  for (insn = get_insns (); insn ; insn = NEXT_INSN (insn))
    if (INSN_P (insn)
	&& for_each_rtx (&PATTERN (insn),
			 rs6000_get_some_local_dynamic_name_1, 0))
      return cfun->machine->some_ld_name;

  abort ();
}

/* Helper function for rs6000_get_some_local_dynamic_name.  */

static int
rs6000_get_some_local_dynamic_name_1 (rtx *px, void *data ATTRIBUTE_UNUSED)
{
  rtx x = *px;

  if (GET_CODE (x) == SYMBOL_REF)
    {
      const char *str = XSTR (x, 0);
      if (SYMBOL_REF_TLS_MODEL (x) == TLS_MODEL_LOCAL_DYNAMIC)
	{
	  cfun->machine->some_ld_name = str;
	  return 1;
	}
    }

  return 0;
}

/* Print an operand.  Recognize special options, documented below.  */

#if TARGET_ELF
#define SMALL_DATA_RELOC ((rs6000_sdata == SDATA_EABI) ? "sda21" : "sdarel")
#define SMALL_DATA_REG ((rs6000_sdata == SDATA_EABI) ? 0 : 13)
#else
#define SMALL_DATA_RELOC "sda21"
#define SMALL_DATA_REG 0
#endif

void
print_operand (FILE *file, rtx x, int code)
{
  int i;
  HOST_WIDE_INT val;
  unsigned HOST_WIDE_INT uval;

  switch (code)
    {
    case '.':
      /* Write out an instruction after the call which may be replaced
	 with glue code by the loader.  This depends on the AIX version.  */
      asm_fprintf (file, RS6000_CALL_GLUE);
      return;

      /* %a is output_address.  */

    case 'A':
      /* If X is a constant integer whose low-order 5 bits are zero,
	 write 'l'.  Otherwise, write 'r'.  This is a kludge to fix a bug
	 in the AIX assembler where "sri" with a zero shift count
	 writes a trash instruction.  */
      if (GET_CODE (x) == CONST_INT && (INTVAL (x) & 31) == 0)
	putc ('l', file);
      else
	putc ('r', file);
      return;

    case 'b':
      /* If constant, low-order 16 bits of constant, unsigned.
	 Otherwise, write normally.  */
      if (INT_P (x))
	fprintf (file, HOST_WIDE_INT_PRINT_DEC, INT_LOWPART (x) & 0xffff);
      else
	print_operand (file, x, 0);
      return;

    case 'B':
      /* If the low-order bit is zero, write 'r'; otherwise, write 'l'
	 for 64-bit mask direction.  */
      putc (((INT_LOWPART(x) & 1) == 0 ? 'r' : 'l'), file);
      return;

      /* %c is output_addr_const if a CONSTANT_ADDRESS_P, otherwise
	 output_operand.  */

    case 'c':
      /* X is a CR register.  Print the number of the GT bit of the CR.  */
      if (GET_CODE (x) != REG || ! CR_REGNO_P (REGNO (x)))
       output_operand_lossage ("invalid %%E value");
      else
       fprintf (file, "%d", 4 * (REGNO (x) - CR0_REGNO) + 1);
      return;

    case 'D':
      /* Like 'J' but get to the EQ bit.  */
      if (GET_CODE (x) != REG)
       abort ();

      /* Bit 1 is EQ bit.  */
      i = 4 * (REGNO (x) - CR0_REGNO) + 2;

      /* If we want bit 31, write a shift count of zero, not 32.  */
      fprintf (file, "%d", i == 31 ? 0 : i + 1);
      return;

    case 'E':
      /* X is a CR register.  Print the number of the EQ bit of the CR */
      if (GET_CODE (x) != REG || ! CR_REGNO_P (REGNO (x)))
	output_operand_lossage ("invalid %%E value");
      else
	fprintf (file, "%d", 4 * (REGNO (x) - CR0_REGNO) + 2);
      return;

    case 'f':
      /* X is a CR register.  Print the shift count needed to move it
	 to the high-order four bits.  */
      if (GET_CODE (x) != REG || ! CR_REGNO_P (REGNO (x)))
	output_operand_lossage ("invalid %%f value");
      else
	fprintf (file, "%d", 4 * (REGNO (x) - CR0_REGNO));
      return;

    case 'F':
      /* Similar, but print the count for the rotate in the opposite
	 direction.  */
      if (GET_CODE (x) != REG || ! CR_REGNO_P (REGNO (x)))
	output_operand_lossage ("invalid %%F value");
      else
	fprintf (file, "%d", 32 - 4 * (REGNO (x) - CR0_REGNO));
      return;

    case 'G':
      /* X is a constant integer.  If it is negative, print "m",
	 otherwise print "z".  This is to make an aze or ame insn.  */
      if (GET_CODE (x) != CONST_INT)
	output_operand_lossage ("invalid %%G value");
      else if (INTVAL (x) >= 0)
	putc ('z', file);
      else
	putc ('m', file);
      return;

    case 'h':
      /* If constant, output low-order five bits.  Otherwise, write
	 normally.  */
      if (INT_P (x))
	fprintf (file, HOST_WIDE_INT_PRINT_DEC, INT_LOWPART (x) & 31);
      else
	print_operand (file, x, 0);
      return;

    case 'H':
      /* If constant, output low-order six bits.  Otherwise, write
	 normally.  */
      if (INT_P (x))
	fprintf (file, HOST_WIDE_INT_PRINT_DEC, INT_LOWPART (x) & 63);
      else
	print_operand (file, x, 0);
      return;

    case 'I':
      /* Print `i' if this is a constant, else nothing.  */
      if (INT_P (x))
	putc ('i', file);
      return;

    case 'j':
      /* Write the bit number in CCR for jump.  */
      i = ccr_bit (x, 0);
      if (i == -1)
	output_operand_lossage ("invalid %%j code");
      else
	fprintf (file, "%d", i);
      return;

    case 'J':
      /* Similar, but add one for shift count in rlinm for scc and pass
	 scc flag to `ccr_bit'.  */
      i = ccr_bit (x, 1);
      if (i == -1)
	output_operand_lossage ("invalid %%J code");
      else
	/* If we want bit 31, write a shift count of zero, not 32.  */
	fprintf (file, "%d", i == 31 ? 0 : i + 1);
      return;

    case 'k':
      /* X must be a constant.  Write the 1's complement of the
	 constant.  */
      if (! INT_P (x))
	output_operand_lossage ("invalid %%k value");
      else
	fprintf (file, HOST_WIDE_INT_PRINT_DEC, ~ INT_LOWPART (x));
      return;

    case 'K':
      /* X must be a symbolic constant on ELF.  Write an
	 expression suitable for an 'addi' that adds in the low 16
	 bits of the MEM.  */
      if (GET_CODE (x) != CONST)
	{
	  print_operand_address (file, x);
	  fputs ("@l", file);
	}
      else
	{
	  if (GET_CODE (XEXP (x, 0)) != PLUS
	      || (GET_CODE (XEXP (XEXP (x, 0), 0)) != SYMBOL_REF
		  && GET_CODE (XEXP (XEXP (x, 0), 0)) != LABEL_REF)
	      || GET_CODE (XEXP (XEXP (x, 0), 1)) != CONST_INT)
	    output_operand_lossage ("invalid %%K value");
	  print_operand_address (file, XEXP (XEXP (x, 0), 0));
	  fputs ("@l", file);
	  /* For GNU as, there must be a non-alphanumeric character
	     between 'l' and the number.  The '-' is added by
	     print_operand() already.  */
	  if (INTVAL (XEXP (XEXP (x, 0), 1)) >= 0)
	    fputs ("+", file);
	  print_operand (file, XEXP (XEXP (x, 0), 1), 0);
	}
      return;

      /* %l is output_asm_label.  */

    case 'L':
      /* Write second word of DImode or DFmode reference.  Works on register
	 or non-indexed memory only.  */
      if (GET_CODE (x) == REG)
	fprintf (file, "%s", reg_names[REGNO (x) + 1]);
      else if (GET_CODE (x) == MEM)
	{
	  /* Handle possible auto-increment.  Since it is pre-increment and
	     we have already done it, we can just use an offset of word.  */
	  if (GET_CODE (XEXP (x, 0)) == PRE_INC
	      || GET_CODE (XEXP (x, 0)) == PRE_DEC)
	    output_address (plus_constant (XEXP (XEXP (x, 0), 0),
					   UNITS_PER_WORD));
	  else
	    output_address (XEXP (adjust_address_nv (x, SImode,
						     UNITS_PER_WORD),
				  0));

	  if (small_data_operand (x, GET_MODE (x)))
	    fprintf (file, "@%s(%s)", SMALL_DATA_RELOC,
		     reg_names[SMALL_DATA_REG]);
	}
      return;
			    
    case 'm':
      /* MB value for a mask operand.  */
      if (! mask_operand (x, SImode))
	output_operand_lossage ("invalid %%m value");

      fprintf (file, "%d", extract_MB (x));
      return;

    case 'M':
      /* ME value for a mask operand.  */
      if (! mask_operand (x, SImode))
	output_operand_lossage ("invalid %%M value");

      fprintf (file, "%d", extract_ME (x));
      return;

      /* %n outputs the negative of its operand.  */

    case 'N':
      /* Write the number of elements in the vector times 4.  */
      if (GET_CODE (x) != PARALLEL)
	output_operand_lossage ("invalid %%N value");
      else
	fprintf (file, "%d", XVECLEN (x, 0) * 4);
      return;

    case 'O':
      /* Similar, but subtract 1 first.  */
      if (GET_CODE (x) != PARALLEL)
	output_operand_lossage ("invalid %%O value");
      else
	fprintf (file, "%d", (XVECLEN (x, 0) - 1) * 4);
      return;

    case 'p':
      /* X is a CONST_INT that is a power of two.  Output the logarithm.  */
      if (! INT_P (x)
	  || INT_LOWPART (x) < 0
	  || (i = exact_log2 (INT_LOWPART (x))) < 0)
	output_operand_lossage ("invalid %%p value");
      else
	fprintf (file, "%d", i);
      return;

    case 'P':
      /* The operand must be an indirect memory reference.  The result
	 is the register name.  */
      if (GET_CODE (x) != MEM || GET_CODE (XEXP (x, 0)) != REG
	  || REGNO (XEXP (x, 0)) >= 32)
	output_operand_lossage ("invalid %%P value");
      else
	fprintf (file, "%s", reg_names[REGNO (XEXP (x, 0))]);
      return;

    case 'q':
      /* This outputs the logical code corresponding to a boolean
	 expression.  The expression may have one or both operands
	 negated (if one, only the first one).  For condition register
         logical operations, it will also treat the negated
         CR codes as NOTs, but not handle NOTs of them.  */
      {
	const char *const *t = 0;
	const char *s;
	enum rtx_code code = GET_CODE (x);
	static const char * const tbl[3][3] = {
	  { "and", "andc", "nor" },
	  { "or", "orc", "nand" },
	  { "xor", "eqv", "xor" } };

	if (code == AND)
	  t = tbl[0];
	else if (code == IOR)
	  t = tbl[1];
	else if (code == XOR)
	  t = tbl[2];
	else
	  output_operand_lossage ("invalid %%q value");

	if (GET_CODE (XEXP (x, 0)) != NOT)
	  s = t[0];
	else
	  {
	    if (GET_CODE (XEXP (x, 1)) == NOT)
	      s = t[2];
	    else
	      s = t[1];
	  }
	
	fputs (s, file);
      }
      return;

    case 'Q':
      if (TARGET_MFCRF)
	fputc (',',file);
        /* FALLTHRU */
      else
	return;

    case 'R':
      /* X is a CR register.  Print the mask for `mtcrf'.  */
      if (GET_CODE (x) != REG || ! CR_REGNO_P (REGNO (x)))
	output_operand_lossage ("invalid %%R value");
      else
	fprintf (file, "%d", 128 >> (REGNO (x) - CR0_REGNO));
      return;

    case 's':
      /* Low 5 bits of 32 - value */
      if (! INT_P (x))
	output_operand_lossage ("invalid %%s value");
      else
	fprintf (file, HOST_WIDE_INT_PRINT_DEC, (32 - INT_LOWPART (x)) & 31);
      return;

    case 'S':
      /* PowerPC64 mask position.  All 0's is excluded.
	 CONST_INT 32-bit mask is considered sign-extended so any
	 transition must occur within the CONST_INT, not on the boundary.  */
      if (! mask64_operand (x, DImode))
	output_operand_lossage ("invalid %%S value");

      uval = INT_LOWPART (x);

      if (uval & 1)	/* Clear Left */
	{
#if HOST_BITS_PER_WIDE_INT > 64
	  uval &= ((unsigned HOST_WIDE_INT) 1 << 64) - 1;
#endif
	  i = 64;
	}
      else		/* Clear Right */
	{
	  uval = ~uval;
#if HOST_BITS_PER_WIDE_INT > 64
	  uval &= ((unsigned HOST_WIDE_INT) 1 << 64) - 1;
#endif
	  i = 63;
	}
      while (uval != 0)
	--i, uval >>= 1;
      if (i < 0)
	abort ();
      fprintf (file, "%d", i);
      return;

    case 't':
      /* Like 'J' but get to the OVERFLOW/UNORDERED bit.  */
      if (GET_CODE (x) != REG || GET_MODE (x) != CCmode)
	abort ();

      /* Bit 3 is OV bit.  */
      i = 4 * (REGNO (x) - CR0_REGNO) + 3;

      /* If we want bit 31, write a shift count of zero, not 32.  */
      fprintf (file, "%d", i == 31 ? 0 : i + 1);
      return;

    case 'T':
      /* Print the symbolic name of a branch target register.  */
      if (GET_CODE (x) != REG || (REGNO (x) != LINK_REGISTER_REGNUM
				  && REGNO (x) != COUNT_REGISTER_REGNUM))
	output_operand_lossage ("invalid %%T value");
      else if (REGNO (x) == LINK_REGISTER_REGNUM)
	fputs (TARGET_NEW_MNEMONICS ? "lr" : "r", file);
      else
	fputs ("ctr", file);
      return;

    case 'u':
      /* High-order 16 bits of constant for use in unsigned operand.  */
      if (! INT_P (x))
	output_operand_lossage ("invalid %%u value");
      else
	fprintf (file, HOST_WIDE_INT_PRINT_HEX, 
		 (INT_LOWPART (x) >> 16) & 0xffff);
      return;

    case 'v':
      /* High-order 16 bits of constant for use in signed operand.  */
      if (! INT_P (x))
	output_operand_lossage ("invalid %%v value");
      else
	fprintf (file, HOST_WIDE_INT_PRINT_HEX,
		 (INT_LOWPART (x) >> 16) & 0xffff);
      return;

    case 'U':
      /* Print `u' if this has an auto-increment or auto-decrement.  */
      if (GET_CODE (x) == MEM
	  && (GET_CODE (XEXP (x, 0)) == PRE_INC
	      || GET_CODE (XEXP (x, 0)) == PRE_DEC))
	putc ('u', file);
      return;

    case 'V':
      /* Print the trap code for this operand.  */
      switch (GET_CODE (x))
	{
	case EQ:
	  fputs ("eq", file);   /* 4 */
	  break;
	case NE:
	  fputs ("ne", file);   /* 24 */
	  break;
	case LT:
	  fputs ("lt", file);   /* 16 */
	  break;
	case LE:
	  fputs ("le", file);   /* 20 */
	  break;
	case GT:
	  fputs ("gt", file);   /* 8 */
	  break;
	case GE:
	  fputs ("ge", file);   /* 12 */
	  break;
	case LTU:
	  fputs ("llt", file);  /* 2 */
	  break;
	case LEU:
	  fputs ("lle", file);  /* 6 */
	  break;
	case GTU:
	  fputs ("lgt", file);  /* 1 */
	  break;
	case GEU:
	  fputs ("lge", file);  /* 5 */
	  break;
	default:
	  abort ();
	}
      break;

    case 'w':
      /* If constant, low-order 16 bits of constant, signed.  Otherwise, write
	 normally.  */
      if (INT_P (x))
	fprintf (file, HOST_WIDE_INT_PRINT_DEC, 
		 ((INT_LOWPART (x) & 0xffff) ^ 0x8000) - 0x8000);
      else
	print_operand (file, x, 0);
      return;

    case 'W':
      /* MB value for a PowerPC64 rldic operand.  */
      val = (GET_CODE (x) == CONST_INT
	     ? INTVAL (x) : CONST_DOUBLE_HIGH (x));

      if (val < 0)
	i = -1;
      else
	for (i = 0; i < HOST_BITS_PER_WIDE_INT; i++)
	  if ((val <<= 1) < 0)
	    break;

#if HOST_BITS_PER_WIDE_INT == 32
      if (GET_CODE (x) == CONST_INT && i >= 0)
	i += 32;  /* zero-extend high-part was all 0's */
      else if (GET_CODE (x) == CONST_DOUBLE && i == 32)
	{
	  val = CONST_DOUBLE_LOW (x);

	  if (val == 0)
	    abort ();
	  else if (val < 0)
	    --i;
	  else
	    for ( ; i < 64; i++)
	      if ((val <<= 1) < 0)
		break;
	}
#endif

      fprintf (file, "%d", i + 1);
      return;

    case 'X':
      if (GET_CODE (x) == MEM
	  && legitimate_indexed_address_p (XEXP (x, 0), 0))
	putc ('x', file);
      return;

    case 'Y':
      /* Like 'L', for third word of TImode  */
      if (GET_CODE (x) == REG)
	fprintf (file, "%s", reg_names[REGNO (x) + 2]);
      else if (GET_CODE (x) == MEM)
	{
	  if (GET_CODE (XEXP (x, 0)) == PRE_INC
	      || GET_CODE (XEXP (x, 0)) == PRE_DEC)
	    output_address (plus_constant (XEXP (XEXP (x, 0), 0), 8));
	  else
	    output_address (XEXP (adjust_address_nv (x, SImode, 8), 0));
	  if (small_data_operand (x, GET_MODE (x)))
	    fprintf (file, "@%s(%s)", SMALL_DATA_RELOC,
		     reg_names[SMALL_DATA_REG]);
	}
      return;
			    
    case 'z':
      /* X is a SYMBOL_REF.  Write out the name preceded by a
	 period and without any trailing data in brackets.  Used for function
	 names.  If we are configured for System V (or the embedded ABI) on
	 the PowerPC, do not emit the period, since those systems do not use
	 TOCs and the like.  */
      if (GET_CODE (x) != SYMBOL_REF)
	abort ();

      if (XSTR (x, 0)[0] != '.')
	{
	  switch (DEFAULT_ABI)
	    {
	    default:
	      abort ();

	    case ABI_AIX:
	      putc ('.', file);
	      break;

	    case ABI_V4:
	    case ABI_DARWIN:
	      break;
	    }
	}
      if (TARGET_AIX)
	RS6000_OUTPUT_BASENAME (file, XSTR (x, 0));
      else
	assemble_name (file, XSTR (x, 0));
      return;

    case 'Z':
      /* Like 'L', for last word of TImode.  */
      if (GET_CODE (x) == REG)
	fprintf (file, "%s", reg_names[REGNO (x) + 3]);
      else if (GET_CODE (x) == MEM)
	{
	  if (GET_CODE (XEXP (x, 0)) == PRE_INC
	      || GET_CODE (XEXP (x, 0)) == PRE_DEC)
	    output_address (plus_constant (XEXP (XEXP (x, 0), 0), 12));
	  else
	    output_address (XEXP (adjust_address_nv (x, SImode, 12), 0));
	  if (small_data_operand (x, GET_MODE (x)))
	    fprintf (file, "@%s(%s)", SMALL_DATA_RELOC,
		     reg_names[SMALL_DATA_REG]);
	}
      return;

      /* Print AltiVec or SPE memory operand.  */
    case 'y':
      {
	rtx tmp;

	if (GET_CODE (x) != MEM)
	  abort ();

	tmp = XEXP (x, 0);

	if (TARGET_E500)
	  {
	    /* Handle [reg].  */
	    if (GET_CODE (tmp) == REG)
	      {
		fprintf (file, "0(%s)", reg_names[REGNO (tmp)]);
		break;
	      }
	    /* Handle [reg+UIMM].  */
	    else if (GET_CODE (tmp) == PLUS &&
		     GET_CODE (XEXP (tmp, 1)) == CONST_INT)
	      {
		int x;

		if (GET_CODE (XEXP (tmp, 0)) != REG)
		  abort ();

		x = INTVAL (XEXP (tmp, 1));
		fprintf (file, "%d(%s)", x, reg_names[REGNO (XEXP (tmp, 0))]);
		break;
	      }

	    /* Fall through.  Must be [reg+reg].  */
	  }
	if (GET_CODE (tmp) == REG)
	  fprintf (file, "0,%s", reg_names[REGNO (tmp)]);
	else if (GET_CODE (tmp) == PLUS && GET_CODE (XEXP (tmp, 1)) == REG)
	  {
	    if (REGNO (XEXP (tmp, 0)) == 0)
	      fprintf (file, "%s,%s", reg_names[ REGNO (XEXP (tmp, 1)) ],
		       reg_names[ REGNO (XEXP (tmp, 0)) ]);
	    else
	      fprintf (file, "%s,%s", reg_names[ REGNO (XEXP (tmp, 0)) ],
		       reg_names[ REGNO (XEXP (tmp, 1)) ]);
	  }
	else
	  abort ();
	break;
      }
			    
    case 0:
      if (GET_CODE (x) == REG)
	fprintf (file, "%s", reg_names[REGNO (x)]);
      else if (GET_CODE (x) == MEM)
	{
	  /* We need to handle PRE_INC and PRE_DEC here, since we need to
	     know the width from the mode.  */
	  if (GET_CODE (XEXP (x, 0)) == PRE_INC)
	    fprintf (file, "%d(%s)", GET_MODE_SIZE (GET_MODE (x)),
		     reg_names[REGNO (XEXP (XEXP (x, 0), 0))]);
	  else if (GET_CODE (XEXP (x, 0)) == PRE_DEC)
	    fprintf (file, "%d(%s)", - GET_MODE_SIZE (GET_MODE (x)),
		     reg_names[REGNO (XEXP (XEXP (x, 0), 0))]);
	  else
	    output_address (XEXP (x, 0));
	}
      else
	output_addr_const (file, x);
      return;

    case '&':
      assemble_name (file, rs6000_get_some_local_dynamic_name ());
      return;

    default:
      output_operand_lossage ("invalid %%xn code");
    }
}

/* Print the address of an operand.  */

void
print_operand_address (FILE *file, rtx x)
{
  if (GET_CODE (x) == REG)
    fprintf (file, "0(%s)", reg_names[ REGNO (x) ]);
  else if (GET_CODE (x) == SYMBOL_REF || GET_CODE (x) == CONST
	   || GET_CODE (x) == LABEL_REF)
    {
      output_addr_const (file, x);
      if (small_data_operand (x, GET_MODE (x)))
	fprintf (file, "@%s(%s)", SMALL_DATA_RELOC,
		 reg_names[SMALL_DATA_REG]);
      else if (TARGET_TOC)
	abort ();
    }
  else if (GET_CODE (x) == PLUS && GET_CODE (XEXP (x, 1)) == REG)
    {
      if (REGNO (XEXP (x, 0)) == 0)
	fprintf (file, "%s,%s", reg_names[ REGNO (XEXP (x, 1)) ],
		 reg_names[ REGNO (XEXP (x, 0)) ]);
      else
	fprintf (file, "%s,%s", reg_names[ REGNO (XEXP (x, 0)) ],
		 reg_names[ REGNO (XEXP (x, 1)) ]);
    }
  else if (GET_CODE (x) == PLUS && GET_CODE (XEXP (x, 1)) == CONST_INT)
    fprintf (file, HOST_WIDE_INT_PRINT_DEC "(%s)",
	     INTVAL (XEXP (x, 1)), reg_names[ REGNO (XEXP (x, 0)) ]);
#if TARGET_ELF
  else if (GET_CODE (x) == LO_SUM && GET_CODE (XEXP (x, 0)) == REG
           && CONSTANT_P (XEXP (x, 1)))
    {
      output_addr_const (file, XEXP (x, 1));
      fprintf (file, "@l(%s)", reg_names[ REGNO (XEXP (x, 0)) ]);
    }
#endif
#if TARGET_MACHO
  else if (GET_CODE (x) == LO_SUM && GET_CODE (XEXP (x, 0)) == REG
           && CONSTANT_P (XEXP (x, 1)))
    {
      fprintf (file, "lo16(");
      output_addr_const (file, XEXP (x, 1));
      fprintf (file, ")(%s)", reg_names[ REGNO (XEXP (x, 0)) ]);
    }
#endif
  else if (legitimate_constant_pool_address_p (x))
    {
      if (TARGET_AIX && (!TARGET_ELF || !TARGET_MINIMAL_TOC))
	{
	  rtx contains_minus = XEXP (x, 1);
	  rtx minus, symref;
	  const char *name;
	  
	  /* Find the (minus (sym) (toc)) buried in X, and temporarily
	     turn it into (sym) for output_addr_const.  */
	  while (GET_CODE (XEXP (contains_minus, 0)) != MINUS)
	    contains_minus = XEXP (contains_minus, 0);

	  minus = XEXP (contains_minus, 0);
	  symref = XEXP (minus, 0);
	  XEXP (contains_minus, 0) = symref;
	  if (TARGET_ELF)
	    {
	      char *newname;

	      name = XSTR (symref, 0);
	      newname = alloca (strlen (name) + sizeof ("@toc"));
	      strcpy (newname, name);
	      strcat (newname, "@toc");
	      XSTR (symref, 0) = newname;
	    }
	  output_addr_const (file, XEXP (x, 1));
	  if (TARGET_ELF)
	    XSTR (symref, 0) = name;
	  XEXP (contains_minus, 0) = minus;
	}
      else
	output_addr_const (file, XEXP (x, 1));

      fprintf (file, "(%s)", reg_names[REGNO (XEXP (x, 0))]);
    }
  else
    abort ();
}

/* Target hook for assembling integer objects.  The PowerPC version has
   to handle fixup entries for relocatable code if RELOCATABLE_NEEDS_FIXUP
   is defined.  It also needs to handle DI-mode objects on 64-bit
   targets.  */

static bool
rs6000_assemble_integer (rtx x, unsigned int size, int aligned_p)
{
#ifdef RELOCATABLE_NEEDS_FIXUP
  /* Special handling for SI values.  */
  if (RELOCATABLE_NEEDS_FIXUP && size == 4 && aligned_p)
    {
      extern int in_toc_section (void);
      static int recurse = 0;
      
      /* For -mrelocatable, we mark all addresses that need to be fixed up
	 in the .fixup section.  */
      if (TARGET_RELOCATABLE
	  && !in_toc_section ()
	  && !in_text_section ()
	  && !recurse
	  && GET_CODE (x) != CONST_INT
	  && GET_CODE (x) != CONST_DOUBLE
	  && CONSTANT_P (x))
	{
	  char buf[256];

	  recurse = 1;
	  ASM_GENERATE_INTERNAL_LABEL (buf, "LCP", fixuplabelno);
	  fixuplabelno++;
	  ASM_OUTPUT_LABEL (asm_out_file, buf);
	  fprintf (asm_out_file, "\t.long\t(");
	  output_addr_const (asm_out_file, x);
	  fprintf (asm_out_file, ")@fixup\n");
	  fprintf (asm_out_file, "\t.section\t\".fixup\",\"aw\"\n");
	  ASM_OUTPUT_ALIGN (asm_out_file, 2);
	  fprintf (asm_out_file, "\t.long\t");
	  assemble_name (asm_out_file, buf);
	  fprintf (asm_out_file, "\n\t.previous\n");
	  recurse = 0;
	  return true;
	}
      /* Remove initial .'s to turn a -mcall-aixdesc function
	 address into the address of the descriptor, not the function
	 itself.  */
      else if (GET_CODE (x) == SYMBOL_REF
	       && XSTR (x, 0)[0] == '.'
	       && DEFAULT_ABI == ABI_AIX)
	{
	  const char *name = XSTR (x, 0);
	  while (*name == '.')
	    name++;

	  fprintf (asm_out_file, "\t.long\t%s\n", name);
	  return true;
	}
    }
#endif /* RELOCATABLE_NEEDS_FIXUP */
  return default_assemble_integer (x, size, aligned_p);
}

#ifdef HAVE_GAS_HIDDEN
/* Emit an assembler directive to set symbol visibility for DECL to
   VISIBILITY_TYPE.  */

static void
rs6000_assemble_visibility (tree decl, int vis)
{
  /* Functions need to have their entry point symbol visibility set as
     well as their descriptor symbol visibility.  */
  if (DEFAULT_ABI == ABI_AIX && TREE_CODE (decl) == FUNCTION_DECL)
    {
      static const char * const visibility_types[] = {
        NULL, "internal", "hidden", "protected"
      };

      const char *name, *type;

      name = ((* targetm.strip_name_encoding)
	      (IDENTIFIER_POINTER (DECL_ASSEMBLER_NAME (decl))));
      type = visibility_types[vis];

      fprintf (asm_out_file, "\t.%s\t%s\n", type, name);
      fprintf (asm_out_file, "\t.%s\t.%s\n", type, name);
    }
  else
    default_assemble_visibility (decl, vis);
}
#endif

enum rtx_code
rs6000_reverse_condition (enum machine_mode mode, enum rtx_code code)
{
  /* Reversal of FP compares takes care -- an ordered compare
     becomes an unordered compare and vice versa.  */
  if (mode == CCFPmode 
      && (!flag_finite_math_only
	  || code == UNLT || code == UNLE || code == UNGT || code == UNGE
	  || code == UNEQ || code == LTGT))
    return reverse_condition_maybe_unordered (code);
  else
    return reverse_condition (code);
}

/* Generate a compare for CODE.  Return a brand-new rtx that
   represents the result of the compare.  */

static rtx
rs6000_generate_compare (enum rtx_code code)
{
  enum machine_mode comp_mode;
  rtx compare_result;

  if (rs6000_compare_fp_p)
    comp_mode = CCFPmode;
  else if (code == GTU || code == LTU
	  || code == GEU || code == LEU)
    comp_mode = CCUNSmode;
  else
    comp_mode = CCmode;

  /* First, the compare.  */
  compare_result = gen_reg_rtx (comp_mode);

  /* SPE FP compare instructions on the GPRs.  Yuck!  */
  if ((TARGET_E500 && !TARGET_FPRS && TARGET_HARD_FLOAT)
      && rs6000_compare_fp_p)
    {
      rtx cmp, or1, or2, or_result, compare_result2;

      /* Note: The E500 comparison instructions set the GT bit (x +
        1), on success.  This explains the mess.  */

      switch (code)
	{
       case EQ: case UNEQ: case NE: case LTGT:
	  cmp = flag_finite_math_only
	    ? gen_tstsfeq_gpr (compare_result, rs6000_compare_op0,
			       rs6000_compare_op1)
	    : gen_cmpsfeq_gpr (compare_result, rs6000_compare_op0,
			       rs6000_compare_op1);
	  break;
       case GT: case GTU: case UNGT: case UNGE: case GE: case GEU:
	  cmp = flag_finite_math_only
	    ? gen_tstsfgt_gpr (compare_result, rs6000_compare_op0,
			       rs6000_compare_op1)
	    : gen_cmpsfgt_gpr (compare_result, rs6000_compare_op0,
			       rs6000_compare_op1);
	  break;
       case LT: case LTU: case UNLT: case UNLE: case LE: case LEU:
	  cmp = flag_finite_math_only
	    ? gen_tstsflt_gpr (compare_result, rs6000_compare_op0,
			       rs6000_compare_op1)
	    : gen_cmpsflt_gpr (compare_result, rs6000_compare_op0,
			       rs6000_compare_op1);
	  break;
	default:
	  abort ();
	}

      /* Synthesize LE and GE from LT/GT || EQ.  */
      if (code == LE || code == GE || code == LEU || code == GEU)
	{
	  emit_insn (cmp);

	  switch (code)
	    {
	    case LE: code = LT; break;
	    case GE: code = GT; break;
	    case LEU: code = LT; break;
	    case GEU: code = GT; break;
	    default: abort ();
	    }

	  or1 = gen_reg_rtx (SImode);
	  or2 = gen_reg_rtx (SImode);
	  or_result = gen_reg_rtx (CCEQmode);
	  compare_result2 = gen_reg_rtx (CCFPmode);

	  /* Do the EQ.  */
	  cmp = flag_finite_math_only
	    ? gen_tstsfeq_gpr (compare_result2, rs6000_compare_op0,
			       rs6000_compare_op1)
	    : gen_cmpsfeq_gpr (compare_result2, rs6000_compare_op0,
			       rs6000_compare_op1);
	  emit_insn (cmp);

	  or1 = gen_rtx_GT (SImode, compare_result, const0_rtx);
	  or2 = gen_rtx_GT (SImode, compare_result2, const0_rtx);

	  /* OR them together.  */
	  cmp = gen_rtx_SET (VOIDmode, or_result,
			     gen_rtx_COMPARE (CCEQmode,
					      gen_rtx_IOR (SImode, or1, or2),
					      const_true_rtx));
	  compare_result = or_result;
	  code = EQ;
	}
      else
	{
	  if (code == NE || code == LTGT)
	    code = NE;
         else
           code = EQ;
	}

      emit_insn (cmp);
    }
  else
    {
      /* Generate XLC-compatible TFmode compare as PARALLEL with extra
         CLOBBERs to match cmptf_internal2 pattern.  */
      if (comp_mode == CCFPmode && TARGET_XL_COMPAT
          && GET_MODE (rs6000_compare_op0) == TFmode
          && (DEFAULT_ABI == ABI_AIX || DEFAULT_ABI == ABI_DARWIN)
          && TARGET_HARD_FLOAT && TARGET_FPRS && TARGET_LONG_DOUBLE_128)
        emit_insn (gen_rtx_PARALLEL (VOIDmode,
          gen_rtvec (9,
		     gen_rtx_SET (VOIDmode,
				  compare_result,
				  gen_rtx_COMPARE (comp_mode,
						   rs6000_compare_op0,
						   rs6000_compare_op1)),
		     gen_rtx_CLOBBER (VOIDmode, gen_rtx_SCRATCH (DFmode)),
		     gen_rtx_CLOBBER (VOIDmode, gen_rtx_SCRATCH (DFmode)),
		     gen_rtx_CLOBBER (VOIDmode, gen_rtx_SCRATCH (DFmode)),
		     gen_rtx_CLOBBER (VOIDmode, gen_rtx_SCRATCH (DFmode)),
		     gen_rtx_CLOBBER (VOIDmode, gen_rtx_SCRATCH (DFmode)),
		     gen_rtx_CLOBBER (VOIDmode, gen_rtx_SCRATCH (DFmode)),
		     gen_rtx_CLOBBER (VOIDmode, gen_rtx_SCRATCH (DFmode)),
		     gen_rtx_CLOBBER (VOIDmode, gen_rtx_SCRATCH (DFmode)))));
      else
	emit_insn (gen_rtx_SET (VOIDmode, compare_result,
				gen_rtx_COMPARE (comp_mode,
						 rs6000_compare_op0, 
						 rs6000_compare_op1)));
    }
  
  /* Some kinds of FP comparisons need an OR operation;
     under flag_finite_math_only we don't bother.  */
  if (rs6000_compare_fp_p
      && ! flag_finite_math_only
      && ! (TARGET_HARD_FLOAT && TARGET_E500 && !TARGET_FPRS)
      && (code == LE || code == GE
	  || code == UNEQ || code == LTGT
	  || code == UNGT || code == UNLT))
    {
      enum rtx_code or1, or2;
      rtx or1_rtx, or2_rtx, compare2_rtx;
      rtx or_result = gen_reg_rtx (CCEQmode);
      
      switch (code)
	{
	case LE: or1 = LT;  or2 = EQ;  break;
	case GE: or1 = GT;  or2 = EQ;  break;
	case UNEQ: or1 = UNORDERED;  or2 = EQ;  break;
	case LTGT: or1 = LT;  or2 = GT;  break;
	case UNGT: or1 = UNORDERED;  or2 = GT;  break;
	case UNLT: or1 = UNORDERED;  or2 = LT;  break;
	default:  abort ();
	}
      validate_condition_mode (or1, comp_mode);
      validate_condition_mode (or2, comp_mode);
      or1_rtx = gen_rtx (or1, SImode, compare_result, const0_rtx);
      or2_rtx = gen_rtx (or2, SImode, compare_result, const0_rtx);
      compare2_rtx = gen_rtx_COMPARE (CCEQmode,
				      gen_rtx_IOR (SImode, or1_rtx, or2_rtx),
				      const_true_rtx);
      emit_insn (gen_rtx_SET (VOIDmode, or_result, compare2_rtx));

      compare_result = or_result;
      code = EQ;
    }

  validate_condition_mode (code, GET_MODE (compare_result));
  
  return gen_rtx (code, VOIDmode, compare_result, const0_rtx);
}


/* Emit the RTL for an sCOND pattern.  */

void
rs6000_emit_sCOND (enum rtx_code code, rtx result)
{
  rtx condition_rtx;
  enum machine_mode op_mode;
  enum rtx_code cond_code;

  condition_rtx = rs6000_generate_compare (code);
  cond_code = GET_CODE (condition_rtx);

  if (TARGET_E500 && rs6000_compare_fp_p
      && !TARGET_FPRS && TARGET_HARD_FLOAT)
    {
      rtx t;

      PUT_MODE (condition_rtx, SImode);
      t = XEXP (condition_rtx, 0);

      if (cond_code != NE && cond_code != EQ)
       abort ();

      if (cond_code == NE)
       emit_insn (gen_e500_flip_eq_bit (t, t));

      emit_insn (gen_move_from_CR_eq_bit (result, t));
      return;
    }

  if (cond_code == NE
      || cond_code == GE || cond_code == LE
      || cond_code == GEU || cond_code == LEU
      || cond_code == ORDERED || cond_code == UNGE || cond_code == UNLE)
    {
      rtx not_result = gen_reg_rtx (CCEQmode);
      rtx not_op, rev_cond_rtx;
      enum machine_mode cc_mode;
      
      cc_mode = GET_MODE (XEXP (condition_rtx, 0));

      rev_cond_rtx = gen_rtx (rs6000_reverse_condition (cc_mode, cond_code),
			      SImode, XEXP (condition_rtx, 0), const0_rtx);
      not_op = gen_rtx_COMPARE (CCEQmode, rev_cond_rtx, const0_rtx);
      emit_insn (gen_rtx_SET (VOIDmode, not_result, not_op));
      condition_rtx = gen_rtx_EQ (VOIDmode, not_result, const0_rtx);
    }

  op_mode = GET_MODE (rs6000_compare_op0);
  if (op_mode == VOIDmode)
    op_mode = GET_MODE (rs6000_compare_op1);

  if (TARGET_POWERPC64 && (op_mode == DImode || rs6000_compare_fp_p))
    {
      PUT_MODE (condition_rtx, DImode);
      convert_move (result, condition_rtx, 0);
    }
  else
    {
      PUT_MODE (condition_rtx, SImode);
      emit_insn (gen_rtx_SET (VOIDmode, result, condition_rtx));
    }
}

/* Emit a branch of kind CODE to location LOC.  */

void
rs6000_emit_cbranch (enum rtx_code code, rtx loc)
{
  rtx condition_rtx, loc_ref;

  condition_rtx = rs6000_generate_compare (code);
  loc_ref = gen_rtx_LABEL_REF (VOIDmode, loc);
  emit_jump_insn (gen_rtx_SET (VOIDmode, pc_rtx,
			       gen_rtx_IF_THEN_ELSE (VOIDmode, condition_rtx,
						     loc_ref, pc_rtx)));
}

/* Return the string to output a conditional branch to LABEL, which is
   the operand number of the label, or -1 if the branch is really a
   conditional return.  

   OP is the conditional expression.  XEXP (OP, 0) is assumed to be a
   condition code register and its mode specifies what kind of
   comparison we made.

   REVERSED is nonzero if we should reverse the sense of the comparison.

   INSN is the insn.  */

char *
output_cbranch (rtx op, const char *label, int reversed, rtx insn)
{
  static char string[64];
  enum rtx_code code = GET_CODE (op);
  rtx cc_reg = XEXP (op, 0);
  enum machine_mode mode = GET_MODE (cc_reg);
  int cc_regno = REGNO (cc_reg) - CR0_REGNO;
  int need_longbranch = label != NULL && get_attr_length (insn) == 8;
  int really_reversed = reversed ^ need_longbranch;
  char *s = string;
  const char *ccode;
  const char *pred;
  rtx note;

  validate_condition_mode (code, mode);

  /* Work out which way this really branches.  We could use
     reverse_condition_maybe_unordered here always but this
     makes the resulting assembler clearer.  */
  if (really_reversed)
    {
      /* Reversal of FP compares takes care -- an ordered compare
	 becomes an unordered compare and vice versa.  */
      if (mode == CCFPmode)
	code = reverse_condition_maybe_unordered (code);
      else
	code = reverse_condition (code);
    }

  if ((TARGET_E500 && !TARGET_FPRS && TARGET_HARD_FLOAT) && mode == CCFPmode)
    {
      /* The efscmp/tst* instructions twiddle bit 2, which maps nicely
	 to the GT bit.  */
      if (code == EQ)
	/* Opposite of GT.  */
	code = GT;
      else if (code == NE)
       code = UNLE;
      else
	abort ();
    }

  switch (code)
    {
      /* Not all of these are actually distinct opcodes, but
	 we distinguish them for clarity of the resulting assembler.  */
    case NE: case LTGT:
      ccode = "ne"; break;
    case EQ: case UNEQ:
      ccode = "eq"; break;
    case GE: case GEU: 
      ccode = "ge"; break;
    case GT: case GTU: case UNGT: 
      ccode = "gt"; break;
    case LE: case LEU: 
      ccode = "le"; break;
    case LT: case LTU: case UNLT: 
      ccode = "lt"; break;
    case UNORDERED: ccode = "un"; break;
    case ORDERED: ccode = "nu"; break;
    case UNGE: ccode = "nl"; break;
    case UNLE: ccode = "ng"; break;
    default:
      abort ();
    }
  
  /* Maybe we have a guess as to how likely the branch is.  
     The old mnemonics don't have a way to specify this information.  */
  pred = "";
  note = find_reg_note (insn, REG_BR_PROB, NULL_RTX);
  if (note != NULL_RTX)
    {
      /* PROB is the difference from 50%.  */
      int prob = INTVAL (XEXP (note, 0)) - REG_BR_PROB_BASE / 2;

      /* Only hint for highly probable/improbable branches on newer
	 cpus as static prediction overrides processor dynamic
	 prediction.  For older cpus we may as well always hint, but
	 assume not taken for branches that are very close to 50% as a
	 mispredicted taken branch is more expensive than a
	 mispredicted not-taken branch.  */ 
      if (rs6000_always_hint
	  || abs (prob) > REG_BR_PROB_BASE / 100 * 48)
	{
	  if (abs (prob) > REG_BR_PROB_BASE / 20
	      && ((prob > 0) ^ need_longbranch))
              pred = "+";
	  else
	    pred = "-";
	}
    }

  if (label == NULL)
    s += sprintf (s, "{b%sr|b%slr%s} ", ccode, ccode, pred);
  else
    s += sprintf (s, "{b%s|b%s%s} ", ccode, ccode, pred);

  /* We need to escape any '%' characters in the reg_names string.
     Assume they'd only be the first character....  */
  if (reg_names[cc_regno + CR0_REGNO][0] == '%')
    *s++ = '%';
  s += sprintf (s, "%s", reg_names[cc_regno + CR0_REGNO]);

  if (label != NULL)
    {
      /* If the branch distance was too far, we may have to use an
	 unconditional branch to go the distance.  */
      if (need_longbranch)
	s += sprintf (s, ",$+8\n\tb %s", label);
      else
	s += sprintf (s, ",%s", label);
    }

  return string;
}

/* Return the string to flip the EQ bit on a CR.  */
char *
output_e500_flip_eq_bit (rtx dst, rtx src)
{
  static char string[64];
  int a, b;

  if (GET_CODE (dst) != REG || ! CR_REGNO_P (REGNO (dst))
      || GET_CODE (src) != REG || ! CR_REGNO_P (REGNO (src)))
    abort ();

  /* EQ bit.  */
  a = 4 * (REGNO (dst) - CR0_REGNO) + 2;
  b = 4 * (REGNO (src) - CR0_REGNO) + 2;

  sprintf (string, "crnot %d,%d", a, b);
  return string;
}

/* Emit a conditional move: move TRUE_COND to DEST if OP of the
   operands of the last comparison is nonzero/true, FALSE_COND if it
   is zero/false.  Return 0 if the hardware has no such operation.  */

int
rs6000_emit_cmove (rtx dest, rtx op, rtx true_cond, rtx false_cond)
{
  enum rtx_code code = GET_CODE (op);
  rtx op0 = rs6000_compare_op0;
  rtx op1 = rs6000_compare_op1;
  REAL_VALUE_TYPE c1;
  enum machine_mode compare_mode = GET_MODE (op0);
  enum machine_mode result_mode = GET_MODE (dest);
  rtx temp;

  /* These modes should always match.  */
  if (GET_MODE (op1) != compare_mode
      /* In the isel case however, we can use a compare immediate, so
	 op1 may be a small constant.  */
      && (!TARGET_ISEL || !short_cint_operand (op1, VOIDmode)))
    return 0;
  if (GET_MODE (true_cond) != result_mode)
    return 0;
  if (GET_MODE (false_cond) != result_mode)
    return 0;

  /* First, work out if the hardware can do this at all, or
     if it's too slow....  */
  if (! rs6000_compare_fp_p)
    {
      if (TARGET_ISEL)
	return rs6000_emit_int_cmove (dest, op, true_cond, false_cond);
      return 0;
    }
  else if (TARGET_E500 && TARGET_HARD_FLOAT && !TARGET_FPRS
	   && GET_MODE_CLASS (compare_mode) == MODE_FLOAT)
    return 0;

  /* Eliminate half of the comparisons by switching operands, this
     makes the remaining code simpler.  */
  if (code == UNLT || code == UNGT || code == UNORDERED || code == NE
      || code == LTGT || code == LT || code == UNLE)
    {
      code = reverse_condition_maybe_unordered (code);
      temp = true_cond;
      true_cond = false_cond;
      false_cond = temp;
    }

  /* UNEQ and LTGT take four instructions for a comparison with zero,
     it'll probably be faster to use a branch here too.  */
  if (code == UNEQ && HONOR_NANS (compare_mode))
    return 0;
  
  if (GET_CODE (op1) == CONST_DOUBLE)
    REAL_VALUE_FROM_CONST_DOUBLE (c1, op1);
    
  /* We're going to try to implement comparisons by performing
     a subtract, then comparing against zero.  Unfortunately,
     Inf - Inf is NaN which is not zero, and so if we don't
     know that the operand is finite and the comparison
     would treat EQ different to UNORDERED, we can't do it.  */
  if (HONOR_INFINITIES (compare_mode)
      && code != GT && code != UNGE
      && (GET_CODE (op1) != CONST_DOUBLE || real_isinf (&c1))
      /* Constructs of the form (a OP b ? a : b) are safe.  */
      && ((! rtx_equal_p (op0, false_cond) && ! rtx_equal_p (op1, false_cond))
	  || (! rtx_equal_p (op0, true_cond) 
	      && ! rtx_equal_p (op1, true_cond))))
    return 0;
  /* At this point we know we can use fsel.  */

  /* Reduce the comparison to a comparison against zero.  */
  temp = gen_reg_rtx (compare_mode);
  emit_insn (gen_rtx_SET (VOIDmode, temp,
			  gen_rtx_MINUS (compare_mode, op0, op1)));
  op0 = temp;
  op1 = CONST0_RTX (compare_mode);

  /* If we don't care about NaNs we can reduce some of the comparisons
     down to faster ones.  */
  if (! HONOR_NANS (compare_mode))
    switch (code)
      {
      case GT:
	code = LE;
	temp = true_cond;
	true_cond = false_cond;
	false_cond = temp;
	break;
      case UNGE:
	code = GE;
	break;
      case UNEQ:
	code = EQ;
	break;
      default:
	break;
      }

  /* Now, reduce everything down to a GE.  */
  switch (code)
    {
    case GE:
      break;

    case LE:
      temp = gen_reg_rtx (compare_mode);
      emit_insn (gen_rtx_SET (VOIDmode, temp, gen_rtx_NEG (compare_mode, op0)));
      op0 = temp;
      break;

    case ORDERED:
      temp = gen_reg_rtx (compare_mode);
      emit_insn (gen_rtx_SET (VOIDmode, temp, gen_rtx_ABS (compare_mode, op0)));
      op0 = temp;
      break;

    case EQ:
      temp = gen_reg_rtx (compare_mode);
      emit_insn (gen_rtx_SET (VOIDmode, temp, 
			      gen_rtx_NEG (compare_mode,
					   gen_rtx_ABS (compare_mode, op0))));
      op0 = temp;
      break;

    case UNGE:
      /* a UNGE 0 <-> (a GE 0 || -a UNLT 0) */
      temp = gen_reg_rtx (result_mode);
      emit_insn (gen_rtx_SET (VOIDmode, temp,
			      gen_rtx_IF_THEN_ELSE (result_mode,
						    gen_rtx_GE (VOIDmode,
								op0, op1),
						    true_cond, false_cond)));
      false_cond = true_cond;
      true_cond = temp;

      temp = gen_reg_rtx (compare_mode);
      emit_insn (gen_rtx_SET (VOIDmode, temp, gen_rtx_NEG (compare_mode, op0)));
      op0 = temp;
      break;

    case GT:
      /* a GT 0 <-> (a GE 0 && -a UNLT 0) */
      temp = gen_reg_rtx (result_mode);
      emit_insn (gen_rtx_SET (VOIDmode, temp,
			      gen_rtx_IF_THEN_ELSE (result_mode, 
						    gen_rtx_GE (VOIDmode,
								op0, op1),
						    true_cond, false_cond)));
      true_cond = false_cond;
      false_cond = temp;

      temp = gen_reg_rtx (compare_mode);
      emit_insn (gen_rtx_SET (VOIDmode, temp, gen_rtx_NEG (compare_mode, op0)));
      op0 = temp;
      break;

    default:
      abort ();
    }

  emit_insn (gen_rtx_SET (VOIDmode, dest,
			  gen_rtx_IF_THEN_ELSE (result_mode,
						gen_rtx_GE (VOIDmode,
							    op0, op1),
						true_cond, false_cond)));
  return 1;
}

/* Same as above, but for ints (isel).  */

static int
rs6000_emit_int_cmove (rtx dest, rtx op, rtx true_cond, rtx false_cond)
{
  rtx condition_rtx, cr;

  /* All isel implementations thus far are 32-bits.  */
  if (GET_MODE (rs6000_compare_op0) != SImode)
    return 0;

  /* We still have to do the compare, because isel doesn't do a
     compare, it just looks at the CRx bits set by a previous compare
     instruction.  */
  condition_rtx = rs6000_generate_compare (GET_CODE (op));
  cr = XEXP (condition_rtx, 0);

  if (GET_MODE (cr) == CCmode)
    emit_insn (gen_isel_signed (dest, condition_rtx,
				true_cond, false_cond, cr));
  else
    emit_insn (gen_isel_unsigned (dest, condition_rtx,
				  true_cond, false_cond, cr));

  return 1;
}

const char *
output_isel (rtx *operands)
{
  enum rtx_code code;

  code = GET_CODE (operands[1]);
  if (code == GE || code == GEU || code == LE || code == LEU || code == NE)
    {
      PUT_CODE (operands[1], reverse_condition (code));
      return "isel %0,%3,%2,%j1";
    }
  else
    return "isel %0,%2,%3,%j1";
}

void
rs6000_emit_minmax (rtx dest, enum rtx_code code, rtx op0, rtx op1)
{
  enum machine_mode mode = GET_MODE (op0);
  enum rtx_code c;
  rtx target;

  if (code == SMAX || code == SMIN)
    c = GE;
  else
    c = GEU;

  if (code == SMAX || code == UMAX)
    target = emit_conditional_move (dest, c, op0, op1, mode, 
				    op0, op1, mode, 0);
  else
    target = emit_conditional_move (dest, c, op0, op1, mode, 
				    op1, op0, mode, 0);
  if (target == NULL_RTX)
    abort ();
  if (target != dest)
    emit_move_insn (dest, target);
}

/* Emit instructions to move SRC to DST.  Called by splitters for
   multi-register moves.  It will emit at most one instruction for
   each register that is accessed; that is, it won't emit li/lis pairs
   (or equivalent for 64-bit code).  One of SRC or DST must be a hard
   register.  */

void
rs6000_split_multireg_move (rtx dst, rtx src)
{
  /* The register number of the first register being moved.  */
  int reg;
  /* The mode that is to be moved.  */
  enum machine_mode mode;
  /* The mode that the move is being done in, and its size.  */
  enum machine_mode reg_mode;
  int reg_mode_size;
  /* The number of registers that will be moved.  */
  int nregs;

  reg = REG_P (dst) ? REGNO (dst) : REGNO (src);
  mode = GET_MODE (dst);
  nregs = HARD_REGNO_NREGS (reg, mode);
  if (FP_REGNO_P (reg))
    reg_mode = DFmode;
  else if (ALTIVEC_REGNO_P (reg))
    reg_mode = V16QImode;
  else
    reg_mode = word_mode;
  reg_mode_size = GET_MODE_SIZE (reg_mode);
  
  if (reg_mode_size * nregs != GET_MODE_SIZE (mode))
    abort ();
  
  if (REG_P (src) && REG_P (dst) && (REGNO (src) < REGNO (dst)))
    {
      /* Move register range backwards, if we might have destructive
	 overlap.  */
      int i;
      for (i = nregs - 1; i >= 0; i--)
	emit_insn (gen_rtx_SET (VOIDmode, 
				simplify_gen_subreg (reg_mode, dst, mode,
						     i * reg_mode_size),
				simplify_gen_subreg (reg_mode, src, mode,
						     i * reg_mode_size)));
    }
  else
    {
      int i;
      int j = -1;
      bool used_update = false;

      if (GET_CODE (src) == MEM && INT_REGNO_P (reg))
        {
          rtx breg;

	  if (GET_CODE (XEXP (src, 0)) == PRE_INC
	      || GET_CODE (XEXP (src, 0)) == PRE_DEC)
	    {
	      rtx delta_rtx;
	      breg = XEXP (XEXP (src, 0), 0);
	      delta_rtx =  GET_CODE (XEXP (src, 0)) == PRE_INC 
		  ? GEN_INT (GET_MODE_SIZE (GET_MODE (src))) 
		  : GEN_INT (-GET_MODE_SIZE (GET_MODE (src))); 
	      emit_insn (TARGET_32BIT
			 ? gen_addsi3 (breg, breg, delta_rtx)
			 : gen_adddi3 (breg, breg, delta_rtx));
	      src = gen_rtx_MEM (mode, breg);
	    }
	  else if (! offsettable_memref_p (src))
	    {
	      rtx newsrc, basereg;
	      basereg = gen_rtx_REG (Pmode, reg);
	      emit_insn (gen_rtx_SET (VOIDmode, basereg, XEXP (src, 0)));
	      newsrc = gen_rtx_MEM (GET_MODE (src), basereg);
	      MEM_COPY_ATTRIBUTES (newsrc, src);
	      src = newsrc;
	    }

	  breg = XEXP (src, 0);
	  if (GET_CODE (breg) == PLUS || GET_CODE (breg) == LO_SUM)
	    breg = XEXP (breg, 0);

	  /* If the base register we are using to address memory is
	     also a destination reg, then change that register last.  */
	  if (REG_P (breg)
	      && REGNO (breg) >= REGNO (dst)
	      && REGNO (breg) < REGNO (dst) + nregs)
	    j = REGNO (breg) - REGNO (dst);
	}

      if (GET_CODE (dst) == MEM && INT_REGNO_P (reg))
	{
	  rtx breg;

	  if (GET_CODE (XEXP (dst, 0)) == PRE_INC
	      || GET_CODE (XEXP (dst, 0)) == PRE_DEC)
	    {
	      rtx delta_rtx;
	      breg = XEXP (XEXP (dst, 0), 0);
	      delta_rtx = GET_CODE (XEXP (dst, 0)) == PRE_INC 
		? GEN_INT (GET_MODE_SIZE (GET_MODE (dst))) 
		: GEN_INT (-GET_MODE_SIZE (GET_MODE (dst))); 

	      /* We have to update the breg before doing the store.
		 Use store with update, if available.  */

	      if (TARGET_UPDATE)
		{
		  rtx nsrc = simplify_gen_subreg (reg_mode, src, mode, 0);
		  emit_insn (TARGET_32BIT
			     ? gen_movsi_update (breg, breg, delta_rtx, nsrc)
			     : gen_movdi_update (breg, breg, delta_rtx, nsrc));
		  used_update = true;
		}
	      else
		emit_insn (TARGET_32BIT
			   ? gen_addsi3 (breg, breg, delta_rtx)
			   : gen_adddi3 (breg, breg, delta_rtx));
	      dst = gen_rtx_MEM (mode, breg);
	    }
	  else if (! offsettable_memref_p (dst))
	    abort ();
	}

      for (i = 0; i < nregs; i++)
	{  
	  /* Calculate index to next subword.  */
	  ++j;
	  if (j == nregs) 
	    j = 0;

	  /* If compiler already emitted move of first word by 
	     store with update, no need to do anything.  */
	  if (j == 0 && used_update)
	    continue;
	  
	  emit_insn (gen_rtx_SET (VOIDmode,
				  simplify_gen_subreg (reg_mode, dst, mode,
						       j * reg_mode_size),
				  simplify_gen_subreg (reg_mode, src, mode,
						       j * reg_mode_size)));
	}
    }
}


/* This page contains routines that are used to determine what the
   function prologue and epilogue code will do and write them out.  */

/* Return the first fixed-point register that is required to be
   saved. 32 if none.  */

int
first_reg_to_save (void)
{
  int first_reg;

  /* Find lowest numbered live register.  */
  for (first_reg = 13; first_reg <= 31; first_reg++)
    if (regs_ever_live[first_reg] 
	&& (! call_used_regs[first_reg]
	    || (first_reg == RS6000_PIC_OFFSET_TABLE_REGNUM
		&& ((DEFAULT_ABI == ABI_V4 && flag_pic != 0)
		    || (DEFAULT_ABI == ABI_DARWIN && flag_pic)
		    || (TARGET_TOC && TARGET_MINIMAL_TOC)))))
      break;

#if TARGET_MACHO
  if (flag_pic
      && current_function_uses_pic_offset_table
      && first_reg > RS6000_PIC_OFFSET_TABLE_REGNUM)
    return RS6000_PIC_OFFSET_TABLE_REGNUM;
#endif

  return first_reg;
}

/* Similar, for FP regs.  */

int
first_fp_reg_to_save (void)
{
  int first_reg;

  /* Find lowest numbered live register.  */
  for (first_reg = 14 + 32; first_reg <= 63; first_reg++)
    if (regs_ever_live[first_reg])
      break;

  return first_reg;
}

/* Similar, for AltiVec regs.  */

static int
first_altivec_reg_to_save (void)
{
  int i;

  /* Stack frame remains as is unless we are in AltiVec ABI.  */
  if (! TARGET_ALTIVEC_ABI)
    return LAST_ALTIVEC_REGNO + 1;

  /* Find lowest numbered live register.  */
  for (i = FIRST_ALTIVEC_REGNO + 20; i <= LAST_ALTIVEC_REGNO; ++i)
    if (regs_ever_live[i])
      break;

  return i;
}

/* Return a 32-bit mask of the AltiVec registers we need to set in
   VRSAVE.  Bit n of the return value is 1 if Vn is live.  The MSB in
   the 32-bit word is 0.  */

static unsigned int
compute_vrsave_mask (void)
{
  unsigned int i, mask = 0;

  /* First, find out if we use _any_ altivec registers.  */
  for (i = FIRST_ALTIVEC_REGNO; i <= LAST_ALTIVEC_REGNO; ++i)
    if (regs_ever_live[i])
      mask |= ALTIVEC_REG_BIT (i);

  if (mask == 0)
    return mask;

  /* Next, remove the argument registers from the set.  These must
     be in the VRSAVE mask set by the caller, so we don't need to add
     them in again.  More importantly, the mask we compute here is
     used to generate CLOBBERs in the set_vrsave insn, and we do not
     wish the argument registers to die.  */
  for (i = cfun->args_info.vregno - 1; i >= ALTIVEC_ARG_MIN_REG; --i)
    mask &= ~ALTIVEC_REG_BIT (i);

  /* Similarly, remove the return value from the set.  */
  {
    bool yes = false;
    diddle_return_value (is_altivec_return_reg, &yes);
    if (yes)
      mask &= ~ALTIVEC_REG_BIT (ALTIVEC_ARG_RETURN);
  }

  return mask;
}

static void
is_altivec_return_reg (rtx reg, void *xyes)
{
  bool *yes = (bool *) xyes;
  if (REGNO (reg) == ALTIVEC_ARG_RETURN)
    *yes = true;
}


/* Calculate the stack information for the current function.  This is
   complicated by having two separate calling sequences, the AIX calling
   sequence and the V.4 calling sequence.

   AIX (and Darwin/Mac OS X) stack frames look like:
							  32-bit  64-bit
	SP---->	+---------------------------------------+
		| back chain to caller			| 0	  0
		+---------------------------------------+
		| saved CR				| 4       8 (8-11)
		+---------------------------------------+
		| saved LR				| 8       16
		+---------------------------------------+
		| reserved for compilers		| 12      24
		+---------------------------------------+
		| reserved for binders			| 16      32
		+---------------------------------------+
		| saved TOC pointer			| 20      40
		+---------------------------------------+
		| Parameter save area (P)		| 24      48
		+---------------------------------------+
		| Alloca space (A)			| 24+P    etc.
		+---------------------------------------+
		| Local variable space (L)		| 24+P+A
		+---------------------------------------+
		| Float/int conversion temporary (X)	| 24+P+A+L
		+---------------------------------------+
		| Save area for AltiVec registers (W)	| 24+P+A+L+X
		+---------------------------------------+
		| AltiVec alignment padding (Y)		| 24+P+A+L+X+W
		+---------------------------------------+
		| Save area for VRSAVE register (Z)	| 24+P+A+L+X+W+Y
		+---------------------------------------+
		| Save area for GP registers (G)	| 24+P+A+X+L+X+W+Y+Z
		+---------------------------------------+
		| Save area for FP registers (F)	| 24+P+A+X+L+X+W+Y+Z+G
		+---------------------------------------+
	old SP->| back chain to caller's caller		|
		+---------------------------------------+

   The required alignment for AIX configurations is two words (i.e., 8
   or 16 bytes).


   V.4 stack frames look like:

	SP---->	+---------------------------------------+
		| back chain to caller			| 0
		+---------------------------------------+
		| caller's saved LR			| 4
		+---------------------------------------+
		| Parameter save area (P)		| 8
		+---------------------------------------+
		| Alloca space (A)			| 8+P
		+---------------------------------------+    
		| Varargs save area (V)			| 8+P+A
		+---------------------------------------+    
		| Local variable space (L)		| 8+P+A+V
		+---------------------------------------+    
		| Float/int conversion temporary (X)	| 8+P+A+V+L
		+---------------------------------------+
		| Save area for AltiVec registers (W)	| 8+P+A+V+L+X
		+---------------------------------------+
		| AltiVec alignment padding (Y)		| 8+P+A+V+L+X+W
		+---------------------------------------+
		| Save area for VRSAVE register (Z)	| 8+P+A+V+L+X+W+Y
		+---------------------------------------+
                | SPE: area for 64-bit GP registers     |
                +---------------------------------------+
                | SPE alignment padding                 |
                +---------------------------------------+
		| saved CR (C)				| 8+P+A+V+L+X+W+Y+Z
		+---------------------------------------+    
		| Save area for GP registers (G)	| 8+P+A+V+L+X+W+Y+Z+C
		+---------------------------------------+    
		| Save area for FP registers (F)	| 8+P+A+V+L+X+W+Y+Z+C+G
		+---------------------------------------+
	old SP->| back chain to caller's caller		|
		+---------------------------------------+

   The required alignment for V.4 is 16 bytes, or 8 bytes if -meabi is
   given.  (But note below and in sysv4.h that we require only 8 and
   may round up the size of our stack frame anyways.  The historical
   reason is early versions of powerpc-linux which didn't properly
   align the stack at program startup.  A happy side-effect is that
   -mno-eabi libraries can be used with -meabi programs.)

   The EABI configuration defaults to the V.4 layout.  However,
   the stack alignment requirements may differ.  If -mno-eabi is not
   given, the required stack alignment is 8 bytes; if -mno-eabi is
   given, the required alignment is 16 bytes.  (But see V.4 comment
   above.)  */

#ifndef ABI_STACK_BOUNDARY
#define ABI_STACK_BOUNDARY STACK_BOUNDARY
#endif

static rs6000_stack_t *
rs6000_stack_info (void)
{
  static rs6000_stack_t info, zero_info;
  rs6000_stack_t *info_ptr = &info;
  int reg_size = TARGET_32BIT ? 4 : 8;
  int ehrd_size;
  int save_align;
  HOST_WIDE_INT non_fixed_size;

  /* Zero all fields portably.  */
  info = zero_info;

  if (TARGET_SPE)
    {
      /* Cache value so we don't rescan instruction chain over and over.  */
      if (cfun->machine->insn_chain_scanned_p == 0)
	{
	  cfun->machine->insn_chain_scanned_p = 1;
	  info_ptr->spe_64bit_regs_used = (int) spe_func_has_64bit_regs_p ();
	}
    }

  /* Select which calling sequence.  */
  info_ptr->abi = DEFAULT_ABI;

  /* Calculate which registers need to be saved & save area size.  */
  info_ptr->first_gp_reg_save = first_reg_to_save ();
  /* Assume that we will have to save RS6000_PIC_OFFSET_TABLE_REGNUM, 
     even if it currently looks like we won't.  */
  if (((TARGET_TOC && TARGET_MINIMAL_TOC)
       || (flag_pic == 1 && DEFAULT_ABI == ABI_V4)
       || (flag_pic && DEFAULT_ABI == ABI_DARWIN))
      && info_ptr->first_gp_reg_save > RS6000_PIC_OFFSET_TABLE_REGNUM)
    info_ptr->gp_size = reg_size * (32 - RS6000_PIC_OFFSET_TABLE_REGNUM);
  else
    info_ptr->gp_size = reg_size * (32 - info_ptr->first_gp_reg_save);

  /* For the SPE, we have an additional upper 32-bits on each GPR.
     Ideally we should save the entire 64-bits only when the upper
     half is used in SIMD instructions.  Since we only record
     registers live (not the size they are used in), this proves
     difficult because we'd have to traverse the instruction chain at
     the right time, taking reload into account.  This is a real pain,
     so we opt to save the GPRs in 64-bits always if but one register
     gets used in 64-bits.  Otherwise, all the registers in the frame
     get saved in 32-bits.

     So... since when we save all GPRs (except the SP) in 64-bits, the
     traditional GP save area will be empty.  */
  if (TARGET_SPE_ABI && info_ptr->spe_64bit_regs_used != 0)
    info_ptr->gp_size = 0;

  info_ptr->first_fp_reg_save = first_fp_reg_to_save ();
  info_ptr->fp_size = 8 * (64 - info_ptr->first_fp_reg_save);

  info_ptr->first_altivec_reg_save = first_altivec_reg_to_save ();
  info_ptr->altivec_size = 16 * (LAST_ALTIVEC_REGNO + 1
				 - info_ptr->first_altivec_reg_save);

  /* Does this function call anything?  */
  info_ptr->calls_p = (! current_function_is_leaf
		       || cfun->machine->ra_needs_full_frame);

  /* Determine if we need to save the link register.  */
  if (rs6000_ra_ever_killed ()
      || (DEFAULT_ABI == ABI_AIX
	  && current_function_profile
	  && !TARGET_PROFILE_KERNEL)
#ifdef TARGET_RELOCATABLE
      || (TARGET_RELOCATABLE && (get_pool_size () != 0))
#endif
      || (info_ptr->first_fp_reg_save != 64
	  && !FP_SAVE_INLINE (info_ptr->first_fp_reg_save))
      || info_ptr->first_altivec_reg_save <= LAST_ALTIVEC_REGNO
      || (DEFAULT_ABI == ABI_V4 && current_function_calls_alloca)
      || (DEFAULT_ABI == ABI_DARWIN
	  && flag_pic
	  && current_function_uses_pic_offset_table)
      || info_ptr->calls_p)
    {
      info_ptr->lr_save_p = 1;
      regs_ever_live[LINK_REGISTER_REGNUM] = 1;
    }

  /* Determine if we need to save the condition code registers.  */
  if (regs_ever_live[CR2_REGNO] 
      || regs_ever_live[CR3_REGNO]
      || regs_ever_live[CR4_REGNO])
    {
      info_ptr->cr_save_p = 1;
      if (DEFAULT_ABI == ABI_V4)
	info_ptr->cr_size = reg_size;
    }

  /* If the current function calls __builtin_eh_return, then we need
     to allocate stack space for registers that will hold data for
     the exception handler.  */
  if (current_function_calls_eh_return)
    {
      unsigned int i;
      for (i = 0; EH_RETURN_DATA_REGNO (i) != INVALID_REGNUM; ++i)
	continue;

      /* SPE saves EH registers in 64-bits.  */
      ehrd_size = i * (TARGET_SPE_ABI
		       && info_ptr->spe_64bit_regs_used != 0
		       ? UNITS_PER_SPE_WORD : UNITS_PER_WORD);
    }
  else
    ehrd_size = 0;

  /* Determine various sizes.  */
  info_ptr->reg_size     = reg_size;
  info_ptr->fixed_size   = RS6000_SAVE_AREA;
  info_ptr->varargs_size = RS6000_VARARGS_AREA;
  info_ptr->vars_size    = RS6000_ALIGN (get_frame_size (), 8);
  info_ptr->parm_size    = RS6000_ALIGN (current_function_outgoing_args_size,
					 TARGET_ALTIVEC ? 16 : 8);

  if (TARGET_SPE_ABI && info_ptr->spe_64bit_regs_used != 0)
    info_ptr->spe_gp_size = 8 * (32 - info_ptr->first_gp_reg_save);
  else
    info_ptr->spe_gp_size = 0;

  if (TARGET_ALTIVEC_ABI)
    info_ptr->vrsave_mask = compute_vrsave_mask ();
  else
    info_ptr->vrsave_mask = 0;

  if (TARGET_ALTIVEC_VRSAVE && info_ptr->vrsave_mask)
    info_ptr->vrsave_size  = 4;
  else
    info_ptr->vrsave_size  = 0;

  /* Calculate the offsets.  */
  switch (DEFAULT_ABI)
    {
    case ABI_NONE:
    default:
      abort ();

    case ABI_AIX:
    case ABI_DARWIN:
      info_ptr->fp_save_offset   = - info_ptr->fp_size;
      info_ptr->gp_save_offset   = info_ptr->fp_save_offset - info_ptr->gp_size;

      if (TARGET_ALTIVEC_ABI)
	{
	  info_ptr->vrsave_save_offset
	    = info_ptr->gp_save_offset - info_ptr->vrsave_size;

	  /* Align stack so vector save area is on a quadword boundary.  */
	  if (info_ptr->altivec_size != 0)
	    info_ptr->altivec_padding_size
	      = 16 - (-info_ptr->vrsave_save_offset % 16);
	  else
	    info_ptr->altivec_padding_size = 0;

	  info_ptr->altivec_save_offset
	    = info_ptr->vrsave_save_offset
	    - info_ptr->altivec_padding_size
	    - info_ptr->altivec_size;

	  /* Adjust for AltiVec case.  */
	  info_ptr->ehrd_offset = info_ptr->altivec_save_offset - ehrd_size;
	}
      else
	info_ptr->ehrd_offset      = info_ptr->gp_save_offset - ehrd_size;
      info_ptr->cr_save_offset   = reg_size; /* first word when 64-bit.  */
      info_ptr->lr_save_offset   = 2*reg_size;
      break;

    case ABI_V4:
      info_ptr->fp_save_offset   = - info_ptr->fp_size;
      info_ptr->gp_save_offset   = info_ptr->fp_save_offset - info_ptr->gp_size;
      info_ptr->cr_save_offset   = info_ptr->gp_save_offset - info_ptr->cr_size;

      if (TARGET_SPE_ABI && info_ptr->spe_64bit_regs_used != 0)
      {
        /* Align stack so SPE GPR save area is aligned on a
           double-word boundary.  */
        if (info_ptr->spe_gp_size != 0)
          info_ptr->spe_padding_size
            = 8 - (-info_ptr->cr_save_offset % 8);
        else
          info_ptr->spe_padding_size = 0;

        info_ptr->spe_gp_save_offset
          = info_ptr->cr_save_offset
          - info_ptr->spe_padding_size
          - info_ptr->spe_gp_size;

        /* Adjust for SPE case.  */
        info_ptr->toc_save_offset
          = info_ptr->spe_gp_save_offset - info_ptr->toc_size;
      }
      else if (TARGET_ALTIVEC_ABI)
	{
	  info_ptr->vrsave_save_offset
	    = info_ptr->cr_save_offset - info_ptr->vrsave_size;

	  /* Align stack so vector save area is on a quadword boundary.  */
	  if (info_ptr->altivec_size != 0)
	    info_ptr->altivec_padding_size
	      = 16 - (-info_ptr->vrsave_save_offset % 16);
	  else
	    info_ptr->altivec_padding_size = 0;

	  info_ptr->altivec_save_offset
	    = info_ptr->vrsave_save_offset
	    - info_ptr->altivec_padding_size
	    - info_ptr->altivec_size;

	  /* Adjust for AltiVec case.  */
	  info_ptr->toc_save_offset
	    = info_ptr->altivec_save_offset - info_ptr->toc_size;
	}
      else
	info_ptr->toc_save_offset  = info_ptr->cr_save_offset - info_ptr->toc_size;
      info_ptr->ehrd_offset      = info_ptr->toc_save_offset - ehrd_size;
      info_ptr->lr_save_offset   = reg_size;
      break;
    }

  save_align = (TARGET_ALTIVEC_ABI || DEFAULT_ABI == ABI_DARWIN) ? 16 : 8;
  info_ptr->save_size    = RS6000_ALIGN (info_ptr->fp_size
					 + info_ptr->gp_size
					 + info_ptr->altivec_size
					 + info_ptr->altivec_padding_size
					 + info_ptr->spe_gp_size
					 + info_ptr->spe_padding_size
					 + ehrd_size
					 + info_ptr->cr_size
					 + info_ptr->lr_size
					 + info_ptr->vrsave_size
					 + info_ptr->toc_size,
					 save_align);

  non_fixed_size	 = (info_ptr->vars_size
			    + info_ptr->parm_size
			    + info_ptr->save_size
			    + info_ptr->varargs_size);

  info_ptr->total_size = RS6000_ALIGN (non_fixed_size + info_ptr->fixed_size,
				       ABI_STACK_BOUNDARY / BITS_PER_UNIT);

  /* Determine if we need to allocate any stack frame:

     For AIX we need to push the stack if a frame pointer is needed
     (because the stack might be dynamically adjusted), if we are
     debugging, if we make calls, or if the sum of fp_save, gp_save,
     and local variables are more than the space needed to save all
     non-volatile registers: 32-bit: 18*8 + 19*4 = 220 or 64-bit: 18*8
     + 18*8 = 288 (GPR13 reserved).

     For V.4 we don't have the stack cushion that AIX uses, but assume
     that the debugger can handle stackless frames.  */

  if (info_ptr->calls_p)
    info_ptr->push_p = 1;

  else if (DEFAULT_ABI == ABI_V4)
    info_ptr->push_p = non_fixed_size != 0;

  else if (frame_pointer_needed)
    info_ptr->push_p = 1;

  else if (TARGET_XCOFF && write_symbols != NO_DEBUG)
    info_ptr->push_p = 1;

  else
    info_ptr->push_p = non_fixed_size > (TARGET_32BIT ? 220 : 288);

  /* Zero offsets if we're not saving those registers.  */
  if (info_ptr->fp_size == 0)
    info_ptr->fp_save_offset = 0;

  if (info_ptr->gp_size == 0)
    info_ptr->gp_save_offset = 0;

  if (! TARGET_ALTIVEC_ABI || info_ptr->altivec_size == 0)
    info_ptr->altivec_save_offset = 0;

  if (! TARGET_ALTIVEC_ABI || info_ptr->vrsave_mask == 0)
    info_ptr->vrsave_save_offset = 0;

  if (! TARGET_SPE_ABI
      || info_ptr->spe_64bit_regs_used == 0
      || info_ptr->spe_gp_size == 0)
    info_ptr->spe_gp_save_offset = 0;

  if (! info_ptr->lr_save_p)
    info_ptr->lr_save_offset = 0;

  if (! info_ptr->cr_save_p)
    info_ptr->cr_save_offset = 0;

  if (! info_ptr->toc_save_p)
    info_ptr->toc_save_offset = 0;

  return info_ptr;
}

/* Return true if the current function uses any GPRs in 64-bit SIMD
   mode.  */

static bool
spe_func_has_64bit_regs_p (void)
{
  rtx insns, insn;

  /* Functions that save and restore all the call-saved registers will
     need to save/restore the registers in 64-bits.  */
  if (current_function_calls_eh_return
      || current_function_calls_setjmp
      || current_function_has_nonlocal_goto)
    return true;

  insns = get_insns ();

  for (insn = NEXT_INSN (insns); insn != NULL_RTX; insn = NEXT_INSN (insn))
    {
      if (INSN_P (insn))
	{
	  rtx i;

	  i = PATTERN (insn);
	  if (GET_CODE (i) == SET
	      && SPE_VECTOR_MODE (GET_MODE (SET_SRC (i))))
	    return true;
	}
    }

  return false;
}

static void
debug_stack_info (rs6000_stack_t *info)
{
  const char *abi_string;

  if (! info)
    info = rs6000_stack_info ();

  fprintf (stderr, "\nStack information for function %s:\n",
	   ((current_function_decl && DECL_NAME (current_function_decl))
	    ? IDENTIFIER_POINTER (DECL_NAME (current_function_decl))
	    : "<unknown>"));

  switch (info->abi)
    {
    default:		 abi_string = "Unknown";	break;
    case ABI_NONE:	 abi_string = "NONE";		break;
    case ABI_AIX:	 abi_string = "AIX";		break;
    case ABI_DARWIN:	 abi_string = "Darwin";		break;
    case ABI_V4:	 abi_string = "V.4";		break;
    }

  fprintf (stderr, "\tABI                 = %5s\n", abi_string);

  if (TARGET_ALTIVEC_ABI)
    fprintf (stderr, "\tALTIVEC ABI extensions enabled.\n");

  if (TARGET_SPE_ABI)
    fprintf (stderr, "\tSPE ABI extensions enabled.\n");

  if (info->first_gp_reg_save != 32)
    fprintf (stderr, "\tfirst_gp_reg_save   = %5d\n", info->first_gp_reg_save);

  if (info->first_fp_reg_save != 64)
    fprintf (stderr, "\tfirst_fp_reg_save   = %5d\n", info->first_fp_reg_save);

  if (info->first_altivec_reg_save <= LAST_ALTIVEC_REGNO)
    fprintf (stderr, "\tfirst_altivec_reg_save = %5d\n",
	     info->first_altivec_reg_save);

  if (info->lr_save_p)
    fprintf (stderr, "\tlr_save_p           = %5d\n", info->lr_save_p);

  if (info->cr_save_p)
    fprintf (stderr, "\tcr_save_p           = %5d\n", info->cr_save_p);

  if (info->toc_save_p)
    fprintf (stderr, "\ttoc_save_p          = %5d\n", info->toc_save_p);

  if (info->vrsave_mask)
    fprintf (stderr, "\tvrsave_mask         = 0x%x\n", info->vrsave_mask);

  if (info->push_p)
    fprintf (stderr, "\tpush_p              = %5d\n", info->push_p);

  if (info->calls_p)
    fprintf (stderr, "\tcalls_p             = %5d\n", info->calls_p);

  if (info->gp_save_offset)
    fprintf (stderr, "\tgp_save_offset      = %5d\n", info->gp_save_offset);

  if (info->fp_save_offset)
    fprintf (stderr, "\tfp_save_offset      = %5d\n", info->fp_save_offset);

  if (info->altivec_save_offset)
    fprintf (stderr, "\taltivec_save_offset = %5d\n",
	     info->altivec_save_offset);

  if (info->spe_gp_save_offset)
    fprintf (stderr, "\tspe_gp_save_offset  = %5d\n",
	     info->spe_gp_save_offset);

  if (info->vrsave_save_offset)
    fprintf (stderr, "\tvrsave_save_offset  = %5d\n",
	     info->vrsave_save_offset);

  if (info->lr_save_offset)
    fprintf (stderr, "\tlr_save_offset      = %5d\n", info->lr_save_offset);

  if (info->cr_save_offset)
    fprintf (stderr, "\tcr_save_offset      = %5d\n", info->cr_save_offset);

  if (info->toc_save_offset)
    fprintf (stderr, "\ttoc_save_offset     = %5d\n", info->toc_save_offset);

  if (info->varargs_save_offset)
    fprintf (stderr, "\tvarargs_save_offset = %5d\n", info->varargs_save_offset);

  if (info->total_size)
    fprintf (stderr, "\ttotal_size          = "HOST_WIDE_INT_PRINT_DEC"\n",
	     info->total_size);

  if (info->varargs_size)
    fprintf (stderr, "\tvarargs_size        = %5d\n", info->varargs_size);

  if (info->vars_size)
    fprintf (stderr, "\tvars_size           = "HOST_WIDE_INT_PRINT_DEC"\n",
	     info->vars_size);

  if (info->parm_size)
    fprintf (stderr, "\tparm_size           = %5d\n", info->parm_size);

  if (info->fixed_size)
    fprintf (stderr, "\tfixed_size          = %5d\n", info->fixed_size);

  if (info->gp_size)
    fprintf (stderr, "\tgp_size             = %5d\n", info->gp_size);

  if (info->spe_gp_size)
    fprintf (stderr, "\tspe_gp_size         = %5d\n", info->spe_gp_size);

  if (info->fp_size)
    fprintf (stderr, "\tfp_size             = %5d\n", info->fp_size);

  if (info->altivec_size)
    fprintf (stderr, "\taltivec_size        = %5d\n", info->altivec_size);

  if (info->vrsave_size)
    fprintf (stderr, "\tvrsave_size         = %5d\n", info->vrsave_size);

  if (info->altivec_padding_size)
    fprintf (stderr, "\taltivec_padding_size= %5d\n",
	     info->altivec_padding_size);

  if (info->spe_padding_size)
    fprintf (stderr, "\tspe_padding_size    = %5d\n",
	     info->spe_padding_size);

  if (info->lr_size)
    fprintf (stderr, "\tlr_size             = %5d\n", info->lr_size);

  if (info->cr_size)
    fprintf (stderr, "\tcr_size             = %5d\n", info->cr_size);

  if (info->toc_size)
    fprintf (stderr, "\ttoc_size            = %5d\n", info->toc_size);

  if (info->save_size)
    fprintf (stderr, "\tsave_size           = %5d\n", info->save_size);

  if (info->reg_size != 4)
    fprintf (stderr, "\treg_size            = %5d\n", info->reg_size);

  fprintf (stderr, "\n");
}

rtx
rs6000_return_addr (int count, rtx frame)
{
  /* Currently we don't optimize very well between prolog and body
     code and for PIC code the code can be actually quite bad, so
     don't try to be too clever here.  */
  if (count != 0 || (DEFAULT_ABI != ABI_AIX && flag_pic))
    {
      cfun->machine->ra_needs_full_frame = 1;

      return
	gen_rtx_MEM
	  (Pmode,
	   memory_address
	   (Pmode,
	    plus_constant (copy_to_reg
			   (gen_rtx_MEM (Pmode,
					 memory_address (Pmode, frame))),
			   RETURN_ADDRESS_OFFSET)));
    }

  cfun->machine->ra_need_lr = 1;
  return get_hard_reg_initial_val (Pmode, LINK_REGISTER_REGNUM);
}

/* Say whether a function is a candidate for sibcall handling or not.
   We do not allow indirect calls to be optimized into sibling calls.
   Also, we can't do it if there are any vector parameters; there's
   nowhere to put the VRsave code so it works; note that functions with
   vector parameters are required to have a prototype, so the argument
   type info must be available here.  (The tail recursion case can work
   with vector parameters, but there's no way to distinguish here.) */
static bool
rs6000_function_ok_for_sibcall (tree decl, tree exp ATTRIBUTE_UNUSED)
{
  tree type;
  if (decl)
    {
      if (TARGET_ALTIVEC_VRSAVE)
        {
	  for (type = TYPE_ARG_TYPES (TREE_TYPE (decl));
	       type; type = TREE_CHAIN (type))
	    {
	      if (TREE_CODE (TREE_VALUE (type)) == VECTOR_TYPE)
		return false;
	    }
        }
      if (DEFAULT_ABI == ABI_DARWIN
	  || (*targetm.binds_local_p) (decl))
	{
	  tree attr_list = TYPE_ATTRIBUTES (TREE_TYPE (decl));

	  if (!lookup_attribute ("longcall", attr_list)
	      || lookup_attribute ("shortcall", attr_list))
	    return true;
	}
    }
  return false;
}

static int
rs6000_ra_ever_killed (void)
{
  rtx top;
  rtx reg;
  rtx insn;

  if (current_function_is_thunk)
    return 0;

  /* regs_ever_live has LR marked as used if any sibcalls are present,
     but this should not force saving and restoring in the
     pro/epilogue.  Likewise, reg_set_between_p thinks a sibcall
     clobbers LR, so that is inappropriate.  */

  /* Also, the prologue can generate a store into LR that
     doesn't really count, like this:

        move LR->R0
        bcl to set PIC register
        move LR->R31
        move R0->LR

     When we're called from the epilogue, we need to avoid counting
     this as a store.  */
         
  push_topmost_sequence ();
  top = get_insns ();
  pop_topmost_sequence ();
  reg = gen_rtx_REG (Pmode, LINK_REGISTER_REGNUM);

  for (insn = NEXT_INSN (top); insn != NULL_RTX; insn = NEXT_INSN (insn))
    {
      if (INSN_P (insn))
	{
	  if (FIND_REG_INC_NOTE (insn, reg))
	    return 1;
	  else if (GET_CODE (insn) == CALL_INSN 
		   && !SIBLING_CALL_P (insn))
	    return 1;
	  else if (set_of (reg, insn) != NULL_RTX
		   && !prologue_epilogue_contains (insn))
	    return 1;
    	}
    }
  return 0;
}

/* Add a REG_MAYBE_DEAD note to the insn.  */
static void
rs6000_maybe_dead (rtx insn)
{
  REG_NOTES (insn) = gen_rtx_EXPR_LIST (REG_MAYBE_DEAD,
					const0_rtx,
					REG_NOTES (insn));
}

/* Emit instructions needed to load the TOC register.
   This is only needed when TARGET_TOC, TARGET_MINIMAL_TOC, and there is
   a constant pool; or for SVR4 -fpic.  */

void
rs6000_emit_load_toc_table (int fromprolog)
{
  rtx dest, insn;
  dest = gen_rtx_REG (Pmode, RS6000_PIC_OFFSET_TABLE_REGNUM);

  if (TARGET_ELF && DEFAULT_ABI == ABI_V4 && flag_pic == 1)
    {
      rtx temp = (fromprolog
		  ? gen_rtx_REG (Pmode, LINK_REGISTER_REGNUM)
		  : gen_reg_rtx (Pmode));
      insn = emit_insn (gen_load_toc_v4_pic_si (temp));
      if (fromprolog)
	rs6000_maybe_dead (insn);
      insn = emit_move_insn (dest, temp);
      if (fromprolog)
	rs6000_maybe_dead (insn);
    }
  else if (TARGET_ELF && DEFAULT_ABI != ABI_AIX && flag_pic == 2)
    {
      char buf[30];
      rtx tempLR = (fromprolog
		    ? gen_rtx_REG (Pmode, LINK_REGISTER_REGNUM)
		    : gen_reg_rtx (Pmode));
      rtx temp0 = (fromprolog
		   ? gen_rtx_REG (Pmode, 0)
		   : gen_reg_rtx (Pmode));

      /* possibly create the toc section */
      if (! toc_initialized)
	{
	  toc_section ();
	  function_section (current_function_decl);
	}

      if (fromprolog)
	{
	  rtx symF, symL;

	  ASM_GENERATE_INTERNAL_LABEL (buf, "LCF", rs6000_pic_labelno);
	  symF = gen_rtx_SYMBOL_REF (Pmode, ggc_strdup (buf));

	  ASM_GENERATE_INTERNAL_LABEL (buf, "LCL", rs6000_pic_labelno);
	  symL = gen_rtx_SYMBOL_REF (Pmode, ggc_strdup (buf));

	  rs6000_maybe_dead (emit_insn (gen_load_toc_v4_PIC_1 (tempLR,
							       symF)));
	  rs6000_maybe_dead (emit_move_insn (dest, tempLR));
	  rs6000_maybe_dead (emit_insn (gen_load_toc_v4_PIC_2 (temp0, dest,
							       symL,
							       symF)));
	}
      else
	{
	  rtx tocsym;

	  tocsym = gen_rtx_SYMBOL_REF (Pmode, toc_label_name);
	  emit_insn (gen_load_toc_v4_PIC_1b (tempLR, tocsym));
	  emit_move_insn (dest, tempLR);
	  emit_move_insn (temp0, gen_rtx_MEM (Pmode, dest));
	}
      insn = emit_insn (gen_addsi3 (dest, temp0, dest));
      if (fromprolog)
	rs6000_maybe_dead (insn);
    }
  else if (TARGET_ELF && !TARGET_AIX && flag_pic == 0 && TARGET_MINIMAL_TOC)
    {
      /* This is for AIX code running in non-PIC ELF32.  */
      char buf[30];
      rtx realsym;
      ASM_GENERATE_INTERNAL_LABEL (buf, "LCTOC", 1);
      realsym = gen_rtx_SYMBOL_REF (Pmode, ggc_strdup (buf));

      insn = emit_insn (gen_elf_high (dest, realsym));
      if (fromprolog)
	rs6000_maybe_dead (insn);
      insn = emit_insn (gen_elf_low (dest, dest, realsym));
      if (fromprolog)
	rs6000_maybe_dead (insn);
    }
  else if (DEFAULT_ABI == ABI_AIX)
    {
      if (TARGET_32BIT)
	insn = emit_insn (gen_load_toc_aix_si (dest));
      else
	insn = emit_insn (gen_load_toc_aix_di (dest));
      if (fromprolog)
	rs6000_maybe_dead (insn);
    }
  else
    abort ();
}

/* Emit instructions to restore the link register after determining where
   its value has been stored.  */

void
rs6000_emit_eh_reg_restore (rtx source, rtx scratch)
{
  rs6000_stack_t *info = rs6000_stack_info ();
  rtx operands[2];

  operands[0] = source;
  operands[1] = scratch;

  if (info->lr_save_p)
    {
      rtx frame_rtx = stack_pointer_rtx;
      HOST_WIDE_INT sp_offset = 0;
      rtx tmp;

      if (frame_pointer_needed
	  || current_function_calls_alloca
	  || info->total_size > 32767)
	{
	  emit_move_insn (operands[1], gen_rtx_MEM (Pmode, frame_rtx));
	  frame_rtx = operands[1];
	}
      else if (info->push_p)
	sp_offset = info->total_size;

      tmp = plus_constant (frame_rtx, info->lr_save_offset + sp_offset);
      tmp = gen_rtx_MEM (Pmode, tmp);
      emit_move_insn (tmp, operands[0]);
    }
  else
    emit_move_insn (gen_rtx_REG (Pmode, LINK_REGISTER_REGNUM), operands[0]);
}

static GTY(()) int set = -1;

int   
get_TOC_alias_set (void)
{
  if (set == -1)
    set = new_alias_set ();
  return set;
}   

/* This returns nonzero if the current function uses the TOC.  This is
   determined by the presence of (use (unspec ... UNSPEC_TOC)), which
   is generated by the ABI_V4 load_toc_* patterns.  */
#if TARGET_ELF
static int
uses_TOC (void) 
{
  rtx insn;

  for (insn = get_insns (); insn; insn = NEXT_INSN (insn))
    if (INSN_P (insn))
      {
	rtx pat = PATTERN (insn);
	int i;

	if (GET_CODE (pat) == PARALLEL) 
	  for (i = 0; i < XVECLEN (pat, 0); i++)
	    {
	      rtx sub = XVECEXP (pat, 0, i);
	      if (GET_CODE (sub) == USE)
		{
		  sub = XEXP (sub, 0);
		  if (GET_CODE (sub) == UNSPEC
		      && XINT (sub, 1) == UNSPEC_TOC)
		    return 1;
		}
	    }
      }
  return 0;
}
#endif

rtx
create_TOC_reference (rtx symbol) 
{
  if (no_new_pseudos)
    regs_ever_live[TOC_REGISTER] = 1;
  return gen_rtx_PLUS (Pmode, 
	   gen_rtx_REG (Pmode, TOC_REGISTER),
	     gen_rtx_CONST (Pmode, 
	       gen_rtx_MINUS (Pmode, symbol, 
		 gen_rtx_SYMBOL_REF (Pmode, toc_label_name))));
}

/* If _Unwind_* has been called from within the same module,
   toc register is not guaranteed to be saved to 40(1) on function
   entry.  Save it there in that case.  */

void
rs6000_aix_emit_builtin_unwind_init (void)
{
  rtx mem;
  rtx stack_top = gen_reg_rtx (Pmode);
  rtx opcode_addr = gen_reg_rtx (Pmode);
  rtx opcode = gen_reg_rtx (SImode);
  rtx tocompare = gen_reg_rtx (SImode);
  rtx no_toc_save_needed = gen_label_rtx ();

  mem = gen_rtx_MEM (Pmode, hard_frame_pointer_rtx);
  emit_move_insn (stack_top, mem);

  mem = gen_rtx_MEM (Pmode,
		     gen_rtx_PLUS (Pmode, stack_top,
				   GEN_INT (2 * GET_MODE_SIZE (Pmode))));
  emit_move_insn (opcode_addr, mem);
  emit_move_insn (opcode, gen_rtx_MEM (SImode, opcode_addr));
  emit_move_insn (tocompare, gen_int_mode (TARGET_32BIT ? 0x80410014
					   : 0xE8410028, SImode));

  do_compare_rtx_and_jump (opcode, tocompare, EQ, 1,
			   SImode, NULL_RTX, NULL_RTX,
			   no_toc_save_needed);

  mem = gen_rtx_MEM (Pmode,
		     gen_rtx_PLUS (Pmode, stack_top,
				   GEN_INT (5 * GET_MODE_SIZE (Pmode))));
  emit_move_insn (mem, gen_rtx_REG (Pmode, 2));
  emit_label (no_toc_save_needed);
}

/* This ties together stack memory (MEM with an alias set of
   rs6000_sr_alias_set) and the change to the stack pointer.  */

static void
rs6000_emit_stack_tie (void)
{
  rtx mem = gen_rtx_MEM (BLKmode, gen_rtx_REG (Pmode, STACK_POINTER_REGNUM));

  set_mem_alias_set (mem, rs6000_sr_alias_set);
  emit_insn (gen_stack_tie (mem));
}

/* Emit the correct code for allocating stack space, as insns.
   If COPY_R12, make sure a copy of the old frame is left in r12.
   The generated code may use hard register 0 as a temporary.  */

static void
rs6000_emit_allocate_stack (HOST_WIDE_INT size, int copy_r12)
{
  rtx insn;
  rtx stack_reg = gen_rtx_REG (Pmode, STACK_POINTER_REGNUM);
  rtx tmp_reg = gen_rtx_REG (Pmode, 0);
  rtx todec = GEN_INT (-size);

  if (current_function_limit_stack)
    {
      if (REG_P (stack_limit_rtx)
	  && REGNO (stack_limit_rtx) > 1 
	  && REGNO (stack_limit_rtx) <= 31)
	{
	  emit_insn (TARGET_32BIT
		     ? gen_addsi3 (tmp_reg,
				   stack_limit_rtx,
				   GEN_INT (size))
		     : gen_adddi3 (tmp_reg,
				   stack_limit_rtx,
				   GEN_INT (size)));

	  emit_insn (gen_cond_trap (LTU, stack_reg, tmp_reg,
				    const0_rtx));
	}
      else if (GET_CODE (stack_limit_rtx) == SYMBOL_REF
	       && TARGET_32BIT
	       && DEFAULT_ABI == ABI_V4)
	{
	  rtx toload = gen_rtx_CONST (VOIDmode,
				      gen_rtx_PLUS (Pmode, 
						    stack_limit_rtx, 
						    GEN_INT (size)));

	  emit_insn (gen_elf_high (tmp_reg, toload));
	  emit_insn (gen_elf_low (tmp_reg, tmp_reg, toload));
	  emit_insn (gen_cond_trap (LTU, stack_reg, tmp_reg,
				    const0_rtx));
	}
      else
	warning ("stack limit expression is not supported");
    }

  if (copy_r12 || ! TARGET_UPDATE)
    emit_move_insn (gen_rtx_REG (Pmode, 12), stack_reg);

  if (TARGET_UPDATE)
    {
      if (size > 32767)
	{
	  /* Need a note here so that try_split doesn't get confused.  */
	  if (get_last_insn() == NULL_RTX)
	    emit_note (NOTE_INSN_DELETED);
	  insn = emit_move_insn (tmp_reg, todec);
	  try_split (PATTERN (insn), insn, 0);
	  todec = tmp_reg;
	}

      insn = emit_insn (TARGET_32BIT
			? gen_movsi_update (stack_reg, stack_reg,
					    todec, stack_reg)
			: gen_movdi_update (stack_reg, stack_reg, 
					    todec, stack_reg));
    }
  else
    {
      insn = emit_insn (TARGET_32BIT
			? gen_addsi3 (stack_reg, stack_reg, todec)
			: gen_adddi3 (stack_reg, stack_reg, todec));
      emit_move_insn (gen_rtx_MEM (Pmode, stack_reg),
		      gen_rtx_REG (Pmode, 12));
    }
 
  RTX_FRAME_RELATED_P (insn) = 1;
  REG_NOTES (insn) = 
    gen_rtx_EXPR_LIST (REG_FRAME_RELATED_EXPR,
		       gen_rtx_SET (VOIDmode, stack_reg, 
				    gen_rtx_PLUS (Pmode, stack_reg,
						  GEN_INT (-size))),
		       REG_NOTES (insn));
}

/* Add to 'insn' a note which is PATTERN (INSN) but with REG replaced
   with (plus:P (reg 1) VAL), and with REG2 replaced with RREG if REG2
   is not NULL.  It would be nice if dwarf2out_frame_debug_expr could
   deduce these equivalences by itself so it wasn't necessary to hold
   its hand so much.  */

static void
rs6000_frame_related (rtx insn, rtx reg, HOST_WIDE_INT val, 
		      rtx reg2, rtx rreg)
{
  rtx real, temp;

  /* copy_rtx will not make unique copies of registers, so we need to
     ensure we don't have unwanted sharing here.  */
  if (reg == reg2)
    reg = gen_raw_REG (GET_MODE (reg), REGNO (reg));

  if (reg == rreg)
    reg = gen_raw_REG (GET_MODE (reg), REGNO (reg));

  real = copy_rtx (PATTERN (insn));

  if (reg2 != NULL_RTX)
    real = replace_rtx (real, reg2, rreg);
  
  real = replace_rtx (real, reg, 
		      gen_rtx_PLUS (Pmode, gen_rtx_REG (Pmode,
							STACK_POINTER_REGNUM),
				    GEN_INT (val)));
  
  /* We expect that 'real' is either a SET or a PARALLEL containing
     SETs (and possibly other stuff).  In a PARALLEL, all the SETs
     are important so they all have to be marked RTX_FRAME_RELATED_P.  */

  if (GET_CODE (real) == SET)
    {
      rtx set = real;
      
      temp = simplify_rtx (SET_SRC (set));
      if (temp)
	SET_SRC (set) = temp;
      temp = simplify_rtx (SET_DEST (set));
      if (temp)
	SET_DEST (set) = temp;
      if (GET_CODE (SET_DEST (set)) == MEM)
	{
	  temp = simplify_rtx (XEXP (SET_DEST (set), 0));
	  if (temp)
	    XEXP (SET_DEST (set), 0) = temp;
	}
    }
  else if (GET_CODE (real) == PARALLEL)
    {
      int i;
      for (i = 0; i < XVECLEN (real, 0); i++)
	if (GET_CODE (XVECEXP (real, 0, i)) == SET)
	  {
	    rtx set = XVECEXP (real, 0, i);
	    
	    temp = simplify_rtx (SET_SRC (set));
	    if (temp)
	      SET_SRC (set) = temp;
	    temp = simplify_rtx (SET_DEST (set));
	    if (temp)
	      SET_DEST (set) = temp;
	    if (GET_CODE (SET_DEST (set)) == MEM)
	      {
		temp = simplify_rtx (XEXP (SET_DEST (set), 0));
		if (temp)
		  XEXP (SET_DEST (set), 0) = temp;
	      }
	    RTX_FRAME_RELATED_P (set) = 1;
	  }
    }
  else
    abort ();

  if (TARGET_SPE)
    real = spe_synthesize_frame_save (real);

  RTX_FRAME_RELATED_P (insn) = 1;
  REG_NOTES (insn) = gen_rtx_EXPR_LIST (REG_FRAME_RELATED_EXPR,
					real,
					REG_NOTES (insn));
}

/* Given an SPE frame note, return a PARALLEL of SETs with the
   original note, plus a synthetic register save.  */

static rtx
spe_synthesize_frame_save (rtx real)
{
  rtx synth, offset, reg, real2;

  if (GET_CODE (real) != SET
      || GET_MODE (SET_SRC (real)) != V2SImode)
    return real;

  /* For the SPE, registers saved in 64-bits, get a PARALLEL for their
     frame related note.  The parallel contains a set of the register
     being saved, and another set to a synthetic register (n+1200).
     This is so we can differentiate between 64-bit and 32-bit saves.
     Words cannot describe this nastiness.  */

  if (GET_CODE (SET_DEST (real)) != MEM
      || GET_CODE (XEXP (SET_DEST (real), 0)) != PLUS
      || GET_CODE (SET_SRC (real)) != REG)
    abort ();

  /* Transform:
       (set (mem (plus (reg x) (const y)))
            (reg z))
     into:
       (set (mem (plus (reg x) (const y+4)))
            (reg z+1200))
  */

  real2 = copy_rtx (real);
  PUT_MODE (SET_DEST (real2), SImode);
  reg = SET_SRC (real2);
  real2 = replace_rtx (real2, reg, gen_rtx_REG (SImode, REGNO (reg)));
  synth = copy_rtx (real2);

  if (BYTES_BIG_ENDIAN)
    {
      offset = XEXP (XEXP (SET_DEST (real2), 0), 1);
      real2 = replace_rtx (real2, offset, GEN_INT (INTVAL (offset) + 4));
    }

  reg = SET_SRC (synth);

  synth = replace_rtx (synth, reg,
		       gen_rtx_REG (SImode, REGNO (reg) + 1200));

  offset = XEXP (XEXP (SET_DEST (synth), 0), 1);
  synth = replace_rtx (synth, offset,
		       GEN_INT (INTVAL (offset)
				+ (BYTES_BIG_ENDIAN ? 0 : 4)));

  RTX_FRAME_RELATED_P (synth) = 1;
  RTX_FRAME_RELATED_P (real2) = 1;
  if (BYTES_BIG_ENDIAN)
    real = gen_rtx_PARALLEL (VOIDmode, gen_rtvec (2, synth, real2));
  else
    real = gen_rtx_PARALLEL (VOIDmode, gen_rtvec (2, real2, synth));

  return real;
}

/* Returns an insn that has a vrsave set operation with the
   appropriate CLOBBERs.  */

static rtx
generate_set_vrsave (rtx reg, rs6000_stack_t *info, int epiloguep)
{
  int nclobs, i;
  rtx insn, clobs[TOTAL_ALTIVEC_REGS + 1];
  rtx vrsave = gen_rtx_REG (SImode, VRSAVE_REGNO);

  clobs[0]
    = gen_rtx_SET (VOIDmode,
		   vrsave,
		   gen_rtx_UNSPEC_VOLATILE (SImode,
					    gen_rtvec (2, reg, vrsave),
					    30));

  nclobs = 1;

  /* We need to clobber the registers in the mask so the scheduler
     does not move sets to VRSAVE before sets of AltiVec registers.

     However, if the function receives nonlocal gotos, reload will set
     all call saved registers live.  We will end up with:

     	(set (reg 999) (mem))
	(parallel [ (set (reg vrsave) (unspec blah))
		    (clobber (reg 999))])

     The clobber will cause the store into reg 999 to be dead, and
     flow will attempt to delete an epilogue insn.  In this case, we
     need an unspec use/set of the register.  */

  for (i = FIRST_ALTIVEC_REGNO; i <= LAST_ALTIVEC_REGNO; ++i)
    if (info->vrsave_mask & ALTIVEC_REG_BIT (i))
      {
	if (!epiloguep || call_used_regs [i])
	  clobs[nclobs++] = gen_rtx_CLOBBER (VOIDmode,
					     gen_rtx_REG (V4SImode, i));
	else
	  {
	    rtx reg = gen_rtx_REG (V4SImode, i);

	    clobs[nclobs++]
	      = gen_rtx_SET (VOIDmode,
			     reg,
			     gen_rtx_UNSPEC (V4SImode,
					     gen_rtvec (1, reg), 27));
	  }
      }

  insn = gen_rtx_PARALLEL (VOIDmode, rtvec_alloc (nclobs));

  for (i = 0; i < nclobs; ++i)
    XVECEXP (insn, 0, i) = clobs[i];

  return insn;
}

/* Save a register into the frame, and emit RTX_FRAME_RELATED_P notes.
   Save REGNO into [FRAME_REG + OFFSET] in mode MODE.  */

static void
emit_frame_save (rtx frame_reg, rtx frame_ptr, enum machine_mode mode, 
		 unsigned int regno, int offset, HOST_WIDE_INT total_size)
{
  rtx reg, offset_rtx, insn, mem, addr, int_rtx;
  rtx replacea, replaceb;

  int_rtx = GEN_INT (offset);

  /* Some cases that need register indexed addressing.  */
  if ((TARGET_ALTIVEC_ABI && ALTIVEC_VECTOR_MODE (mode))
      || (TARGET_SPE_ABI
	  && SPE_VECTOR_MODE (mode)
	  && !SPE_CONST_OFFSET_OK (offset)))
    {
      /* Whomever calls us must make sure r11 is available in the
         flow path of instructions in the prologue.  */
      offset_rtx = gen_rtx_REG (Pmode, 11);
      emit_move_insn (offset_rtx, int_rtx);

      replacea = offset_rtx;
      replaceb = int_rtx;
    }
  else
    {
      offset_rtx = int_rtx;
      replacea = NULL_RTX;
      replaceb = NULL_RTX;
    }

  reg = gen_rtx_REG (mode, regno);
  addr = gen_rtx_PLUS (Pmode, frame_reg, offset_rtx);
  mem = gen_rtx_MEM (mode, addr);
  set_mem_alias_set (mem, rs6000_sr_alias_set);

  insn = emit_move_insn (mem, reg);

  rs6000_frame_related (insn, frame_ptr, total_size, replacea, replaceb);
}

/* Emit an offset memory reference suitable for a frame store, while
   converting to a valid addressing mode.  */

static rtx
gen_frame_mem_offset (enum machine_mode mode, rtx reg, int offset)
{
  rtx int_rtx, offset_rtx;

  int_rtx = GEN_INT (offset);

  if (TARGET_SPE_ABI && SPE_VECTOR_MODE (mode))
    {
      offset_rtx = gen_rtx_REG (Pmode, FIXED_SCRATCH);
      emit_move_insn (offset_rtx, int_rtx);
    }
  else
    offset_rtx = int_rtx;

  return gen_rtx_MEM (mode, gen_rtx_PLUS (Pmode, reg, offset_rtx));
}

/* Emit function prologue as insns.  */

void
rs6000_emit_prologue (void)
{
  rs6000_stack_t *info = rs6000_stack_info ();
  enum machine_mode reg_mode = Pmode;
  int reg_size = TARGET_32BIT ? 4 : 8;
  rtx sp_reg_rtx = gen_rtx_REG (Pmode, STACK_POINTER_REGNUM);
  rtx frame_ptr_rtx = gen_rtx_REG (Pmode, 12);
  rtx frame_reg_rtx = sp_reg_rtx;
  rtx cr_save_rtx = NULL_RTX;
  rtx insn;
  int saving_FPRs_inline;
  int using_store_multiple;
  HOST_WIDE_INT sp_offset = 0;
  
   if (TARGET_SPE_ABI && info->spe_64bit_regs_used != 0)
     {
       reg_mode = V2SImode;
       reg_size = 8;
     }

  using_store_multiple = (TARGET_MULTIPLE && ! TARGET_POWERPC64
			  && (!TARGET_SPE_ABI
			      || info->spe_64bit_regs_used == 0)
			  && info->first_gp_reg_save < 31);
  saving_FPRs_inline = (info->first_fp_reg_save == 64
			|| FP_SAVE_INLINE (info->first_fp_reg_save)
			|| current_function_calls_eh_return
			|| cfun->machine->ra_need_lr);

  /* For V.4, update stack before we do any saving and set back pointer.  */
  if (info->push_p
      && (DEFAULT_ABI == ABI_V4
	  || current_function_calls_eh_return))
    {
      if (info->total_size < 32767)
	sp_offset = info->total_size;
      else
	frame_reg_rtx = frame_ptr_rtx;
      rs6000_emit_allocate_stack (info->total_size, 
				  (frame_reg_rtx != sp_reg_rtx
				   && (info->cr_save_p
				       || info->lr_save_p
				       || info->first_fp_reg_save < 64
				       || info->first_gp_reg_save < 32
				       )));
      if (frame_reg_rtx != sp_reg_rtx)
	rs6000_emit_stack_tie ();
    }

  /* Save AltiVec registers if needed.  */
  if (TARGET_ALTIVEC_ABI && info->altivec_size != 0)
    {
      int i;

      /* There should be a non inline version of this, for when we
	 are saving lots of vector registers.  */
      for (i = info->first_altivec_reg_save; i <= LAST_ALTIVEC_REGNO; ++i)
	if (info->vrsave_mask & ALTIVEC_REG_BIT (i))
	  {
	    rtx areg, savereg, mem;
	    int offset;

	    offset = info->altivec_save_offset + sp_offset
	      + 16 * (i - info->first_altivec_reg_save);

	    savereg = gen_rtx_REG (V4SImode, i);

	    areg = gen_rtx_REG (Pmode, 0);
	    emit_move_insn (areg, GEN_INT (offset));

	    /* AltiVec addressing mode is [reg+reg].  */
	    mem = gen_rtx_MEM (V4SImode,
			       gen_rtx_PLUS (Pmode, frame_reg_rtx, areg));
			       
	    set_mem_alias_set (mem, rs6000_sr_alias_set);

	    insn = emit_move_insn (mem, savereg);

	    rs6000_frame_related (insn, frame_ptr_rtx, info->total_size,
				  areg, GEN_INT (offset));
	  }
    }

  /* VRSAVE is a bit vector representing which AltiVec registers
     are used.  The OS uses this to determine which vector
     registers to save on a context switch.  We need to save
     VRSAVE on the stack frame, add whatever AltiVec registers we
     used in this function, and do the corresponding magic in the
     epilogue.  */

  if (TARGET_ALTIVEC && TARGET_ALTIVEC_VRSAVE
      && info->vrsave_mask != 0)
    {
      rtx reg, mem, vrsave;
      int offset;

      /* Get VRSAVE onto a GPR.  Note that ABI_V4 might be using r12
	 as frame_reg_rtx and r11 as the static chain pointer for
	 nested functions.  */
      reg = gen_rtx_REG (SImode, 0);
      vrsave = gen_rtx_REG (SImode, VRSAVE_REGNO);
      if (TARGET_MACHO)
	emit_insn (gen_get_vrsave_internal (reg));
      else
	emit_insn (gen_rtx_SET (VOIDmode, reg, vrsave));

      /* Save VRSAVE.  */
      offset = info->vrsave_save_offset + sp_offset;
      mem
	= gen_rtx_MEM (SImode,
		       gen_rtx_PLUS (Pmode, frame_reg_rtx, GEN_INT (offset)));
      set_mem_alias_set (mem, rs6000_sr_alias_set);
      insn = emit_move_insn (mem, reg);

      /* Include the registers in the mask.  */
      emit_insn (gen_iorsi3 (reg, reg, GEN_INT ((int) info->vrsave_mask)));

      insn = emit_insn (generate_set_vrsave (reg, info, 0));
    }

  /* If we use the link register, get it into r0.  */
  if (info->lr_save_p)
    emit_move_insn (gen_rtx_REG (Pmode, 0),
		    gen_rtx_REG (Pmode, LINK_REGISTER_REGNUM));

  /* If we need to save CR, put it into r12.  */
  if (info->cr_save_p && frame_reg_rtx != frame_ptr_rtx)
    {
      cr_save_rtx = gen_rtx_REG (SImode, 12);
      emit_insn (gen_movesi_from_cr (cr_save_rtx));
    }

  /* Do any required saving of fpr's.  If only one or two to save, do
     it ourselves.  Otherwise, call function.  */
  if (saving_FPRs_inline)
    {
      int i;
      for (i = 0; i < 64 - info->first_fp_reg_save; i++)
	if ((regs_ever_live[info->first_fp_reg_save+i] 
	     && ! call_used_regs[info->first_fp_reg_save+i]))
	  emit_frame_save (frame_reg_rtx, frame_ptr_rtx, DFmode,
			   info->first_fp_reg_save + i,
			   info->fp_save_offset + sp_offset + 8 * i,
			   info->total_size);
    }
  else if (info->first_fp_reg_save != 64)
    {
      int i;
      char rname[30];
      const char *alloc_rname;
      rtvec p;
      p = rtvec_alloc (2 + 64 - info->first_fp_reg_save);
      
      RTVEC_ELT (p, 0) = gen_rtx_CLOBBER (VOIDmode, 
					  gen_rtx_REG (Pmode, 
						       LINK_REGISTER_REGNUM));
      sprintf (rname, "%s%d%s", SAVE_FP_PREFIX,
	       info->first_fp_reg_save - 32, SAVE_FP_SUFFIX);
      alloc_rname = ggc_strdup (rname);
      RTVEC_ELT (p, 1) = gen_rtx_USE (VOIDmode,
				      gen_rtx_SYMBOL_REF (Pmode,
							  alloc_rname));
      for (i = 0; i < 64 - info->first_fp_reg_save; i++)
	{
	  rtx addr, reg, mem;
	  reg = gen_rtx_REG (DFmode, info->first_fp_reg_save + i);
	  addr = gen_rtx_PLUS (Pmode, frame_reg_rtx,
			       GEN_INT (info->fp_save_offset 
					+ sp_offset + 8*i));
	  mem = gen_rtx_MEM (DFmode, addr);
	  set_mem_alias_set (mem, rs6000_sr_alias_set);

	  RTVEC_ELT (p, i + 2) = gen_rtx_SET (VOIDmode, mem, reg);
	}
      insn = emit_insn (gen_rtx_PARALLEL (VOIDmode, p));
      rs6000_frame_related (insn, frame_ptr_rtx, info->total_size, 
			    NULL_RTX, NULL_RTX);
    }

  /* Save GPRs.  This is done as a PARALLEL if we are using
     the store-multiple instructions.  */
  if (using_store_multiple)
    {
      rtvec p;
      int i;
      p = rtvec_alloc (32 - info->first_gp_reg_save);
      for (i = 0; i < 32 - info->first_gp_reg_save; i++)
	{
	  rtx addr, reg, mem;
	  reg = gen_rtx_REG (reg_mode, info->first_gp_reg_save + i);
	  addr = gen_rtx_PLUS (Pmode, frame_reg_rtx, 
			       GEN_INT (info->gp_save_offset 
					+ sp_offset 
					+ reg_size * i));
	  mem = gen_rtx_MEM (reg_mode, addr);
	  set_mem_alias_set (mem, rs6000_sr_alias_set);

	  RTVEC_ELT (p, i) = gen_rtx_SET (VOIDmode, mem, reg);
	}
      insn = emit_insn (gen_rtx_PARALLEL (VOIDmode, p));
      rs6000_frame_related (insn, frame_ptr_rtx, info->total_size, 
			    NULL_RTX, NULL_RTX);
    }
  else
    {
      int i;
      for (i = 0; i < 32 - info->first_gp_reg_save; i++)
	if ((regs_ever_live[info->first_gp_reg_save+i] 
	     && (! call_used_regs[info->first_gp_reg_save+i]
		 || (i+info->first_gp_reg_save
		     == RS6000_PIC_OFFSET_TABLE_REGNUM
		     && TARGET_TOC && TARGET_MINIMAL_TOC)))
	    || (i+info->first_gp_reg_save == RS6000_PIC_OFFSET_TABLE_REGNUM
		&& ((DEFAULT_ABI == ABI_V4 && flag_pic != 0)
		    || (DEFAULT_ABI == ABI_DARWIN && flag_pic))))
	  {
	    rtx addr, reg, mem;
	    reg = gen_rtx_REG (reg_mode, info->first_gp_reg_save + i);

	    if (TARGET_SPE_ABI && info->spe_64bit_regs_used != 0)
	      {
		int offset = info->spe_gp_save_offset + sp_offset + 8 * i;
		rtx b;

		if (!SPE_CONST_OFFSET_OK (offset))
		  {
		    b = gen_rtx_REG (Pmode, FIXED_SCRATCH);
		    emit_move_insn (b, GEN_INT (offset));
		  }
		else
		  b = GEN_INT (offset);

		addr = gen_rtx_PLUS (Pmode, frame_reg_rtx, b);
		mem = gen_rtx_MEM (V2SImode, addr);
		set_mem_alias_set (mem, rs6000_sr_alias_set);
		insn = emit_move_insn (mem, reg);

		if (GET_CODE (b) == CONST_INT)
		  rs6000_frame_related (insn, frame_ptr_rtx, info->total_size,
					NULL_RTX, NULL_RTX);
		else
		  rs6000_frame_related (insn, frame_ptr_rtx, info->total_size,
					b, GEN_INT (offset));
	      }
	    else
	      {
		addr = gen_rtx_PLUS (Pmode, frame_reg_rtx, 
				     GEN_INT (info->gp_save_offset 
					      + sp_offset 
					      + reg_size * i));
		mem = gen_rtx_MEM (reg_mode, addr);
		set_mem_alias_set (mem, rs6000_sr_alias_set);

		insn = emit_move_insn (mem, reg);
		rs6000_frame_related (insn, frame_ptr_rtx, info->total_size, 
				      NULL_RTX, NULL_RTX);
	      }
	  }
    }

  /* ??? There's no need to emit actual instructions here, but it's the
     easiest way to get the frame unwind information emitted.  */
  if (current_function_calls_eh_return)
    {
      unsigned int i, regno;

      /* In AIX ABI we need to pretend we save r2 here.  */
      if (TARGET_AIX)
	{
	  rtx addr, reg, mem;

	  reg = gen_rtx_REG (reg_mode, 2);
	  addr = gen_rtx_PLUS (Pmode, frame_reg_rtx,
			       GEN_INT (sp_offset + 5 * reg_size));
	  mem = gen_rtx_MEM (reg_mode, addr);
	  set_mem_alias_set (mem, rs6000_sr_alias_set);

	  insn = emit_move_insn (mem, reg);
	  rs6000_frame_related (insn, frame_ptr_rtx, info->total_size, 
				NULL_RTX, NULL_RTX);
	  PATTERN (insn) = gen_blockage ();
	}

      for (i = 0; ; ++i)
	{
	  regno = EH_RETURN_DATA_REGNO (i);
	  if (regno == INVALID_REGNUM)
	    break;

	  emit_frame_save (frame_reg_rtx, frame_ptr_rtx, reg_mode, regno,
			   info->ehrd_offset + sp_offset
			   + reg_size * (int) i,
			   info->total_size);
	}
    }

  /* Save lr if we used it.  */
  if (info->lr_save_p)
    {
      rtx addr = gen_rtx_PLUS (Pmode, frame_reg_rtx,
			       GEN_INT (info->lr_save_offset + sp_offset));
      rtx reg = gen_rtx_REG (Pmode, 0);
      rtx mem = gen_rtx_MEM (Pmode, addr);
      /* This should not be of rs6000_sr_alias_set, because of
	 __builtin_return_address.  */
      
      insn = emit_move_insn (mem, reg);
      rs6000_frame_related (insn, frame_ptr_rtx, info->total_size, 
			    reg, gen_rtx_REG (Pmode, LINK_REGISTER_REGNUM));
    }

  /* Save CR if we use any that must be preserved.  */
  if (info->cr_save_p)
    {
      rtx addr = gen_rtx_PLUS (Pmode, frame_reg_rtx,
			       GEN_INT (info->cr_save_offset + sp_offset));
      rtx mem = gen_rtx_MEM (SImode, addr);

      set_mem_alias_set (mem, rs6000_sr_alias_set);

      /* If r12 was used to hold the original sp, copy cr into r0 now
	 that it's free.  */
      if (REGNO (frame_reg_rtx) == 12)
	{
	  cr_save_rtx = gen_rtx_REG (SImode, 0);
	  emit_insn (gen_movesi_from_cr (cr_save_rtx));
	}
      insn = emit_move_insn (mem, cr_save_rtx);

      /* Now, there's no way that dwarf2out_frame_debug_expr is going
	 to understand '(unspec:SI [(reg:CC 68) ...] UNSPEC_MOVESI_FROM_CR)'.
	 But that's OK.  All we have to do is specify that _one_ condition
	 code register is saved in this stack slot.  The thrower's epilogue
	 will then restore all the call-saved registers.
	 We use CR2_REGNO (70) to be compatible with gcc-2.95 on Linux.  */
      rs6000_frame_related (insn, frame_ptr_rtx, info->total_size, 
			    cr_save_rtx, gen_rtx_REG (SImode, CR2_REGNO));
    }

  /* Update stack and set back pointer unless this is V.4, 
     for which it was done previously.  */
  if (info->push_p
      && !(DEFAULT_ABI == ABI_V4 || current_function_calls_eh_return))
    rs6000_emit_allocate_stack (info->total_size, FALSE);

  /* Set frame pointer, if needed.  */
  if (frame_pointer_needed)
    {
      insn = emit_move_insn (gen_rtx_REG (Pmode, FRAME_POINTER_REGNUM), 
			     sp_reg_rtx);
      RTX_FRAME_RELATED_P (insn) = 1;
    }

  /* If we are using RS6000_PIC_OFFSET_TABLE_REGNUM, we need to set it up.  */
  if ((TARGET_TOC && TARGET_MINIMAL_TOC && get_pool_size () != 0)
      || (DEFAULT_ABI == ABI_V4 && flag_pic == 1
	  && regs_ever_live[RS6000_PIC_OFFSET_TABLE_REGNUM]))
  {
    /* If emit_load_toc_table will use the link register, we need to save
       it.  We use R12 for this purpose because emit_load_toc_table
       can use register 0.  This allows us to use a plain 'blr' to return
       from the procedure more often.  */
    int save_LR_around_toc_setup = (TARGET_ELF
				    && DEFAULT_ABI != ABI_AIX
				    && flag_pic
				    && ! info->lr_save_p
				    && EXIT_BLOCK_PTR->pred != NULL);
    if (save_LR_around_toc_setup)
      {
	rtx lr = gen_rtx_REG (Pmode, LINK_REGISTER_REGNUM);
	rs6000_maybe_dead (emit_move_insn (frame_ptr_rtx, lr));
	rs6000_emit_load_toc_table (TRUE);
	rs6000_maybe_dead (emit_move_insn (lr, frame_ptr_rtx));
      }
    else
      rs6000_emit_load_toc_table (TRUE);
  }

#if TARGET_MACHO
  if (DEFAULT_ABI == ABI_DARWIN
      && flag_pic && current_function_uses_pic_offset_table)
    {
      rtx dest = gen_rtx_REG (Pmode, LINK_REGISTER_REGNUM);
      const char *picbase = machopic_function_base_name ();
      rtx src = gen_rtx_SYMBOL_REF (Pmode, picbase);

      rs6000_maybe_dead (emit_insn (gen_load_macho_picbase (dest, src)));

      rs6000_maybe_dead (
	emit_move_insn (gen_rtx_REG (Pmode, RS6000_PIC_OFFSET_TABLE_REGNUM),
			gen_rtx_REG (Pmode, LINK_REGISTER_REGNUM)));
    }
#endif
}

/* Write function prologue.  */

static void
rs6000_output_function_prologue (FILE *file, 
				 HOST_WIDE_INT size ATTRIBUTE_UNUSED)
{
  rs6000_stack_t *info = rs6000_stack_info ();

  if (TARGET_DEBUG_STACK)
    debug_stack_info (info);

  /* Write .extern for any function we will call to save and restore
     fp values.  */
  if (info->first_fp_reg_save < 64
      && !FP_SAVE_INLINE (info->first_fp_reg_save))
    fprintf (file, "\t.extern %s%d%s\n\t.extern %s%d%s\n",
	     SAVE_FP_PREFIX, info->first_fp_reg_save - 32, SAVE_FP_SUFFIX,
	     RESTORE_FP_PREFIX, info->first_fp_reg_save - 32,
	     RESTORE_FP_SUFFIX);

  /* Write .extern for AIX common mode routines, if needed.  */
  if (! TARGET_POWER && ! TARGET_POWERPC && ! common_mode_defined)
    {
      fputs ("\t.extern __mulh\n", file);
      fputs ("\t.extern __mull\n", file);
      fputs ("\t.extern __divss\n", file);
      fputs ("\t.extern __divus\n", file);
      fputs ("\t.extern __quoss\n", file);
      fputs ("\t.extern __quous\n", file);
      common_mode_defined = 1;
    }

  if (! HAVE_prologue)
    {
      start_sequence ();

      /* A NOTE_INSN_DELETED is supposed to be at the start and end of
	 the "toplevel" insn chain.  */
      emit_note (NOTE_INSN_DELETED);
      rs6000_emit_prologue ();
      emit_note (NOTE_INSN_DELETED);

      /* Expand INSN_ADDRESSES so final() doesn't crash.  */
      {
	rtx insn;
	unsigned addr = 0;
	for (insn = get_insns (); insn != 0; insn = NEXT_INSN (insn))
	  {
	    INSN_ADDRESSES_NEW (insn, addr);
	    addr += 4;
	  }
      }

      if (TARGET_DEBUG_STACK)
	debug_rtx_list (get_insns (), 100);
      final (get_insns (), file, FALSE, FALSE);
      end_sequence ();
    }

  rs6000_pic_labelno++;
}
  
/* Emit function epilogue as insns.

   At present, dwarf2out_frame_debug_expr doesn't understand
   register restores, so we don't bother setting RTX_FRAME_RELATED_P
   anywhere in the epilogue.  Most of the insns below would in any case
   need special notes to explain where r11 is in relation to the stack.  */

void
rs6000_emit_epilogue (int sibcall)
{
  rs6000_stack_t *info;
  int restoring_FPRs_inline;
  int using_load_multiple;
  int using_mfcr_multiple;
  int use_backchain_to_restore_sp;
  int sp_offset = 0;
  rtx sp_reg_rtx = gen_rtx_REG (Pmode, 1);
  rtx frame_reg_rtx = sp_reg_rtx;
  enum machine_mode reg_mode = Pmode;
  int reg_size = TARGET_32BIT ? 4 : 8;
  int i;

  info = rs6000_stack_info ();

  if (TARGET_SPE_ABI && info->spe_64bit_regs_used != 0)
    {
      reg_mode = V2SImode;
      reg_size = 8;
    }

  using_load_multiple = (TARGET_MULTIPLE && ! TARGET_POWERPC64
			 && (!TARGET_SPE_ABI
			     || info->spe_64bit_regs_used == 0)
			 && info->first_gp_reg_save < 31);
  restoring_FPRs_inline = (sibcall
			   || current_function_calls_eh_return
			   || info->first_fp_reg_save == 64
			   || FP_SAVE_INLINE (info->first_fp_reg_save));
  use_backchain_to_restore_sp = (frame_pointer_needed 
				 || current_function_calls_alloca
				 || info->total_size > 32767);
  using_mfcr_multiple = (rs6000_cpu == PROCESSOR_PPC601
			 || rs6000_cpu == PROCESSOR_PPC603
			 || rs6000_cpu == PROCESSOR_PPC750
			 || optimize_size);

  /* If we have a frame pointer, a call to alloca,  or a large stack
     frame, restore the old stack pointer using the backchain.  Otherwise,
     we know what size to update it with.  */
  if (use_backchain_to_restore_sp)
    {
      /* Under V.4, don't reset the stack pointer until after we're done
	 loading the saved registers.  */
      if (DEFAULT_ABI == ABI_V4)
	frame_reg_rtx = gen_rtx_REG (Pmode, 11);

      emit_move_insn (frame_reg_rtx,
		      gen_rtx_MEM (Pmode, sp_reg_rtx));
      
    }
  else if (info->push_p)
    {
      if (DEFAULT_ABI == ABI_V4
	  || current_function_calls_eh_return)
	sp_offset = info->total_size;
      else
	{
	  emit_insn (TARGET_32BIT
		     ? gen_addsi3 (sp_reg_rtx, sp_reg_rtx,
				   GEN_INT (info->total_size))
		     : gen_adddi3 (sp_reg_rtx, sp_reg_rtx,
				   GEN_INT (info->total_size)));
	}
    }
  
  /* Restore AltiVec registers if needed.  */
  if (TARGET_ALTIVEC_ABI && info->altivec_size != 0)
    {
      int i;

      for (i = info->first_altivec_reg_save; i <= LAST_ALTIVEC_REGNO; ++i)
	if (info->vrsave_mask & ALTIVEC_REG_BIT (i))
	  {
	    rtx addr, areg, mem;

	    areg = gen_rtx_REG (Pmode, 0);
	    emit_move_insn
	      (areg, GEN_INT (info->altivec_save_offset
			      + sp_offset
			      + 16 * (i - info->first_altivec_reg_save)));

	    /* AltiVec addressing mode is [reg+reg].  */
	    addr = gen_rtx_PLUS (Pmode, frame_reg_rtx, areg);
	    mem = gen_rtx_MEM (V4SImode, addr);
	    set_mem_alias_set (mem, rs6000_sr_alias_set);

	    emit_move_insn (gen_rtx_REG (V4SImode, i), mem);
	  }
    }

  /* Restore VRSAVE if needed.  */
  if (TARGET_ALTIVEC && TARGET_ALTIVEC_VRSAVE
      && info->vrsave_mask != 0)
    {
      rtx addr, mem, reg;

      addr = gen_rtx_PLUS (Pmode, frame_reg_rtx,
			   GEN_INT (info->vrsave_save_offset + sp_offset));
      mem = gen_rtx_MEM (SImode, addr);
      set_mem_alias_set (mem, rs6000_sr_alias_set);
      reg = gen_rtx_REG (SImode, 12);
      emit_move_insn (reg, mem);

      emit_insn (generate_set_vrsave (reg, info, 1));
    }

  /* Get the old lr if we saved it.  */
  if (info->lr_save_p)
    {
      rtx mem = gen_frame_mem_offset (Pmode, frame_reg_rtx,
				      info->lr_save_offset + sp_offset);

      set_mem_alias_set (mem, rs6000_sr_alias_set);

      emit_move_insn (gen_rtx_REG (Pmode, 0), mem);
    }
  
  /* Get the old cr if we saved it.  */
  if (info->cr_save_p)
    {
      rtx addr = gen_rtx_PLUS (Pmode, frame_reg_rtx,
			       GEN_INT (info->cr_save_offset + sp_offset));
      rtx mem = gen_rtx_MEM (SImode, addr);

      set_mem_alias_set (mem, rs6000_sr_alias_set);

      emit_move_insn (gen_rtx_REG (SImode, 12), mem);
    }
  
  /* Set LR here to try to overlap restores below.  */
  if (info->lr_save_p)
    emit_move_insn (gen_rtx_REG (Pmode, LINK_REGISTER_REGNUM),
		    gen_rtx_REG (Pmode, 0));
  
  /* Load exception handler data registers, if needed.  */
  if (current_function_calls_eh_return)
    {
      unsigned int i, regno;

      if (TARGET_AIX)
	{
	  rtx addr = gen_rtx_PLUS (Pmode, frame_reg_rtx,
				   GEN_INT (sp_offset + 5 * reg_size));
	  rtx mem = gen_rtx_MEM (reg_mode, addr);

	  set_mem_alias_set (mem, rs6000_sr_alias_set);

	  emit_move_insn (gen_rtx_REG (reg_mode, 2), mem);
	}

      for (i = 0; ; ++i)
	{
	  rtx mem;

	  regno = EH_RETURN_DATA_REGNO (i);
	  if (regno == INVALID_REGNUM)
	    break;

	  mem = gen_frame_mem_offset (reg_mode, frame_reg_rtx,
				      info->ehrd_offset + sp_offset
				      + reg_size * (int) i);
	  set_mem_alias_set (mem, rs6000_sr_alias_set);

	  emit_move_insn (gen_rtx_REG (reg_mode, regno), mem);
	}
    }
  
  /* Restore GPRs.  This is done as a PARALLEL if we are using
     the load-multiple instructions.  */
  if (using_load_multiple)
    {
      rtvec p;
      p = rtvec_alloc (32 - info->first_gp_reg_save);
      for (i = 0; i < 32 - info->first_gp_reg_save; i++)
	{
	  rtx addr = gen_rtx_PLUS (Pmode, frame_reg_rtx, 
				   GEN_INT (info->gp_save_offset 
					    + sp_offset 
					    + reg_size * i));
	  rtx mem = gen_rtx_MEM (reg_mode, addr);

	  set_mem_alias_set (mem, rs6000_sr_alias_set);

	  RTVEC_ELT (p, i) = 
	    gen_rtx_SET (VOIDmode,
			 gen_rtx_REG (reg_mode, info->first_gp_reg_save + i),
			 mem);
	}
      emit_insn (gen_rtx_PARALLEL (VOIDmode, p));
    }
  else
    for (i = 0; i < 32 - info->first_gp_reg_save; i++)
      if ((regs_ever_live[info->first_gp_reg_save+i] 
	   && (! call_used_regs[info->first_gp_reg_save+i]
	       || (i+info->first_gp_reg_save == RS6000_PIC_OFFSET_TABLE_REGNUM
		   && TARGET_TOC && TARGET_MINIMAL_TOC)))
	  || (i+info->first_gp_reg_save == RS6000_PIC_OFFSET_TABLE_REGNUM
	      && ((DEFAULT_ABI == ABI_V4 && flag_pic != 0)
		  || (DEFAULT_ABI == ABI_DARWIN && flag_pic))))
	{
	  rtx addr = gen_rtx_PLUS (Pmode, frame_reg_rtx, 
				   GEN_INT (info->gp_save_offset 
					    + sp_offset 
					    + reg_size * i));
	  rtx mem = gen_rtx_MEM (reg_mode, addr);

	  /* Restore 64-bit quantities for SPE.  */
	  if (TARGET_SPE_ABI && info->spe_64bit_regs_used != 0)
	    {
	      int offset = info->spe_gp_save_offset + sp_offset + 8 * i;
	      rtx b;

	      if (!SPE_CONST_OFFSET_OK (offset))
		{
		  b = gen_rtx_REG (Pmode, FIXED_SCRATCH);
		  emit_move_insn (b, GEN_INT (offset));
		}
	      else
		b = GEN_INT (offset);

	      addr = gen_rtx_PLUS (Pmode, frame_reg_rtx, b);
	      mem = gen_rtx_MEM (V2SImode, addr);
	    }

	  set_mem_alias_set (mem, rs6000_sr_alias_set);

	  emit_move_insn (gen_rtx_REG (reg_mode, 
				       info->first_gp_reg_save + i), mem);
	}

  /* Restore fpr's if we need to do it without calling a function.  */
  if (restoring_FPRs_inline)
    for (i = 0; i < 64 - info->first_fp_reg_save; i++)
      if ((regs_ever_live[info->first_fp_reg_save+i] 
	   && ! call_used_regs[info->first_fp_reg_save+i]))
	{
	  rtx addr, mem;
	  addr = gen_rtx_PLUS (Pmode, frame_reg_rtx,
			       GEN_INT (info->fp_save_offset 
					+ sp_offset 
					+ 8 * i));
	  mem = gen_rtx_MEM (DFmode, addr);
	  set_mem_alias_set (mem, rs6000_sr_alias_set);

	  emit_move_insn (gen_rtx_REG (DFmode, 
				       info->first_fp_reg_save + i),
			  mem);
	}

  /* If we saved cr, restore it here.  Just those that were used.  */
  if (info->cr_save_p)
    {
      rtx r12_rtx = gen_rtx_REG (SImode, 12);
      int count = 0;
      
      if (using_mfcr_multiple)
	{
	  for (i = 0; i < 8; i++)
	    if (regs_ever_live[CR0_REGNO+i] && ! call_used_regs[CR0_REGNO+i])
	      count++;
	  if (count == 0)
	    abort ();
	}

      if (using_mfcr_multiple && count > 1)
	{
	  rtvec p;
	  int ndx;
	  
	  p = rtvec_alloc (count);

	  ndx = 0;
	  for (i = 0; i < 8; i++)
	    if (regs_ever_live[CR0_REGNO+i] && ! call_used_regs[CR0_REGNO+i])
	      {
		rtvec r = rtvec_alloc (2);
		RTVEC_ELT (r, 0) = r12_rtx;
		RTVEC_ELT (r, 1) = GEN_INT (1 << (7-i));
		RTVEC_ELT (p, ndx) =
		  gen_rtx_SET (VOIDmode, gen_rtx_REG (CCmode, CR0_REGNO+i), 
			       gen_rtx_UNSPEC (CCmode, r, UNSPEC_MOVESI_TO_CR));
		ndx++;
	      }
	  emit_insn (gen_rtx_PARALLEL (VOIDmode, p));
	  if (ndx != count)
	    abort ();
	}
      else
	for (i = 0; i < 8; i++)
	  if (regs_ever_live[CR0_REGNO+i] && ! call_used_regs[CR0_REGNO+i])
	    {
	      emit_insn (gen_movsi_to_cr_one (gen_rtx_REG (CCmode, 
							   CR0_REGNO+i),
					      r12_rtx));
	    }
    }

  /* If this is V.4, unwind the stack pointer after all of the loads
     have been done.  We need to emit a block here so that sched
     doesn't decide to move the sp change before the register restores
     (which may not have any obvious dependency on the stack).  This
     doesn't hurt performance, because there is no scheduling that can
     be done after this point.  */
  if (DEFAULT_ABI == ABI_V4
      || current_function_calls_eh_return)
    {
      if (frame_reg_rtx != sp_reg_rtx)
	  rs6000_emit_stack_tie ();

      if (use_backchain_to_restore_sp)
	{
	  emit_move_insn (sp_reg_rtx, frame_reg_rtx);
	}
      else if (sp_offset != 0)
	{
	  emit_insn (TARGET_32BIT
		     ? gen_addsi3 (sp_reg_rtx, sp_reg_rtx,
				   GEN_INT (sp_offset))
		     : gen_adddi3 (sp_reg_rtx, sp_reg_rtx,
				   GEN_INT (sp_offset)));
	}
    }

  if (current_function_calls_eh_return)
    {
      rtx sa = EH_RETURN_STACKADJ_RTX;
      emit_insn (TARGET_32BIT
		 ? gen_addsi3 (sp_reg_rtx, sp_reg_rtx, sa)
		 : gen_adddi3 (sp_reg_rtx, sp_reg_rtx, sa));
    }

  if (!sibcall)
    {
      rtvec p;
      if (! restoring_FPRs_inline)
	p = rtvec_alloc (3 + 64 - info->first_fp_reg_save);
      else
	p = rtvec_alloc (2);

      RTVEC_ELT (p, 0) = gen_rtx_RETURN (VOIDmode);
      RTVEC_ELT (p, 1) = gen_rtx_USE (VOIDmode, 
				      gen_rtx_REG (Pmode, 
						   LINK_REGISTER_REGNUM));

      /* If we have to restore more than two FP registers, branch to the
	 restore function.  It will return to our caller.  */
      if (! restoring_FPRs_inline)
	{
	  int i;
	  char rname[30];
	  const char *alloc_rname;

	  sprintf (rname, "%s%d%s", RESTORE_FP_PREFIX, 
		   info->first_fp_reg_save - 32, RESTORE_FP_SUFFIX);
	  alloc_rname = ggc_strdup (rname);
	  RTVEC_ELT (p, 2) = gen_rtx_USE (VOIDmode,
					  gen_rtx_SYMBOL_REF (Pmode,
							      alloc_rname));

	  for (i = 0; i < 64 - info->first_fp_reg_save; i++)
	    {
	      rtx addr, mem;
	      addr = gen_rtx_PLUS (Pmode, sp_reg_rtx,
				   GEN_INT (info->fp_save_offset + 8*i));
	      mem = gen_rtx_MEM (DFmode, addr);
	      set_mem_alias_set (mem, rs6000_sr_alias_set);

	      RTVEC_ELT (p, i+3) = 
		gen_rtx_SET (VOIDmode,
			     gen_rtx_REG (DFmode, info->first_fp_reg_save + i),
			     mem);
	    }
	}
      
      emit_jump_insn (gen_rtx_PARALLEL (VOIDmode, p));
    }
}

/* Write function epilogue.  */

static void
rs6000_output_function_epilogue (FILE *file, 
				 HOST_WIDE_INT size ATTRIBUTE_UNUSED)
{
  rs6000_stack_t *info = rs6000_stack_info ();

  if (! HAVE_epilogue)
    {
      rtx insn = get_last_insn ();
      /* If the last insn was a BARRIER, we don't have to write anything except
	 the trace table.  */
      if (GET_CODE (insn) == NOTE)
	insn = prev_nonnote_insn (insn);
      if (insn == 0 ||  GET_CODE (insn) != BARRIER)
	{
	  /* This is slightly ugly, but at least we don't have two
	     copies of the epilogue-emitting code.  */
	  start_sequence ();

	  /* A NOTE_INSN_DELETED is supposed to be at the start
	     and end of the "toplevel" insn chain.  */
	  emit_note (NOTE_INSN_DELETED);
	  rs6000_emit_epilogue (FALSE);
	  emit_note (NOTE_INSN_DELETED);

	  /* Expand INSN_ADDRESSES so final() doesn't crash.  */
	  {
	    rtx insn;
	    unsigned addr = 0;
	    for (insn = get_insns (); insn != 0; insn = NEXT_INSN (insn))
	      {
		INSN_ADDRESSES_NEW (insn, addr);
		addr += 4;
	      }
	  }

	  if (TARGET_DEBUG_STACK)
	    debug_rtx_list (get_insns (), 100);
	  final (get_insns (), file, FALSE, FALSE);
	  end_sequence ();
	}
    }

#if TARGET_MACHO
  macho_branch_islands ();
  /* Mach-O doesn't support labels at the end of objects, so if
     it looks like we might want one, insert a NOP.  */
  {
    rtx insn = get_last_insn ();
    while (insn
	   && NOTE_P (insn)
	   && NOTE_LINE_NUMBER (insn) != NOTE_INSN_DELETED_LABEL)
      insn = PREV_INSN (insn);
    if (insn 
	&& (LABEL_P (insn) 
	    || (NOTE_P (insn)
		&& NOTE_LINE_NUMBER (insn) == NOTE_INSN_DELETED_LABEL)))
      fputs ("\tnop\n", file);
  }
#endif

  /* Output a traceback table here.  See /usr/include/sys/debug.h for info
     on its format.

     We don't output a traceback table if -finhibit-size-directive was
     used.  The documentation for -finhibit-size-directive reads
     ``don't output a @code{.size} assembler directive, or anything
     else that would cause trouble if the function is split in the
     middle, and the two halves are placed at locations far apart in
     memory.''  The traceback table has this property, since it
     includes the offset from the start of the function to the
     traceback table itself.

     System V.4 Powerpc's (and the embedded ABI derived from it) use a
     different traceback table.  */
  if (DEFAULT_ABI == ABI_AIX && ! flag_inhibit_size_directive
      && rs6000_traceback != traceback_none)
    {
      const char *fname = NULL;
      const char *language_string = lang_hooks.name;
      int fixed_parms = 0, float_parms = 0, parm_info = 0;
      int i;
      int optional_tbtab;

      if (rs6000_traceback == traceback_full)
	optional_tbtab = 1;
      else if (rs6000_traceback == traceback_part)
	optional_tbtab = 0;
      else
	optional_tbtab = !optimize_size && !TARGET_ELF;

      if (optional_tbtab)
	{
	  fname = XSTR (XEXP (DECL_RTL (current_function_decl), 0), 0);
	  while (*fname == '.')	/* V.4 encodes . in the name */
	    fname++;

	  /* Need label immediately before tbtab, so we can compute
	     its offset from the function start.  */
	  ASM_OUTPUT_INTERNAL_LABEL_PREFIX (file, "LT");
	  ASM_OUTPUT_LABEL (file, fname);
	}

      /* The .tbtab pseudo-op can only be used for the first eight
	 expressions, since it can't handle the possibly variable
	 length fields that follow.  However, if you omit the optional
	 fields, the assembler outputs zeros for all optional fields
	 anyways, giving each variable length field is minimum length
	 (as defined in sys/debug.h).  Thus we can not use the .tbtab
	 pseudo-op at all.  */

      /* An all-zero word flags the start of the tbtab, for debuggers
	 that have to find it by searching forward from the entry
	 point or from the current pc.  */
      fputs ("\t.long 0\n", file);

      /* Tbtab format type.  Use format type 0.  */
      fputs ("\t.byte 0,", file);

      /* Language type.  Unfortunately, there does not seem to be any
	 official way to discover the language being compiled, so we
	 use language_string.
	 C is 0.  Fortran is 1.  Pascal is 2.  Ada is 3.  C++ is 9.
	 Java is 13.  Objective-C is 14.  */
      if (! strcmp (language_string, "GNU C"))
	i = 0;
      else if (! strcmp (language_string, "GNU F77"))
	i = 1;
      else if (! strcmp (language_string, "GNU Pascal"))
	i = 2;
      else if (! strcmp (language_string, "GNU Ada"))
	i = 3;
      else if (! strcmp (language_string, "GNU C++"))
	i = 9;
      else if (! strcmp (language_string, "GNU Java"))
	i = 13;
      else if (! strcmp (language_string, "GNU Objective-C"))
	i = 14;
      else
	abort ();
      fprintf (file, "%d,", i);

      /* 8 single bit fields: global linkage (not set for C extern linkage,
	 apparently a PL/I convention?), out-of-line epilogue/prologue, offset
	 from start of procedure stored in tbtab, internal function, function
	 has controlled storage, function has no toc, function uses fp,
	 function logs/aborts fp operations.  */
      /* Assume that fp operations are used if any fp reg must be saved.  */
      fprintf (file, "%d,",
	       (optional_tbtab << 5) | ((info->first_fp_reg_save != 64) << 1));

      /* 6 bitfields: function is interrupt handler, name present in
	 proc table, function calls alloca, on condition directives
	 (controls stack walks, 3 bits), saves condition reg, saves
	 link reg.  */
      /* The `function calls alloca' bit seems to be set whenever reg 31 is
	 set up as a frame pointer, even when there is no alloca call.  */
      fprintf (file, "%d,",
	       ((optional_tbtab << 6)
		| ((optional_tbtab & frame_pointer_needed) << 5)
		| (info->cr_save_p << 1)
		| (info->lr_save_p)));

      /* 3 bitfields: saves backchain, fixup code, number of fpr saved
	 (6 bits).  */
      fprintf (file, "%d,",
	       (info->push_p << 7) | (64 - info->first_fp_reg_save));

      /* 2 bitfields: spare bits (2 bits), number of gpr saved (6 bits).  */
      fprintf (file, "%d,", (32 - first_reg_to_save ()));

      if (optional_tbtab)
	{
	  /* Compute the parameter info from the function decl argument
	     list.  */
	  tree decl;
	  int next_parm_info_bit = 31;

	  for (decl = DECL_ARGUMENTS (current_function_decl);
	       decl; decl = TREE_CHAIN (decl))
	    {
	      rtx parameter = DECL_INCOMING_RTL (decl);
	      enum machine_mode mode = GET_MODE (parameter);

	      if (GET_CODE (parameter) == REG)
		{
		  if (GET_MODE_CLASS (mode) == MODE_FLOAT)
		    {
		      int bits;

		      float_parms++;

		      if (mode == SFmode)
			bits = 0x2;
		      else if (mode == DFmode || mode == TFmode)
			bits = 0x3;
		      else
			abort ();

		      /* If only one bit will fit, don't or in this entry.  */
		      if (next_parm_info_bit > 0)
			parm_info |= (bits << (next_parm_info_bit - 1));
		      next_parm_info_bit -= 2;
		    }
		  else
		    {
		      fixed_parms += ((GET_MODE_SIZE (mode)
				       + (UNITS_PER_WORD - 1))
				      / UNITS_PER_WORD);
		      next_parm_info_bit -= 1;
		    }
		}
	    }
	}

      /* Number of fixed point parameters.  */
      /* This is actually the number of words of fixed point parameters; thus
	 an 8 byte struct counts as 2; and thus the maximum value is 8.  */
      fprintf (file, "%d,", fixed_parms);

      /* 2 bitfields: number of floating point parameters (7 bits), parameters
	 all on stack.  */
      /* This is actually the number of fp registers that hold parameters;
	 and thus the maximum value is 13.  */
      /* Set parameters on stack bit if parameters are not in their original
	 registers, regardless of whether they are on the stack?  Xlc
	 seems to set the bit when not optimizing.  */
      fprintf (file, "%d\n", ((float_parms << 1) | (! optimize)));

      if (! optional_tbtab)
	return;

      /* Optional fields follow.  Some are variable length.  */

      /* Parameter types, left adjusted bit fields: 0 fixed, 10 single float,
	 11 double float.  */
      /* There is an entry for each parameter in a register, in the order that
	 they occur in the parameter list.  Any intervening arguments on the
	 stack are ignored.  If the list overflows a long (max possible length
	 34 bits) then completely leave off all elements that don't fit.  */
      /* Only emit this long if there was at least one parameter.  */
      if (fixed_parms || float_parms)
	fprintf (file, "\t.long %d\n", parm_info);

      /* Offset from start of code to tb table.  */
      fputs ("\t.long ", file);
      ASM_OUTPUT_INTERNAL_LABEL_PREFIX (file, "LT");
#if TARGET_AIX
      RS6000_OUTPUT_BASENAME (file, fname);
#else
      assemble_name (file, fname);
#endif
      fputs ("-.", file);
#if TARGET_AIX
      RS6000_OUTPUT_BASENAME (file, fname);
#else
      assemble_name (file, fname);
#endif
      putc ('\n', file);

      /* Interrupt handler mask.  */
      /* Omit this long, since we never set the interrupt handler bit
	 above.  */

      /* Number of CTL (controlled storage) anchors.  */
      /* Omit this long, since the has_ctl bit is never set above.  */

      /* Displacement into stack of each CTL anchor.  */
      /* Omit this list of longs, because there are no CTL anchors.  */

      /* Length of function name.  */
      if (*fname == '*')
	++fname;
      fprintf (file, "\t.short %d\n", (int) strlen (fname));

      /* Function name.  */
      assemble_string (fname, strlen (fname));

      /* Register for alloca automatic storage; this is always reg 31.
	 Only emit this if the alloca bit was set above.  */
      if (frame_pointer_needed)
	fputs ("\t.byte 31\n", file);

      fputs ("\t.align 2\n", file);
    }
}

/* A C compound statement that outputs the assembler code for a thunk
   function, used to implement C++ virtual function calls with
   multiple inheritance.  The thunk acts as a wrapper around a virtual
   function, adjusting the implicit object parameter before handing
   control off to the real function.

   First, emit code to add the integer DELTA to the location that
   contains the incoming first argument.  Assume that this argument
   contains a pointer, and is the one used to pass the `this' pointer
   in C++.  This is the incoming argument *before* the function
   prologue, e.g. `%o0' on a sparc.  The addition must preserve the
   values of all other incoming arguments.

   After the addition, emit code to jump to FUNCTION, which is a
   `FUNCTION_DECL'.  This is a direct pure jump, not a call, and does
   not touch the return address.  Hence returning from FUNCTION will
   return to whoever called the current `thunk'.

   The effect must be as if FUNCTION had been called directly with the
   adjusted first argument.  This macro is responsible for emitting
   all of the code for a thunk function; output_function_prologue()
   and output_function_epilogue() are not invoked.

   The THUNK_FNDECL is redundant.  (DELTA and FUNCTION have already
   been extracted from it.)  It might possibly be useful on some
   targets, but probably not.

   If you do not define this macro, the target-independent code in the
   C++ frontend will generate a less efficient heavyweight thunk that
   calls FUNCTION instead of jumping to it.  The generic approach does
   not support varargs.  */

static void
rs6000_output_mi_thunk (FILE *file, tree thunk_fndecl ATTRIBUTE_UNUSED, 
			HOST_WIDE_INT delta, HOST_WIDE_INT vcall_offset, 
			tree function)
{
  rtx this, insn, funexp;

  reload_completed = 1;
  epilogue_completed = 1;
  no_new_pseudos = 1;

  /* Mark the end of the (empty) prologue.  */
  emit_note (NOTE_INSN_PROLOGUE_END);

  /* Find the "this" pointer.  If the function returns a structure,
     the structure return pointer is in r3.  */
  if (aggregate_value_p (TREE_TYPE (TREE_TYPE (function)), function))
    this = gen_rtx_REG (Pmode, 4);
  else
    this = gen_rtx_REG (Pmode, 3);

  /* Apply the constant offset, if required.  */
  if (delta)
    {
      rtx delta_rtx = GEN_INT (delta);
      emit_insn (TARGET_32BIT
		 ? gen_addsi3 (this, this, delta_rtx)
		 : gen_adddi3 (this, this, delta_rtx));
    }

  /* Apply the offset from the vtable, if required.  */
  if (vcall_offset)
    {
      rtx vcall_offset_rtx = GEN_INT (vcall_offset);
      rtx tmp = gen_rtx_REG (Pmode, 12);

      emit_move_insn (tmp, gen_rtx_MEM (Pmode, this));
      if (((unsigned HOST_WIDE_INT) vcall_offset) + 0x8000 >= 0x10000)
	{
	  emit_insn (TARGET_32BIT
		     ? gen_addsi3 (tmp, tmp, vcall_offset_rtx)
		     : gen_adddi3 (tmp, tmp, vcall_offset_rtx));
	  emit_move_insn (tmp, gen_rtx_MEM (Pmode, tmp));
	}
      else
	{
	  rtx loc = gen_rtx_PLUS (Pmode, tmp, vcall_offset_rtx);

	  emit_move_insn (tmp, gen_rtx_MEM (Pmode, loc));
	}
      emit_insn (TARGET_32BIT
		 ? gen_addsi3 (this, this, tmp)
		 : gen_adddi3 (this, this, tmp));
    }

  /* Generate a tail call to the target function.  */
  if (!TREE_USED (function))
    {
      assemble_external (function);
      TREE_USED (function) = 1;
    }
  funexp = XEXP (DECL_RTL (function), 0);
  funexp = gen_rtx_MEM (FUNCTION_MODE, funexp);

#if TARGET_MACHO
  if (MACHOPIC_INDIRECT)
    funexp = machopic_indirect_call_target (funexp);
#endif

  /* gen_sibcall expects reload to convert scratch pseudo to LR so we must
     generate sibcall RTL explicitly to avoid constraint abort.  */
  insn = emit_call_insn (
	   gen_rtx_PARALLEL (VOIDmode,
	     gen_rtvec (4,
			gen_rtx_CALL (VOIDmode,
				      funexp, const0_rtx),
			gen_rtx_USE (VOIDmode, const0_rtx),
			gen_rtx_USE (VOIDmode,
				     gen_rtx_REG (SImode,
						  LINK_REGISTER_REGNUM)),
			gen_rtx_RETURN (VOIDmode))));
  SIBLING_CALL_P (insn) = 1;
  emit_barrier ();

  /* Run just enough of rest_of_compilation to get the insns emitted.
     There's not really enough bulk here to make other passes such as
     instruction scheduling worth while.  Note that use_thunk calls
     assemble_start_function and assemble_end_function.  */
  insn = get_insns ();
  insn_locators_initialize ();
  shorten_branches (insn);
  final_start_function (insn, file, 1);
  final (insn, file, 1, 0);
  final_end_function ();

  reload_completed = 0;
  epilogue_completed = 0;
  no_new_pseudos = 0;
}

/* A quick summary of the various types of 'constant-pool tables'
   under PowerPC:

   Target	Flags		Name		One table per	
   AIX		(none)		AIX TOC		object file
   AIX		-mfull-toc	AIX TOC		object file
   AIX		-mminimal-toc	AIX minimal TOC	translation unit
   SVR4/EABI	(none)		SVR4 SDATA	object file
   SVR4/EABI	-fpic		SVR4 pic	object file
   SVR4/EABI	-fPIC		SVR4 PIC	translation unit
   SVR4/EABI	-mrelocatable	EABI TOC	function
   SVR4/EABI	-maix		AIX TOC		object file
   SVR4/EABI	-maix -mminimal-toc 
				AIX minimal TOC	translation unit

   Name			Reg.	Set by	entries	      contains:
					made by	 addrs?	fp?	sum?

   AIX TOC		2	crt0	as	 Y	option	option
   AIX minimal TOC	30	prolog	gcc	 Y	Y	option
   SVR4 SDATA		13	crt0	gcc	 N	Y	N
   SVR4 pic		30	prolog	ld	 Y	not yet	N
   SVR4 PIC		30	prolog	gcc	 Y	option	option
   EABI TOC		30	prolog	gcc	 Y	option	option

*/

/* Hash functions for the hash table.  */

static unsigned
rs6000_hash_constant (rtx k)
{
  enum rtx_code code = GET_CODE (k);
  enum machine_mode mode = GET_MODE (k);
  unsigned result = (code << 3) ^ mode;
  const char *format;
  int flen, fidx;
  
  format = GET_RTX_FORMAT (code);
  flen = strlen (format);
  fidx = 0;

  switch (code)
    {
    case LABEL_REF:
      return result * 1231 + (unsigned) INSN_UID (XEXP (k, 0));

    case CONST_DOUBLE:
      if (mode != VOIDmode)
	return real_hash (CONST_DOUBLE_REAL_VALUE (k)) * result;
      flen = 2;
      break;

    case CODE_LABEL:
      fidx = 3;
      break;

    default:
      break;
    }

  for (; fidx < flen; fidx++)
    switch (format[fidx])
      {
      case 's':
	{
	  unsigned i, len;
	  const char *str = XSTR (k, fidx);
	  len = strlen (str);
	  result = result * 613 + len;
	  for (i = 0; i < len; i++)
	    result = result * 613 + (unsigned) str[i];
	  break;
	}
      case 'u':
      case 'e':
	result = result * 1231 + rs6000_hash_constant (XEXP (k, fidx));
	break;
      case 'i':
      case 'n':
	result = result * 613 + (unsigned) XINT (k, fidx);
	break;
      case 'w':
	if (sizeof (unsigned) >= sizeof (HOST_WIDE_INT))
	  result = result * 613 + (unsigned) XWINT (k, fidx);
	else
	  {
	    size_t i;
	    for (i = 0; i < sizeof(HOST_WIDE_INT)/sizeof(unsigned); i++)
	      result = result * 613 + (unsigned) (XWINT (k, fidx)
						  >> CHAR_BIT * i);
	  }
	break;
      case '0':
	break;
      default:
	abort ();
      }

  return result;
}

static unsigned
toc_hash_function (const void *hash_entry)
{
  const struct toc_hash_struct *thc = 
    (const struct toc_hash_struct *) hash_entry;
  return rs6000_hash_constant (thc->key) ^ thc->key_mode;
}

/* Compare H1 and H2 for equivalence.  */

static int
toc_hash_eq (const void *h1, const void *h2)
{
  rtx r1 = ((const struct toc_hash_struct *) h1)->key;
  rtx r2 = ((const struct toc_hash_struct *) h2)->key;

  if (((const struct toc_hash_struct *) h1)->key_mode
      != ((const struct toc_hash_struct *) h2)->key_mode)
    return 0;

  return rtx_equal_p (r1, r2);
}

/* These are the names given by the C++ front-end to vtables, and
   vtable-like objects.  Ideally, this logic should not be here;
   instead, there should be some programmatic way of inquiring as
   to whether or not an object is a vtable.  */

#define VTABLE_NAME_P(NAME)				\
  (strncmp ("_vt.", name, strlen("_vt.")) == 0		\
  || strncmp ("_ZTV", name, strlen ("_ZTV")) == 0	\
  || strncmp ("_ZTT", name, strlen ("_ZTT")) == 0	\
  || strncmp ("_ZTI", name, strlen ("_ZTI")) == 0	\
  || strncmp ("_ZTC", name, strlen ("_ZTC")) == 0) 

void
rs6000_output_symbol_ref (FILE *file, rtx x)
{
  /* Currently C++ toc references to vtables can be emitted before it
     is decided whether the vtable is public or private.  If this is
     the case, then the linker will eventually complain that there is
     a reference to an unknown section.  Thus, for vtables only, 
     we emit the TOC reference to reference the symbol and not the
     section.  */
  const char *name = XSTR (x, 0);

  if (VTABLE_NAME_P (name)) 
    {
      RS6000_OUTPUT_BASENAME (file, name);
    }
  else
    assemble_name (file, name);
}

/* Output a TOC entry.  We derive the entry name from what is being
   written.  */

void
output_toc (FILE *file, rtx x, int labelno, enum machine_mode mode)
{
  char buf[256];
  const char *name = buf;
  const char *real_name;
  rtx base = x;
  int offset = 0;

  if (TARGET_NO_TOC)
    abort ();

  /* When the linker won't eliminate them, don't output duplicate
     TOC entries (this happens on AIX if there is any kind of TOC,
     and on SVR4 under -fPIC or -mrelocatable).  Don't do this for
     CODE_LABELs.  */
  if (TARGET_TOC && GET_CODE (x) != LABEL_REF)
    {
      struct toc_hash_struct *h;
      void * * found;
      
      /* Create toc_hash_table.  This can't be done at OVERRIDE_OPTIONS
         time because GGC is not initialized at that point.  */
      if (toc_hash_table == NULL)
	toc_hash_table = htab_create_ggc (1021, toc_hash_function, 
					  toc_hash_eq, NULL);

      h = ggc_alloc (sizeof (*h));
      h->key = x;
      h->key_mode = mode;
      h->labelno = labelno;
      
      found = htab_find_slot (toc_hash_table, h, 1);
      if (*found == NULL)
	*found = h;
      else  /* This is indeed a duplicate.  
	       Set this label equal to that label.  */
	{
	  fputs ("\t.set ", file);
	  ASM_OUTPUT_INTERNAL_LABEL_PREFIX (file, "LC");
	  fprintf (file, "%d,", labelno);
	  ASM_OUTPUT_INTERNAL_LABEL_PREFIX (file, "LC");
	  fprintf (file, "%d\n", ((*(const struct toc_hash_struct **) 
					      found)->labelno));
	  return;
	}
    }

  /* If we're going to put a double constant in the TOC, make sure it's
     aligned properly when strict alignment is on.  */
  if (GET_CODE (x) == CONST_DOUBLE
      && STRICT_ALIGNMENT
      && GET_MODE_BITSIZE (mode) >= 64
      && ! (TARGET_NO_FP_IN_TOC && ! TARGET_MINIMAL_TOC)) {
    ASM_OUTPUT_ALIGN (file, 3);
  }

  (*targetm.asm_out.internal_label) (file, "LC", labelno);

  /* Handle FP constants specially.  Note that if we have a minimal
     TOC, things we put here aren't actually in the TOC, so we can allow
     FP constants.  */
  if (GET_CODE (x) == CONST_DOUBLE && GET_MODE (x) == TFmode)
    {
      REAL_VALUE_TYPE rv;
      long k[4];

      REAL_VALUE_FROM_CONST_DOUBLE (rv, x);
      REAL_VALUE_TO_TARGET_LONG_DOUBLE (rv, k);

      if (TARGET_64BIT)
	{
	  if (TARGET_MINIMAL_TOC)
	    fputs (DOUBLE_INT_ASM_OP, file);
	  else
	    fprintf (file, "\t.tc FT_%lx_%lx_%lx_%lx[TC],",
		     k[0] & 0xffffffff, k[1] & 0xffffffff,
		     k[2] & 0xffffffff, k[3] & 0xffffffff);
	  fprintf (file, "0x%lx%08lx,0x%lx%08lx\n",
		   k[0] & 0xffffffff, k[1] & 0xffffffff,
		   k[2] & 0xffffffff, k[3] & 0xffffffff);
	  return;
	}
      else
	{
	  if (TARGET_MINIMAL_TOC)
	    fputs ("\t.long ", file);
	  else
	    fprintf (file, "\t.tc FT_%lx_%lx_%lx_%lx[TC],",
		     k[0] & 0xffffffff, k[1] & 0xffffffff,
		     k[2] & 0xffffffff, k[3] & 0xffffffff);
	  fprintf (file, "0x%lx,0x%lx,0x%lx,0x%lx\n",
		   k[0] & 0xffffffff, k[1] & 0xffffffff,
		   k[2] & 0xffffffff, k[3] & 0xffffffff);
	  return;
	}
    }
  else if (GET_CODE (x) == CONST_DOUBLE && GET_MODE (x) == DFmode)
    {
      REAL_VALUE_TYPE rv;
      long k[2];

      REAL_VALUE_FROM_CONST_DOUBLE (rv, x);
      REAL_VALUE_TO_TARGET_DOUBLE (rv, k);

      if (TARGET_64BIT)
	{
	  if (TARGET_MINIMAL_TOC)
	    fputs (DOUBLE_INT_ASM_OP, file);
	  else
	    fprintf (file, "\t.tc FD_%lx_%lx[TC],",
		     k[0] & 0xffffffff, k[1] & 0xffffffff);
	  fprintf (file, "0x%lx%08lx\n",
		   k[0] & 0xffffffff, k[1] & 0xffffffff);
	  return;
	}
      else
	{
	  if (TARGET_MINIMAL_TOC)
	    fputs ("\t.long ", file);
	  else
	    fprintf (file, "\t.tc FD_%lx_%lx[TC],",
		     k[0] & 0xffffffff, k[1] & 0xffffffff);
	  fprintf (file, "0x%lx,0x%lx\n",
		   k[0] & 0xffffffff, k[1] & 0xffffffff);
	  return;
	}
    }
  else if (GET_CODE (x) == CONST_DOUBLE && GET_MODE (x) == SFmode)
    {
      REAL_VALUE_TYPE rv;
      long l;

      REAL_VALUE_FROM_CONST_DOUBLE (rv, x);
      REAL_VALUE_TO_TARGET_SINGLE (rv, l);

      if (TARGET_64BIT)
	{
	  if (TARGET_MINIMAL_TOC)
	    fputs (DOUBLE_INT_ASM_OP, file);
	  else
	    fprintf (file, "\t.tc FS_%lx[TC],", l & 0xffffffff);
	  fprintf (file, "0x%lx00000000\n", l & 0xffffffff);
	  return;
	}
      else
	{
	  if (TARGET_MINIMAL_TOC)
	    fputs ("\t.long ", file);
	  else
	    fprintf (file, "\t.tc FS_%lx[TC],", l & 0xffffffff);
	  fprintf (file, "0x%lx\n", l & 0xffffffff);
	  return;
	}
    }
  else if (GET_MODE (x) == VOIDmode
	   && (GET_CODE (x) == CONST_INT || GET_CODE (x) == CONST_DOUBLE))
    {
      unsigned HOST_WIDE_INT low;
      HOST_WIDE_INT high;

      if (GET_CODE (x) == CONST_DOUBLE)
	{
	  low = CONST_DOUBLE_LOW (x);
	  high = CONST_DOUBLE_HIGH (x);
	}
      else
#if HOST_BITS_PER_WIDE_INT == 32
	{
	  low = INTVAL (x);
	  high = (low & 0x80000000) ? ~0 : 0;
	}
#else
	{
          low = INTVAL (x) & 0xffffffff;
          high = (HOST_WIDE_INT) INTVAL (x) >> 32;
	}
#endif

      /* TOC entries are always Pmode-sized, but since this
	 is a bigendian machine then if we're putting smaller
	 integer constants in the TOC we have to pad them.
	 (This is still a win over putting the constants in
	 a separate constant pool, because then we'd have
	 to have both a TOC entry _and_ the actual constant.)

	 For a 32-bit target, CONST_INT values are loaded and shifted
	 entirely within `low' and can be stored in one TOC entry.  */

      if (TARGET_64BIT && POINTER_SIZE < GET_MODE_BITSIZE (mode))
	abort ();/* It would be easy to make this work, but it doesn't now.  */

      if (POINTER_SIZE > GET_MODE_BITSIZE (mode))
	{
#if HOST_BITS_PER_WIDE_INT == 32
	  lshift_double (low, high, POINTER_SIZE - GET_MODE_BITSIZE (mode),
			 POINTER_SIZE, &low, &high, 0);
#else
	  low |= high << 32;
	  low <<= POINTER_SIZE - GET_MODE_BITSIZE (mode);
	  high = (HOST_WIDE_INT) low >> 32;
	  low &= 0xffffffff;
#endif
	}

      if (TARGET_64BIT)
	{
	  if (TARGET_MINIMAL_TOC)
	    fputs (DOUBLE_INT_ASM_OP, file);
	  else
	    fprintf (file, "\t.tc ID_%lx_%lx[TC],",
		     (long) high & 0xffffffff, (long) low & 0xffffffff);
	  fprintf (file, "0x%lx%08lx\n",
		   (long) high & 0xffffffff, (long) low & 0xffffffff);
	  return;
	}
      else
	{
	  if (POINTER_SIZE < GET_MODE_BITSIZE (mode))
	    {
	      if (TARGET_MINIMAL_TOC)
		fputs ("\t.long ", file);
	      else
		fprintf (file, "\t.tc ID_%lx_%lx[TC],",
			 (long) high & 0xffffffff, (long) low & 0xffffffff);
	      fprintf (file, "0x%lx,0x%lx\n",
		       (long) high & 0xffffffff, (long) low & 0xffffffff);
	    }
	  else
	    {
	      if (TARGET_MINIMAL_TOC)
		fputs ("\t.long ", file);
	      else
		fprintf (file, "\t.tc IS_%lx[TC],", (long) low & 0xffffffff);
	      fprintf (file, "0x%lx\n", (long) low & 0xffffffff);
	    }
	  return;
	}
    }

  if (GET_CODE (x) == CONST)
    {
      if (GET_CODE (XEXP (x, 0)) != PLUS)
	abort ();

      base = XEXP (XEXP (x, 0), 0);
      offset = INTVAL (XEXP (XEXP (x, 0), 1));
    }
  
  if (GET_CODE (base) == SYMBOL_REF)
    name = XSTR (base, 0);
  else if (GET_CODE (base) == LABEL_REF)
    ASM_GENERATE_INTERNAL_LABEL (buf, "L", CODE_LABEL_NUMBER (XEXP (base, 0)));
  else if (GET_CODE (base) == CODE_LABEL)
    ASM_GENERATE_INTERNAL_LABEL (buf, "L", CODE_LABEL_NUMBER (base));
  else
    abort ();

  real_name = (*targetm.strip_name_encoding) (name);
  if (TARGET_MINIMAL_TOC)
    fputs (TARGET_32BIT ? "\t.long " : DOUBLE_INT_ASM_OP, file);
  else
    {
      fprintf (file, "\t.tc %s", real_name);

      if (offset < 0)
	fprintf (file, ".N%d", - offset);
      else if (offset)
	fprintf (file, ".P%d", offset);

      fputs ("[TC],", file);
    }

  /* Currently C++ toc references to vtables can be emitted before it
     is decided whether the vtable is public or private.  If this is
     the case, then the linker will eventually complain that there is
     a TOC reference to an unknown section.  Thus, for vtables only,
     we emit the TOC reference to reference the symbol and not the
     section.  */
  if (VTABLE_NAME_P (name))
    {
      RS6000_OUTPUT_BASENAME (file, name);
      if (offset < 0)
	fprintf (file, "%d", offset);
      else if (offset > 0)
	fprintf (file, "+%d", offset);
    }
  else
    output_addr_const (file, x);
  putc ('\n', file);
}

/* Output an assembler pseudo-op to write an ASCII string of N characters
   starting at P to FILE.

   On the RS/6000, we have to do this using the .byte operation and
   write out special characters outside the quoted string.
   Also, the assembler is broken; very long strings are truncated,
   so we must artificially break them up early.  */

void
output_ascii (FILE *file, const char *p, int n)
{
  char c;
  int i, count_string;
  const char *for_string = "\t.byte \"";
  const char *for_decimal = "\t.byte ";
  const char *to_close = NULL;

  count_string = 0;
  for (i = 0; i < n; i++)
    {
      c = *p++;
      if (c >= ' ' && c < 0177)
	{
	  if (for_string)
	    fputs (for_string, file);
	  putc (c, file);

	  /* Write two quotes to get one.  */
	  if (c == '"')
	    {
	      putc (c, file);
	      ++count_string;
	    }

	  for_string = NULL;
	  for_decimal = "\"\n\t.byte ";
	  to_close = "\"\n";
	  ++count_string;

	  if (count_string >= 512)
	    {
	      fputs (to_close, file);

	      for_string = "\t.byte \"";
	      for_decimal = "\t.byte ";
	      to_close = NULL;
	      count_string = 0;
	    }
	}
      else
	{
	  if (for_decimal)
	    fputs (for_decimal, file);
	  fprintf (file, "%d", c);

	  for_string = "\n\t.byte \"";
	  for_decimal = ", ";
	  to_close = "\n";
	  count_string = 0;
	}
    }

  /* Now close the string if we have written one.  Then end the line.  */
  if (to_close)
    fputs (to_close, file);
}

/* Generate a unique section name for FILENAME for a section type
   represented by SECTION_DESC.  Output goes into BUF.

   SECTION_DESC can be any string, as long as it is different for each
   possible section type.

   We name the section in the same manner as xlc.  The name begins with an
   underscore followed by the filename (after stripping any leading directory
   names) with the last period replaced by the string SECTION_DESC.  If
   FILENAME does not contain a period, SECTION_DESC is appended to the end of
   the name.  */

void
rs6000_gen_section_name (char **buf, const char *filename, 
		         const char *section_desc)
{
  const char *q, *after_last_slash, *last_period = 0;
  char *p;
  int len;

  after_last_slash = filename;
  for (q = filename; *q; q++)
    {
      if (*q == '/')
	after_last_slash = q + 1;
      else if (*q == '.')
	last_period = q;
    }

  len = strlen (after_last_slash) + strlen (section_desc) + 2;
  *buf = (char *) xmalloc (len);

  p = *buf;
  *p++ = '_';

  for (q = after_last_slash; *q; q++)
    {
      if (q == last_period)
        {
	  strcpy (p, section_desc);
	  p += strlen (section_desc);
	  break;
        }

      else if (ISALNUM (*q))
        *p++ = *q;
    }

  if (last_period == 0)
    strcpy (p, section_desc);
  else
    *p = '\0';
}

/* Emit profile function.  */

void
output_profile_hook (int labelno ATTRIBUTE_UNUSED)
{
  if (TARGET_PROFILE_KERNEL)
    return;

  if (DEFAULT_ABI == ABI_AIX)
    {
#ifndef NO_PROFILE_COUNTERS
# define NO_PROFILE_COUNTERS 0
#endif
      if (NO_PROFILE_COUNTERS)  
	emit_library_call (init_one_libfunc (RS6000_MCOUNT), 0, VOIDmode, 0);
      else
	{
	  char buf[30];
	  const char *label_name;
	  rtx fun;

	  ASM_GENERATE_INTERNAL_LABEL (buf, "LP", labelno);
	  label_name = (*targetm.strip_name_encoding) (ggc_strdup (buf));
	  fun = gen_rtx_SYMBOL_REF (Pmode, label_name);

	  emit_library_call (init_one_libfunc (RS6000_MCOUNT), 0, VOIDmode, 1,
			     fun, Pmode);
	}
    }
  else if (DEFAULT_ABI == ABI_DARWIN)
    {
      const char *mcount_name = RS6000_MCOUNT;
      int caller_addr_regno = LINK_REGISTER_REGNUM;

      /* Be conservative and always set this, at least for now.  */
      current_function_uses_pic_offset_table = 1;

#if TARGET_MACHO
      /* For PIC code, set up a stub and collect the caller's address
	 from r0, which is where the prologue puts it.  */
      if (MACHOPIC_INDIRECT)
	{
	  mcount_name = machopic_stub_name (mcount_name);
	  if (current_function_uses_pic_offset_table)
	    caller_addr_regno = 0;
	}
#endif
      emit_library_call (gen_rtx_SYMBOL_REF (Pmode, mcount_name),
			 0, VOIDmode, 1,
			 gen_rtx_REG (Pmode, caller_addr_regno), Pmode);
    }
}

/* Write function profiler code.  */

void
output_function_profiler (FILE *file, int labelno)
{
  char buf[100];
  int save_lr = 8;

  switch (DEFAULT_ABI)
    {
    default:
      abort ();

    case ABI_V4:
      save_lr = 4;
      if (!TARGET_32BIT)
	{
	  warning ("no profiling of 64-bit code for this ABI");
	  return;
	}
      ASM_GENERATE_INTERNAL_LABEL (buf, "LP", labelno);
      fprintf (file, "\tmflr %s\n", reg_names[0]);
      if (flag_pic == 1)
	{
	  fputs ("\tbl _GLOBAL_OFFSET_TABLE_@local-4\n", file);
	  asm_fprintf (file, "\t{st|stw} %s,%d(%s)\n",
		       reg_names[0], save_lr, reg_names[1]);
	  asm_fprintf (file, "\tmflr %s\n", reg_names[12]);
	  asm_fprintf (file, "\t{l|lwz} %s,", reg_names[0]);
	  assemble_name (file, buf);
	  asm_fprintf (file, "@got(%s)\n", reg_names[12]);
	}
      else if (flag_pic > 1)
	{
	  asm_fprintf (file, "\t{st|stw} %s,%d(%s)\n",
		       reg_names[0], save_lr, reg_names[1]);
	  /* Now, we need to get the address of the label.  */
	  fputs ("\tbl 1f\n\t.long ", file);
	  assemble_name (file, buf);
	  fputs ("-.\n1:", file);
	  asm_fprintf (file, "\tmflr %s\n", reg_names[11]);
	  asm_fprintf (file, "\t{l|lwz} %s,0(%s)\n", 
		       reg_names[0], reg_names[11]);
	  asm_fprintf (file, "\t{cax|add} %s,%s,%s\n",
		       reg_names[0], reg_names[0], reg_names[11]);
	}
      else
	{
	  asm_fprintf (file, "\t{liu|lis} %s,", reg_names[12]);
	  assemble_name (file, buf);
	  fputs ("@ha\n", file);
	  asm_fprintf (file, "\t{st|stw} %s,%d(%s)\n",
		       reg_names[0], save_lr, reg_names[1]);
	  asm_fprintf (file, "\t{cal|la} %s,", reg_names[0]);
	  assemble_name (file, buf);
	  asm_fprintf (file, "@l(%s)\n", reg_names[12]);
	}

      /* ABI_V4 saves the static chain reg with ASM_OUTPUT_REG_PUSH.  */
      fprintf (file, "\tbl %s%s\n",
	       RS6000_MCOUNT, flag_pic ? "@plt" : "");

      break;

    case ABI_AIX:
    case ABI_DARWIN:
      if (!TARGET_PROFILE_KERNEL)
	{
	  /* Don't do anything, done in output_profile_hook ().  */
	}
      else
	{
	  if (TARGET_32BIT)
	    abort ();

	  asm_fprintf (file, "\tmflr %s\n", reg_names[0]);
	  asm_fprintf (file, "\tstd %s,16(%s)\n", reg_names[0], reg_names[1]);

	  if (current_function_needs_context)
	    {
	      asm_fprintf (file, "\tstd %s,24(%s)\n",
			   reg_names[STATIC_CHAIN_REGNUM], reg_names[1]);
	      fprintf (file, "\tbl %s\n", RS6000_MCOUNT);
	      asm_fprintf (file, "\tld %s,24(%s)\n",
			   reg_names[STATIC_CHAIN_REGNUM], reg_names[1]);
	    }
	  else
	    fprintf (file, "\tbl %s\n", RS6000_MCOUNT);
	}
      break;
    }
}


static int
rs6000_use_dfa_pipeline_interface (void)
{
  return 1;
}

/* Power4 load update and store update instructions are cracked into a
   load or store and an integer insn which are executed in the same cycle.
   Branches have their own dispatch slot which does not count against the
   GCC issue rate, but it changes the program flow so there are no other
   instructions to issue in this cycle.  */

static int
rs6000_variable_issue (FILE *stream ATTRIBUTE_UNUSED, 
		       int verbose ATTRIBUTE_UNUSED, 
		       rtx insn, int more)
{
  if (GET_CODE (PATTERN (insn)) == USE
      || GET_CODE (PATTERN (insn)) == CLOBBER)
    return more;

  if (rs6000_sched_groups)
    {
      if (is_microcoded_insn (insn))
        return 0;
      else if (is_cracked_insn (insn))
        return more > 2 ? more - 2 : 0;
    }

  return more - 1;
}

/* Adjust the cost of a scheduling dependency.  Return the new cost of
   a dependency LINK or INSN on DEP_INSN.  COST is the current cost.  */

static int
rs6000_adjust_cost (rtx insn, rtx link, rtx dep_insn ATTRIBUTE_UNUSED, 
		    int cost)
{
  if (! recog_memoized (insn))
    return 0;

  if (REG_NOTE_KIND (link) != 0)
    return 0;

  if (REG_NOTE_KIND (link) == 0)
    {
      /* Data dependency; DEP_INSN writes a register that INSN reads
	 some cycles later.  */
      switch (get_attr_type (insn))
	{
	case TYPE_JMPREG:
	  /* Tell the first scheduling pass about the latency between
	     a mtctr and bctr (and mtlr and br/blr).  The first
	     scheduling pass will not know about this latency since
	     the mtctr instruction, which has the latency associated
	     to it, will be generated by reload.  */
	  return TARGET_POWER ? 5 : 4;
	case TYPE_BRANCH:
	  /* Leave some extra cycles between a compare and its
	     dependent branch, to inhibit expensive mispredicts.  */
	  if ((rs6000_cpu_attr == CPU_PPC603
	       || rs6000_cpu_attr == CPU_PPC604
	       || rs6000_cpu_attr == CPU_PPC604E
	       || rs6000_cpu_attr == CPU_PPC620
	       || rs6000_cpu_attr == CPU_PPC630
	       || rs6000_cpu_attr == CPU_PPC750
	       || rs6000_cpu_attr == CPU_PPC7400
	       || rs6000_cpu_attr == CPU_PPC7450
	       || rs6000_cpu_attr == CPU_POWER4
	       || rs6000_cpu_attr == CPU_POWER5)
	      && recog_memoized (dep_insn)
	      && (INSN_CODE (dep_insn) >= 0)
	      && (get_attr_type (dep_insn) == TYPE_CMP
		  || get_attr_type (dep_insn) == TYPE_COMPARE
		  || get_attr_type (dep_insn) == TYPE_DELAYED_COMPARE
		  || get_attr_type (dep_insn) == TYPE_IMUL_COMPARE
		  || get_attr_type (dep_insn) == TYPE_LMUL_COMPARE
		  || get_attr_type (dep_insn) == TYPE_FPCOMPARE
		  || get_attr_type (dep_insn) == TYPE_CR_LOGICAL
		  || get_attr_type (dep_insn) == TYPE_DELAYED_CR))
	    return cost + 2;
	default:
	  break;
	}
      /* Fall out to return default cost.  */
    }

  return cost;
}

/* The function returns a true if INSN is microcoded.
   Return false otherwise.  */

static bool
is_microcoded_insn (rtx insn)
{
  if (!insn || !INSN_P (insn)
      || GET_CODE (PATTERN (insn)) == USE
      || GET_CODE (PATTERN (insn)) == CLOBBER)
    return false;

  if (rs6000_sched_groups)
    {
      enum attr_type type = get_attr_type (insn);
      if (type == TYPE_LOAD_EXT_U
	  || type == TYPE_LOAD_EXT_UX
	  || type == TYPE_LOAD_UX
	  || type == TYPE_STORE_UX
	  || type == TYPE_MFCR)
        return true;
    }

  return false;
}

/* The function returns a nonzero value if INSN can be scheduled only
   as the first insn in a dispatch group ("dispatch-slot restricted").
   In this case, the returned value indicates how many dispatch slots
   the insn occupies (at the beginning of the group).
   Return 0 otherwise.  */

static int
is_dispatch_slot_restricted (rtx insn)
{
  enum attr_type type;

  if (!rs6000_sched_groups)
    return 0;

  if (!insn
      || insn == NULL_RTX
      || GET_CODE (insn) == NOTE
      || GET_CODE (PATTERN (insn)) == USE
      || GET_CODE (PATTERN (insn)) == CLOBBER)
    return 0;

  type = get_attr_type (insn);

  switch (type)
    {
    case TYPE_MFCR:
    case TYPE_MFCRF:
    case TYPE_MTCR:
    case TYPE_DELAYED_CR:
    case TYPE_CR_LOGICAL:
    case TYPE_MTJMPR:
    case TYPE_MFJMPR:
      return 1;
    case TYPE_IDIV:
    case TYPE_LDIV:
      return 2;
    default:
      if (rs6000_cpu == PROCESSOR_POWER5
	  && is_cracked_insn (insn))
	return 2;
      return 0;
    }
}

/* The function returns true if INSN is cracked into 2 instructions
   by the processor (and therefore occupies 2 issue slots).  */

static bool
is_cracked_insn (rtx insn)
{
  if (!insn || !INSN_P (insn)
      || GET_CODE (PATTERN (insn)) == USE
      || GET_CODE (PATTERN (insn)) == CLOBBER)
    return false;

  if (rs6000_sched_groups)
    {
      enum attr_type type = get_attr_type (insn);
      if (type == TYPE_LOAD_U || type == TYPE_STORE_U
	       || type == TYPE_FPLOAD_U || type == TYPE_FPSTORE_U
	       || type == TYPE_FPLOAD_UX || type == TYPE_FPSTORE_UX
	       || type == TYPE_LOAD_EXT || type == TYPE_DELAYED_CR
	       || type == TYPE_COMPARE || type == TYPE_DELAYED_COMPARE
	       || type == TYPE_IMUL_COMPARE || type == TYPE_LMUL_COMPARE
	       || type == TYPE_IDIV || type == TYPE_LDIV
	       || type == TYPE_INSERT_WORD)
        return true;
    }

  return false;
}

/* The function returns true if INSN can be issued only from
   the branch slot.  */

static bool
is_branch_slot_insn (rtx insn)
{
  if (!insn || !INSN_P (insn)
      || GET_CODE (PATTERN (insn)) == USE
      || GET_CODE (PATTERN (insn)) == CLOBBER)
    return false;

  if (rs6000_sched_groups)
    {
      enum attr_type type = get_attr_type (insn);
      if (type == TYPE_BRANCH || type == TYPE_JMPREG)
	return true;	 
      return false;
    }

  return false;
}

/* A C statement (sans semicolon) to update the integer scheduling
   priority INSN_PRIORITY (INSN). Increase the priority to execute the
   INSN earlier, reduce the priority to execute INSN later.  Do not
   define this macro if you do not need to adjust the scheduling
   priorities of insns.  */

static int
rs6000_adjust_priority (rtx insn ATTRIBUTE_UNUSED, int priority)
{
  /* On machines (like the 750) which have asymmetric integer units,
     where one integer unit can do multiply and divides and the other
     can't, reduce the priority of multiply/divide so it is scheduled
     before other integer operations.  */

#if 0
  if (! INSN_P (insn))
    return priority;

  if (GET_CODE (PATTERN (insn)) == USE)
    return priority;

  switch (rs6000_cpu_attr) {
  case CPU_PPC750:
    switch (get_attr_type (insn))
      {
      default:
	break;

      case TYPE_IMUL:
      case TYPE_IDIV:
	fprintf (stderr, "priority was %#x (%d) before adjustment\n",
		 priority, priority);
	if (priority >= 0 && priority < 0x01000000)
	  priority >>= 3;
	break;
      }
  }
#endif

  if (is_dispatch_slot_restricted (insn)
      && reload_completed
      && current_sched_info->sched_max_insns_priority 
      && rs6000_sched_restricted_insns_priority)
    {

      /* Prioritize insns that can be dispatched only in the first dispatch slot.  */
      if (rs6000_sched_restricted_insns_priority == 1)
	/* Attach highest priority to insn. This means that in 
	   haifa-sched.c:ready_sort(), dispatch-slot restriction considerations 
	   precede 'priority' (critical path) considerations.  */
	return current_sched_info->sched_max_insns_priority; 
      else if (rs6000_sched_restricted_insns_priority == 2)
	/* Increase priority of insn by a minimal amount. This means that in 
	   haifa-sched.c:ready_sort(), only 'priority' (critical path) considerations
	   precede dispatch-slot restriction considerations.  */
	return (priority + 1); 
    } 

  return priority;
}

/* Return how many instructions the machine can issue per cycle.  */

static int
rs6000_issue_rate (void)
{
  /* Use issue rate of 1 for first scheduling pass to decrease degradation.  */
  if (!reload_completed)
    return 1;

  switch (rs6000_cpu_attr) {
  case CPU_RIOS1:  /* ? */
  case CPU_RS64A:
  case CPU_PPC601: /* ? */
  case CPU_PPC7450:
    return 3;
  case CPU_PPC440:
  case CPU_PPC603:
  case CPU_PPC750:
  case CPU_PPC7400:
  case CPU_PPC8540:
    return 2; 
  case CPU_RIOS2:
  case CPU_PPC604:
  case CPU_PPC604E:
  case CPU_PPC620:
  case CPU_PPC630:
    return 4;
  case CPU_POWER4:
  case CPU_POWER5:
    return 5;
  default:
    return 1;
  }
}

/* Return how many instructions to look ahead for better insn
   scheduling.  */

static int
rs6000_use_sched_lookahead (void)
{
  if (rs6000_cpu_attr == CPU_PPC8540)
    return 4;
  return 0;
}

/* Determine is PAT refers to memory.  */

static bool
is_mem_ref (rtx pat)
{
  const char * fmt;
  int i, j;
  bool ret = false;

  if (GET_CODE (pat) == MEM)
    return true;

  /* Recursively process the pattern.  */
  fmt = GET_RTX_FORMAT (GET_CODE (pat));

  for (i = GET_RTX_LENGTH (GET_CODE (pat)) - 1; i >= 0 && !ret; i--)
    {
      if (fmt[i] == 'e')
	ret |= is_mem_ref (XEXP (pat, i));
      else if (fmt[i] == 'E')
	for (j = XVECLEN (pat, i) - 1; j >= 0; j--)
	  ret |= is_mem_ref (XVECEXP (pat, i, j));
    }

  return ret;
}

/* Determine if PAT is a PATTERN of a load insn.  */
 
static bool
is_load_insn1 (rtx pat)
{
  if (!pat || pat == NULL_RTX)
    return false;

  if (GET_CODE (pat) == SET)
    return is_mem_ref (SET_SRC (pat));

  if (GET_CODE (pat) == PARALLEL)
    {
      int i;

      for (i = 0; i < XVECLEN (pat, 0); i++)
	if (is_load_insn1 (XVECEXP (pat, 0, i)))
	  return true;
    }

  return false;
}

/* Determine if INSN loads from memory.  */

static bool
is_load_insn (rtx insn)
{
  if (!insn || !INSN_P (insn))
    return false;

  if (GET_CODE (insn) == CALL_INSN)
    return false;

  return is_load_insn1 (PATTERN (insn));
}

/* Determine if PAT is a PATTERN of a store insn.  */

static bool
is_store_insn1 (rtx pat)
{
  if (!pat || pat == NULL_RTX)
    return false;

  if (GET_CODE (pat) == SET)
    return is_mem_ref (SET_DEST (pat));

  if (GET_CODE (pat) == PARALLEL)
    {
      int i;

      for (i = 0; i < XVECLEN (pat, 0); i++)
	if (is_store_insn1 (XVECEXP (pat, 0, i)))
	  return true;
    }

  return false;
}

/* Determine if INSN stores to memory.  */

static bool
is_store_insn (rtx insn)
{
  if (!insn || !INSN_P (insn))
    return false;

  return is_store_insn1 (PATTERN (insn));
}

/* Returns whether the dependence between INSN and NEXT is considered
   costly by the given target.  */

static bool
rs6000_is_costly_dependence (rtx insn, rtx next, rtx link, int cost, int distance)
{      
  /* If the flag is not enbled - no dependence is considered costly;
     allow all dependent insns in the same group. 
     This is the most aggressive option.  */
  if (rs6000_sched_costly_dep == no_dep_costly)
    return false;

  /* If the flag is set to 1 - a dependence is always considered costly; 
     do not allow dependent instructions in the same group.
     This is the most conservative option.  */
  if (rs6000_sched_costly_dep == all_deps_costly)
    return true;       

  if (rs6000_sched_costly_dep == store_to_load_dep_costly 
      && is_load_insn (next) 
      && is_store_insn (insn))
    /* Prevent load after store in the same group.  */
    return true;

  if (rs6000_sched_costly_dep == true_store_to_load_dep_costly
      && is_load_insn (next) 
      && is_store_insn (insn)
      && (!link || (int) REG_NOTE_KIND (link) == 0))
     /* Prevent load after store in the same group if it is a true dependence.  */
     return true;
    
  /* The flag is set to X; dependences with latency >= X are considered costly, 
     and will not be scheduled in the same group.  */
  if (rs6000_sched_costly_dep <= max_dep_latency
      && ((cost - distance) >= (int)rs6000_sched_costly_dep))
    return true;

  return false;
}

/* Return the next insn after INSN that is found before TAIL is reached, 
   skipping any "non-active" insns - insns that will not actually occupy
   an issue slot.  Return NULL_RTX if such an insn is not found.  */

static rtx
get_next_active_insn (rtx insn, rtx tail)
{
  rtx next_insn;

  if (!insn || insn == tail)
    return NULL_RTX;

  next_insn = NEXT_INSN (insn);

  while (next_insn
  	 && next_insn != tail
	 && (GET_CODE(next_insn) == NOTE
	     || GET_CODE (PATTERN (next_insn)) == USE
	     || GET_CODE (PATTERN (next_insn)) == CLOBBER))
    {
      next_insn = NEXT_INSN (next_insn);
    }

  if (!next_insn || next_insn == tail)
    return NULL_RTX;

  return next_insn;
}

/* Return whether the presence of INSN causes a dispatch group termination
   of group WHICH_GROUP.

   If WHICH_GROUP == current_group, this function will return true if INSN
   causes the termination of the current group (i.e, the dispatch group to
   which INSN belongs). This means that INSN will be the last insn in the
   group it belongs to.

   If WHICH_GROUP == previous_group, this function will return true if INSN
   causes the termination of the previous group (i.e, the dispatch group that
   precedes the group to which INSN belongs).  This means that INSN will be
   the first insn in the group it belongs to).  */

static bool
insn_terminates_group_p (rtx insn, enum group_termination which_group)
{
  enum attr_type type;

  if (! insn)
    return false;

  type = get_attr_type (insn);

  if (is_microcoded_insn (insn))
    return true;

  if (which_group == current_group)
    {
      if (is_branch_slot_insn (insn))
        return true;
      return false;
    }
  else if (which_group == previous_group)
    {
      if (is_dispatch_slot_restricted (insn))
        return true;
      return false;
    }

  return false;
}

/* Return true if it is recommended to keep NEXT_INSN "far" (in a separate
   dispatch group) from the insns in GROUP_INSNS.  Return false otherwise.  */

static bool
is_costly_group (rtx *group_insns, rtx next_insn)
{
  int i;
  rtx link;
  int cost;
  int issue_rate = rs6000_issue_rate ();

  for (i = 0; i < issue_rate; i++)
    {
      rtx insn = group_insns[i];
      if (!insn)
        continue;
      for (link = INSN_DEPEND (insn); link != 0; link = XEXP (link, 1))
        {
          rtx next = XEXP (link, 0);
          if (next == next_insn)
            {
              cost = insn_cost (insn, link, next_insn);
              if (rs6000_is_costly_dependence (insn, next_insn, link, cost, 0))
                return true;
            }
        }
    }

  return false;
}

/* Utility of the function redefine_groups. 
   Check if it is too costly to schedule NEXT_INSN together with GROUP_INSNS
   in the same dispatch group.  If so, insert nops before NEXT_INSN, in order
   to keep it "far" (in a separate group) from GROUP_INSNS, following
   one of the following schemes, depending on the value of the flag
   -minsert_sched_nops = X:
   (1) X == sched_finish_regroup_exact: insert exactly as many nops as needed
       in order to force NEXT_INSN into a separate group.
   (2) X < sched_finish_regroup_exact: insert exactly X nops.  
   GROUP_END, CAN_ISSUE_MORE and GROUP_COUNT record the state after nop 
   insertion (has a group just ended, how many vacant issue slots remain in the
   last group, and how many dispatch groups were encountered so far).  */

static int 
force_new_group (int sched_verbose, FILE *dump, rtx *group_insns, rtx next_insn,
		 bool *group_end, int can_issue_more, int *group_count)
{
  rtx nop;
  bool force;
  int issue_rate = rs6000_issue_rate ();
  bool end = *group_end;
  int i;

  if (next_insn == NULL_RTX)
    return can_issue_more;

  if (rs6000_sched_insert_nops > sched_finish_regroup_exact)
    return can_issue_more;

  force = is_costly_group (group_insns, next_insn);
  if (!force)
    return can_issue_more;

  if (sched_verbose > 6)
    fprintf (dump,"force: group count = %d, can_issue_more = %d\n",
			*group_count ,can_issue_more);

  if (rs6000_sched_insert_nops == sched_finish_regroup_exact)
    {
      if (*group_end)
        can_issue_more = 0;

      /* Since only a branch can be issued in the last issue_slot, it is
	 sufficient to insert 'can_issue_more - 1' nops if next_insn is not
	 a branch. If next_insn is a branch, we insert 'can_issue_more' nops;
	 in this case the last nop will start a new group and the branch will be
	 forced to the new group.  */
      if (can_issue_more && !is_branch_slot_insn (next_insn))
        can_issue_more--;

      while (can_issue_more > 0)
        {
          nop = gen_nop();
          emit_insn_before (nop, next_insn);
          can_issue_more--;
        }

      *group_end = true;
      return 0;
    } 

  if (rs6000_sched_insert_nops < sched_finish_regroup_exact)
    {
      int n_nops = rs6000_sched_insert_nops;

      /* Nops can't be issued from the branch slot, so the effective 
         issue_rate for nops is 'issue_rate - 1'.  */
      if (can_issue_more == 0)
        can_issue_more = issue_rate;
      can_issue_more--;
      if (can_issue_more == 0)
        {
          can_issue_more = issue_rate - 1;
          (*group_count)++;
          end = true;
          for (i = 0; i < issue_rate; i++)
            {
              group_insns[i] = 0;
            }
        }

      while (n_nops > 0)
        {
          nop = gen_nop ();
          emit_insn_before (nop, next_insn);
          if (can_issue_more == issue_rate - 1) /* new group begins */
            end = false;
          can_issue_more--;
          if (can_issue_more == 0)
            {
              can_issue_more = issue_rate - 1;
              (*group_count)++;
              end = true;
              for (i = 0; i < issue_rate; i++)
                {
                  group_insns[i] = 0;
                } 
            }	
          n_nops--;
        }

      /* Scale back relative to 'issue_rate' (instead of 'issue_rate - 1').  */
      can_issue_more++; 

      *group_end = /* Is next_insn going to start a new group?  */
	  (end 
	   || (can_issue_more == 1 && !is_branch_slot_insn (next_insn))
	   || (can_issue_more <= 2 && is_cracked_insn (next_insn))
	   || (can_issue_more < issue_rate &&
	      insn_terminates_group_p (next_insn, previous_group)));
      if (*group_end && end)
        (*group_count)--;

      if (sched_verbose > 6)
        fprintf (dump, "done force: group count = %d, can_issue_more = %d\n",
			*group_count, can_issue_more);
      return can_issue_more;	
    } 

  return can_issue_more;
}

/* This function tries to synch the dispatch groups that the compiler "sees"
   with the dispatch groups that the processor dispatcher is expected to 
   form in practice.  It tries to achieve this synchronization by forcing the
   estimated processor grouping on the compiler (as opposed to the function
   'pad_goups' which tries to force the scheduler's grouping on the processor).

   The function scans the insn sequence between PREV_HEAD_INSN and TAIL and
   examines the (estimated) dispatch groups that will be formed by the processor
   dispatcher.  It marks these group boundaries to reflect the estimated
   processor grouping, overriding the grouping that the scheduler had marked.
   Depending on the value of the flag '-minsert-sched-nops' this function can
   force certain insns into separate groups or force a certain distance between
   them by inserting nops, for example, if there exists a "costly dependence"
   between the insns.

   The function estimates the group boundaries that the processor will form as
   folllows:  It keeps track of how many vacant issue slots are available after
   each insn.  A subsequent insn will start a new group if one of the following
   4 cases applies:
   - no more vacant issue slots remain in the current dispatch group.
   - only the last issue slot, which is the branch slot, is vacant, but the next
     insn is not a branch.
   - only the last 2 or less issue slots, including the branch slot, are vacant,
     which means that a cracked insn (which occupies two issue slots) can't be
     issued in this group.
   - less than 'issue_rate' slots are vacant, and the next insn always needs to 
     start a new group.  */

static int
redefine_groups (FILE *dump, int sched_verbose, rtx prev_head_insn, rtx tail)
{
  rtx insn, next_insn;
  int issue_rate;
  int can_issue_more;
  int slot, i;
  bool group_end;
  int group_count = 0;
  rtx *group_insns;

  /* Initialize.  */
  issue_rate = rs6000_issue_rate ();
  group_insns = alloca (issue_rate * sizeof (rtx));
  for (i = 0; i < issue_rate; i++) 
    {
      group_insns[i] = 0;
    }
  can_issue_more = issue_rate;
  slot = 0;
  insn = get_next_active_insn (prev_head_insn, tail);
  group_end = false;

  while (insn != NULL_RTX)
    {
      slot = (issue_rate - can_issue_more);
      group_insns[slot] = insn;
      can_issue_more =
        rs6000_variable_issue (dump, sched_verbose, insn, can_issue_more);
      if (insn_terminates_group_p (insn, current_group))
        can_issue_more = 0;

      next_insn = get_next_active_insn (insn, tail);
      if (next_insn == NULL_RTX)
        return group_count + 1;

      group_end = /* Is next_insn going to start a new group?  */
        (can_issue_more == 0
         || (can_issue_more == 1 && !is_branch_slot_insn (next_insn))
         || (can_issue_more <= 2 && is_cracked_insn (next_insn))
         || (can_issue_more < issue_rate &&
             insn_terminates_group_p (next_insn, previous_group)));

      can_issue_more = force_new_group (sched_verbose, dump, group_insns, 
			next_insn, &group_end, can_issue_more, &group_count);

      if (group_end)
        {
          group_count++;
          can_issue_more = 0;
          for (i = 0; i < issue_rate; i++)
            {
              group_insns[i] = 0;
            }
        }

      if (GET_MODE (next_insn) == TImode && can_issue_more)
        PUT_MODE(next_insn, VOIDmode);
      else if (!can_issue_more && GET_MODE (next_insn) != TImode)
        PUT_MODE (next_insn, TImode);

      insn = next_insn;
      if (can_issue_more == 0)
        can_issue_more = issue_rate;
   } /* while */

  return group_count;
}

/* Scan the insn sequence between PREV_HEAD_INSN and TAIL and examine the
   dispatch group boundaries that the scheduler had marked.  Pad with nops
   any dispatch groups which have vacant issue slots, in order to force the
   scheduler's grouping on the processor dispatcher.  The function
   returns the number of dispatch groups found.  */

static int
pad_groups (FILE *dump, int sched_verbose, rtx prev_head_insn, rtx tail)
{
  rtx insn, next_insn;
  rtx nop;
  int issue_rate;
  int can_issue_more;
  int group_end;
  int group_count = 0;

  /* Initialize issue_rate.  */
  issue_rate = rs6000_issue_rate ();
  can_issue_more = issue_rate;

  insn = get_next_active_insn (prev_head_insn, tail);
  next_insn = get_next_active_insn (insn, tail);

  while (insn != NULL_RTX)
    {
      can_issue_more =
      	rs6000_variable_issue (dump, sched_verbose, insn, can_issue_more);

      group_end = (next_insn == NULL_RTX || GET_MODE (next_insn) == TImode);

      if (next_insn == NULL_RTX)
        break;

      if (group_end)
        {
          /* If the scheduler had marked group termination at this location
             (between insn and next_indn), and neither insn nor next_insn will
             force group termination, pad the group with nops to force group
             termination.  */
          if (can_issue_more
              && (rs6000_sched_insert_nops == sched_finish_pad_groups)
              && !insn_terminates_group_p (insn, current_group)
              && !insn_terminates_group_p (next_insn, previous_group))
            {
              if (!is_branch_slot_insn(next_insn))
                can_issue_more--;

              while (can_issue_more)
                {
                  nop = gen_nop ();
                  emit_insn_before (nop, next_insn);
                  can_issue_more--;
                }
            }

          can_issue_more = issue_rate;
          group_count++;
        }

      insn = next_insn;
      next_insn = get_next_active_insn (insn, tail);
    }

  return group_count;
}

/* The following function is called at the end of scheduling BB.
   After reload, it inserts nops at insn group bundling.  */

static void
rs6000_sched_finish (FILE *dump, int sched_verbose)
{
  int n_groups;

  if (sched_verbose)
    fprintf (dump, "=== Finishing schedule.\n");

  if (reload_completed && rs6000_sched_groups)
    {
      if (rs6000_sched_insert_nops == sched_finish_none)
        return;

      if (rs6000_sched_insert_nops == sched_finish_pad_groups)
        n_groups = pad_groups (dump, sched_verbose,
				current_sched_info->prev_head,
  			   	current_sched_info->next_tail);
      else
        n_groups = redefine_groups (dump, sched_verbose,
				current_sched_info->prev_head,
  				current_sched_info->next_tail);

      if (sched_verbose >= 6)
	{
    	  fprintf (dump, "ngroups = %d\n", n_groups);
	  print_rtl (dump, current_sched_info->prev_head);
	  fprintf (dump, "Done finish_sched\n");
	}
    }
}

/* Length in units of the trampoline for entering a nested function.  */

int
rs6000_trampoline_size (void)
{
  int ret = 0;

  switch (DEFAULT_ABI)
    {
    default:
      abort ();

    case ABI_AIX:
      ret = (TARGET_32BIT) ? 12 : 24;
      break;

    case ABI_DARWIN:
    case ABI_V4:
      ret = (TARGET_32BIT) ? 40 : 48;
      break;
    }

  return ret;
}

/* Emit RTL insns to initialize the variable parts of a trampoline.
   FNADDR is an RTX for the address of the function's pure code.
   CXT is an RTX for the static chain value for the function.  */

void
rs6000_initialize_trampoline (rtx addr, rtx fnaddr, rtx cxt)
{
  enum machine_mode pmode = Pmode;
  int regsize = (TARGET_32BIT) ? 4 : 8;
  rtx ctx_reg = force_reg (pmode, cxt);

  switch (DEFAULT_ABI)
    {
    default:
      abort ();

/* Macros to shorten the code expansions below.  */
#define MEM_DEREF(addr) gen_rtx_MEM (pmode, memory_address (pmode, addr))
#define MEM_PLUS(addr,offset) \
  gen_rtx_MEM (pmode, memory_address (pmode, plus_constant (addr, offset)))

    /* Under AIX, just build the 3 word function descriptor */
    case ABI_AIX:
      {
	rtx fn_reg = gen_reg_rtx (pmode);
	rtx toc_reg = gen_reg_rtx (pmode);
	emit_move_insn (fn_reg, MEM_DEREF (fnaddr));
	emit_move_insn (toc_reg, MEM_PLUS (fnaddr, regsize));
	emit_move_insn (MEM_DEREF (addr), fn_reg);
	emit_move_insn (MEM_PLUS (addr, regsize), toc_reg);
	emit_move_insn (MEM_PLUS (addr, 2*regsize), ctx_reg);
      }
      break;

    /* Under V.4/eabi/darwin, __trampoline_setup does the real work.  */
    case ABI_DARWIN:
    case ABI_V4:
      emit_library_call (gen_rtx_SYMBOL_REF (SImode, "__trampoline_setup"),
			 FALSE, VOIDmode, 4,
			 addr, pmode,
			 GEN_INT (rs6000_trampoline_size ()), SImode,
			 fnaddr, pmode,
			 ctx_reg, pmode);
      break;
    }

  return;
}


/* Table of valid machine attributes.  */

const struct attribute_spec rs6000_attribute_table[] =
{
  /* { name, min_len, max_len, decl_req, type_req, fn_type_req, handler } */
  { "altivec",   1, 1, false, true,  false, rs6000_handle_altivec_attribute },
  { "longcall",  0, 0, false, true,  true,  rs6000_handle_longcall_attribute },
  { "shortcall", 0, 0, false, true,  true,  rs6000_handle_longcall_attribute },
  { NULL,        0, 0, false, false, false, NULL }
};

/* Handle the "altivec" attribute.  The attribute may have
   arguments as follows:

       __attribute__((altivec(vector__)))
       __attribute__((altivec(pixel__)))       (always followed by 'unsigned short')
       __attribute__((altivec(bool__)))        (always followed by 'unsigned')

  and may appear more than once (e.g., 'vector bool char') in a
  given declaration.  */

static tree
rs6000_handle_altivec_attribute (tree *node, tree name, tree args,
				 int flags ATTRIBUTE_UNUSED,
				 bool *no_add_attrs)
{
  tree type = *node, result = NULL_TREE;
  enum machine_mode mode;
  int unsigned_p;
  char altivec_type
    = ((args && TREE_CODE (args) == TREE_LIST && TREE_VALUE (args)
       && TREE_CODE (TREE_VALUE (args)) == IDENTIFIER_NODE)
       ? *IDENTIFIER_POINTER (TREE_VALUE (args))
       : '?');

  while (POINTER_TYPE_P (type)
	 || TREE_CODE (type) == FUNCTION_TYPE
	 || TREE_CODE (type) == METHOD_TYPE
	 || TREE_CODE (type) == ARRAY_TYPE)
    type = TREE_TYPE (type);

  mode = TYPE_MODE (type);

  if (rs6000_warn_altivec_long
      && (type == long_unsigned_type_node || type == long_integer_type_node))
    warning ("use of 'long' in AltiVec types is deprecated; use 'int'");

  switch (altivec_type)
    {
    case 'v':
      unsigned_p = TREE_UNSIGNED (type);
      switch (mode)
	{
	  case SImode:
	    result = (unsigned_p ? unsigned_V4SI_type_node : V4SI_type_node);
	    break;
	  case HImode:
	    result = (unsigned_p ? unsigned_V8HI_type_node : V8HI_type_node);
	    break;
	  case QImode:
	    result = (unsigned_p ? unsigned_V16QI_type_node : V16QI_type_node);
	    break;
	  case SFmode: result = V4SF_type_node; break;
	    /* If the user says 'vector int bool', we may be handed the 'bool'
	       attribute _before_ the 'vector' attribute, and so select the proper
	       type in the 'b' case below.  */
	  case V4SImode: case V8HImode: case V16QImode: result = type;
	  default: break;
	}
      break;
    case 'b':
      switch (mode)
	{
	  case SImode: case V4SImode: result = bool_V4SI_type_node; break;
	  case HImode: case V8HImode: result = bool_V8HI_type_node; break;
	  case QImode: case V16QImode: result = bool_V16QI_type_node;
	  default: break;
	}
      break;
    case 'p':
      switch (mode)
	{
	  case V8HImode: result = pixel_V8HI_type_node;
	  default: break;
	}
    default: break;
    }

  if (result && result != type && TYPE_READONLY (type))
    result = build_qualified_type (result, TYPE_QUAL_CONST);

  *no_add_attrs = true;  /* No need to hang on to the attribute.  */

  if (!result)
    warning ("`%s' attribute ignored", IDENTIFIER_POINTER (name));
  else
    *node = reconstruct_complex_type (*node, result);

  return NULL_TREE;
}

/* AltiVec defines four built-in scalar types that serve as vector
   elements; we must teach the compiler how to mangle them.  */

static const char *
rs6000_mangle_fundamental_type (tree type)
{
  if (type == bool_char_type_node) return "U6__boolc";
  if (type == bool_short_type_node) return "U6__bools";
  if (type == pixel_type_node) return "u7__pixel";
  if (type == bool_int_type_node) return "U6__booli";

  /* For all other types, use normal C++ mangling.  */
  return NULL;
}

/* Handle a "longcall" or "shortcall" attribute; arguments as in
   struct attribute_spec.handler.  */

static tree
rs6000_handle_longcall_attribute (tree *node, tree name, 
				  tree args ATTRIBUTE_UNUSED, 
				  int flags ATTRIBUTE_UNUSED, 
				  bool *no_add_attrs)
{
  if (TREE_CODE (*node) != FUNCTION_TYPE
      && TREE_CODE (*node) != FIELD_DECL
      && TREE_CODE (*node) != TYPE_DECL)
    {
      warning ("`%s' attribute only applies to functions",
	       IDENTIFIER_POINTER (name));
      *no_add_attrs = true;
    }

  return NULL_TREE;
}

/* Set longcall attributes on all functions declared when
   rs6000_default_long_calls is true.  */
static void
rs6000_set_default_type_attributes (tree type)
{
  if (rs6000_default_long_calls
      && (TREE_CODE (type) == FUNCTION_TYPE
	  || TREE_CODE (type) == METHOD_TYPE))
    TYPE_ATTRIBUTES (type) = tree_cons (get_identifier ("longcall"),
					NULL_TREE,
					TYPE_ATTRIBUTES (type));
}

/* Return a reference suitable for calling a function with the
   longcall attribute.  */

struct rtx_def *
rs6000_longcall_ref (rtx call_ref)
{
  const char *call_name;
  tree node;

  if (GET_CODE (call_ref) != SYMBOL_REF)
    return call_ref;

  /* System V adds '.' to the internal name, so skip them.  */
  call_name = XSTR (call_ref, 0);
  if (*call_name == '.')
    {
      while (*call_name == '.')
	call_name++;

      node = get_identifier (call_name);
      call_ref = gen_rtx_SYMBOL_REF (VOIDmode, IDENTIFIER_POINTER (node));
    }

  return force_reg (Pmode, call_ref);
}

#ifdef USING_ELFOS_H

/* A C statement or statements to switch to the appropriate section
   for output of RTX in mode MODE.  You can assume that RTX is some
   kind of constant in RTL.  The argument MODE is redundant except in
   the case of a `const_int' rtx.  Select the section by calling
   `text_section' or one of the alternatives for other sections.

   Do not define this macro if you put all constants in the read-only
   data section.  */

static void
rs6000_elf_select_rtx_section (enum machine_mode mode, rtx x, 
			       unsigned HOST_WIDE_INT align)
{
  if (ASM_OUTPUT_SPECIAL_POOL_ENTRY_P (x, mode))
    toc_section ();
  else
    default_elf_select_rtx_section (mode, x, align);
}

/* A C statement or statements to switch to the appropriate
   section for output of DECL.  DECL is either a `VAR_DECL' node
   or a constant of some sort.  RELOC indicates whether forming
   the initial value of DECL requires link-time relocations.  */

static void
rs6000_elf_select_section (tree decl, int reloc, 
			   unsigned HOST_WIDE_INT align)
{
  /* Pretend that we're always building for a shared library when
     ABI_AIX, because otherwise we end up with dynamic relocations
     in read-only sections.  This happens for function pointers,
     references to vtables in typeinfo, and probably other cases.  */
  default_elf_select_section_1 (decl, reloc, align,
				flag_pic || DEFAULT_ABI == ABI_AIX);
}

/* A C statement to build up a unique section name, expressed as a
   STRING_CST node, and assign it to DECL_SECTION_NAME (decl).
   RELOC indicates whether the initial value of EXP requires
   link-time relocations.  If you do not define this macro, GCC will use
   the symbol name prefixed by `.' as the section name.  Note - this
   macro can now be called for uninitialized data items as well as
   initialized data and functions.  */

static void
rs6000_elf_unique_section (tree decl, int reloc)
{
  /* As above, pretend that we're always building for a shared library
     when ABI_AIX, to avoid dynamic relocations in read-only sections.  */
  default_unique_section_1 (decl, reloc,
			    flag_pic || DEFAULT_ABI == ABI_AIX);
}

/* For a SYMBOL_REF, set generic flags and then perform some
   target-specific processing.

   When the AIX ABI is requested on a non-AIX system, replace the
   function name with the real name (with a leading .) rather than the
   function descriptor name.  This saves a lot of overriding code to
   read the prefixes.  */

static void
rs6000_elf_encode_section_info (tree decl, rtx rtl, int first)
{
  default_encode_section_info (decl, rtl, first);

  if (first
      && TREE_CODE (decl) == FUNCTION_DECL
      && !TARGET_AIX
      && DEFAULT_ABI == ABI_AIX)
    {
      rtx sym_ref = XEXP (rtl, 0);
      size_t len = strlen (XSTR (sym_ref, 0));
      char *str = alloca (len + 2);
      str[0] = '.';
      memcpy (str + 1, XSTR (sym_ref, 0), len + 1);
      XSTR (sym_ref, 0) = ggc_alloc_string (str, len + 1);
    }
}

static bool
rs6000_elf_in_small_data_p (tree decl)
{
  if (rs6000_sdata == SDATA_NONE)
    return false;

  /* We want to merge strings, so we never consider them small data.  */
  if (TREE_CODE (decl) == STRING_CST)
    return false;

  /* Functions are never in the small data area.  */
  if (TREE_CODE (decl) == FUNCTION_DECL)
    return false;

  /* Thread-local vars can't go in the small data area.  */
  if (TREE_CODE (decl) == VAR_DECL && DECL_THREAD_LOCAL (decl))
    return false;

  if (TREE_CODE (decl) == VAR_DECL && DECL_SECTION_NAME (decl))
    {
      const char *section = TREE_STRING_POINTER (DECL_SECTION_NAME (decl));
      if (strcmp (section, ".sdata") == 0
	  || strcmp (section, ".sdata2") == 0
	  || strcmp (section, ".sbss") == 0
	  || strcmp (section, ".sbss2") == 0
	  || strcmp (section, ".PPC.EMB.sdata0") == 0
	  || strcmp (section, ".PPC.EMB.sbss0") == 0)
	return true;
    }
  else
    {
      HOST_WIDE_INT size = int_size_in_bytes (TREE_TYPE (decl));

      if (size > 0
	  && (unsigned HOST_WIDE_INT) size <= g_switch_value
	  /* If it's not public, and we're not going to reference it there,
	     there's no need to put it in the small data section.  */
	  && (rs6000_sdata != SDATA_DATA || TREE_PUBLIC (decl)))
	return true;
    }

  return false;
}

#endif /* USING_ELFOS_H */


/* Return a REG that occurs in ADDR with coefficient 1.
   ADDR can be effectively incremented by incrementing REG.

   r0 is special and we must not select it as an address
   register by this routine since our caller will try to
   increment the returned register via an "la" instruction.  */

struct rtx_def *
find_addr_reg (rtx addr)
{
  while (GET_CODE (addr) == PLUS)
    {
      if (GET_CODE (XEXP (addr, 0)) == REG
	  && REGNO (XEXP (addr, 0)) != 0)
	addr = XEXP (addr, 0);
      else if (GET_CODE (XEXP (addr, 1)) == REG
	       && REGNO (XEXP (addr, 1)) != 0)
	addr = XEXP (addr, 1);
      else if (CONSTANT_P (XEXP (addr, 0)))
	addr = XEXP (addr, 1);
      else if (CONSTANT_P (XEXP (addr, 1)))
	addr = XEXP (addr, 0);
      else
	abort ();
    }
  if (GET_CODE (addr) == REG && REGNO (addr) != 0)
    return addr;
  abort ();
}

void
rs6000_fatal_bad_address (rtx op)
{
  fatal_insn ("bad address", op);
}

#if TARGET_MACHO

#if 0
/* Returns 1 if OP is either a symbol reference or a sum of a symbol
   reference and a constant.  */

int
symbolic_operand (rtx op)
{
  switch (GET_CODE (op))
    {
    case SYMBOL_REF:
    case LABEL_REF:
      return 1;
    case CONST:
      op = XEXP (op, 0);
      return (GET_CODE (op) == SYMBOL_REF ||
	      (GET_CODE (XEXP (op, 0)) == SYMBOL_REF
	       || GET_CODE (XEXP (op, 0)) == LABEL_REF)
	      && GET_CODE (XEXP (op, 1)) == CONST_INT);
    default:
      return 0;
    }
}
#endif

#if TARGET_MACHO

static tree branch_island_list = 0;

/* Remember to generate a branch island for far calls to the given
   function.  */

static void 
add_compiler_branch_island (tree label_name, tree function_name, int line_number)
{
  tree branch_island = build_tree_list (function_name, label_name);
  TREE_TYPE (branch_island) = build_int_2 (line_number, 0);
  TREE_CHAIN (branch_island) = branch_island_list;
  branch_island_list = branch_island;
}

#define BRANCH_ISLAND_LABEL_NAME(BRANCH_ISLAND)     TREE_VALUE (BRANCH_ISLAND)
#define BRANCH_ISLAND_FUNCTION_NAME(BRANCH_ISLAND)  TREE_PURPOSE (BRANCH_ISLAND)
#define BRANCH_ISLAND_LINE_NUMBER(BRANCH_ISLAND)    \
		TREE_INT_CST_LOW (TREE_TYPE (BRANCH_ISLAND))

/* Generate far-jump branch islands for everything on the
   branch_island_list.  Invoked immediately after the last instruction
   of the epilogue has been emitted; the branch-islands must be
   appended to, and contiguous with, the function body.  Mach-O stubs
   are generated in machopic_output_stub().  */

static void
macho_branch_islands (void)
{
  char tmp_buf[512];
  tree branch_island;

  for (branch_island = branch_island_list;
       branch_island;
       branch_island = TREE_CHAIN (branch_island))
    {
      const char *label =
	IDENTIFIER_POINTER (BRANCH_ISLAND_LABEL_NAME (branch_island));
      const char *name  =
	darwin_strip_name_encoding (
	  IDENTIFIER_POINTER (BRANCH_ISLAND_FUNCTION_NAME (branch_island)));
      char name_buf[512];
      /* Cheap copy of the details from the Darwin ASM_OUTPUT_LABELREF().  */
      if (name[0] == '*' || name[0] == '&')
	strcpy (name_buf, name+1);
      else
	{
	  name_buf[0] = '_';
	  strcpy (name_buf+1, name);
	}
      strcpy (tmp_buf, "\n");
      strcat (tmp_buf, label);
#if defined (DBX_DEBUGGING_INFO) || defined (XCOFF_DEBUGGING_INFO)
      if (write_symbols == DBX_DEBUG || write_symbols == XCOFF_DEBUG)
	fprintf (asm_out_file, "\t.stabd 68,0," HOST_WIDE_INT_PRINT_UNSIGNED "\n",
		 BRANCH_ISLAND_LINE_NUMBER(branch_island));
#endif /* DBX_DEBUGGING_INFO || XCOFF_DEBUGGING_INFO */
      if (flag_pic)
	{
	  strcat (tmp_buf, ":\n\tmflr r0\n\tbcl 20,31,");
	  strcat (tmp_buf, label);
	  strcat (tmp_buf, "_pic\n");
	  strcat (tmp_buf, label);
	  strcat (tmp_buf, "_pic:\n\tmflr r11\n");
 
	  strcat (tmp_buf, "\taddis r11,r11,ha16(");
	  strcat (tmp_buf, name_buf);
	  strcat (tmp_buf, " - ");
	  strcat (tmp_buf, label);
	  strcat (tmp_buf, "_pic)\n");
 		   
	  strcat (tmp_buf, "\tmtlr r0\n");
  
	  strcat (tmp_buf, "\taddi r12,r11,lo16(");
	  strcat (tmp_buf, name_buf);
	  strcat (tmp_buf, " - ");
	  strcat (tmp_buf, label);
	  strcat (tmp_buf, "_pic)\n");
 
	  strcat (tmp_buf, "\tmtctr r12\n\tbctr\n");
	}
      else
	{
	  strcat (tmp_buf, ":\nlis r12,hi16(");
	  strcat (tmp_buf, name_buf);
	  strcat (tmp_buf, ")\n\tori r12,r12,lo16(");
	  strcat (tmp_buf, name_buf);
	  strcat (tmp_buf, ")\n\tmtctr r12\n\tbctr");
	}
      output_asm_insn (tmp_buf, 0);
#if defined (DBX_DEBUGGING_INFO) || defined (XCOFF_DEBUGGING_INFO)
      if (write_symbols == DBX_DEBUG || write_symbols == XCOFF_DEBUG)
	fprintf(asm_out_file, "\t.stabd 68,0," HOST_WIDE_INT_PRINT_UNSIGNED "\n",
		BRANCH_ISLAND_LINE_NUMBER (branch_island));
#endif /* DBX_DEBUGGING_INFO || XCOFF_DEBUGGING_INFO */
    }

  branch_island_list = 0;
}

/* NO_PREVIOUS_DEF checks in the link list whether the function name is
   already there or not.  */

static int
no_previous_def (tree function_name)
{
  tree branch_island;
  for (branch_island = branch_island_list;
       branch_island;
       branch_island = TREE_CHAIN (branch_island))
    if (function_name == BRANCH_ISLAND_FUNCTION_NAME (branch_island))
      return 0;
  return 1;
}

/* GET_PREV_LABEL gets the label name from the previous definition of
   the function.  */

static tree
get_prev_label (tree function_name)
{
  tree branch_island;
  for (branch_island = branch_island_list;
       branch_island;
       branch_island = TREE_CHAIN (branch_island))
    if (function_name == BRANCH_ISLAND_FUNCTION_NAME (branch_island))
      return BRANCH_ISLAND_LABEL_NAME (branch_island);
  return 0;
}

/* INSN is either a function call or a millicode call.  It may have an
   unconditional jump in its delay slot.  

   CALL_DEST is the routine we are calling.  */

char *
output_call (rtx insn, rtx *operands, int dest_operand_number, int cookie_operand_number)
{
  static char buf[256];
  if (GET_CODE (operands[dest_operand_number]) == SYMBOL_REF
      && (INTVAL (operands[cookie_operand_number]) & CALL_LONG))
    {
      tree labelname;
      tree funname = get_identifier (XSTR (operands[dest_operand_number], 0));
      
      if (no_previous_def (funname))
	{
	  int line_number = 0;
	  rtx label_rtx = gen_label_rtx ();
	  char *label_buf, temp_buf[256];
	  ASM_GENERATE_INTERNAL_LABEL (temp_buf, "L",
				       CODE_LABEL_NUMBER (label_rtx));
	  label_buf = temp_buf[0] == '*' ? temp_buf + 1 : temp_buf;
	  labelname = get_identifier (label_buf);
	  for (; insn && GET_CODE (insn) != NOTE; insn = PREV_INSN (insn));
	  if (insn)
	    line_number = NOTE_LINE_NUMBER (insn);
	  add_compiler_branch_island (labelname, funname, line_number);
	}
      else
	labelname = get_prev_label (funname);

      /* "jbsr foo, L42" is Mach-O for "Link as 'bl foo' if a 'bl'
	 instruction will reach 'foo', otherwise link as 'bl L42'".
	 "L42" should be a 'branch island', that will do a far jump to
	 'foo'.  Branch islands are generated in
	 macho_branch_islands().  */
      sprintf (buf, "jbsr %%z%d,%.246s",
	       dest_operand_number, IDENTIFIER_POINTER (labelname));
    }
  else
    sprintf (buf, "bl %%z%d", dest_operand_number);
  return buf;
}

#endif /* TARGET_MACHO */

/* Generate PIC and indirect symbol stubs.  */

void
machopic_output_stub (FILE *file, const char *symb, const char *stub)
{
  unsigned int length;
  char *symbol_name, *lazy_ptr_name;
  char *local_label_0;
  static int label = 0;

  /* Lose our funky encoding stuff so it doesn't contaminate the stub.  */
  symb = (*targetm.strip_name_encoding) (symb);


  length = strlen (symb);
  symbol_name = alloca (length + 32);
  GEN_SYMBOL_NAME_FOR_SYMBOL (symbol_name, symb, length);

  lazy_ptr_name = alloca (length + 32);
  GEN_LAZY_PTR_NAME_FOR_SYMBOL (lazy_ptr_name, symb, length);

  if (flag_pic == 2)
    machopic_picsymbol_stub1_section ();
  else
    machopic_symbol_stub1_section ();
  fprintf (file, "\t.align 2\n");

  fprintf (file, "%s:\n", stub);
  fprintf (file, "\t.indirect_symbol %s\n", symbol_name);

  if (flag_pic == 2)
    {
      label++;
      local_label_0 = alloca (sizeof("\"L0000000000$spb\""));
      sprintf (local_label_0, "\"L%011d$spb\"", label);
    
      fprintf (file, "\tmflr r0\n");
      fprintf (file, "\tbcl 20,31,%s\n", local_label_0);
      fprintf (file, "%s:\n\tmflr r11\n", local_label_0);
      fprintf (file, "\taddis r11,r11,ha16(%s-%s)\n",
	       lazy_ptr_name, local_label_0);
      fprintf (file, "\tmtlr r0\n");
      fprintf (file, "\tlwzu r12,lo16(%s-%s)(r11)\n",
	       lazy_ptr_name, local_label_0);
      fprintf (file, "\tmtctr r12\n");
      fprintf (file, "\tbctr\n");
    }
  else
   {
     fprintf (file, "\tlis r11,ha16(%s)\n", lazy_ptr_name);
     fprintf (file, "\tlwzu r12,lo16(%s)(r11)\n", lazy_ptr_name);
     fprintf (file, "\tmtctr r12\n");
     fprintf (file, "\tbctr\n");
   }
  
  machopic_lazy_symbol_ptr_section ();
  fprintf (file, "%s:\n", lazy_ptr_name);
  fprintf (file, "\t.indirect_symbol %s\n", symbol_name);
  fprintf (file, "\t.long dyld_stub_binding_helper\n");
}

/* Legitimize PIC addresses.  If the address is already
   position-independent, we return ORIG.  Newly generated
   position-independent addresses go into a reg.  This is REG if non
   zero, otherwise we allocate register(s) as necessary.  */

#define SMALL_INT(X) ((unsigned) (INTVAL(X) + 0x8000) < 0x10000)

rtx
rs6000_machopic_legitimize_pic_address (rtx orig, enum machine_mode mode, 
					rtx reg)
{
  rtx base, offset;

  if (reg == NULL && ! reload_in_progress && ! reload_completed)
    reg = gen_reg_rtx (Pmode);

  if (GET_CODE (orig) == CONST)
    {
      if (GET_CODE (XEXP (orig, 0)) == PLUS
	  && XEXP (XEXP (orig, 0), 0) == pic_offset_table_rtx)
	return orig;

      if (GET_CODE (XEXP (orig, 0)) == PLUS)
	{
	  /* Use a different reg for the intermediate value, as
	     it will be marked UNCHANGING.  */
	  rtx reg_temp = no_new_pseudos ? reg : gen_reg_rtx (Pmode);

	  base =
	    rs6000_machopic_legitimize_pic_address (XEXP (XEXP (orig, 0), 0),
						    Pmode, reg_temp);
	  offset =
	    rs6000_machopic_legitimize_pic_address (XEXP (XEXP (orig, 0), 1),
						    Pmode, reg);
	}
      else
	abort ();

      if (GET_CODE (offset) == CONST_INT)
	{
	  if (SMALL_INT (offset))
	    return plus_constant (base, INTVAL (offset));
	  else if (! reload_in_progress && ! reload_completed)
	    offset = force_reg (Pmode, offset);
	  else
	    {
 	      rtx mem = force_const_mem (Pmode, orig);
	      return machopic_legitimize_pic_address (mem, Pmode, reg);
	    }
	}
      return gen_rtx (PLUS, Pmode, base, offset);
    }

  /* Fall back on generic machopic code.  */
  return machopic_legitimize_pic_address (orig, mode, reg);
}

/* This is just a placeholder to make linking work without having to
   add this to the generic Darwin EXTRA_SECTIONS.  If -mcall-aix is
   ever needed for Darwin (not too likely!) this would have to get a
   real definition.  */

void
toc_section (void)
{
}

#endif /* TARGET_MACHO */

#if TARGET_ELF
static unsigned int
rs6000_elf_section_type_flags (tree decl, const char *name, int reloc)
{
  return default_section_type_flags_1 (decl, name, reloc,
				       flag_pic || DEFAULT_ABI == ABI_AIX);
}

/* Record an element in the table of global constructors.  SYMBOL is
   a SYMBOL_REF of the function to be called; PRIORITY is a number
   between 0 and MAX_INIT_PRIORITY.

   This differs from default_named_section_asm_out_constructor in
   that we have special handling for -mrelocatable.  */

static void
rs6000_elf_asm_out_constructor (rtx symbol, int priority)
{
  const char *section = ".ctors";
  char buf[16];

  if (priority != DEFAULT_INIT_PRIORITY)
    {
      sprintf (buf, ".ctors.%.5u",
               /* Invert the numbering so the linker puts us in the proper
                  order; constructors are run from right to left, and the
                  linker sorts in increasing order.  */
               MAX_INIT_PRIORITY - priority);
      section = buf;
    }

  named_section_flags (section, SECTION_WRITE);
  assemble_align (POINTER_SIZE);

  if (TARGET_RELOCATABLE)
    {
      fputs ("\t.long (", asm_out_file);
      output_addr_const (asm_out_file, symbol);
      fputs (")@fixup\n", asm_out_file);
    }
  else
    assemble_integer (symbol, POINTER_SIZE / BITS_PER_UNIT, POINTER_SIZE, 1);
}

static void
rs6000_elf_asm_out_destructor (rtx symbol, int priority)
{
  const char *section = ".dtors";
  char buf[16];

  if (priority != DEFAULT_INIT_PRIORITY)
    {
      sprintf (buf, ".dtors.%.5u",
               /* Invert the numbering so the linker puts us in the proper
                  order; constructors are run from right to left, and the
                  linker sorts in increasing order.  */
               MAX_INIT_PRIORITY - priority);
      section = buf;
    }

  named_section_flags (section, SECTION_WRITE);
  assemble_align (POINTER_SIZE);

  if (TARGET_RELOCATABLE)
    {
      fputs ("\t.long (", asm_out_file);
      output_addr_const (asm_out_file, symbol);
      fputs (")@fixup\n", asm_out_file);
    }
  else
    assemble_integer (symbol, POINTER_SIZE / BITS_PER_UNIT, POINTER_SIZE, 1);
}

void
rs6000_elf_declare_function_name (FILE *file, const char *name, tree decl)
{
  if (TARGET_64BIT)
    {
      fputs ("\t.section\t\".opd\",\"aw\"\n\t.align 3\n", file);
      ASM_OUTPUT_LABEL (file, name);
      fputs (DOUBLE_INT_ASM_OP, file);
      putc ('.', file);
      assemble_name (file, name);
      fputs (",.TOC.@tocbase,0\n\t.previous\n\t.size\t", file);
      assemble_name (file, name);
      fputs (",24\n\t.type\t.", file);
      assemble_name (file, name);
      fputs (",@function\n", file);
      if (TREE_PUBLIC (decl) && ! DECL_WEAK (decl))
	{
	  fputs ("\t.globl\t.", file);
	  assemble_name (file, name);
	  putc ('\n', file);
	}
      ASM_DECLARE_RESULT (file, DECL_RESULT (decl));
      putc ('.', file);
      ASM_OUTPUT_LABEL (file, name);
      return;
    }

  if (TARGET_RELOCATABLE
      && (get_pool_size () != 0 || current_function_profile)
      && uses_TOC ())
    {
      char buf[256];

      (*targetm.asm_out.internal_label) (file, "LCL", rs6000_pic_labelno);

      ASM_GENERATE_INTERNAL_LABEL (buf, "LCTOC", 1);
      fprintf (file, "\t.long ");
      assemble_name (file, buf);
      putc ('-', file);
      ASM_GENERATE_INTERNAL_LABEL (buf, "LCF", rs6000_pic_labelno);
      assemble_name (file, buf);
      putc ('\n', file);
    }

  ASM_OUTPUT_TYPE_DIRECTIVE (file, name, "function");
  ASM_DECLARE_RESULT (file, DECL_RESULT (decl));

  if (DEFAULT_ABI == ABI_AIX)
    {
      const char *desc_name, *orig_name;

      orig_name = (*targetm.strip_name_encoding) (name);
      desc_name = orig_name;
      while (*desc_name == '.')
	desc_name++;

      if (TREE_PUBLIC (decl))
	fprintf (file, "\t.globl %s\n", desc_name);

      fprintf (file, "%s\n", MINIMAL_TOC_SECTION_ASM_OP);
      fprintf (file, "%s:\n", desc_name);
      fprintf (file, "\t.long %s\n", orig_name);
      fputs ("\t.long _GLOBAL_OFFSET_TABLE_\n", file);
      if (DEFAULT_ABI == ABI_AIX)
	fputs ("\t.long 0\n", file);
      fprintf (file, "\t.previous\n");
    }
  ASM_OUTPUT_LABEL (file, name);
}

static void
rs6000_elf_end_indicate_exec_stack (void)
{
  if (TARGET_32BIT)
    file_end_indicate_exec_stack ();
}
#endif

#if TARGET_XCOFF
static void
rs6000_xcoff_asm_globalize_label (FILE *stream, const char *name)
{
  fputs (GLOBAL_ASM_OP, stream);
  RS6000_OUTPUT_BASENAME (stream, name);
  putc ('\n', stream);
}

static void
rs6000_xcoff_asm_named_section (const char *name, unsigned int flags)
{
  int smclass;
  static const char * const suffix[3] = { "PR", "RO", "RW" };

  if (flags & SECTION_CODE)
    smclass = 0;
  else if (flags & SECTION_WRITE)
    smclass = 2;
  else
    smclass = 1;

  fprintf (asm_out_file, "\t.csect %s%s[%s],%u\n",
	   (flags & SECTION_CODE) ? "." : "",
	   name, suffix[smclass], flags & SECTION_ENTSIZE);
}

static void
rs6000_xcoff_select_section (tree decl, int reloc, 
			    unsigned HOST_WIDE_INT align ATTRIBUTE_UNUSED)
{
  if (decl_readonly_section_1 (decl, reloc, 1))
    {
      if (TREE_PUBLIC (decl))
        read_only_data_section ();
      else
        read_only_private_data_section ();
    }
  else
    {
      if (TREE_PUBLIC (decl))
        data_section ();
      else
        private_data_section ();
    }
}

static void
rs6000_xcoff_unique_section (tree decl, int reloc ATTRIBUTE_UNUSED)
{
  const char *name;

  /* Use select_section for private and uninitialized data.  */
  if (!TREE_PUBLIC (decl)
      || DECL_COMMON (decl)
      || DECL_INITIAL (decl) == NULL_TREE
      || DECL_INITIAL (decl) == error_mark_node
      || (flag_zero_initialized_in_bss
	  && initializer_zerop (DECL_INITIAL (decl))))
    return;

  name = IDENTIFIER_POINTER (DECL_ASSEMBLER_NAME (decl));
  name = (*targetm.strip_name_encoding) (name);
  DECL_SECTION_NAME (decl) = build_string (strlen (name), name);
}

/* Select section for constant in constant pool.

   On RS/6000, all constants are in the private read-only data area.
   However, if this is being placed in the TOC it must be output as a
   toc entry.  */

static void
rs6000_xcoff_select_rtx_section (enum machine_mode mode, rtx x, 
				unsigned HOST_WIDE_INT align ATTRIBUTE_UNUSED)
{
  if (ASM_OUTPUT_SPECIAL_POOL_ENTRY_P (x, mode))
    toc_section ();
  else
    read_only_private_data_section ();
}

/* Remove any trailing [DS] or the like from the symbol name.  */

static const char *
rs6000_xcoff_strip_name_encoding (const char *name)
{
  size_t len;
  if (*name == '*')
    name++;
  len = strlen (name);
  if (name[len - 1] == ']')
    return ggc_alloc_string (name, len - 4);
  else
    return name;
}

/* Section attributes.  AIX is always PIC.  */

static unsigned int
rs6000_xcoff_section_type_flags (tree decl, const char *name, int reloc)
{
  unsigned int align;
  unsigned int flags = default_section_type_flags_1 (decl, name, reloc, 1);

  /* Align to at least UNIT size.  */
  if (flags & SECTION_CODE)
    align = MIN_UNITS_PER_WORD;
  else
    /* Increase alignment of large objects if not already stricter.  */
    align = MAX ((DECL_ALIGN (decl) / BITS_PER_UNIT),
		 int_size_in_bytes (TREE_TYPE (decl)) > MIN_UNITS_PER_WORD
		 ? UNITS_PER_FP_WORD : MIN_UNITS_PER_WORD);

  return flags | (exact_log2 (align) & SECTION_ENTSIZE);
}

/* Output at beginning of assembler file.

   Initialize the section names for the RS/6000 at this point.

   Specify filename, including full path, to assembler.

   We want to go into the TOC section so at least one .toc will be emitted.
   Also, in order to output proper .bs/.es pairs, we need at least one static
   [RW] section emitted.

   Finally, declare mcount when profiling to make the assembler happy.  */

static void
rs6000_xcoff_file_start (void)
{
  rs6000_gen_section_name (&xcoff_bss_section_name,
			   main_input_filename, ".bss_");
  rs6000_gen_section_name (&xcoff_private_data_section_name,
			   main_input_filename, ".rw_");
  rs6000_gen_section_name (&xcoff_read_only_section_name,
			   main_input_filename, ".ro_");

  fputs ("\t.file\t", asm_out_file);
  output_quoted_string (asm_out_file, main_input_filename);
  fputc ('\n', asm_out_file);
  toc_section ();
  if (write_symbols != NO_DEBUG)
    private_data_section ();
  text_section ();
  if (profile_flag)
    fprintf (asm_out_file, "\t.extern %s\n", RS6000_MCOUNT);
  rs6000_file_start ();
}

/* Output at end of assembler file.
   On the RS/6000, referencing data should automatically pull in text.  */

static void
rs6000_xcoff_file_end (void)
{
  text_section ();
  fputs ("_section_.text:\n", asm_out_file);
  data_section ();
  fputs (TARGET_32BIT
	 ? "\t.long _section_.text\n" : "\t.llong _section_.text\n",
	 asm_out_file);
}
#endif /* TARGET_XCOFF */

#if TARGET_MACHO
/* Cross-module name binding.  Darwin does not support overriding
   functions at dynamic-link time.  */

static bool
rs6000_binds_local_p (tree decl)
{
  return default_binds_local_p_1 (decl, 0);
}
#endif

/* Compute a (partial) cost for rtx X.  Return true if the complete
   cost has been computed, and false if subexpressions should be
   scanned.  In either case, *TOTAL contains the cost result.  */

static bool
rs6000_rtx_costs (rtx x, int code, int outer_code ATTRIBUTE_UNUSED, 
		  int *total)
{
  switch (code)
    {
      /* On the RS/6000, if it is valid in the insn, it is free.
	 So this always returns 0.  */
    case CONST_INT:
    case CONST:
    case LABEL_REF:
    case SYMBOL_REF:
    case CONST_DOUBLE:
    case HIGH:
      *total = 0;
      return true;

    case PLUS:
      *total = ((GET_CODE (XEXP (x, 1)) == CONST_INT
		 && ((unsigned HOST_WIDE_INT) (INTVAL (XEXP (x, 1))
					       + 0x8000) >= 0x10000)
		 && ((INTVAL (XEXP (x, 1)) & 0xffff) != 0))
		? COSTS_N_INSNS (2)
		: COSTS_N_INSNS (1));
      return true;

    case AND:
    case IOR:
    case XOR:
      *total = ((GET_CODE (XEXP (x, 1)) == CONST_INT
		 && (INTVAL (XEXP (x, 1)) & (~ (HOST_WIDE_INT) 0xffff)) != 0
		 && ((INTVAL (XEXP (x, 1)) & 0xffff) != 0))
		? COSTS_N_INSNS (2)
		: COSTS_N_INSNS (1));
      return true;

    case MULT:
      if (optimize_size)
	{
	  *total = COSTS_N_INSNS (2);
	  return true;
	}
      switch (rs6000_cpu)
	{
	case PROCESSOR_RIOS1:
	case PROCESSOR_PPC405:
	  *total = (GET_CODE (XEXP (x, 1)) != CONST_INT
		    ? COSTS_N_INSNS (5)
		    : (INTVAL (XEXP (x, 1)) >= -256
		       && INTVAL (XEXP (x, 1)) <= 255)
		    ? COSTS_N_INSNS (3) : COSTS_N_INSNS (4));
	  return true;

	case PROCESSOR_PPC440:
	  *total = (GET_CODE (XEXP (x, 1)) != CONST_INT
		    ? COSTS_N_INSNS (3)
		    : COSTS_N_INSNS (2));
	  return true;

	case PROCESSOR_RS64A:
	  *total = (GET_CODE (XEXP (x, 1)) != CONST_INT
		    ? GET_MODE (XEXP (x, 1)) != DImode
		    ? COSTS_N_INSNS (20) : COSTS_N_INSNS (34)
		    : (INTVAL (XEXP (x, 1)) >= -256
		       && INTVAL (XEXP (x, 1)) <= 255)
		    ? COSTS_N_INSNS (8) : COSTS_N_INSNS (12));
	  return true;

	case PROCESSOR_RIOS2:
	case PROCESSOR_MPCCORE:
	case PROCESSOR_PPC604e:
	  *total = COSTS_N_INSNS (2);
	  return true;

	case PROCESSOR_PPC601:
	  *total = COSTS_N_INSNS (5);
	  return true;

	case PROCESSOR_PPC603:
	case PROCESSOR_PPC7400:
	case PROCESSOR_PPC750:
	  *total = (GET_CODE (XEXP (x, 1)) != CONST_INT
		    ? COSTS_N_INSNS (5)
		    : (INTVAL (XEXP (x, 1)) >= -256
		       && INTVAL (XEXP (x, 1)) <= 255)
		    ? COSTS_N_INSNS (2) : COSTS_N_INSNS (3));
	  return true;

	case PROCESSOR_PPC7450:
	  *total = (GET_CODE (XEXP (x, 1)) != CONST_INT
		    ? COSTS_N_INSNS (4)
		    : COSTS_N_INSNS (3));
	  return true;

	case PROCESSOR_PPC403:
	case PROCESSOR_PPC604:
	case PROCESSOR_PPC8540:
	  *total = COSTS_N_INSNS (4);
	  return true;

	case PROCESSOR_PPC620:
	case PROCESSOR_PPC630:
	  *total = (GET_CODE (XEXP (x, 1)) != CONST_INT
		    ? GET_MODE (XEXP (x, 1)) != DImode
		    ? COSTS_N_INSNS (5) : COSTS_N_INSNS (7)
		    : (INTVAL (XEXP (x, 1)) >= -256
		       && INTVAL (XEXP (x, 1)) <= 255)
		    ? COSTS_N_INSNS (3) : COSTS_N_INSNS (4));
	  return true;

	case PROCESSOR_POWER4:
	case PROCESSOR_POWER5:
	  *total = (GET_CODE (XEXP (x, 1)) != CONST_INT
		    ? GET_MODE (XEXP (x, 1)) != DImode
		    ? COSTS_N_INSNS (3) : COSTS_N_INSNS (4)
		    : COSTS_N_INSNS (2));
	  return true;

	default:
	  abort ();
	}

    case DIV:
    case MOD:
      if (GET_CODE (XEXP (x, 1)) == CONST_INT
	  && exact_log2 (INTVAL (XEXP (x, 1))) >= 0)
	{
	  *total = COSTS_N_INSNS (2);
	  return true;
	}
      /* FALLTHRU */

    case UDIV:
    case UMOD:
      switch (rs6000_cpu)
	{
	case PROCESSOR_RIOS1:
	  *total = COSTS_N_INSNS (19);
	  return true;

	case PROCESSOR_RIOS2:
	  *total = COSTS_N_INSNS (13);
	  return true;

	case PROCESSOR_RS64A:
	  *total = (GET_MODE (XEXP (x, 1)) != DImode
		    ? COSTS_N_INSNS (65)
		    : COSTS_N_INSNS (67));
	  return true;

	case PROCESSOR_MPCCORE:
	  *total = COSTS_N_INSNS (6);
	  return true;

	case PROCESSOR_PPC403:
	  *total = COSTS_N_INSNS (33);
	  return true;

	case PROCESSOR_PPC405:
	  *total = COSTS_N_INSNS (35);
	  return true;

	case PROCESSOR_PPC440:
	  *total = COSTS_N_INSNS (34);
	  return true;

	case PROCESSOR_PPC601:
	  *total = COSTS_N_INSNS (36);
	  return true;

	case PROCESSOR_PPC603:
	  *total = COSTS_N_INSNS (37);
	  return true;

	case PROCESSOR_PPC604:
	case PROCESSOR_PPC604e:
	  *total = COSTS_N_INSNS (20);
	  return true;

	case PROCESSOR_PPC620:
	case PROCESSOR_PPC630:
	  *total = (GET_MODE (XEXP (x, 1)) != DImode
		    ? COSTS_N_INSNS (21)
		    : COSTS_N_INSNS (37));
	  return true;

	case PROCESSOR_PPC750:
	case PROCESSOR_PPC8540:
	case PROCESSOR_PPC7400:
	  *total = COSTS_N_INSNS (19);
	  return true;

	case PROCESSOR_PPC7450:
	  *total = COSTS_N_INSNS (23);
	  return true;

	case PROCESSOR_POWER4:
	case PROCESSOR_POWER5:
	  *total = (GET_MODE (XEXP (x, 1)) != DImode
		    ? COSTS_N_INSNS (18)
		    : COSTS_N_INSNS (34));
	  return true;

	default:
	  abort ();
	}

    case FFS:
      *total = COSTS_N_INSNS (4);
      return true;

    case MEM:
      /* MEM should be slightly more expensive than (plus (reg) (const)).  */
      *total = 5;
      return true;

    default:
      return false;
    }
}

/* A C expression returning the cost of moving data from a register of class
   CLASS1 to one of CLASS2.  */

int
rs6000_register_move_cost (enum machine_mode mode, 
			   enum reg_class from, enum reg_class to)
{
  /*  Moves from/to GENERAL_REGS.  */
  if (reg_classes_intersect_p (to, GENERAL_REGS)
      || reg_classes_intersect_p (from, GENERAL_REGS))
    {
      if (! reg_classes_intersect_p (to, GENERAL_REGS))
	from = to;

      if (from == FLOAT_REGS || from == ALTIVEC_REGS)
	return (rs6000_memory_move_cost (mode, from, 0)
		+ rs6000_memory_move_cost (mode, GENERAL_REGS, 0));

/* It's more expensive to move CR_REGS than CR0_REGS because of the shift....  */
      else if (from == CR_REGS)
	return 4;

      else
/* A move will cost one instruction per GPR moved.  */
	return 2 * HARD_REGNO_NREGS (0, mode);
    }

/* Moving between two similar registers is just one instruction.  */
  else if (reg_classes_intersect_p (to, from))
    return mode == TFmode ? 4 : 2;

/* Everything else has to go through GENERAL_REGS.  */
  else
    return (rs6000_register_move_cost (mode, GENERAL_REGS, to) 
	    + rs6000_register_move_cost (mode, from, GENERAL_REGS));
}

/* A C expressions returning the cost of moving data of MODE from a register to
   or from memory.  */

int
rs6000_memory_move_cost (enum machine_mode mode, enum reg_class class, 
			 int in ATTRIBUTE_UNUSED)
{
  if (reg_classes_intersect_p (class, GENERAL_REGS))
    return 4 * HARD_REGNO_NREGS (0, mode);
  else if (reg_classes_intersect_p (class, FLOAT_REGS))
    return 4 * HARD_REGNO_NREGS (32, mode);
  else if (reg_classes_intersect_p (class, ALTIVEC_REGS))
    return 4 * HARD_REGNO_NREGS (FIRST_ALTIVEC_REGNO, mode);
  else
    return 4 + rs6000_register_move_cost (mode, class, GENERAL_REGS);
}

/* Return an RTX representing where to find the function value of a
   function returning MODE.  */
static rtx
rs6000_complex_function_value (enum machine_mode mode)
{
  unsigned int regno;
  rtx r1, r2;
  enum machine_mode inner = GET_MODE_INNER (mode);
  unsigned int inner_bytes = GET_MODE_SIZE (inner);

  if (FLOAT_MODE_P (mode) && TARGET_HARD_FLOAT && TARGET_FPRS)
    regno = FP_ARG_RETURN;
  else
    {
      regno = GP_ARG_RETURN;

      /* 32-bit is OK since it'll go in r3/r4.  */
      if (TARGET_32BIT && inner_bytes >= 4)
	return gen_rtx_REG (mode, regno);
    }

  if (inner_bytes >= 8)
    return gen_rtx_REG (mode, regno);

  r1 = gen_rtx_EXPR_LIST (inner, gen_rtx_REG (inner, regno),
			  const0_rtx);
  r2 = gen_rtx_EXPR_LIST (inner, gen_rtx_REG (inner, regno + 1),
			  GEN_INT (inner_bytes));
  return gen_rtx_PARALLEL (mode, gen_rtvec (2, r1, r2));
}

/* Define how to find the value returned by a function.
   VALTYPE is the data type of the value (as a tree).
   If the precise function being called is known, FUNC is its FUNCTION_DECL;
   otherwise, FUNC is 0.

   On the SPE, both FPs and vectors are returned in r3.

   On RS/6000 an integer value is in r3 and a floating-point value is in
   fp1, unless -msoft-float.  */

rtx
rs6000_function_value (tree valtype, tree func ATTRIBUTE_UNUSED)
{
  enum machine_mode mode;
  unsigned int regno;

  if (TARGET_32BIT && TARGET_POWERPC64 && TYPE_MODE (valtype) == DImode)
    {
      /* Long long return value need be split in -mpowerpc64, 32bit ABI.  */
      return gen_rtx_PARALLEL (DImode,
	gen_rtvec (2,
		   gen_rtx_EXPR_LIST (VOIDmode,
				      gen_rtx_REG (SImode, GP_ARG_RETURN),
				      const0_rtx),
		   gen_rtx_EXPR_LIST (VOIDmode,
				      gen_rtx_REG (SImode,
						   GP_ARG_RETURN + 1),
				      GEN_INT (4))));
    }

  if ((INTEGRAL_TYPE_P (valtype)
       && TYPE_PRECISION (valtype) < BITS_PER_WORD)
      || POINTER_TYPE_P (valtype))
    mode = TARGET_32BIT ? SImode : DImode;
  else
    mode = TYPE_MODE (valtype);

  if (SCALAR_FLOAT_TYPE_P (valtype) && TARGET_HARD_FLOAT && TARGET_FPRS)
    regno = FP_ARG_RETURN;
  else if (TREE_CODE (valtype) == COMPLEX_TYPE
	   && targetm.calls.split_complex_arg)
    return rs6000_complex_function_value (mode);
  else if (TREE_CODE (valtype) == VECTOR_TYPE
	   && TARGET_ALTIVEC && TARGET_ALTIVEC_ABI)
    regno = ALTIVEC_ARG_RETURN;
  else
    regno = GP_ARG_RETURN;

  return gen_rtx_REG (mode, regno);
}

/* Define how to find the value returned by a library function
   assuming the value has mode MODE.  */
rtx
rs6000_libcall_value (enum machine_mode mode)
{
  unsigned int regno;

  if (GET_MODE_CLASS (mode) == MODE_FLOAT
	   && TARGET_HARD_FLOAT && TARGET_FPRS)
    regno = FP_ARG_RETURN;
  else if (ALTIVEC_VECTOR_MODE (mode)
	   && TARGET_ALTIVEC && TARGET_ALTIVEC_ABI)
    regno = ALTIVEC_ARG_RETURN;
  else if (COMPLEX_MODE_P (mode) && targetm.calls.split_complex_arg)
    return rs6000_complex_function_value (mode);
  else
    regno = GP_ARG_RETURN;

  return gen_rtx_REG (mode, regno);
}

/* Define the offset between two registers, FROM to be eliminated and its
   replacement TO, at the start of a routine.  */
HOST_WIDE_INT
rs6000_initial_elimination_offset (int from, int to)
{
  rs6000_stack_t *info = rs6000_stack_info ();
  HOST_WIDE_INT offset;

  if (from == FRAME_POINTER_REGNUM && to == STACK_POINTER_REGNUM)
    offset = info->push_p ? 0 : -info->total_size;
  else if (from == ARG_POINTER_REGNUM && to == FRAME_POINTER_REGNUM)
    offset = info->total_size;
  else if (from == ARG_POINTER_REGNUM && to == STACK_POINTER_REGNUM)
    offset = info->push_p ? info->total_size : 0;
  else if (from == RS6000_PIC_OFFSET_TABLE_REGNUM)
    offset = 0;
  else
    abort ();

  return offset;
}

/* Return true if TYPE is of type __ev64_opaque__.  */

static bool
is_ev64_opaque_type (tree type)
{
  return (TARGET_SPE
	  && (type == opaque_V2SI_type_node
	      || type == opaque_V2SF_type_node
	      || type == opaque_p_V2SI_type_node));
}

static rtx
rs6000_dwarf_register_span (rtx reg)
{
  unsigned regno;

  if (!TARGET_SPE || !SPE_VECTOR_MODE (GET_MODE (reg)))
    return NULL_RTX;

  regno = REGNO (reg);

  /* The duality of the SPE register size wreaks all kinds of havoc.
     This is a way of distinguishing r0 in 32-bits from r0 in
     64-bits.  */
  return
    gen_rtx_PARALLEL (VOIDmode,
		      BYTES_BIG_ENDIAN
		      ? gen_rtvec (2,
				   gen_rtx_REG (SImode, regno + 1200),
				   gen_rtx_REG (SImode, regno))
		      : gen_rtvec (2,
				   gen_rtx_REG (SImode, regno),
				   gen_rtx_REG (SImode, regno + 1200)));
}

/* Map internal gcc register numbers to DWARF2 register numbers.  */

unsigned int
rs6000_dbx_register_number (unsigned int regno)
{
  if (regno <= 63 || write_symbols != DWARF2_DEBUG)
    return regno;
  if (regno == MQ_REGNO)
    return 100;
  if (regno == LINK_REGISTER_REGNUM)
    return 108;
  if (regno == COUNT_REGISTER_REGNUM)
    return 109;
  if (CR_REGNO_P (regno))
    return regno - CR0_REGNO + 86;
  if (regno == XER_REGNO)
    return 101;
  if (ALTIVEC_REGNO_P (regno))
    return regno - FIRST_ALTIVEC_REGNO + 1124;
  if (regno == VRSAVE_REGNO)
    return 356;
  if (regno == VSCR_REGNO)
    return 67;
  if (regno == SPE_ACC_REGNO)
    return 99;
  if (regno == SPEFSCR_REGNO)
    return 612;
  /* SPE high reg number.  We get these values of regno from
     rs6000_dwarf_register_span.  */
  if (regno >= 1200 && regno < 1232)
    return regno;

  abort ();
}

#include "gt-rs6000.h"
