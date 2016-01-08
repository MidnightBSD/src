/*
 * IDENTIFICATION:
 * stub generated Thu Jun 11 18:17:45 2015
 * with a MiG generated Thu Jun 11 16:16:11 PDT 2015 by kmacy@serenity
 * OPTIONS: 
 *	KernelServer
 */

/* Module mach_vm */

#define	__MIG_check__Request__mach_vm_subsystem__ 1

#include <sys/cdefs.h>
#include <sys/types.h>
#ifdef _KERNEL
#include <sys/mach/ndr.h>
#include <sys/mach/kern_return.h>
#include <sys/mach/notify.h>
#include <sys/mach/mach_types.h>
#include <sys/mach/message.h>
#include <sys/mach/mig_errors.h>
#else /* !_KERNEL */
#include <string.h>
#include <mach/ndr.h>
#include <mach/boolean.h>
#include <mach/kern_return.h>
#include <mach/notify.h>
#include <mach/mach_types.h>
#include <mach/message.h>
#include <mach/mig_errors.h>
#endif /*_KERNEL */

#include <sys/mach/std_types.h>
#include <sys/mach/mig.h>
#include <sys/mach/ipc_sync.h>
#include <sys/mach/ipc/ipc_voucher.h>
#include <sys/mach/ipc_host.h>
#include <sys/mach/ipc_tt.h>
#include <sys/mach/ipc_mig.h>
#include <sys/mach/mig.h>
#include <sys/mach/mach_types.h>
#include <sys/mach_debug/mach_debug_types.h>

#ifndef	mig_internal
#define	mig_internal	static __inline__
#endif	/* mig_internal */

#ifndef	mig_external
#define mig_external
#endif	/* mig_external */

#if	!defined(__MigTypeCheck) && defined(TypeCheck)
#define	__MigTypeCheck		TypeCheck	/* Legacy setting */
#endif	/* !defined(__MigTypeCheck) */

#if	!defined(__MigKernelSpecificCode) && defined(_MIG_KERNEL_SPECIFIC_CODE_)
#define	__MigKernelSpecificCode	_MIG_KERNEL_SPECIFIC_CODE_	/* Legacy setting */
#endif	/* !defined(__MigKernelSpecificCode) */

#ifndef	LimitCheck
#define	LimitCheck 0
#endif	/* LimitCheck */

#ifndef	min
#define	min(a,b)  ( ((a) < (b))? (a): (b) )
#endif	/* min */

#if !defined(_WALIGN_)
#define _WALIGN_(x) (((x) + 7) & ~7)
#endif /* !defined(_WALIGN_) */

#if !defined(_WALIGNSZ_)
#define _WALIGNSZ_(x) _WALIGN_(sizeof(x))
#endif /* !defined(_WALIGNSZ_) */

#ifndef	UseStaticTemplates
#define	UseStaticTemplates	1
#endif	/* UseStaticTemplates */

#define _WALIGN_(x) (((x) + 7) & ~7)
#define _WALIGNSZ_(x) _WALIGN_(sizeof(x))
#ifndef	__DeclareRcvRpc
#define	__DeclareRcvRpc(_NUM_, _NAME_)
#endif	/* __DeclareRcvRpc */

#ifndef	__BeforeRcvRpc
#define	__BeforeRcvRpc(_NUM_, _NAME_)
#endif	/* __BeforeRcvRpc */

#ifndef	__AfterRcvRpc
#define	__AfterRcvRpc(_NUM_, _NAME_)
#endif	/* __AfterRcvRpc */

#ifndef	__DeclareRcvSimple
#define	__DeclareRcvSimple(_NUM_, _NAME_)
#endif	/* __DeclareRcvSimple */

#ifndef	__BeforeRcvSimple
#define	__BeforeRcvSimple(_NUM_, _NAME_)
#endif	/* __BeforeRcvSimple */

#ifndef	__AfterRcvSimple
#define	__AfterRcvSimple(_NUM_, _NAME_)
#endif	/* __AfterRcvSimple */

#define novalue void
#if	__MigKernelSpecificCode
#define msgh_request_port	msgh_remote_port
#define MACH_MSGH_BITS_REQUEST(bits)	MACH_MSGH_BITS_REMOTE(bits)
#define msgh_reply_port		msgh_local_port
#define MACH_MSGH_BITS_REPLY(bits)	MACH_MSGH_BITS_LOCAL(bits)
#else
#define msgh_request_port	msgh_local_port
#define MACH_MSGH_BITS_REQUEST(bits)	MACH_MSGH_BITS_LOCAL(bits)
#define msgh_reply_port		msgh_remote_port
#define MACH_MSGH_BITS_REPLY(bits)	MACH_MSGH_BITS_REMOTE(bits)
#endif /* __MigKernelSpecificCode */

#define MIG_RETURN_ERROR(X, code)	{\
				((mig_reply_error_t *)X)->RetCode = code;\
				((mig_reply_error_t *)X)->NDR = NDR_record;\
				return;\
				}

/* typedefs for all requests */

#ifndef __Request__mach_vm_subsystem__defined
#define __Request__mach_vm_subsystem__defined

#ifdef  __MigPackStructs
#pragma pack(4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		/* start of the kernel processed data */
		mach_msg_body_t msgh_body;
		/* end of the kernel processed data */
		NDR_record_t NDR;
		mach_vm_address_t address;
		mach_vm_size_t size;
		boolean_t set_maximum;
		vm_prot_t new_protection;
	} __Request__mach_vm_protect_t;
#ifdef  __MigPackStructs
#pragma pack()
#endif

#ifdef  __MigPackStructs
#pragma pack(4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		/* start of the kernel processed data */
		mach_msg_body_t msgh_body;
		/* end of the kernel processed data */
		NDR_record_t NDR;
		mach_vm_address_t address;
		mach_vm_size_t size;
		vm_inherit_t new_inheritance;
	} __Request__mach_vm_inherit_t;
#ifdef  __MigPackStructs
#pragma pack()
#endif

#ifdef  __MigPackStructs
#pragma pack(4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		/* start of the kernel processed data */
		mach_msg_body_t msgh_body;
		/* end of the kernel processed data */
		NDR_record_t NDR;
		mach_vm_address_t address;
		mach_vm_size_t size;
	} __Request__mach_vm_read_t;
#ifdef  __MigPackStructs
#pragma pack()
#endif

#ifdef  __MigPackStructs
#pragma pack(4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		/* start of the kernel processed data */
		mach_msg_body_t msgh_body;
		/* end of the kernel processed data */
		NDR_record_t NDR;
		mach_vm_read_entry_t data_list;
		natural_t count;
	} __Request__mach_vm_read_list_t;
#ifdef  __MigPackStructs
#pragma pack()
#endif

#ifdef  __MigPackStructs
#pragma pack(4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		/* start of the kernel processed data */
		mach_msg_body_t msgh_body;
		mach_msg_ool_descriptor_t data;
		/* end of the kernel processed data */
		NDR_record_t NDR;
		mach_vm_address_t address;
		mach_msg_type_number_t dataCnt;
	} __Request__mach_vm_write_t;
#ifdef  __MigPackStructs
#pragma pack()
#endif

#ifdef  __MigPackStructs
#pragma pack(4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		/* start of the kernel processed data */
		mach_msg_body_t msgh_body;
		/* end of the kernel processed data */
		NDR_record_t NDR;
		mach_vm_address_t source_address;
		mach_vm_size_t size;
		mach_vm_address_t dest_address;
	} __Request__mach_vm_copy_t;
#ifdef  __MigPackStructs
#pragma pack()
#endif

#ifdef  __MigPackStructs
#pragma pack(4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		/* start of the kernel processed data */
		mach_msg_body_t msgh_body;
		/* end of the kernel processed data */
		NDR_record_t NDR;
		mach_vm_address_t address;
		mach_vm_size_t size;
		mach_vm_address_t data;
	} __Request__mach_vm_read_overwrite_t;
#ifdef  __MigPackStructs
#pragma pack()
#endif

#ifdef  __MigPackStructs
#pragma pack(4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		/* start of the kernel processed data */
		mach_msg_body_t msgh_body;
		/* end of the kernel processed data */
		NDR_record_t NDR;
		mach_vm_address_t address;
		mach_vm_size_t size;
		vm_sync_t sync_flags;
	} __Request__mach_vm_msync_t;
#ifdef  __MigPackStructs
#pragma pack()
#endif

#ifdef  __MigPackStructs
#pragma pack(4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		/* start of the kernel processed data */
		mach_msg_body_t msgh_body;
		/* end of the kernel processed data */
		NDR_record_t NDR;
		mach_vm_address_t address;
		mach_vm_size_t size;
		vm_behavior_t new_behavior;
	} __Request__mach_vm_behavior_set_t;
#ifdef  __MigPackStructs
#pragma pack()
#endif

#ifdef  __MigPackStructs
#pragma pack(4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		/* start of the kernel processed data */
		mach_msg_body_t msgh_body;
		/* end of the kernel processed data */
		NDR_record_t NDR;
		mach_vm_address_t address;
		mach_vm_size_t size;
		vm_machine_attribute_t attribute;
		vm_machine_attribute_val_t value;
	} __Request__mach_vm_machine_attribute_t;
#ifdef  __MigPackStructs
#pragma pack()
#endif

#ifdef  __MigPackStructs
#pragma pack(4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		/* start of the kernel processed data */
		mach_msg_body_t msgh_body;
		mach_msg_port_descriptor_t src_task;
		/* end of the kernel processed data */
		NDR_record_t NDR;
		mach_vm_address_t target_address;
		mach_vm_size_t size;
		mach_vm_offset_t mask;
		int flags;
		mach_vm_address_t src_address;
		boolean_t copy;
		vm_inherit_t inheritance;
	} __Request__mach_vm_remap_t;
#ifdef  __MigPackStructs
#pragma pack()
#endif

#ifdef  __MigPackStructs
#pragma pack(4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		/* start of the kernel processed data */
		mach_msg_body_t msgh_body;
		/* end of the kernel processed data */
		NDR_record_t NDR;
		mach_vm_offset_t offset;
	} __Request__mach_vm_page_query_t;
#ifdef  __MigPackStructs
#pragma pack()
#endif

#ifdef  __MigPackStructs
#pragma pack(4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		/* start of the kernel processed data */
		mach_msg_body_t msgh_body;
		/* end of the kernel processed data */
		NDR_record_t NDR;
		mach_vm_address_t address;
		natural_t nesting_depth;
		mach_msg_type_number_t infoCnt;
	} __Request__mach_vm_region_recurse_t;
#ifdef  __MigPackStructs
#pragma pack()
#endif

#ifdef  __MigPackStructs
#pragma pack(4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		/* start of the kernel processed data */
		mach_msg_body_t msgh_body;
		/* end of the kernel processed data */
		NDR_record_t NDR;
		mach_vm_address_t address;
		vm_region_flavor_t flavor;
		mach_msg_type_number_t infoCnt;
	} __Request__mach_vm_region_t;
#ifdef  __MigPackStructs
#pragma pack()
#endif

#ifdef  __MigPackStructs
#pragma pack(4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		/* start of the kernel processed data */
		mach_msg_body_t msgh_body;
		mach_msg_port_descriptor_t parent_handle;
		/* end of the kernel processed data */
		NDR_record_t NDR;
		memory_object_size_t size;
		memory_object_offset_t offset;
		vm_prot_t permission;
	} __Request___mach_make_memory_entry_t;
#ifdef  __MigPackStructs
#pragma pack()
#endif

#ifdef  __MigPackStructs
#pragma pack(4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		/* start of the kernel processed data */
		mach_msg_body_t msgh_body;
		/* end of the kernel processed data */
		NDR_record_t NDR;
		mach_vm_address_t address;
		vm_purgable_t control;
		int state;
	} __Request__mach_vm_purgable_control_t;
#ifdef  __MigPackStructs
#pragma pack()
#endif

#ifdef  __MigPackStructs
#pragma pack(4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		/* start of the kernel processed data */
		mach_msg_body_t msgh_body;
		/* end of the kernel processed data */
		NDR_record_t NDR;
		mach_vm_address_t address;
		vm_page_info_flavor_t flavor;
		mach_msg_type_number_t infoCnt;
	} __Request__mach_vm_page_info_t;
#ifdef  __MigPackStructs
#pragma pack()
#endif
#endif /* !__Request__mach_vm_subsystem__defined */

/* typedefs for all replies */

#ifndef __Reply__mach_vm_subsystem__defined
#define __Reply__mach_vm_subsystem__defined

#ifdef  __MigPackStructs
#pragma pack(4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		NDR_record_t NDR;
		kern_return_t RetCode;
	} __Reply__mach_vm_protect_t;
#ifdef  __MigPackStructs
#pragma pack()
#endif

#ifdef  __MigPackStructs
#pragma pack(4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		NDR_record_t NDR;
		kern_return_t RetCode;
	} __Reply__mach_vm_inherit_t;
#ifdef  __MigPackStructs
#pragma pack()
#endif

#ifdef  __MigPackStructs
#pragma pack(4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		/* start of the kernel processed data */
		mach_msg_body_t msgh_body;
		mach_msg_ool_descriptor_t data;
		/* end of the kernel processed data */
		NDR_record_t NDR;
		mach_msg_type_number_t dataCnt;
	} __Reply__mach_vm_read_t;
#ifdef  __MigPackStructs
#pragma pack()
#endif

#ifdef  __MigPackStructs
#pragma pack(4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		NDR_record_t NDR;
		kern_return_t RetCode;
		mach_vm_read_entry_t data_list;
	} __Reply__mach_vm_read_list_t;
#ifdef  __MigPackStructs
#pragma pack()
#endif

#ifdef  __MigPackStructs
#pragma pack(4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		NDR_record_t NDR;
		kern_return_t RetCode;
	} __Reply__mach_vm_write_t;
#ifdef  __MigPackStructs
#pragma pack()
#endif

#ifdef  __MigPackStructs
#pragma pack(4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		NDR_record_t NDR;
		kern_return_t RetCode;
	} __Reply__mach_vm_copy_t;
#ifdef  __MigPackStructs
#pragma pack()
#endif

#ifdef  __MigPackStructs
#pragma pack(4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		NDR_record_t NDR;
		kern_return_t RetCode;
		mach_vm_size_t outsize;
	} __Reply__mach_vm_read_overwrite_t;
#ifdef  __MigPackStructs
#pragma pack()
#endif

#ifdef  __MigPackStructs
#pragma pack(4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		NDR_record_t NDR;
		kern_return_t RetCode;
	} __Reply__mach_vm_msync_t;
#ifdef  __MigPackStructs
#pragma pack()
#endif

#ifdef  __MigPackStructs
#pragma pack(4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		NDR_record_t NDR;
		kern_return_t RetCode;
	} __Reply__mach_vm_behavior_set_t;
#ifdef  __MigPackStructs
#pragma pack()
#endif

#ifdef  __MigPackStructs
#pragma pack(4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		NDR_record_t NDR;
		kern_return_t RetCode;
		vm_machine_attribute_val_t value;
	} __Reply__mach_vm_machine_attribute_t;
#ifdef  __MigPackStructs
#pragma pack()
#endif

#ifdef  __MigPackStructs
#pragma pack(4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		NDR_record_t NDR;
		kern_return_t RetCode;
		mach_vm_address_t target_address;
		vm_prot_t cur_protection;
		vm_prot_t max_protection;
	} __Reply__mach_vm_remap_t;
#ifdef  __MigPackStructs
#pragma pack()
#endif

#ifdef  __MigPackStructs
#pragma pack(4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		NDR_record_t NDR;
		kern_return_t RetCode;
		integer_t disposition;
		integer_t ref_count;
	} __Reply__mach_vm_page_query_t;
#ifdef  __MigPackStructs
#pragma pack()
#endif

#ifdef  __MigPackStructs
#pragma pack(4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		NDR_record_t NDR;
		kern_return_t RetCode;
		mach_vm_address_t address;
		mach_vm_size_t size;
		natural_t nesting_depth;
		mach_msg_type_number_t infoCnt;
		int info[19];
	} __Reply__mach_vm_region_recurse_t;
#ifdef  __MigPackStructs
#pragma pack()
#endif

#ifdef  __MigPackStructs
#pragma pack(4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		/* start of the kernel processed data */
		mach_msg_body_t msgh_body;
		mach_msg_port_descriptor_t object_name;
		/* end of the kernel processed data */
		NDR_record_t NDR;
		mach_vm_address_t address;
		mach_vm_size_t size;
		mach_msg_type_number_t infoCnt;
		int info[10];
	} __Reply__mach_vm_region_t;
#ifdef  __MigPackStructs
#pragma pack()
#endif

#ifdef  __MigPackStructs
#pragma pack(4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		/* start of the kernel processed data */
		mach_msg_body_t msgh_body;
		mach_msg_port_descriptor_t object_handle;
		/* end of the kernel processed data */
		NDR_record_t NDR;
		memory_object_size_t size;
	} __Reply___mach_make_memory_entry_t;
#ifdef  __MigPackStructs
#pragma pack()
#endif

#ifdef  __MigPackStructs
#pragma pack(4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		NDR_record_t NDR;
		kern_return_t RetCode;
		int state;
	} __Reply__mach_vm_purgable_control_t;
#ifdef  __MigPackStructs
#pragma pack()
#endif

#ifdef  __MigPackStructs
#pragma pack(4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		NDR_record_t NDR;
		kern_return_t RetCode;
		mach_msg_type_number_t infoCnt;
		int info[32];
	} __Reply__mach_vm_page_info_t;
#ifdef  __MigPackStructs
#pragma pack()
#endif
#endif /* !__Reply__mach_vm_subsystem__defined */


/* union of all replies */

#ifndef __ReplyUnion__mach_vm_subsystem__defined
#define __ReplyUnion__mach_vm_subsystem__defined
union __ReplyUnion__mach_vm_subsystem {
	__Reply__mach_vm_protect_t Reply_mach_vm_protect;
	__Reply__mach_vm_inherit_t Reply_mach_vm_inherit;
	__Reply__mach_vm_read_t Reply_mach_vm_read;
	__Reply__mach_vm_read_list_t Reply_mach_vm_read_list;
	__Reply__mach_vm_write_t Reply_mach_vm_write;
	__Reply__mach_vm_copy_t Reply_mach_vm_copy;
	__Reply__mach_vm_read_overwrite_t Reply_mach_vm_read_overwrite;
	__Reply__mach_vm_msync_t Reply_mach_vm_msync;
	__Reply__mach_vm_behavior_set_t Reply_mach_vm_behavior_set;
	__Reply__mach_vm_machine_attribute_t Reply_mach_vm_machine_attribute;
	__Reply__mach_vm_remap_t Reply_mach_vm_remap;
	__Reply__mach_vm_page_query_t Reply_mach_vm_page_query;
	__Reply__mach_vm_region_recurse_t Reply_mach_vm_region_recurse;
	__Reply__mach_vm_region_t Reply_mach_vm_region;
	__Reply___mach_make_memory_entry_t Reply__mach_make_memory_entry;
	__Reply__mach_vm_purgable_control_t Reply_mach_vm_purgable_control;
	__Reply__mach_vm_page_info_t Reply_mach_vm_page_info;
};
#endif /* __RequestUnion__mach_vm_subsystem__defined */
/* Forward Declarations */


mig_internal novalue _Xmach_vm_protect
	(mach_msg_header_t *InHeadP, mach_msg_header_t *OutHeadP);

mig_internal novalue _Xmach_vm_inherit
	(mach_msg_header_t *InHeadP, mach_msg_header_t *OutHeadP);

mig_internal novalue _Xmach_vm_read
	(mach_msg_header_t *InHeadP, mach_msg_header_t *OutHeadP);

mig_internal novalue _Xmach_vm_read_list
	(mach_msg_header_t *InHeadP, mach_msg_header_t *OutHeadP);

mig_internal novalue _Xmach_vm_write
	(mach_msg_header_t *InHeadP, mach_msg_header_t *OutHeadP);

mig_internal novalue _Xmach_vm_copy
	(mach_msg_header_t *InHeadP, mach_msg_header_t *OutHeadP);

mig_internal novalue _Xmach_vm_read_overwrite
	(mach_msg_header_t *InHeadP, mach_msg_header_t *OutHeadP);

mig_internal novalue _Xmach_vm_msync
	(mach_msg_header_t *InHeadP, mach_msg_header_t *OutHeadP);

mig_internal novalue _Xmach_vm_behavior_set
	(mach_msg_header_t *InHeadP, mach_msg_header_t *OutHeadP);

mig_internal novalue _Xmach_vm_machine_attribute
	(mach_msg_header_t *InHeadP, mach_msg_header_t *OutHeadP);

mig_internal novalue _Xmach_vm_remap
	(mach_msg_header_t *InHeadP, mach_msg_header_t *OutHeadP);

mig_internal novalue _Xmach_vm_page_query
	(mach_msg_header_t *InHeadP, mach_msg_header_t *OutHeadP);

mig_internal novalue _Xmach_vm_region_recurse
	(mach_msg_header_t *InHeadP, mach_msg_header_t *OutHeadP);

mig_internal novalue _Xmach_vm_region
	(mach_msg_header_t *InHeadP, mach_msg_header_t *OutHeadP);

mig_internal novalue _X_mach_make_memory_entry
	(mach_msg_header_t *InHeadP, mach_msg_header_t *OutHeadP);

mig_internal novalue _Xmach_vm_purgable_control
	(mach_msg_header_t *InHeadP, mach_msg_header_t *OutHeadP);

mig_internal novalue _Xmach_vm_page_info
	(mach_msg_header_t *InHeadP, mach_msg_header_t *OutHeadP);


#if ( __MigTypeCheck )
#if __MIG_check__Request__mach_vm_subsystem__
#if !defined(__MIG_check__Request__mach_vm_protect_t__defined)
#define __MIG_check__Request__mach_vm_protect_t__defined

mig_internal kern_return_t __MIG_check__Request__mach_vm_protect_t(__attribute__((__unused__)) __Request__mach_vm_protect_t *In0P)
{

	typedef __Request__mach_vm_protect_t __Request;
#if	__MigTypeCheck
	if (!(In0P->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX) ||
	    (In0P->msgh_body.msgh_descriptor_count != 0) ||
	    (In0P->Head.msgh_size != (mach_msg_size_t)sizeof(__Request)))
		return MIG_BAD_ARGUMENTS;
#endif	/* __MigTypeCheck */

	return MACH_MSG_SUCCESS;
}
#endif /* !defined(__MIG_check__Request__mach_vm_protect_t__defined) */
#endif /* __MIG_check__Request__mach_vm_subsystem__ */
#endif /* ( __MigTypeCheck ) */


/* Routine mach_vm_protect */
#ifdef	mig_external
mig_external
#else
extern
#endif	/* mig_external */
kern_return_t mach_vm_protect
#if	defined(LINTLIBRARY)
    (target_task, address, size, set_maximum, new_protection)
	vm_map_t target_task;
	mach_vm_address_t address;
	mach_vm_size_t size;
	boolean_t set_maximum;
	vm_prot_t new_protection;
{ return mach_vm_protect(target_task, address, size, set_maximum, new_protection); }
#else
(
	vm_map_t target_task,
	mach_vm_address_t address,
	mach_vm_size_t size,
	boolean_t set_maximum,
	vm_prot_t new_protection
);
#endif	/* defined(LINTLIBRARY) */

/* Routine mach_vm_protect */
mig_internal novalue _Xmach_vm_protect
	(mach_msg_header_t *InHeadP, mach_msg_header_t *OutHeadP)
{

#ifdef  __MigPackStructs
#pragma pack(4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		/* start of the kernel processed data */
		mach_msg_body_t msgh_body;
		/* end of the kernel processed data */
		NDR_record_t NDR;
		mach_vm_address_t address;
		mach_vm_size_t size;
		boolean_t set_maximum;
		vm_prot_t new_protection;
		mach_msg_trailer_t trailer;
	} Request;
#ifdef  __MigPackStructs
#pragma pack()
#endif
	typedef __Request__mach_vm_protect_t __Request;
	typedef __Reply__mach_vm_protect_t Reply;

	/*
	 * typedef struct {
	 * 	mach_msg_header_t Head;
	 * 	NDR_record_t NDR;
	 * 	kern_return_t RetCode;
	 * } mig_reply_error_t;
	 */

	Request *In0P = (Request *) InHeadP;
	Reply *OutP = (Reply *) OutHeadP;
#ifdef	__MIG_check__Request__mach_vm_protect_t__defined
	kern_return_t check_result;
#endif	/* __MIG_check__Request__mach_vm_protect_t__defined */

#if	__MigKernelSpecificCode
#else
#endif /* __MigKernelSpecificCode */
	vm_map_t target_task;

	__DeclareRcvRpc(4802, "mach_vm_protect")
	__BeforeRcvRpc(4802, "mach_vm_protect")
/* RetCArg=0x0 rtSimpleRequest=0 */

#if	defined(__MIG_check__Request__mach_vm_protect_t__defined)
	check_result = __MIG_check__Request__mach_vm_protect_t((__Request *)In0P);
	if (check_result != MACH_MSG_SUCCESS)
		{ MIG_RETURN_ERROR(OutP, check_result); }
#endif	/* defined(__MIG_check__Request__mach_vm_protect_t__defined) */

	target_task = convert_port_entry_to_map(In0P->Head.msgh_request_port);

	OutP->RetCode = mach_vm_protect(target_task, In0P->address, In0P->size, In0P->set_maximum, In0P->new_protection);
	vm_map_deallocate(target_task);
#if	__MigKernelSpecificCode
#endif /* __MigKernelSpecificCode */

	OutP->NDR = NDR_record;


	__AfterRcvRpc(4802, "mach_vm_protect")
}

#if ( __MigTypeCheck )
#if __MIG_check__Request__mach_vm_subsystem__
#if !defined(__MIG_check__Request__mach_vm_inherit_t__defined)
#define __MIG_check__Request__mach_vm_inherit_t__defined

mig_internal kern_return_t __MIG_check__Request__mach_vm_inherit_t(__attribute__((__unused__)) __Request__mach_vm_inherit_t *In0P)
{

	typedef __Request__mach_vm_inherit_t __Request;
#if	__MigTypeCheck
	if (!(In0P->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX) ||
	    (In0P->msgh_body.msgh_descriptor_count != 0) ||
	    (In0P->Head.msgh_size != (mach_msg_size_t)sizeof(__Request)))
		return MIG_BAD_ARGUMENTS;
#endif	/* __MigTypeCheck */

	return MACH_MSG_SUCCESS;
}
#endif /* !defined(__MIG_check__Request__mach_vm_inherit_t__defined) */
#endif /* __MIG_check__Request__mach_vm_subsystem__ */
#endif /* ( __MigTypeCheck ) */


/* Routine mach_vm_inherit */
#ifdef	mig_external
mig_external
#else
extern
#endif	/* mig_external */
kern_return_t mach_vm_inherit
#if	defined(LINTLIBRARY)
    (target_task, address, size, new_inheritance)
	vm_map_t target_task;
	mach_vm_address_t address;
	mach_vm_size_t size;
	vm_inherit_t new_inheritance;
{ return mach_vm_inherit(target_task, address, size, new_inheritance); }
#else
(
	vm_map_t target_task,
	mach_vm_address_t address,
	mach_vm_size_t size,
	vm_inherit_t new_inheritance
);
#endif	/* defined(LINTLIBRARY) */

/* Routine mach_vm_inherit */
mig_internal novalue _Xmach_vm_inherit
	(mach_msg_header_t *InHeadP, mach_msg_header_t *OutHeadP)
{

#ifdef  __MigPackStructs
#pragma pack(4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		/* start of the kernel processed data */
		mach_msg_body_t msgh_body;
		/* end of the kernel processed data */
		NDR_record_t NDR;
		mach_vm_address_t address;
		mach_vm_size_t size;
		vm_inherit_t new_inheritance;
		mach_msg_trailer_t trailer;
	} Request;
#ifdef  __MigPackStructs
#pragma pack()
#endif
	typedef __Request__mach_vm_inherit_t __Request;
	typedef __Reply__mach_vm_inherit_t Reply;

	/*
	 * typedef struct {
	 * 	mach_msg_header_t Head;
	 * 	NDR_record_t NDR;
	 * 	kern_return_t RetCode;
	 * } mig_reply_error_t;
	 */

	Request *In0P = (Request *) InHeadP;
	Reply *OutP = (Reply *) OutHeadP;
#ifdef	__MIG_check__Request__mach_vm_inherit_t__defined
	kern_return_t check_result;
#endif	/* __MIG_check__Request__mach_vm_inherit_t__defined */

#if	__MigKernelSpecificCode
#else
#endif /* __MigKernelSpecificCode */
	vm_map_t target_task;

	__DeclareRcvRpc(4803, "mach_vm_inherit")
	__BeforeRcvRpc(4803, "mach_vm_inherit")
/* RetCArg=0x0 rtSimpleRequest=0 */

#if	defined(__MIG_check__Request__mach_vm_inherit_t__defined)
	check_result = __MIG_check__Request__mach_vm_inherit_t((__Request *)In0P);
	if (check_result != MACH_MSG_SUCCESS)
		{ MIG_RETURN_ERROR(OutP, check_result); }
#endif	/* defined(__MIG_check__Request__mach_vm_inherit_t__defined) */

	target_task = convert_port_entry_to_map(In0P->Head.msgh_request_port);

	OutP->RetCode = mach_vm_inherit(target_task, In0P->address, In0P->size, In0P->new_inheritance);
	vm_map_deallocate(target_task);
#if	__MigKernelSpecificCode
#endif /* __MigKernelSpecificCode */

	OutP->NDR = NDR_record;


	__AfterRcvRpc(4803, "mach_vm_inherit")
}

#if ( __MigTypeCheck )
#if __MIG_check__Request__mach_vm_subsystem__
#if !defined(__MIG_check__Request__mach_vm_read_t__defined)
#define __MIG_check__Request__mach_vm_read_t__defined

mig_internal kern_return_t __MIG_check__Request__mach_vm_read_t(__attribute__((__unused__)) __Request__mach_vm_read_t *In0P)
{

	typedef __Request__mach_vm_read_t __Request;
#if	__MigTypeCheck
	if (!(In0P->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX) ||
	    (In0P->msgh_body.msgh_descriptor_count != 0) ||
	    (In0P->Head.msgh_size != (mach_msg_size_t)sizeof(__Request)))
		return MIG_BAD_ARGUMENTS;
#endif	/* __MigTypeCheck */

	return MACH_MSG_SUCCESS;
}
#endif /* !defined(__MIG_check__Request__mach_vm_read_t__defined) */
#endif /* __MIG_check__Request__mach_vm_subsystem__ */
#endif /* ( __MigTypeCheck ) */


/* Routine mach_vm_read */
#ifdef	mig_external
mig_external
#else
extern
#endif	/* mig_external */
kern_return_t mach_vm_read
#if	defined(LINTLIBRARY)
    (target_task, address, size, data, dataCnt)
	vm_map_t target_task;
	mach_vm_address_t address;
	mach_vm_size_t size;
	vm_offset_t *data;
	mach_msg_type_number_t *dataCnt;
{ return mach_vm_read(target_task, address, size, data, dataCnt); }
#else
(
	vm_map_t target_task,
	mach_vm_address_t address,
	mach_vm_size_t size,
	vm_offset_t *data,
	mach_msg_type_number_t *dataCnt
);
#endif	/* defined(LINTLIBRARY) */

/* Routine mach_vm_read */
mig_internal novalue _Xmach_vm_read
	(mach_msg_header_t *InHeadP, mach_msg_header_t *OutHeadP)
{

#ifdef  __MigPackStructs
#pragma pack(4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		/* start of the kernel processed data */
		mach_msg_body_t msgh_body;
		/* end of the kernel processed data */
		NDR_record_t NDR;
		mach_vm_address_t address;
		mach_vm_size_t size;
		mach_msg_trailer_t trailer;
	} Request;
#ifdef  __MigPackStructs
#pragma pack()
#endif
	typedef __Request__mach_vm_read_t __Request;
	typedef __Reply__mach_vm_read_t Reply;

	/*
	 * typedef struct {
	 * 	mach_msg_header_t Head;
	 * 	NDR_record_t NDR;
	 * 	kern_return_t RetCode;
	 * } mig_reply_error_t;
	 */

	Request *In0P = (Request *) InHeadP;
	Reply *OutP = (Reply *) OutHeadP;
#ifdef	__MIG_check__Request__mach_vm_read_t__defined
	kern_return_t check_result;
#endif	/* __MIG_check__Request__mach_vm_read_t__defined */

#if	__MigKernelSpecificCode
#if	UseStaticTemplates
	const static mach_msg_ool_descriptor_t dataTemplate = {
		.address = (void *)0,
		.size = 0,
		.deallocate = FALSE,
		.copy = MACH_MSG_VIRTUAL_COPY,
		.type = MACH_MSG_OOL_DESCRIPTOR,
	};
#endif	/* UseStaticTemplates */

#else
#if	UseStaticTemplates
	const static mach_msg_ool_descriptor_t dataTemplate = {
		.address = (void *)0,
		.size = 0,
		.deallocate = FALSE,
		.copy = MACH_MSG_VIRTUAL_COPY,
		.type = MACH_MSG_OOL_DESCRIPTOR,
	};
#endif	/* UseStaticTemplates */

#endif /* __MigKernelSpecificCode */
	kern_return_t RetCode;
	vm_map_t target_task;

	__DeclareRcvRpc(4804, "mach_vm_read")
	__BeforeRcvRpc(4804, "mach_vm_read")
/* RetCArg=0x0 rtSimpleRequest=0 */

#if	defined(__MIG_check__Request__mach_vm_read_t__defined)
	check_result = __MIG_check__Request__mach_vm_read_t((__Request *)In0P);
	if (check_result != MACH_MSG_SUCCESS)
		{ MIG_RETURN_ERROR(OutP, check_result); }
#endif	/* defined(__MIG_check__Request__mach_vm_read_t__defined) */

#if	UseStaticTemplates
	OutP->data = dataTemplate;
#else	/* UseStaticTemplates */
	OutP->data.deallocate =  FALSE;
	OutP->data.copy = MACH_MSG_VIRTUAL_COPY;
	OutP->data.type = MACH_MSG_OOL_DESCRIPTOR;
#endif	/* UseStaticTemplates */


	target_task = convert_port_to_map(In0P->Head.msgh_request_port);

	RetCode = mach_vm_read(target_task, In0P->address, In0P->size, (vm_offset_t *)&(OutP->data.address), &OutP->dataCnt);
	vm_map_deallocate(target_task);
	if (RetCode != KERN_SUCCESS) {
		MIG_RETURN_ERROR(OutP, RetCode);
	}
#if	__MigKernelSpecificCode
#endif /* __MigKernelSpecificCode */
	OutP->data.size = OutP->dataCnt;


	OutP->NDR = NDR_record;


	OutP->Head.msgh_bits |= MACH_MSGH_BITS_COMPLEX;
	OutP->Head.msgh_size = (sizeof(Reply));
	OutP->msgh_body.msgh_descriptor_count = 1;
	__AfterRcvRpc(4804, "mach_vm_read")
}

#if ( __MigTypeCheck )
#if __MIG_check__Request__mach_vm_subsystem__
#if !defined(__MIG_check__Request__mach_vm_read_list_t__defined)
#define __MIG_check__Request__mach_vm_read_list_t__defined

mig_internal kern_return_t __MIG_check__Request__mach_vm_read_list_t(__attribute__((__unused__)) __Request__mach_vm_read_list_t *In0P)
{

	typedef __Request__mach_vm_read_list_t __Request;
#if	__MigTypeCheck
	if (!(In0P->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX) ||
	    (In0P->msgh_body.msgh_descriptor_count != 0) ||
	    (In0P->Head.msgh_size != (mach_msg_size_t)sizeof(__Request)))
		return MIG_BAD_ARGUMENTS;
#endif	/* __MigTypeCheck */

	return MACH_MSG_SUCCESS;
}
#endif /* !defined(__MIG_check__Request__mach_vm_read_list_t__defined) */
#endif /* __MIG_check__Request__mach_vm_subsystem__ */
#endif /* ( __MigTypeCheck ) */


/* Routine mach_vm_read_list */
#ifdef	mig_external
mig_external
#else
extern
#endif	/* mig_external */
kern_return_t mach_vm_read_list
#if	defined(LINTLIBRARY)
    (target_task, data_list, count)
	vm_map_t target_task;
	mach_vm_read_entry_t data_list;
	natural_t count;
{ return mach_vm_read_list(target_task, data_list, count); }
#else
(
	vm_map_t target_task,
	mach_vm_read_entry_t data_list,
	natural_t count
);
#endif	/* defined(LINTLIBRARY) */

/* Routine mach_vm_read_list */
mig_internal novalue _Xmach_vm_read_list
	(mach_msg_header_t *InHeadP, mach_msg_header_t *OutHeadP)
{

#ifdef  __MigPackStructs
#pragma pack(4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		/* start of the kernel processed data */
		mach_msg_body_t msgh_body;
		/* end of the kernel processed data */
		NDR_record_t NDR;
		mach_vm_read_entry_t data_list;
		natural_t count;
		mach_msg_trailer_t trailer;
	} Request;
#ifdef  __MigPackStructs
#pragma pack()
#endif
	typedef __Request__mach_vm_read_list_t __Request;
	typedef __Reply__mach_vm_read_list_t Reply;

	/*
	 * typedef struct {
	 * 	mach_msg_header_t Head;
	 * 	NDR_record_t NDR;
	 * 	kern_return_t RetCode;
	 * } mig_reply_error_t;
	 */

	Request *In0P = (Request *) InHeadP;
	Reply *OutP = (Reply *) OutHeadP;
#ifdef	__MIG_check__Request__mach_vm_read_list_t__defined
	kern_return_t check_result;
#endif	/* __MIG_check__Request__mach_vm_read_list_t__defined */

#if	__MigKernelSpecificCode
#else
#endif /* __MigKernelSpecificCode */
	vm_map_t target_task;

	__DeclareRcvRpc(4805, "mach_vm_read_list")
	__BeforeRcvRpc(4805, "mach_vm_read_list")
/* RetCArg=0x0 rtSimpleRequest=0 */

#if	defined(__MIG_check__Request__mach_vm_read_list_t__defined)
	check_result = __MIG_check__Request__mach_vm_read_list_t((__Request *)In0P);
	if (check_result != MACH_MSG_SUCCESS)
		{ MIG_RETURN_ERROR(OutP, check_result); }
#endif	/* defined(__MIG_check__Request__mach_vm_read_list_t__defined) */

	target_task = convert_port_to_map(In0P->Head.msgh_request_port);

	OutP->RetCode = mach_vm_read_list(target_task, In0P->data_list, In0P->count);
	vm_map_deallocate(target_task);
	if (OutP->RetCode != KERN_SUCCESS) {
		MIG_RETURN_ERROR(OutP, OutP->RetCode);
	}
#if	__MigKernelSpecificCode
#endif /* __MigKernelSpecificCode */

	OutP->NDR = NDR_record;


	{   typedef struct { char data[4096]; } *sp;
	    * (sp) OutP->data_list = * (sp) In0P->data_list;
	}

	OutP->Head.msgh_size = (sizeof(Reply));
	__AfterRcvRpc(4805, "mach_vm_read_list")
}

#if ( __MigTypeCheck )
#if __MIG_check__Request__mach_vm_subsystem__
#if !defined(__MIG_check__Request__mach_vm_write_t__defined)
#define __MIG_check__Request__mach_vm_write_t__defined

mig_internal kern_return_t __MIG_check__Request__mach_vm_write_t(__attribute__((__unused__)) __Request__mach_vm_write_t *In0P)
{

	typedef __Request__mach_vm_write_t __Request;
#if	__MigTypeCheck
	if (!(In0P->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX) ||
	    (In0P->msgh_body.msgh_descriptor_count != 1) ||
	    (In0P->Head.msgh_size != (mach_msg_size_t)sizeof(__Request)))
		return MIG_BAD_ARGUMENTS;
#endif	/* __MigTypeCheck */

#if	__MigTypeCheck
	if (In0P->data.type != MACH_MSG_OOL_DESCRIPTOR)
		return MIG_TYPE_ERROR;
#endif	/* __MigTypeCheck */

#if __MigTypeCheck
	if (In0P->data.size != In0P->dataCnt)
		return MIG_TYPE_ERROR;
#endif	/* __MigTypeCheck */

	return MACH_MSG_SUCCESS;
}
#endif /* !defined(__MIG_check__Request__mach_vm_write_t__defined) */
#endif /* __MIG_check__Request__mach_vm_subsystem__ */
#endif /* ( __MigTypeCheck ) */


/* Routine mach_vm_write */
#ifdef	mig_external
mig_external
#else
extern
#endif	/* mig_external */
kern_return_t mach_vm_write
#if	defined(LINTLIBRARY)
    (target_task, address, data, dataCnt)
	vm_map_t target_task;
	mach_vm_address_t address;
	vm_offset_t data;
	mach_msg_type_number_t dataCnt;
{ return mach_vm_write(target_task, address, data, dataCnt); }
#else
(
	vm_map_t target_task,
	mach_vm_address_t address,
	vm_offset_t data,
	mach_msg_type_number_t dataCnt
);
#endif	/* defined(LINTLIBRARY) */

/* Routine mach_vm_write */
mig_internal novalue _Xmach_vm_write
	(mach_msg_header_t *InHeadP, mach_msg_header_t *OutHeadP)
{

#ifdef  __MigPackStructs
#pragma pack(4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		/* start of the kernel processed data */
		mach_msg_body_t msgh_body;
		mach_msg_ool_descriptor_t data;
		/* end of the kernel processed data */
		NDR_record_t NDR;
		mach_vm_address_t address;
		mach_msg_type_number_t dataCnt;
		mach_msg_trailer_t trailer;
	} Request;
#ifdef  __MigPackStructs
#pragma pack()
#endif
	typedef __Request__mach_vm_write_t __Request;
	typedef __Reply__mach_vm_write_t Reply;

	/*
	 * typedef struct {
	 * 	mach_msg_header_t Head;
	 * 	NDR_record_t NDR;
	 * 	kern_return_t RetCode;
	 * } mig_reply_error_t;
	 */

	Request *In0P = (Request *) InHeadP;
	Reply *OutP = (Reply *) OutHeadP;
#ifdef	__MIG_check__Request__mach_vm_write_t__defined
	kern_return_t check_result;
#endif	/* __MIG_check__Request__mach_vm_write_t__defined */

#if	__MigKernelSpecificCode
#else
#endif /* __MigKernelSpecificCode */
	vm_map_t target_task;

	__DeclareRcvRpc(4806, "mach_vm_write")
	__BeforeRcvRpc(4806, "mach_vm_write")
/* RetCArg=0x0 rtSimpleRequest=0 */

#if	defined(__MIG_check__Request__mach_vm_write_t__defined)
	check_result = __MIG_check__Request__mach_vm_write_t((__Request *)In0P);
	if (check_result != MACH_MSG_SUCCESS)
		{ MIG_RETURN_ERROR(OutP, check_result); }
#endif	/* defined(__MIG_check__Request__mach_vm_write_t__defined) */

	target_task = convert_port_to_map(In0P->Head.msgh_request_port);

	OutP->RetCode = mach_vm_write(target_task, In0P->address, (vm_offset_t)(In0P->data.address), In0P->dataCnt);
	vm_map_deallocate(target_task);
#if	__MigKernelSpecificCode
#endif /* __MigKernelSpecificCode */

	OutP->NDR = NDR_record;


	__AfterRcvRpc(4806, "mach_vm_write")
}

#if ( __MigTypeCheck )
#if __MIG_check__Request__mach_vm_subsystem__
#if !defined(__MIG_check__Request__mach_vm_copy_t__defined)
#define __MIG_check__Request__mach_vm_copy_t__defined

mig_internal kern_return_t __MIG_check__Request__mach_vm_copy_t(__attribute__((__unused__)) __Request__mach_vm_copy_t *In0P)
{

	typedef __Request__mach_vm_copy_t __Request;
#if	__MigTypeCheck
	if (!(In0P->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX) ||
	    (In0P->msgh_body.msgh_descriptor_count != 0) ||
	    (In0P->Head.msgh_size != (mach_msg_size_t)sizeof(__Request)))
		return MIG_BAD_ARGUMENTS;
#endif	/* __MigTypeCheck */

	return MACH_MSG_SUCCESS;
}
#endif /* !defined(__MIG_check__Request__mach_vm_copy_t__defined) */
#endif /* __MIG_check__Request__mach_vm_subsystem__ */
#endif /* ( __MigTypeCheck ) */


/* Routine mach_vm_copy */
#ifdef	mig_external
mig_external
#else
extern
#endif	/* mig_external */
kern_return_t mach_vm_copy
#if	defined(LINTLIBRARY)
    (target_task, source_address, size, dest_address)
	vm_map_t target_task;
	mach_vm_address_t source_address;
	mach_vm_size_t size;
	mach_vm_address_t dest_address;
{ return mach_vm_copy(target_task, source_address, size, dest_address); }
#else
(
	vm_map_t target_task,
	mach_vm_address_t source_address,
	mach_vm_size_t size,
	mach_vm_address_t dest_address
);
#endif	/* defined(LINTLIBRARY) */

/* Routine mach_vm_copy */
mig_internal novalue _Xmach_vm_copy
	(mach_msg_header_t *InHeadP, mach_msg_header_t *OutHeadP)
{

#ifdef  __MigPackStructs
#pragma pack(4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		/* start of the kernel processed data */
		mach_msg_body_t msgh_body;
		/* end of the kernel processed data */
		NDR_record_t NDR;
		mach_vm_address_t source_address;
		mach_vm_size_t size;
		mach_vm_address_t dest_address;
		mach_msg_trailer_t trailer;
	} Request;
#ifdef  __MigPackStructs
#pragma pack()
#endif
	typedef __Request__mach_vm_copy_t __Request;
	typedef __Reply__mach_vm_copy_t Reply;

	/*
	 * typedef struct {
	 * 	mach_msg_header_t Head;
	 * 	NDR_record_t NDR;
	 * 	kern_return_t RetCode;
	 * } mig_reply_error_t;
	 */

	Request *In0P = (Request *) InHeadP;
	Reply *OutP = (Reply *) OutHeadP;
#ifdef	__MIG_check__Request__mach_vm_copy_t__defined
	kern_return_t check_result;
#endif	/* __MIG_check__Request__mach_vm_copy_t__defined */

#if	__MigKernelSpecificCode
#else
#endif /* __MigKernelSpecificCode */
	vm_map_t target_task;

	__DeclareRcvRpc(4807, "mach_vm_copy")
	__BeforeRcvRpc(4807, "mach_vm_copy")
/* RetCArg=0x0 rtSimpleRequest=0 */

#if	defined(__MIG_check__Request__mach_vm_copy_t__defined)
	check_result = __MIG_check__Request__mach_vm_copy_t((__Request *)In0P);
	if (check_result != MACH_MSG_SUCCESS)
		{ MIG_RETURN_ERROR(OutP, check_result); }
#endif	/* defined(__MIG_check__Request__mach_vm_copy_t__defined) */

	target_task = convert_port_to_map(In0P->Head.msgh_request_port);

	OutP->RetCode = mach_vm_copy(target_task, In0P->source_address, In0P->size, In0P->dest_address);
	vm_map_deallocate(target_task);
#if	__MigKernelSpecificCode
#endif /* __MigKernelSpecificCode */

	OutP->NDR = NDR_record;


	__AfterRcvRpc(4807, "mach_vm_copy")
}

#if ( __MigTypeCheck )
#if __MIG_check__Request__mach_vm_subsystem__
#if !defined(__MIG_check__Request__mach_vm_read_overwrite_t__defined)
#define __MIG_check__Request__mach_vm_read_overwrite_t__defined

mig_internal kern_return_t __MIG_check__Request__mach_vm_read_overwrite_t(__attribute__((__unused__)) __Request__mach_vm_read_overwrite_t *In0P)
{

	typedef __Request__mach_vm_read_overwrite_t __Request;
#if	__MigTypeCheck
	if (!(In0P->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX) ||
	    (In0P->msgh_body.msgh_descriptor_count != 0) ||
	    (In0P->Head.msgh_size != (mach_msg_size_t)sizeof(__Request)))
		return MIG_BAD_ARGUMENTS;
#endif	/* __MigTypeCheck */

	return MACH_MSG_SUCCESS;
}
#endif /* !defined(__MIG_check__Request__mach_vm_read_overwrite_t__defined) */
#endif /* __MIG_check__Request__mach_vm_subsystem__ */
#endif /* ( __MigTypeCheck ) */


/* Routine mach_vm_read_overwrite */
#ifdef	mig_external
mig_external
#else
extern
#endif	/* mig_external */
kern_return_t mach_vm_read_overwrite
#if	defined(LINTLIBRARY)
    (target_task, address, size, data, outsize)
	vm_map_t target_task;
	mach_vm_address_t address;
	mach_vm_size_t size;
	mach_vm_address_t data;
	mach_vm_size_t *outsize;
{ return mach_vm_read_overwrite(target_task, address, size, data, outsize); }
#else
(
	vm_map_t target_task,
	mach_vm_address_t address,
	mach_vm_size_t size,
	mach_vm_address_t data,
	mach_vm_size_t *outsize
);
#endif	/* defined(LINTLIBRARY) */

/* Routine mach_vm_read_overwrite */
mig_internal novalue _Xmach_vm_read_overwrite
	(mach_msg_header_t *InHeadP, mach_msg_header_t *OutHeadP)
{

#ifdef  __MigPackStructs
#pragma pack(4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		/* start of the kernel processed data */
		mach_msg_body_t msgh_body;
		/* end of the kernel processed data */
		NDR_record_t NDR;
		mach_vm_address_t address;
		mach_vm_size_t size;
		mach_vm_address_t data;
		mach_msg_trailer_t trailer;
	} Request;
#ifdef  __MigPackStructs
#pragma pack()
#endif
	typedef __Request__mach_vm_read_overwrite_t __Request;
	typedef __Reply__mach_vm_read_overwrite_t Reply;

	/*
	 * typedef struct {
	 * 	mach_msg_header_t Head;
	 * 	NDR_record_t NDR;
	 * 	kern_return_t RetCode;
	 * } mig_reply_error_t;
	 */

	Request *In0P = (Request *) InHeadP;
	Reply *OutP = (Reply *) OutHeadP;
#ifdef	__MIG_check__Request__mach_vm_read_overwrite_t__defined
	kern_return_t check_result;
#endif	/* __MIG_check__Request__mach_vm_read_overwrite_t__defined */

#if	__MigKernelSpecificCode
#else
#endif /* __MigKernelSpecificCode */
	vm_map_t target_task;

	__DeclareRcvRpc(4808, "mach_vm_read_overwrite")
	__BeforeRcvRpc(4808, "mach_vm_read_overwrite")
/* RetCArg=0x0 rtSimpleRequest=0 */

#if	defined(__MIG_check__Request__mach_vm_read_overwrite_t__defined)
	check_result = __MIG_check__Request__mach_vm_read_overwrite_t((__Request *)In0P);
	if (check_result != MACH_MSG_SUCCESS)
		{ MIG_RETURN_ERROR(OutP, check_result); }
#endif	/* defined(__MIG_check__Request__mach_vm_read_overwrite_t__defined) */

	target_task = convert_port_to_map(In0P->Head.msgh_request_port);

	OutP->RetCode = mach_vm_read_overwrite(target_task, In0P->address, In0P->size, In0P->data, &OutP->outsize);
	vm_map_deallocate(target_task);
	if (OutP->RetCode != KERN_SUCCESS) {
		MIG_RETURN_ERROR(OutP, OutP->RetCode);
	}
#if	__MigKernelSpecificCode
#endif /* __MigKernelSpecificCode */

	OutP->NDR = NDR_record;


	OutP->Head.msgh_size = (sizeof(Reply));
	__AfterRcvRpc(4808, "mach_vm_read_overwrite")
}

#if ( __MigTypeCheck )
#if __MIG_check__Request__mach_vm_subsystem__
#if !defined(__MIG_check__Request__mach_vm_msync_t__defined)
#define __MIG_check__Request__mach_vm_msync_t__defined

mig_internal kern_return_t __MIG_check__Request__mach_vm_msync_t(__attribute__((__unused__)) __Request__mach_vm_msync_t *In0P)
{

	typedef __Request__mach_vm_msync_t __Request;
#if	__MigTypeCheck
	if (!(In0P->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX) ||
	    (In0P->msgh_body.msgh_descriptor_count != 0) ||
	    (In0P->Head.msgh_size != (mach_msg_size_t)sizeof(__Request)))
		return MIG_BAD_ARGUMENTS;
#endif	/* __MigTypeCheck */

	return MACH_MSG_SUCCESS;
}
#endif /* !defined(__MIG_check__Request__mach_vm_msync_t__defined) */
#endif /* __MIG_check__Request__mach_vm_subsystem__ */
#endif /* ( __MigTypeCheck ) */


/* Routine mach_vm_msync */
#ifdef	mig_external
mig_external
#else
extern
#endif	/* mig_external */
kern_return_t mach_vm_msync
#if	defined(LINTLIBRARY)
    (target_task, address, size, sync_flags)
	vm_map_t target_task;
	mach_vm_address_t address;
	mach_vm_size_t size;
	vm_sync_t sync_flags;
{ return mach_vm_msync(target_task, address, size, sync_flags); }
#else
(
	vm_map_t target_task,
	mach_vm_address_t address,
	mach_vm_size_t size,
	vm_sync_t sync_flags
);
#endif	/* defined(LINTLIBRARY) */

/* Routine mach_vm_msync */
mig_internal novalue _Xmach_vm_msync
	(mach_msg_header_t *InHeadP, mach_msg_header_t *OutHeadP)
{

#ifdef  __MigPackStructs
#pragma pack(4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		/* start of the kernel processed data */
		mach_msg_body_t msgh_body;
		/* end of the kernel processed data */
		NDR_record_t NDR;
		mach_vm_address_t address;
		mach_vm_size_t size;
		vm_sync_t sync_flags;
		mach_msg_trailer_t trailer;
	} Request;
#ifdef  __MigPackStructs
#pragma pack()
#endif
	typedef __Request__mach_vm_msync_t __Request;
	typedef __Reply__mach_vm_msync_t Reply;

	/*
	 * typedef struct {
	 * 	mach_msg_header_t Head;
	 * 	NDR_record_t NDR;
	 * 	kern_return_t RetCode;
	 * } mig_reply_error_t;
	 */

	Request *In0P = (Request *) InHeadP;
	Reply *OutP = (Reply *) OutHeadP;
#ifdef	__MIG_check__Request__mach_vm_msync_t__defined
	kern_return_t check_result;
#endif	/* __MIG_check__Request__mach_vm_msync_t__defined */

#if	__MigKernelSpecificCode
#else
#endif /* __MigKernelSpecificCode */
	vm_map_t target_task;

	__DeclareRcvRpc(4809, "mach_vm_msync")
	__BeforeRcvRpc(4809, "mach_vm_msync")
/* RetCArg=0x0 rtSimpleRequest=0 */

#if	defined(__MIG_check__Request__mach_vm_msync_t__defined)
	check_result = __MIG_check__Request__mach_vm_msync_t((__Request *)In0P);
	if (check_result != MACH_MSG_SUCCESS)
		{ MIG_RETURN_ERROR(OutP, check_result); }
#endif	/* defined(__MIG_check__Request__mach_vm_msync_t__defined) */

	target_task = convert_port_to_map(In0P->Head.msgh_request_port);

	OutP->RetCode = mach_vm_msync(target_task, In0P->address, In0P->size, In0P->sync_flags);
	vm_map_deallocate(target_task);
#if	__MigKernelSpecificCode
#endif /* __MigKernelSpecificCode */

	OutP->NDR = NDR_record;


	__AfterRcvRpc(4809, "mach_vm_msync")
}

#if ( __MigTypeCheck )
#if __MIG_check__Request__mach_vm_subsystem__
#if !defined(__MIG_check__Request__mach_vm_behavior_set_t__defined)
#define __MIG_check__Request__mach_vm_behavior_set_t__defined

mig_internal kern_return_t __MIG_check__Request__mach_vm_behavior_set_t(__attribute__((__unused__)) __Request__mach_vm_behavior_set_t *In0P)
{

	typedef __Request__mach_vm_behavior_set_t __Request;
#if	__MigTypeCheck
	if (!(In0P->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX) ||
	    (In0P->msgh_body.msgh_descriptor_count != 0) ||
	    (In0P->Head.msgh_size != (mach_msg_size_t)sizeof(__Request)))
		return MIG_BAD_ARGUMENTS;
#endif	/* __MigTypeCheck */

	return MACH_MSG_SUCCESS;
}
#endif /* !defined(__MIG_check__Request__mach_vm_behavior_set_t__defined) */
#endif /* __MIG_check__Request__mach_vm_subsystem__ */
#endif /* ( __MigTypeCheck ) */


/* Routine mach_vm_behavior_set */
#ifdef	mig_external
mig_external
#else
extern
#endif	/* mig_external */
kern_return_t mach_vm_behavior_set
#if	defined(LINTLIBRARY)
    (target_task, address, size, new_behavior)
	vm_map_t target_task;
	mach_vm_address_t address;
	mach_vm_size_t size;
	vm_behavior_t new_behavior;
{ return mach_vm_behavior_set(target_task, address, size, new_behavior); }
#else
(
	vm_map_t target_task,
	mach_vm_address_t address,
	mach_vm_size_t size,
	vm_behavior_t new_behavior
);
#endif	/* defined(LINTLIBRARY) */

/* Routine mach_vm_behavior_set */
mig_internal novalue _Xmach_vm_behavior_set
	(mach_msg_header_t *InHeadP, mach_msg_header_t *OutHeadP)
{

#ifdef  __MigPackStructs
#pragma pack(4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		/* start of the kernel processed data */
		mach_msg_body_t msgh_body;
		/* end of the kernel processed data */
		NDR_record_t NDR;
		mach_vm_address_t address;
		mach_vm_size_t size;
		vm_behavior_t new_behavior;
		mach_msg_trailer_t trailer;
	} Request;
#ifdef  __MigPackStructs
#pragma pack()
#endif
	typedef __Request__mach_vm_behavior_set_t __Request;
	typedef __Reply__mach_vm_behavior_set_t Reply;

	/*
	 * typedef struct {
	 * 	mach_msg_header_t Head;
	 * 	NDR_record_t NDR;
	 * 	kern_return_t RetCode;
	 * } mig_reply_error_t;
	 */

	Request *In0P = (Request *) InHeadP;
	Reply *OutP = (Reply *) OutHeadP;
#ifdef	__MIG_check__Request__mach_vm_behavior_set_t__defined
	kern_return_t check_result;
#endif	/* __MIG_check__Request__mach_vm_behavior_set_t__defined */

#if	__MigKernelSpecificCode
#else
#endif /* __MigKernelSpecificCode */
	vm_map_t target_task;

	__DeclareRcvRpc(4810, "mach_vm_behavior_set")
	__BeforeRcvRpc(4810, "mach_vm_behavior_set")
/* RetCArg=0x0 rtSimpleRequest=0 */

#if	defined(__MIG_check__Request__mach_vm_behavior_set_t__defined)
	check_result = __MIG_check__Request__mach_vm_behavior_set_t((__Request *)In0P);
	if (check_result != MACH_MSG_SUCCESS)
		{ MIG_RETURN_ERROR(OutP, check_result); }
#endif	/* defined(__MIG_check__Request__mach_vm_behavior_set_t__defined) */

	target_task = convert_port_to_map(In0P->Head.msgh_request_port);

	OutP->RetCode = mach_vm_behavior_set(target_task, In0P->address, In0P->size, In0P->new_behavior);
	vm_map_deallocate(target_task);
#if	__MigKernelSpecificCode
#endif /* __MigKernelSpecificCode */

	OutP->NDR = NDR_record;


	__AfterRcvRpc(4810, "mach_vm_behavior_set")
}

#if ( __MigTypeCheck )
#if __MIG_check__Request__mach_vm_subsystem__
#if !defined(__MIG_check__Request__mach_vm_machine_attribute_t__defined)
#define __MIG_check__Request__mach_vm_machine_attribute_t__defined

mig_internal kern_return_t __MIG_check__Request__mach_vm_machine_attribute_t(__attribute__((__unused__)) __Request__mach_vm_machine_attribute_t *In0P)
{

	typedef __Request__mach_vm_machine_attribute_t __Request;
#if	__MigTypeCheck
	if (!(In0P->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX) ||
	    (In0P->msgh_body.msgh_descriptor_count != 0) ||
	    (In0P->Head.msgh_size != (mach_msg_size_t)sizeof(__Request)))
		return MIG_BAD_ARGUMENTS;
#endif	/* __MigTypeCheck */

	return MACH_MSG_SUCCESS;
}
#endif /* !defined(__MIG_check__Request__mach_vm_machine_attribute_t__defined) */
#endif /* __MIG_check__Request__mach_vm_subsystem__ */
#endif /* ( __MigTypeCheck ) */


/* Routine mach_vm_machine_attribute */
#ifdef	mig_external
mig_external
#else
extern
#endif	/* mig_external */
kern_return_t mach_vm_machine_attribute
#if	defined(LINTLIBRARY)
    (target_task, address, size, attribute, value)
	vm_map_t target_task;
	mach_vm_address_t address;
	mach_vm_size_t size;
	vm_machine_attribute_t attribute;
	vm_machine_attribute_val_t *value;
{ return mach_vm_machine_attribute(target_task, address, size, attribute, value); }
#else
(
	vm_map_t target_task,
	mach_vm_address_t address,
	mach_vm_size_t size,
	vm_machine_attribute_t attribute,
	vm_machine_attribute_val_t *value
);
#endif	/* defined(LINTLIBRARY) */

/* Routine mach_vm_machine_attribute */
mig_internal novalue _Xmach_vm_machine_attribute
	(mach_msg_header_t *InHeadP, mach_msg_header_t *OutHeadP)
{

#ifdef  __MigPackStructs
#pragma pack(4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		/* start of the kernel processed data */
		mach_msg_body_t msgh_body;
		/* end of the kernel processed data */
		NDR_record_t NDR;
		mach_vm_address_t address;
		mach_vm_size_t size;
		vm_machine_attribute_t attribute;
		vm_machine_attribute_val_t value;
		mach_msg_trailer_t trailer;
	} Request;
#ifdef  __MigPackStructs
#pragma pack()
#endif
	typedef __Request__mach_vm_machine_attribute_t __Request;
	typedef __Reply__mach_vm_machine_attribute_t Reply;

	/*
	 * typedef struct {
	 * 	mach_msg_header_t Head;
	 * 	NDR_record_t NDR;
	 * 	kern_return_t RetCode;
	 * } mig_reply_error_t;
	 */

	Request *In0P = (Request *) InHeadP;
	Reply *OutP = (Reply *) OutHeadP;
#ifdef	__MIG_check__Request__mach_vm_machine_attribute_t__defined
	kern_return_t check_result;
#endif	/* __MIG_check__Request__mach_vm_machine_attribute_t__defined */

#if	__MigKernelSpecificCode
#else
#endif /* __MigKernelSpecificCode */
	vm_map_t target_task;

	__DeclareRcvRpc(4812, "mach_vm_machine_attribute")
	__BeforeRcvRpc(4812, "mach_vm_machine_attribute")
/* RetCArg=0x0 rtSimpleRequest=0 */

#if	defined(__MIG_check__Request__mach_vm_machine_attribute_t__defined)
	check_result = __MIG_check__Request__mach_vm_machine_attribute_t((__Request *)In0P);
	if (check_result != MACH_MSG_SUCCESS)
		{ MIG_RETURN_ERROR(OutP, check_result); }
#endif	/* defined(__MIG_check__Request__mach_vm_machine_attribute_t__defined) */

	target_task = convert_port_to_map(In0P->Head.msgh_request_port);

	OutP->RetCode = mach_vm_machine_attribute(target_task, In0P->address, In0P->size, In0P->attribute, &In0P->value);
	vm_map_deallocate(target_task);
	if (OutP->RetCode != KERN_SUCCESS) {
		MIG_RETURN_ERROR(OutP, OutP->RetCode);
	}
#if	__MigKernelSpecificCode
#endif /* __MigKernelSpecificCode */

	OutP->NDR = NDR_record;


	OutP->value = In0P->value;

	OutP->Head.msgh_size = (sizeof(Reply));
	__AfterRcvRpc(4812, "mach_vm_machine_attribute")
}

#if ( __MigTypeCheck )
#if __MIG_check__Request__mach_vm_subsystem__
#if !defined(__MIG_check__Request__mach_vm_remap_t__defined)
#define __MIG_check__Request__mach_vm_remap_t__defined

mig_internal kern_return_t __MIG_check__Request__mach_vm_remap_t(__attribute__((__unused__)) __Request__mach_vm_remap_t *In0P)
{

	typedef __Request__mach_vm_remap_t __Request;
#if	__MigTypeCheck
	if (!(In0P->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX) ||
	    (In0P->msgh_body.msgh_descriptor_count != 1) ||
	    (In0P->Head.msgh_size != (mach_msg_size_t)sizeof(__Request)))
		return MIG_BAD_ARGUMENTS;
#endif	/* __MigTypeCheck */

#if	__MigTypeCheck
	if (In0P->src_task.type != MACH_MSG_PORT_DESCRIPTOR ||
	    In0P->src_task.disposition != 17)
		return MIG_TYPE_ERROR;
#endif	/* __MigTypeCheck */

	return MACH_MSG_SUCCESS;
}
#endif /* !defined(__MIG_check__Request__mach_vm_remap_t__defined) */
#endif /* __MIG_check__Request__mach_vm_subsystem__ */
#endif /* ( __MigTypeCheck ) */


/* Routine mach_vm_remap */
#ifdef	mig_external
mig_external
#else
extern
#endif	/* mig_external */
kern_return_t mach_vm_remap
#if	defined(LINTLIBRARY)
    (target_task, target_address, size, mask, flags, src_task, src_address, copy, cur_protection, max_protection, inheritance)
	vm_map_t target_task;
	mach_vm_address_t *target_address;
	mach_vm_size_t size;
	mach_vm_offset_t mask;
	int flags;
	vm_map_t src_task;
	mach_vm_address_t src_address;
	boolean_t copy;
	vm_prot_t *cur_protection;
	vm_prot_t *max_protection;
	vm_inherit_t inheritance;
{ return mach_vm_remap(target_task, target_address, size, mask, flags, src_task, src_address, copy, cur_protection, max_protection, inheritance); }
#else
(
	vm_map_t target_task,
	mach_vm_address_t *target_address,
	mach_vm_size_t size,
	mach_vm_offset_t mask,
	int flags,
	vm_map_t src_task,
	mach_vm_address_t src_address,
	boolean_t copy,
	vm_prot_t *cur_protection,
	vm_prot_t *max_protection,
	vm_inherit_t inheritance
);
#endif	/* defined(LINTLIBRARY) */

/* Routine mach_vm_remap */
mig_internal novalue _Xmach_vm_remap
	(mach_msg_header_t *InHeadP, mach_msg_header_t *OutHeadP)
{

#ifdef  __MigPackStructs
#pragma pack(4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		/* start of the kernel processed data */
		mach_msg_body_t msgh_body;
		mach_msg_port_descriptor_t src_task;
		/* end of the kernel processed data */
		NDR_record_t NDR;
		mach_vm_address_t target_address;
		mach_vm_size_t size;
		mach_vm_offset_t mask;
		int flags;
		mach_vm_address_t src_address;
		boolean_t copy;
		vm_inherit_t inheritance;
		mach_msg_trailer_t trailer;
	} Request;
#ifdef  __MigPackStructs
#pragma pack()
#endif
	typedef __Request__mach_vm_remap_t __Request;
	typedef __Reply__mach_vm_remap_t Reply;

	/*
	 * typedef struct {
	 * 	mach_msg_header_t Head;
	 * 	NDR_record_t NDR;
	 * 	kern_return_t RetCode;
	 * } mig_reply_error_t;
	 */

	Request *In0P = (Request *) InHeadP;
	Reply *OutP = (Reply *) OutHeadP;
#ifdef	__MIG_check__Request__mach_vm_remap_t__defined
	kern_return_t check_result;
#endif	/* __MIG_check__Request__mach_vm_remap_t__defined */

#if	__MigKernelSpecificCode
#else
#endif /* __MigKernelSpecificCode */
	vm_map_t target_task;
	vm_map_t src_task;

	__DeclareRcvRpc(4813, "mach_vm_remap")
	__BeforeRcvRpc(4813, "mach_vm_remap")
/* RetCArg=0x0 rtSimpleRequest=0 */

#if	defined(__MIG_check__Request__mach_vm_remap_t__defined)
	check_result = __MIG_check__Request__mach_vm_remap_t((__Request *)In0P);
	if (check_result != MACH_MSG_SUCCESS)
		{ MIG_RETURN_ERROR(OutP, check_result); }
#endif	/* defined(__MIG_check__Request__mach_vm_remap_t__defined) */

	target_task = convert_port_to_map(In0P->Head.msgh_request_port);

	src_task = convert_port_to_map(In0P->src_task.name);

	OutP->RetCode = mach_vm_remap(target_task, &In0P->target_address, In0P->size, In0P->mask, In0P->flags, src_task, In0P->src_address, In0P->copy, &OutP->cur_protection, &OutP->max_protection, In0P->inheritance);
	vm_map_deallocate(src_task);
	vm_map_deallocate(target_task);
	if (OutP->RetCode != KERN_SUCCESS) {
		MIG_RETURN_ERROR(OutP, OutP->RetCode);
	}
#if	__MigKernelSpecificCode

	if (IP_VALID((ipc_port_t)In0P->src_task.name))
		ipc_port_release_send((ipc_port_t)In0P->src_task.name);
#endif /* __MigKernelSpecificCode */

	OutP->NDR = NDR_record;


	OutP->target_address = In0P->target_address;

	OutP->Head.msgh_size = (sizeof(Reply));
	__AfterRcvRpc(4813, "mach_vm_remap")
}

#if ( __MigTypeCheck )
#if __MIG_check__Request__mach_vm_subsystem__
#if !defined(__MIG_check__Request__mach_vm_page_query_t__defined)
#define __MIG_check__Request__mach_vm_page_query_t__defined

mig_internal kern_return_t __MIG_check__Request__mach_vm_page_query_t(__attribute__((__unused__)) __Request__mach_vm_page_query_t *In0P)
{

	typedef __Request__mach_vm_page_query_t __Request;
#if	__MigTypeCheck
	if (!(In0P->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX) ||
	    (In0P->msgh_body.msgh_descriptor_count != 0) ||
	    (In0P->Head.msgh_size != (mach_msg_size_t)sizeof(__Request)))
		return MIG_BAD_ARGUMENTS;
#endif	/* __MigTypeCheck */

	return MACH_MSG_SUCCESS;
}
#endif /* !defined(__MIG_check__Request__mach_vm_page_query_t__defined) */
#endif /* __MIG_check__Request__mach_vm_subsystem__ */
#endif /* ( __MigTypeCheck ) */


/* Routine mach_vm_page_query */
#ifdef	mig_external
mig_external
#else
extern
#endif	/* mig_external */
kern_return_t mach_vm_page_query
#if	defined(LINTLIBRARY)
    (target_map, offset, disposition, ref_count)
	vm_map_t target_map;
	mach_vm_offset_t offset;
	integer_t *disposition;
	integer_t *ref_count;
{ return mach_vm_page_query(target_map, offset, disposition, ref_count); }
#else
(
	vm_map_t target_map,
	mach_vm_offset_t offset,
	integer_t *disposition,
	integer_t *ref_count
);
#endif	/* defined(LINTLIBRARY) */

/* Routine mach_vm_page_query */
mig_internal novalue _Xmach_vm_page_query
	(mach_msg_header_t *InHeadP, mach_msg_header_t *OutHeadP)
{

#ifdef  __MigPackStructs
#pragma pack(4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		/* start of the kernel processed data */
		mach_msg_body_t msgh_body;
		/* end of the kernel processed data */
		NDR_record_t NDR;
		mach_vm_offset_t offset;
		mach_msg_trailer_t trailer;
	} Request;
#ifdef  __MigPackStructs
#pragma pack()
#endif
	typedef __Request__mach_vm_page_query_t __Request;
	typedef __Reply__mach_vm_page_query_t Reply;

	/*
	 * typedef struct {
	 * 	mach_msg_header_t Head;
	 * 	NDR_record_t NDR;
	 * 	kern_return_t RetCode;
	 * } mig_reply_error_t;
	 */

	Request *In0P = (Request *) InHeadP;
	Reply *OutP = (Reply *) OutHeadP;
#ifdef	__MIG_check__Request__mach_vm_page_query_t__defined
	kern_return_t check_result;
#endif	/* __MIG_check__Request__mach_vm_page_query_t__defined */

#if	__MigKernelSpecificCode
#else
#endif /* __MigKernelSpecificCode */
	vm_map_t target_map;

	__DeclareRcvRpc(4814, "mach_vm_page_query")
	__BeforeRcvRpc(4814, "mach_vm_page_query")
/* RetCArg=0x0 rtSimpleRequest=0 */

#if	defined(__MIG_check__Request__mach_vm_page_query_t__defined)
	check_result = __MIG_check__Request__mach_vm_page_query_t((__Request *)In0P);
	if (check_result != MACH_MSG_SUCCESS)
		{ MIG_RETURN_ERROR(OutP, check_result); }
#endif	/* defined(__MIG_check__Request__mach_vm_page_query_t__defined) */

	target_map = convert_port_to_map(In0P->Head.msgh_request_port);

	OutP->RetCode = mach_vm_page_query(target_map, In0P->offset, &OutP->disposition, &OutP->ref_count);
	vm_map_deallocate(target_map);
	if (OutP->RetCode != KERN_SUCCESS) {
		MIG_RETURN_ERROR(OutP, OutP->RetCode);
	}
#if	__MigKernelSpecificCode
#endif /* __MigKernelSpecificCode */

	OutP->NDR = NDR_record;


	OutP->Head.msgh_size = (sizeof(Reply));
	__AfterRcvRpc(4814, "mach_vm_page_query")
}

#if ( __MigTypeCheck )
#if __MIG_check__Request__mach_vm_subsystem__
#if !defined(__MIG_check__Request__mach_vm_region_recurse_t__defined)
#define __MIG_check__Request__mach_vm_region_recurse_t__defined

mig_internal kern_return_t __MIG_check__Request__mach_vm_region_recurse_t(__attribute__((__unused__)) __Request__mach_vm_region_recurse_t *In0P)
{

	typedef __Request__mach_vm_region_recurse_t __Request;
#if	__MigTypeCheck
	if (!(In0P->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX) ||
	    (In0P->msgh_body.msgh_descriptor_count != 0) ||
	    (In0P->Head.msgh_size != (mach_msg_size_t)sizeof(__Request)))
		return MIG_BAD_ARGUMENTS;
#endif	/* __MigTypeCheck */

	return MACH_MSG_SUCCESS;
}
#endif /* !defined(__MIG_check__Request__mach_vm_region_recurse_t__defined) */
#endif /* __MIG_check__Request__mach_vm_subsystem__ */
#endif /* ( __MigTypeCheck ) */


/* Routine mach_vm_region_recurse */
#ifdef	mig_external
mig_external
#else
extern
#endif	/* mig_external */
kern_return_t mach_vm_region_recurse
#if	defined(LINTLIBRARY)
    (target_task, address, size, nesting_depth, info, infoCnt)
	vm_map_t target_task;
	mach_vm_address_t *address;
	mach_vm_size_t *size;
	natural_t *nesting_depth;
	vm_region_recurse_info_t info;
	mach_msg_type_number_t *infoCnt;
{ return mach_vm_region_recurse(target_task, address, size, nesting_depth, info, infoCnt); }
#else
(
	vm_map_t target_task,
	mach_vm_address_t *address,
	mach_vm_size_t *size,
	natural_t *nesting_depth,
	vm_region_recurse_info_t info,
	mach_msg_type_number_t *infoCnt
);
#endif	/* defined(LINTLIBRARY) */

/* Routine mach_vm_region_recurse */
mig_internal novalue _Xmach_vm_region_recurse
	(mach_msg_header_t *InHeadP, mach_msg_header_t *OutHeadP)
{

#ifdef  __MigPackStructs
#pragma pack(4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		/* start of the kernel processed data */
		mach_msg_body_t msgh_body;
		/* end of the kernel processed data */
		NDR_record_t NDR;
		mach_vm_address_t address;
		natural_t nesting_depth;
		mach_msg_type_number_t infoCnt;
		mach_msg_trailer_t trailer;
	} Request;
#ifdef  __MigPackStructs
#pragma pack()
#endif
	typedef __Request__mach_vm_region_recurse_t __Request;
	typedef __Reply__mach_vm_region_recurse_t Reply;

	/*
	 * typedef struct {
	 * 	mach_msg_header_t Head;
	 * 	NDR_record_t NDR;
	 * 	kern_return_t RetCode;
	 * } mig_reply_error_t;
	 */

	Request *In0P = (Request *) InHeadP;
	Reply *OutP = (Reply *) OutHeadP;
#ifdef	__MIG_check__Request__mach_vm_region_recurse_t__defined
	kern_return_t check_result;
#endif	/* __MIG_check__Request__mach_vm_region_recurse_t__defined */

#if	__MigKernelSpecificCode
#else
#endif /* __MigKernelSpecificCode */
	vm_map_t target_task;

	__DeclareRcvRpc(4815, "mach_vm_region_recurse")
	__BeforeRcvRpc(4815, "mach_vm_region_recurse")
/* RetCArg=0x0 rtSimpleRequest=0 */

#if	defined(__MIG_check__Request__mach_vm_region_recurse_t__defined)
	check_result = __MIG_check__Request__mach_vm_region_recurse_t((__Request *)In0P);
	if (check_result != MACH_MSG_SUCCESS)
		{ MIG_RETURN_ERROR(OutP, check_result); }
#endif	/* defined(__MIG_check__Request__mach_vm_region_recurse_t__defined) */

	target_task = convert_port_to_map(In0P->Head.msgh_request_port);

	OutP->infoCnt = 19;
	if (In0P->infoCnt < OutP->infoCnt)
		OutP->infoCnt = In0P->infoCnt;

	OutP->RetCode = mach_vm_region_recurse(target_task, &In0P->address, &OutP->size, &In0P->nesting_depth, OutP->info, &OutP->infoCnt);
	vm_map_deallocate(target_task);
	if (OutP->RetCode != KERN_SUCCESS) {
		MIG_RETURN_ERROR(OutP, OutP->RetCode);
	}
#if	__MigKernelSpecificCode
#endif /* __MigKernelSpecificCode */

	OutP->NDR = NDR_record;


	OutP->address = In0P->address;

	OutP->nesting_depth = In0P->nesting_depth;
	OutP->Head.msgh_size = (sizeof(Reply) - 76) + (_WALIGN_((4 * OutP->infoCnt)));

	__AfterRcvRpc(4815, "mach_vm_region_recurse")
}

#if ( __MigTypeCheck )
#if __MIG_check__Request__mach_vm_subsystem__
#if !defined(__MIG_check__Request__mach_vm_region_t__defined)
#define __MIG_check__Request__mach_vm_region_t__defined

mig_internal kern_return_t __MIG_check__Request__mach_vm_region_t(__attribute__((__unused__)) __Request__mach_vm_region_t *In0P)
{

	typedef __Request__mach_vm_region_t __Request;
#if	__MigTypeCheck
	if (!(In0P->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX) ||
	    (In0P->msgh_body.msgh_descriptor_count != 0) ||
	    (In0P->Head.msgh_size != (mach_msg_size_t)sizeof(__Request)))
		return MIG_BAD_ARGUMENTS;
#endif	/* __MigTypeCheck */

	return MACH_MSG_SUCCESS;
}
#endif /* !defined(__MIG_check__Request__mach_vm_region_t__defined) */
#endif /* __MIG_check__Request__mach_vm_subsystem__ */
#endif /* ( __MigTypeCheck ) */


/* Routine mach_vm_region */
#ifdef	mig_external
mig_external
#else
extern
#endif	/* mig_external */
kern_return_t mach_vm_region
#if	defined(LINTLIBRARY)
    (target_task, address, size, flavor, info, infoCnt, object_name)
	vm_map_t target_task;
	mach_vm_address_t *address;
	mach_vm_size_t *size;
	vm_region_flavor_t flavor;
	vm_region_info_t info;
	mach_msg_type_number_t *infoCnt;
	mach_port_t *object_name;
{ return mach_vm_region(target_task, address, size, flavor, info, infoCnt, object_name); }
#else
(
	vm_map_t target_task,
	mach_vm_address_t *address,
	mach_vm_size_t *size,
	vm_region_flavor_t flavor,
	vm_region_info_t info,
	mach_msg_type_number_t *infoCnt,
	mach_port_t *object_name
);
#endif	/* defined(LINTLIBRARY) */

/* Routine mach_vm_region */
mig_internal novalue _Xmach_vm_region
	(mach_msg_header_t *InHeadP, mach_msg_header_t *OutHeadP)
{

#ifdef  __MigPackStructs
#pragma pack(4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		/* start of the kernel processed data */
		mach_msg_body_t msgh_body;
		/* end of the kernel processed data */
		NDR_record_t NDR;
		mach_vm_address_t address;
		vm_region_flavor_t flavor;
		mach_msg_type_number_t infoCnt;
		mach_msg_trailer_t trailer;
	} Request;
#ifdef  __MigPackStructs
#pragma pack()
#endif
	typedef __Request__mach_vm_region_t __Request;
	typedef __Reply__mach_vm_region_t Reply;

	/*
	 * typedef struct {
	 * 	mach_msg_header_t Head;
	 * 	NDR_record_t NDR;
	 * 	kern_return_t RetCode;
	 * } mig_reply_error_t;
	 */

	Request *In0P = (Request *) InHeadP;
	Reply *OutP = (Reply *) OutHeadP;
#ifdef	__MIG_check__Request__mach_vm_region_t__defined
	kern_return_t check_result;
#endif	/* __MIG_check__Request__mach_vm_region_t__defined */

#if	__MigKernelSpecificCode
#if	UseStaticTemplates
	const static mach_msg_port_descriptor_t object_nameTemplate = {
		.name = MACH_PORT_NULL,
		.disposition = 17,
		.type = MACH_MSG_PORT_DESCRIPTOR,
	};
#endif	/* UseStaticTemplates */

#else
#if	UseStaticTemplates
	const static mach_msg_port_descriptor_t object_nameTemplate = {
		.name = MACH_PORT_NULL,
		.disposition = 17,
		.type = MACH_MSG_PORT_DESCRIPTOR,
	};
#endif	/* UseStaticTemplates */

#endif /* __MigKernelSpecificCode */
	kern_return_t RetCode;
	vm_map_t target_task;

	__DeclareRcvRpc(4816, "mach_vm_region")
	__BeforeRcvRpc(4816, "mach_vm_region")
/* RetCArg=0x0 rtSimpleRequest=0 */

#if	defined(__MIG_check__Request__mach_vm_region_t__defined)
	check_result = __MIG_check__Request__mach_vm_region_t((__Request *)In0P);
	if (check_result != MACH_MSG_SUCCESS)
		{ MIG_RETURN_ERROR(OutP, check_result); }
#endif	/* defined(__MIG_check__Request__mach_vm_region_t__defined) */

#if	UseStaticTemplates
	OutP->object_name = object_nameTemplate;
#else	/* UseStaticTemplates */
#if __MigKernelSpecificCode
	OutP->object_name.disposition = 17;
#else
	OutP->object_name.disposition = 17;
#endif /* __MigKernelSpecificCode */
	OutP->object_name.type = MACH_MSG_PORT_DESCRIPTOR;
#endif	/* UseStaticTemplates */


	target_task = convert_port_to_map(In0P->Head.msgh_request_port);

	OutP->infoCnt = 10;
	if (In0P->infoCnt < OutP->infoCnt)
		OutP->infoCnt = In0P->infoCnt;

	RetCode = mach_vm_region(target_task, &In0P->address, &OutP->size, In0P->flavor, OutP->info, &OutP->infoCnt, &OutP->object_name.name);
	vm_map_deallocate(target_task);
	if (RetCode != KERN_SUCCESS) {
		MIG_RETURN_ERROR(OutP, RetCode);
	}
#if	__MigKernelSpecificCode
#endif /* __MigKernelSpecificCode */

	OutP->NDR = NDR_record;


	OutP->address = In0P->address;
	OutP->Head.msgh_size = (sizeof(Reply) - 40) + (_WALIGN_((4 * OutP->infoCnt)));

	OutP->Head.msgh_bits |= MACH_MSGH_BITS_COMPLEX;
	OutP->msgh_body.msgh_descriptor_count = 1;
	__AfterRcvRpc(4816, "mach_vm_region")
}

#if ( __MigTypeCheck )
#if __MIG_check__Request__mach_vm_subsystem__
#if !defined(__MIG_check__Request___mach_make_memory_entry_t__defined)
#define __MIG_check__Request___mach_make_memory_entry_t__defined

mig_internal kern_return_t __MIG_check__Request___mach_make_memory_entry_t(__attribute__((__unused__)) __Request___mach_make_memory_entry_t *In0P)
{

	typedef __Request___mach_make_memory_entry_t __Request;
#if	__MigTypeCheck
	if (!(In0P->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX) ||
	    (In0P->msgh_body.msgh_descriptor_count != 1) ||
	    (In0P->Head.msgh_size != (mach_msg_size_t)sizeof(__Request)))
		return MIG_BAD_ARGUMENTS;
#endif	/* __MigTypeCheck */

#if	__MigTypeCheck
	if (In0P->parent_handle.type != MACH_MSG_PORT_DESCRIPTOR ||
	    In0P->parent_handle.disposition != 17)
		return MIG_TYPE_ERROR;
#endif	/* __MigTypeCheck */

	return MACH_MSG_SUCCESS;
}
#endif /* !defined(__MIG_check__Request___mach_make_memory_entry_t__defined) */
#endif /* __MIG_check__Request__mach_vm_subsystem__ */
#endif /* ( __MigTypeCheck ) */


/* Routine _mach_make_memory_entry */
#ifdef	mig_external
mig_external
#else
extern
#endif	/* mig_external */
kern_return_t _mach_make_memory_entry
#if	defined(LINTLIBRARY)
    (target_task, size, offset, permission, object_handle, parent_handle)
	vm_map_t target_task;
	memory_object_size_t *size;
	memory_object_offset_t offset;
	vm_prot_t permission;
	mem_entry_name_port_t *object_handle;
	mem_entry_name_port_t parent_handle;
{ return _mach_make_memory_entry(target_task, size, offset, permission, object_handle, parent_handle); }
#else
(
	vm_map_t target_task,
	memory_object_size_t *size,
	memory_object_offset_t offset,
	vm_prot_t permission,
	mem_entry_name_port_t *object_handle,
	mem_entry_name_port_t parent_handle
);
#endif	/* defined(LINTLIBRARY) */

/* Routine _mach_make_memory_entry */
mig_internal novalue _X_mach_make_memory_entry
	(mach_msg_header_t *InHeadP, mach_msg_header_t *OutHeadP)
{

#ifdef  __MigPackStructs
#pragma pack(4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		/* start of the kernel processed data */
		mach_msg_body_t msgh_body;
		mach_msg_port_descriptor_t parent_handle;
		/* end of the kernel processed data */
		NDR_record_t NDR;
		memory_object_size_t size;
		memory_object_offset_t offset;
		vm_prot_t permission;
		mach_msg_trailer_t trailer;
	} Request;
#ifdef  __MigPackStructs
#pragma pack()
#endif
	typedef __Request___mach_make_memory_entry_t __Request;
	typedef __Reply___mach_make_memory_entry_t Reply;

	/*
	 * typedef struct {
	 * 	mach_msg_header_t Head;
	 * 	NDR_record_t NDR;
	 * 	kern_return_t RetCode;
	 * } mig_reply_error_t;
	 */

	Request *In0P = (Request *) InHeadP;
	Reply *OutP = (Reply *) OutHeadP;
#ifdef	__MIG_check__Request___mach_make_memory_entry_t__defined
	kern_return_t check_result;
#endif	/* __MIG_check__Request___mach_make_memory_entry_t__defined */

#if	__MigKernelSpecificCode
#if	UseStaticTemplates
	const static mach_msg_port_descriptor_t object_handleTemplate = {
		.name = MACH_PORT_NULL,
		.disposition = 17,
		.type = MACH_MSG_PORT_DESCRIPTOR,
	};
#endif	/* UseStaticTemplates */

#else
#if	UseStaticTemplates
	const static mach_msg_port_descriptor_t object_handleTemplate = {
		.name = MACH_PORT_NULL,
		.disposition = 17,
		.type = MACH_MSG_PORT_DESCRIPTOR,
	};
#endif	/* UseStaticTemplates */

#endif /* __MigKernelSpecificCode */
	kern_return_t RetCode;
	vm_map_t target_task;
	mem_entry_name_port_t object_handle;

	__DeclareRcvRpc(4817, "_mach_make_memory_entry")
	__BeforeRcvRpc(4817, "_mach_make_memory_entry")
/* RetCArg=0x0 rtSimpleRequest=0 */

#if	defined(__MIG_check__Request___mach_make_memory_entry_t__defined)
	check_result = __MIG_check__Request___mach_make_memory_entry_t((__Request *)In0P);
	if (check_result != MACH_MSG_SUCCESS)
		{ MIG_RETURN_ERROR(OutP, check_result); }
#endif	/* defined(__MIG_check__Request___mach_make_memory_entry_t__defined) */

#if	UseStaticTemplates
	OutP->object_handle = object_handleTemplate;
#else	/* UseStaticTemplates */
#if __MigKernelSpecificCode
	OutP->object_handle.disposition = 17;
#else
	OutP->object_handle.disposition = 17;
#endif /* __MigKernelSpecificCode */
	OutP->object_handle.type = MACH_MSG_PORT_DESCRIPTOR;
#endif	/* UseStaticTemplates */


	target_task = convert_port_to_map(In0P->Head.msgh_request_port);

	RetCode = _mach_make_memory_entry(target_task, &In0P->size, In0P->offset, In0P->permission, &object_handle, null_conversion(In0P->parent_handle.name));
	vm_map_deallocate(target_task);
	if (RetCode != KERN_SUCCESS) {
		MIG_RETURN_ERROR(OutP, RetCode);
	}
#if	__MigKernelSpecificCode

	if (IP_VALID((ipc_port_t)In0P->parent_handle.name))
		ipc_port_release_send((ipc_port_t)In0P->parent_handle.name);
#endif /* __MigKernelSpecificCode */
	OutP->object_handle.name = (mach_port_t)null_conversion(object_handle);


	OutP->NDR = NDR_record;


	OutP->size = In0P->size;

	OutP->Head.msgh_bits |= MACH_MSGH_BITS_COMPLEX;
	OutP->Head.msgh_size = (sizeof(Reply));
	OutP->msgh_body.msgh_descriptor_count = 1;
	__AfterRcvRpc(4817, "_mach_make_memory_entry")
}

#if ( __MigTypeCheck )
#if __MIG_check__Request__mach_vm_subsystem__
#if !defined(__MIG_check__Request__mach_vm_purgable_control_t__defined)
#define __MIG_check__Request__mach_vm_purgable_control_t__defined

mig_internal kern_return_t __MIG_check__Request__mach_vm_purgable_control_t(__attribute__((__unused__)) __Request__mach_vm_purgable_control_t *In0P)
{

	typedef __Request__mach_vm_purgable_control_t __Request;
#if	__MigTypeCheck
	if (!(In0P->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX) ||
	    (In0P->msgh_body.msgh_descriptor_count != 0) ||
	    (In0P->Head.msgh_size != (mach_msg_size_t)sizeof(__Request)))
		return MIG_BAD_ARGUMENTS;
#endif	/* __MigTypeCheck */

	return MACH_MSG_SUCCESS;
}
#endif /* !defined(__MIG_check__Request__mach_vm_purgable_control_t__defined) */
#endif /* __MIG_check__Request__mach_vm_subsystem__ */
#endif /* ( __MigTypeCheck ) */


/* Routine mach_vm_purgable_control */
#ifdef	mig_external
mig_external
#else
extern
#endif	/* mig_external */
kern_return_t mach_vm_purgable_control
#if	defined(LINTLIBRARY)
    (target_task, address, control, state)
	vm_map_t target_task;
	mach_vm_address_t address;
	vm_purgable_t control;
	int *state;
{ return mach_vm_purgable_control(target_task, address, control, state); }
#else
(
	vm_map_t target_task,
	mach_vm_address_t address,
	vm_purgable_t control,
	int *state
);
#endif	/* defined(LINTLIBRARY) */

/* Routine mach_vm_purgable_control */
mig_internal novalue _Xmach_vm_purgable_control
	(mach_msg_header_t *InHeadP, mach_msg_header_t *OutHeadP)
{

#ifdef  __MigPackStructs
#pragma pack(4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		/* start of the kernel processed data */
		mach_msg_body_t msgh_body;
		/* end of the kernel processed data */
		NDR_record_t NDR;
		mach_vm_address_t address;
		vm_purgable_t control;
		int state;
		mach_msg_trailer_t trailer;
	} Request;
#ifdef  __MigPackStructs
#pragma pack()
#endif
	typedef __Request__mach_vm_purgable_control_t __Request;
	typedef __Reply__mach_vm_purgable_control_t Reply;

	/*
	 * typedef struct {
	 * 	mach_msg_header_t Head;
	 * 	NDR_record_t NDR;
	 * 	kern_return_t RetCode;
	 * } mig_reply_error_t;
	 */

	Request *In0P = (Request *) InHeadP;
	Reply *OutP = (Reply *) OutHeadP;
#ifdef	__MIG_check__Request__mach_vm_purgable_control_t__defined
	kern_return_t check_result;
#endif	/* __MIG_check__Request__mach_vm_purgable_control_t__defined */

#if	__MigKernelSpecificCode
#else
#endif /* __MigKernelSpecificCode */
	vm_map_t target_task;

	__DeclareRcvRpc(4818, "mach_vm_purgable_control")
	__BeforeRcvRpc(4818, "mach_vm_purgable_control")
/* RetCArg=0x0 rtSimpleRequest=0 */

#if	defined(__MIG_check__Request__mach_vm_purgable_control_t__defined)
	check_result = __MIG_check__Request__mach_vm_purgable_control_t((__Request *)In0P);
	if (check_result != MACH_MSG_SUCCESS)
		{ MIG_RETURN_ERROR(OutP, check_result); }
#endif	/* defined(__MIG_check__Request__mach_vm_purgable_control_t__defined) */

	target_task = convert_port_to_map(In0P->Head.msgh_request_port);

	OutP->RetCode = mach_vm_purgable_control(target_task, In0P->address, In0P->control, &In0P->state);
	vm_map_deallocate(target_task);
	if (OutP->RetCode != KERN_SUCCESS) {
		MIG_RETURN_ERROR(OutP, OutP->RetCode);
	}
#if	__MigKernelSpecificCode
#endif /* __MigKernelSpecificCode */

	OutP->NDR = NDR_record;


	OutP->state = In0P->state;

	OutP->Head.msgh_size = (sizeof(Reply));
	__AfterRcvRpc(4818, "mach_vm_purgable_control")
}

#if ( __MigTypeCheck )
#if __MIG_check__Request__mach_vm_subsystem__
#if !defined(__MIG_check__Request__mach_vm_page_info_t__defined)
#define __MIG_check__Request__mach_vm_page_info_t__defined

mig_internal kern_return_t __MIG_check__Request__mach_vm_page_info_t(__attribute__((__unused__)) __Request__mach_vm_page_info_t *In0P)
{

	typedef __Request__mach_vm_page_info_t __Request;
#if	__MigTypeCheck
	if (!(In0P->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX) ||
	    (In0P->msgh_body.msgh_descriptor_count != 0) ||
	    (In0P->Head.msgh_size != (mach_msg_size_t)sizeof(__Request)))
		return MIG_BAD_ARGUMENTS;
#endif	/* __MigTypeCheck */

	return MACH_MSG_SUCCESS;
}
#endif /* !defined(__MIG_check__Request__mach_vm_page_info_t__defined) */
#endif /* __MIG_check__Request__mach_vm_subsystem__ */
#endif /* ( __MigTypeCheck ) */


/* Routine mach_vm_page_info */
#ifdef	mig_external
mig_external
#else
extern
#endif	/* mig_external */
kern_return_t mach_vm_page_info
#if	defined(LINTLIBRARY)
    (target_task, address, flavor, info, infoCnt)
	vm_map_t target_task;
	mach_vm_address_t address;
	vm_page_info_flavor_t flavor;
	vm_page_info_t info;
	mach_msg_type_number_t *infoCnt;
{ return mach_vm_page_info(target_task, address, flavor, info, infoCnt); }
#else
(
	vm_map_t target_task,
	mach_vm_address_t address,
	vm_page_info_flavor_t flavor,
	vm_page_info_t info,
	mach_msg_type_number_t *infoCnt
);
#endif	/* defined(LINTLIBRARY) */

/* Routine mach_vm_page_info */
mig_internal novalue _Xmach_vm_page_info
	(mach_msg_header_t *InHeadP, mach_msg_header_t *OutHeadP)
{

#ifdef  __MigPackStructs
#pragma pack(4)
#endif
	typedef struct {
		mach_msg_header_t Head;
		/* start of the kernel processed data */
		mach_msg_body_t msgh_body;
		/* end of the kernel processed data */
		NDR_record_t NDR;
		mach_vm_address_t address;
		vm_page_info_flavor_t flavor;
		mach_msg_type_number_t infoCnt;
		mach_msg_trailer_t trailer;
	} Request;
#ifdef  __MigPackStructs
#pragma pack()
#endif
	typedef __Request__mach_vm_page_info_t __Request;
	typedef __Reply__mach_vm_page_info_t Reply;

	/*
	 * typedef struct {
	 * 	mach_msg_header_t Head;
	 * 	NDR_record_t NDR;
	 * 	kern_return_t RetCode;
	 * } mig_reply_error_t;
	 */

	Request *In0P = (Request *) InHeadP;
	Reply *OutP = (Reply *) OutHeadP;
#ifdef	__MIG_check__Request__mach_vm_page_info_t__defined
	kern_return_t check_result;
#endif	/* __MIG_check__Request__mach_vm_page_info_t__defined */

#if	__MigKernelSpecificCode
#else
#endif /* __MigKernelSpecificCode */
	vm_map_t target_task;

	__DeclareRcvRpc(4819, "mach_vm_page_info")
	__BeforeRcvRpc(4819, "mach_vm_page_info")
/* RetCArg=0x0 rtSimpleRequest=0 */

#if	defined(__MIG_check__Request__mach_vm_page_info_t__defined)
	check_result = __MIG_check__Request__mach_vm_page_info_t((__Request *)In0P);
	if (check_result != MACH_MSG_SUCCESS)
		{ MIG_RETURN_ERROR(OutP, check_result); }
#endif	/* defined(__MIG_check__Request__mach_vm_page_info_t__defined) */

	target_task = convert_port_to_map(In0P->Head.msgh_request_port);

	OutP->infoCnt = 32;
	if (In0P->infoCnt < OutP->infoCnt)
		OutP->infoCnt = In0P->infoCnt;

	OutP->RetCode = mach_vm_page_info(target_task, In0P->address, In0P->flavor, OutP->info, &OutP->infoCnt);
	vm_map_deallocate(target_task);
	if (OutP->RetCode != KERN_SUCCESS) {
		MIG_RETURN_ERROR(OutP, OutP->RetCode);
	}
#if	__MigKernelSpecificCode
#endif /* __MigKernelSpecificCode */

	OutP->NDR = NDR_record;

	OutP->Head.msgh_size = (sizeof(Reply) - 128) + (_WALIGN_((4 * OutP->infoCnt)));

	__AfterRcvRpc(4819, "mach_vm_page_info")
}


#ifdef	mig_external
mig_external
#else
extern
#endif	/* mig_external */
boolean_t mach_vm_server(
		mach_msg_header_t *InHeadP,
		mach_msg_header_t *OutHeadP);

#ifdef	mig_external
mig_external
#else
extern
#endif	/* mig_external */
mig_routine_t mach_vm_server_routine(
		mach_msg_header_t *InHeadP);


/* Description of this subsystem, for use in direct RPC */
extern const struct mach_vm_subsystem mach_vm_subsystem;
const struct mach_vm_subsystem {
	mig_server_routine_t 	server;	/* Server routine */
	mach_msg_id_t	start;	/* Min routine number */
	mach_msg_id_t	end;	/* Max routine number + 1 */
	unsigned int	maxsize;	/* Max msg size */
	vm_address_t	reserved;	/* Reserved */
	struct routine_descriptor	/*Array of routine descriptors */
		routine[20];
} mach_vm_subsystem = {
	mach_vm_server_routine,
	4800,
	4820,
	(mach_msg_size_t)sizeof(union __ReplyUnion__mach_vm_subsystem),
	(vm_address_t)0,
	{
		{0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0},
          { (mig_impl_routine_t) 0,
            (mig_stub_routine_t) _Xmach_vm_protect, 7, 0, (routine_arg_descriptor_t)0, (mach_msg_size_t)sizeof(__Reply__mach_vm_protect_t) },
          { (mig_impl_routine_t) 0,
            (mig_stub_routine_t) _Xmach_vm_inherit, 6, 0, (routine_arg_descriptor_t)0, (mach_msg_size_t)sizeof(__Reply__mach_vm_inherit_t) },
          { (mig_impl_routine_t) 0,
            (mig_stub_routine_t) _Xmach_vm_read, 7, 0, (routine_arg_descriptor_t)0, (mach_msg_size_t)sizeof(__Reply__mach_vm_read_t) },
          { (mig_impl_routine_t) 0,
            (mig_stub_routine_t) _Xmach_vm_read_list, 3, 0, (routine_arg_descriptor_t)0, (mach_msg_size_t)sizeof(__Reply__mach_vm_read_list_t) },
          { (mig_impl_routine_t) 0,
            (mig_stub_routine_t) _Xmach_vm_write, 5, 0, (routine_arg_descriptor_t)0, (mach_msg_size_t)sizeof(__Reply__mach_vm_write_t) },
          { (mig_impl_routine_t) 0,
            (mig_stub_routine_t) _Xmach_vm_copy, 7, 0, (routine_arg_descriptor_t)0, (mach_msg_size_t)sizeof(__Reply__mach_vm_copy_t) },
          { (mig_impl_routine_t) 0,
            (mig_stub_routine_t) _Xmach_vm_read_overwrite, 8, 0, (routine_arg_descriptor_t)0, (mach_msg_size_t)sizeof(__Reply__mach_vm_read_overwrite_t) },
          { (mig_impl_routine_t) 0,
            (mig_stub_routine_t) _Xmach_vm_msync, 6, 0, (routine_arg_descriptor_t)0, (mach_msg_size_t)sizeof(__Reply__mach_vm_msync_t) },
          { (mig_impl_routine_t) 0,
            (mig_stub_routine_t) _Xmach_vm_behavior_set, 6, 0, (routine_arg_descriptor_t)0, (mach_msg_size_t)sizeof(__Reply__mach_vm_behavior_set_t) },
		{0, 0, 0, 0, 0, 0},
          { (mig_impl_routine_t) 0,
            (mig_stub_routine_t) _Xmach_vm_machine_attribute, 7, 0, (routine_arg_descriptor_t)0, (mach_msg_size_t)sizeof(__Reply__mach_vm_machine_attribute_t) },
          { (mig_impl_routine_t) 0,
            (mig_stub_routine_t) _Xmach_vm_remap, 14, 0, (routine_arg_descriptor_t)0, (mach_msg_size_t)sizeof(__Reply__mach_vm_remap_t) },
          { (mig_impl_routine_t) 0,
            (mig_stub_routine_t) _Xmach_vm_page_query, 5, 0, (routine_arg_descriptor_t)0, (mach_msg_size_t)sizeof(__Reply__mach_vm_page_query_t) },
          { (mig_impl_routine_t) 0,
            (mig_stub_routine_t) _Xmach_vm_region_recurse, 6, 0, (routine_arg_descriptor_t)0, (mach_msg_size_t)sizeof(__Reply__mach_vm_region_recurse_t) },
          { (mig_impl_routine_t) 0,
            (mig_stub_routine_t) _Xmach_vm_region, 7, 0, (routine_arg_descriptor_t)0, (mach_msg_size_t)sizeof(__Reply__mach_vm_region_t) },
          { (mig_impl_routine_t) 0,
            (mig_stub_routine_t) _X_mach_make_memory_entry, 7, 0, (routine_arg_descriptor_t)0, (mach_msg_size_t)sizeof(__Reply___mach_make_memory_entry_t) },
          { (mig_impl_routine_t) 0,
            (mig_stub_routine_t) _Xmach_vm_purgable_control, 5, 0, (routine_arg_descriptor_t)0, (mach_msg_size_t)sizeof(__Reply__mach_vm_purgable_control_t) },
          { (mig_impl_routine_t) 0,
            (mig_stub_routine_t) _Xmach_vm_page_info, 6, 0, (routine_arg_descriptor_t)0, (mach_msg_size_t)sizeof(__Reply__mach_vm_page_info_t) },
	}
};

mig_external boolean_t mach_vm_server
	(mach_msg_header_t *InHeadP, mach_msg_header_t *OutHeadP)
{
	/*
	 * typedef struct {
	 * 	mach_msg_header_t Head;
	 * 	NDR_record_t NDR;
	 * 	kern_return_t RetCode;
	 * } mig_reply_error_t;
	 */

	register mig_routine_t routine;

	OutHeadP->msgh_bits = MACH_MSGH_BITS(MACH_MSGH_BITS_REPLY(InHeadP->msgh_bits), 0);
	OutHeadP->msgh_remote_port = InHeadP->msgh_reply_port;
	/* Minimal size: routine() will update it if different */
	OutHeadP->msgh_size = (mach_msg_size_t)sizeof(mig_reply_error_t);
	OutHeadP->msgh_local_port = MACH_PORT_NULL;
	OutHeadP->msgh_id = InHeadP->msgh_id + 100;

	if ((InHeadP->msgh_id > 4819) || (InHeadP->msgh_id < 4800) ||
	    ((routine = mach_vm_subsystem.routine[InHeadP->msgh_id - 4800].stub_routine) == 0)) {
		((mig_reply_error_t *)OutHeadP)->NDR = NDR_record;
		((mig_reply_error_t *)OutHeadP)->RetCode = MIG_BAD_ID;
		return FALSE;
	}
	(*routine) (InHeadP, OutHeadP);
	return TRUE;
}

mig_external mig_routine_t mach_vm_server_routine
	(mach_msg_header_t *InHeadP)
{
	register int msgh_id;

	msgh_id = InHeadP->msgh_id - 4800;

	if ((msgh_id > 19) || (msgh_id < 0))
		return 0;

	return mach_vm_subsystem.routine[msgh_id].stub_routine;
}
