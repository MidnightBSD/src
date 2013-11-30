/* Definitions of target machine for GCC for IA-32.
   Copyright (C) 1988, 1992, 1994, 1995, 1996, 1996, 1997, 1998, 1999,
   2000, 2001, 2002, 2003, 2004 Free Software Foundation, Inc.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GCC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

/* Functions in i386.c */
extern void override_options (void);
extern void optimization_options (int, int);

extern int ix86_can_use_return_insn_p (void);
extern int ix86_frame_pointer_required (void);
extern void ix86_setup_frame_addresses (void);

extern void ix86_file_end (void);
extern HOST_WIDE_INT ix86_initial_elimination_offset (int, int);
extern void ix86_expand_prologue (void);
extern void ix86_expand_epilogue (int);

extern void ix86_output_addr_vec_elt (FILE *, int);
extern void ix86_output_addr_diff_elt (FILE *, int, int);

#ifdef RTX_CODE
extern int ix86_aligned_p (rtx);

extern int standard_80387_constant_p (rtx);
extern const char *standard_80387_constant_opcode (rtx);
extern rtx standard_80387_constant_rtx (int);
extern int standard_sse_constant_p (rtx);
extern int symbolic_reference_mentioned_p (rtx);
extern bool extended_reg_mentioned_p (rtx);
extern bool x86_extended_QIreg_mentioned_p (rtx);
extern bool x86_extended_reg_mentioned_p (rtx);

extern int any_fp_register_operand (rtx, enum machine_mode);
extern int register_and_not_any_fp_reg_operand (rtx, enum machine_mode);

extern int fp_register_operand (rtx, enum machine_mode);
extern int register_and_not_fp_reg_operand (rtx, enum machine_mode);

extern int x86_64_general_operand (rtx, enum machine_mode);
extern int x86_64_szext_general_operand (rtx, enum machine_mode);
extern int x86_64_nonmemory_operand (rtx, enum machine_mode);
extern int x86_64_szext_nonmemory_operand (rtx, enum machine_mode);
extern int x86_64_immediate_operand (rtx, enum machine_mode);
extern int x86_64_zext_immediate_operand (rtx, enum machine_mode);
extern int symbolic_operand (rtx, enum machine_mode);
extern int tls_symbolic_operand (rtx, enum machine_mode);
extern int global_dynamic_symbolic_operand (rtx, enum machine_mode);
extern int local_dynamic_symbolic_operand (rtx, enum machine_mode);
extern int initial_exec_symbolic_operand (rtx, enum machine_mode);
extern int local_exec_symbolic_operand (rtx, enum machine_mode);
extern int pic_symbolic_operand (rtx, enum machine_mode);
extern int call_insn_operand (rtx, enum machine_mode);
extern int sibcall_insn_operand (rtx, enum machine_mode);
extern int constant_call_address_operand (rtx, enum machine_mode);
extern int const0_operand (rtx, enum machine_mode);
extern int const1_operand (rtx, enum machine_mode);
extern int const248_operand (rtx, enum machine_mode);
extern int incdec_operand (rtx, enum machine_mode);
extern int reg_no_sp_operand (rtx, enum machine_mode);
extern int mmx_reg_operand (rtx, enum machine_mode);
extern int general_no_elim_operand (rtx, enum machine_mode);
extern int nonmemory_no_elim_operand (rtx, enum machine_mode);
extern int q_regs_operand (rtx, enum machine_mode);
extern int non_q_regs_operand (rtx, enum machine_mode);
extern int sse_comparison_operator (rtx, enum machine_mode);
extern int fcmov_comparison_operator (rtx, enum machine_mode);
extern int cmp_fp_expander_operand (rtx, enum machine_mode);
extern int ix86_comparison_operator (rtx, enum machine_mode);
extern int ext_register_operand (rtx, enum machine_mode);
extern int binary_fp_operator (rtx, enum machine_mode);
extern int mult_operator (rtx, enum machine_mode);
extern int div_operator (rtx, enum machine_mode);
extern int arith_or_logical_operator (rtx, enum machine_mode);
extern int promotable_binary_operator (rtx, enum machine_mode);
extern int memory_displacement_operand (rtx, enum machine_mode);
extern int cmpsi_operand (rtx, enum machine_mode);
extern int long_memory_operand (rtx, enum machine_mode);
extern int aligned_operand (rtx, enum machine_mode);
extern int compare_operator (rtx, enum machine_mode);
extern enum machine_mode ix86_cc_mode (enum rtx_code, rtx, rtx);

extern int ix86_expand_movstr (rtx, rtx, rtx, rtx);
extern int ix86_expand_clrstr (rtx, rtx, rtx);
extern int ix86_expand_strlen (rtx, rtx, rtx, rtx);

extern bool legitimate_constant_p (rtx);
extern bool constant_address_p (rtx);
extern bool legitimate_pic_operand_p (rtx);
extern int legitimate_pic_address_disp_p (rtx);
extern int legitimate_address_p (enum machine_mode, rtx, int);
extern rtx legitimize_pic_address (rtx, rtx);
extern rtx legitimize_address (rtx, rtx, enum machine_mode);

extern void print_reg (rtx, int, FILE*);
extern void print_operand (FILE*, rtx, int);
extern void print_operand_address (FILE*, rtx);
extern bool output_addr_const_extra (FILE*, rtx);

extern void split_di (rtx[], int, rtx[], rtx[]);
extern void split_ti (rtx[], int, rtx[], rtx[]);

extern const char *output_set_got (rtx);
extern const char *output_387_binary_op (rtx, rtx*);
extern const char *output_fix_trunc (rtx, rtx*);
extern const char *output_fp_compare (rtx, rtx*, int, int);

extern void i386_dwarf_output_addr_const (FILE*, rtx);
extern void i386_output_dwarf_dtprel (FILE*, int, rtx);

extern void ix86_expand_clear (rtx);
extern void ix86_expand_move (enum machine_mode, rtx[]);
extern void ix86_expand_vector_move (enum machine_mode, rtx[]);
extern void ix86_expand_binary_operator (enum rtx_code,
					 enum machine_mode, rtx[]);
extern int ix86_binary_operator_ok (enum rtx_code, enum machine_mode, rtx[]);
extern void ix86_expand_unary_operator (enum rtx_code, enum machine_mode,
					rtx[]);
extern int ix86_unary_operator_ok (enum rtx_code, enum machine_mode, rtx[]);
extern int ix86_match_ccmode (rtx, enum machine_mode);
extern rtx ix86_expand_compare (enum rtx_code, rtx *, rtx *);
extern int ix86_use_fcomi_compare (enum rtx_code);
extern void ix86_expand_branch (enum rtx_code, rtx);
extern int ix86_expand_setcc (enum rtx_code, rtx);
extern int ix86_expand_int_movcc (rtx[]);
extern int ix86_expand_fp_movcc (rtx[]);
extern int ix86_expand_int_addcc (rtx[]);
extern void ix86_expand_call (rtx, rtx, rtx, rtx, rtx, int);
extern void x86_initialize_trampoline (rtx, rtx, rtx);
extern rtx ix86_zero_extend_to_Pmode (rtx);
extern void ix86_split_long_move (rtx[]);
extern void ix86_split_ashldi (rtx *, rtx);
extern void ix86_split_ashrdi (rtx *, rtx);
extern void ix86_split_lshrdi (rtx *, rtx);
extern rtx ix86_find_base_term (rtx);
extern int ix86_check_movabs (rtx, int);

extern rtx assign_386_stack_local (enum machine_mode, int);
extern int ix86_attr_length_immediate_default (rtx, int);
extern int ix86_attr_length_address_default (rtx);

extern enum machine_mode ix86_fp_compare_mode (enum rtx_code);

extern int x86_64_sign_extended_value (rtx);
extern int x86_64_zero_extended_value (rtx);
extern rtx ix86_libcall_value (enum machine_mode);
extern bool ix86_function_value_regno_p (int);
extern bool ix86_function_arg_regno_p (int);
extern int ix86_function_arg_boundary (enum machine_mode, tree);
extern int ix86_return_in_memory (tree);
extern void ix86_va_start (tree, rtx);
extern rtx ix86_va_arg (tree, tree);
extern void ix86_setup_incoming_varargs (CUMULATIVE_ARGS *,
					 enum machine_mode, tree, int *, int);

extern rtx ix86_force_to_memory (enum machine_mode, rtx);
extern void ix86_free_from_memory (enum machine_mode);
extern void ix86_split_fp_branch (enum rtx_code code, rtx, rtx, rtx, rtx, rtx);
extern int ix86_hard_regno_mode_ok (int, enum machine_mode);
extern int ix86_register_move_cost (enum machine_mode, enum reg_class,
				    enum reg_class);
extern int ix86_secondary_memory_needed (enum reg_class, enum reg_class,
					 enum machine_mode, int);
extern enum reg_class ix86_preferred_reload_class (rtx, enum reg_class);
extern int ix86_memory_move_cost (enum machine_mode, enum reg_class, int);
extern void emit_i387_cw_initialization (rtx, rtx);
extern bool ix86_fp_jump_nontrivial_p (enum rtx_code);
extern void x86_order_regs_for_local_alloc (void);
extern void x86_function_profiler (FILE *, int);
extern void x86_emit_floatuns (rtx [2]);


#ifdef TREE_CODE
extern void init_cumulative_args (CUMULATIVE_ARGS *, tree, rtx, tree);
extern rtx function_arg (CUMULATIVE_ARGS *, enum machine_mode, tree, int);
extern int function_arg_pass_by_reference (CUMULATIVE_ARGS *,
					   enum machine_mode, tree, int);
extern void function_arg_advance (CUMULATIVE_ARGS *, enum machine_mode,
				  tree, int);
extern rtx ix86_function_value (tree);
extern void ix86_init_builtins (void);
extern rtx ix86_expand_builtin (tree, rtx, rtx, enum machine_mode, int);
#endif

#endif

#ifdef TREE_CODE
extern int ix86_return_pops_args (tree, tree, int);

extern int ix86_data_alignment (tree, int);
extern int ix86_local_alignment (tree, int);
extern int ix86_constant_alignment (tree, int);
extern tree ix86_handle_dll_attribute (tree *, tree, tree, int, bool *);
extern tree ix86_handle_shared_attribute (tree *, tree, tree, int, bool *);

extern unsigned int i386_pe_section_type_flags (tree, const char *, int);
extern void i386_pe_asm_named_section (const char *, unsigned int);
extern int x86_field_alignment (tree, int);
#endif

extern rtx ix86_tls_get_addr (void);
extern bool ix86_must_pass_in_stack (enum machine_mode mode, tree);

extern void ix86_expand_vector_init (rtx, rtx);
/* In winnt.c  */
extern int i386_pe_dllexport_name_p (const char *);
extern int i386_pe_dllimport_name_p (const char *);
extern void i386_pe_unique_section (tree, int);
extern void i386_pe_declare_function_type (FILE *, const char *, int);
extern void i386_pe_record_external_function (const char *);
extern void i386_pe_record_exported_symbol (const char *, int);
extern void i386_pe_asm_file_end (FILE *);
extern void i386_pe_encode_section_info (tree, rtx, int);
extern const char *i386_pe_strip_name_encoding (const char *);
extern const char *i386_pe_strip_name_encoding_full (const char *);
extern void i386_pe_output_labelref (FILE *, const char *);
