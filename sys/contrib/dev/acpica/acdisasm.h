/******************************************************************************
 *
 * Name: acdisasm.h - AML disassembler
 *       $Revision: 1.1.1.2 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2004, Intel Corp.
 * All rights reserved.
 *
 * 2. License
 *
 * 2.1. This is your license from Intel Corp. under its intellectual property
 * rights.  You may have additional license terms from the party that provided
 * you this software, covering your right to use that party's intellectual
 * property rights.
 *
 * 2.2. Intel grants, free of charge, to any person ("Licensee") obtaining a
 * copy of the source code appearing in this file ("Covered Code") an
 * irrevocable, perpetual, worldwide license under Intel's copyrights in the
 * base code distributed originally by Intel ("Original Intel Code") to copy,
 * make derivatives, distribute, use and display any portion of the Covered
 * Code in any form, with the right to sublicense such rights; and
 *
 * 2.3. Intel grants Licensee a non-exclusive and non-transferable patent
 * license (with the right to sublicense), under only those claims of Intel
 * patents that are infringed by the Original Intel Code, to make, use, sell,
 * offer to sell, and import the Covered Code and derivative works thereof
 * solely to the minimum extent necessary to exercise the above copyright
 * license, and in no event shall the patent license extend to any additions
 * to or modifications of the Original Intel Code.  No other license or right
 * is granted directly or by implication, estoppel or otherwise;
 *
 * The above copyright and patent license is granted only if the following
 * conditions are met:
 *
 * 3. Conditions
 *
 * 3.1. Redistribution of Source with Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification with rights to further distribute source must include
 * the above Copyright Notice, the above License, this list of Conditions,
 * and the following Disclaimer and Export Compliance provision.  In addition,
 * Licensee must cause all Covered Code to which Licensee contributes to
 * contain a file documenting the changes Licensee made to create that Covered
 * Code and the date of any change.  Licensee must include in that file the
 * documentation of any changes made by any predecessor Licensee.  Licensee
 * must include a prominent statement that the modification is derived,
 * directly or indirectly, from Original Intel Code.
 *
 * 3.2. Redistribution of Source with no Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification without rights to further distribute source must
 * include the following Disclaimer and Export Compliance provision in the
 * documentation and/or other materials provided with distribution.  In
 * addition, Licensee may not authorize further sublicense of source of any
 * portion of the Covered Code, and must include terms to the effect that the
 * license from Licensee to its licensee is limited to the intellectual
 * property embodied in the software Licensee provides to its licensee, and
 * not to intellectual property embodied in modifications its licensee may
 * make.
 *
 * 3.3. Redistribution of Executable. Redistribution in executable form of any
 * substantial portion of the Covered Code or modification must reproduce the
 * above Copyright Notice, and the following Disclaimer and Export Compliance
 * provision in the documentation and/or other materials provided with the
 * distribution.
 *
 * 3.4. Intel retains all right, title, and interest in and to the Original
 * Intel Code.
 *
 * 3.5. Neither the name Intel nor any other trademark owned or controlled by
 * Intel shall be used in advertising or otherwise to promote the sale, use or
 * other dealings in products derived from or relating to the Covered Code
 * without prior written authorization from Intel.
 *
 * 4. Disclaimer and Export Compliance
 *
 * 4.1. INTEL MAKES NO WARRANTY OF ANY KIND REGARDING ANY SOFTWARE PROVIDED
 * HERE.  ANY SOFTWARE ORIGINATING FROM INTEL OR DERIVED FROM INTEL SOFTWARE
 * IS PROVIDED "AS IS," AND INTEL WILL NOT PROVIDE ANY SUPPORT,  ASSISTANCE,
 * INSTALLATION, TRAINING OR OTHER SERVICES.  INTEL WILL NOT PROVIDE ANY
 * UPDATES, ENHANCEMENTS OR EXTENSIONS.  INTEL SPECIFICALLY DISCLAIMS ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGEMENT AND FITNESS FOR A
 * PARTICULAR PURPOSE.
 *
 * 4.2. IN NO EVENT SHALL INTEL HAVE ANY LIABILITY TO LICENSEE, ITS LICENSEES
 * OR ANY OTHER THIRD PARTY, FOR ANY LOST PROFITS, LOST DATA, LOSS OF USE OR
 * COSTS OF PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES, OR FOR ANY INDIRECT,
 * SPECIAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THIS AGREEMENT, UNDER ANY
 * CAUSE OF ACTION OR THEORY OF LIABILITY, AND IRRESPECTIVE OF WHETHER INTEL
 * HAS ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES.  THESE LIMITATIONS
 * SHALL APPLY NOTWITHSTANDING THE FAILURE OF THE ESSENTIAL PURPOSE OF ANY
 * LIMITED REMEDY.
 *
 * 4.3. Licensee shall not export, either directly or indirectly, any of this
 * software or system incorporating such software without first obtaining any
 * required license or other approval from the U. S. Department of Commerce or
 * any other agency or department of the United States Government.  In the
 * event Licensee exports any such software from the United States or
 * re-exports any such software from a foreign destination, Licensee shall
 * ensure that the distribution and export/re-export of the software is in
 * compliance with all laws, regulations, orders, or other restrictions of the
 * U.S. Export Administration Regulations. Licensee agrees that neither it nor
 * any of its subsidiaries will export/re-export any technical data, process,
 * software, or service, directly or indirectly, to any country for which the
 * United States government or any agency thereof requires an export license,
 * other governmental approval, or letter of assurance, without first obtaining
 * such license, approval or letter.
 *
 *****************************************************************************/

#ifndef __ACDISASM_H__
#define __ACDISASM_H__

#include <contrib/dev/acpica/amlresrc.h>


#define BLOCK_NONE              0
#define BLOCK_PAREN             1
#define BLOCK_BRACE             2
#define BLOCK_COMMA_LIST        4

typedef struct acpi_external_list
{
    char                        *Path;
    struct acpi_external_list   *Next;

} ACPI_EXTERNAL_LIST;

extern ACPI_EXTERNAL_LIST       *AcpiGbl_ExternalList;
extern const char               *AcpiGbl_IoDecode[2];
extern const char               *AcpiGbl_WordDecode[4];
extern const char               *AcpiGbl_ConsumeDecode[2];
extern const char               *AcpiGbl_MinDecode[2];
extern const char               *AcpiGbl_MaxDecode[2];
extern const char               *AcpiGbl_DECDecode[2];
extern const char               *AcpiGbl_RNGDecode[4];
extern const char               *AcpiGbl_MEMDecode[4];
extern const char               *AcpiGbl_RWDecode[2];
extern const char               *AcpiGbl_IrqDecode[2];
extern const char               *AcpiGbl_HEDecode[2];
extern const char               *AcpiGbl_LLDecode[2];
extern const char               *AcpiGbl_SHRDecode[2];
extern const char               *AcpiGbl_TYPDecode[4];
extern const char               *AcpiGbl_BMDecode[2];
extern const char               *AcpiGbl_SIZDecode[4];
extern const char               *AcpiGbl_LockRule[ACPI_NUM_LOCK_RULES];
extern const char               *AcpiGbl_AccessTypes[ACPI_NUM_ACCESS_TYPES];
extern const char               *AcpiGbl_UpdateRules[ACPI_NUM_UPDATE_RULES];
extern const char               *AcpiGbl_MatchOps[ACPI_NUM_MATCH_OPS];


typedef struct acpi_op_walk_info
{
    UINT32                  Level;
    UINT32                  BitOffset;

} ACPI_OP_WALK_INFO;

typedef
ACPI_STATUS (*ASL_WALK_CALLBACK) (
    ACPI_PARSE_OBJECT           *Op,
    UINT32                      Level,
    void                        *Context);


/*
 * dmwalk
 */

void
AcpiDmWalkParseTree (
    ACPI_PARSE_OBJECT       *Op,
    ASL_WALK_CALLBACK       DescendingCallback,
    ASL_WALK_CALLBACK       AscendingCallback,
    void                    *Context);

ACPI_STATUS
AcpiDmDescendingOp (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);

ACPI_STATUS
AcpiDmAscendingOp (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);


/*
 * dmopcode
 */

void
AcpiDmValidateName (
    char                    *Name,
    ACPI_PARSE_OBJECT       *Op);

UINT32
AcpiDmDumpName (
    char                    *Name);

void
AcpiDmUnicode (
    ACPI_PARSE_OBJECT       *Op);

void
AcpiDmDisassemble (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Origin,
    UINT32                  NumOpcodes);

void
AcpiDmNamestring (
    char                    *Name);

void
AcpiDmDisplayPath (
    ACPI_PARSE_OBJECT       *Op);

void
AcpiDmDisassembleOneOp (
    ACPI_WALK_STATE         *WalkState,
    ACPI_OP_WALK_INFO       *Info,
    ACPI_PARSE_OBJECT       *Op);

void
AcpiDmDecodeInternalObject (
    ACPI_OPERAND_OBJECT     *ObjDesc);

UINT32
AcpiDmBlockType (
    ACPI_PARSE_OBJECT       *Op);

UINT32
AcpiDmListType (
    ACPI_PARSE_OBJECT       *Op);

ACPI_STATUS
AcpiPsDisplayObjectPathname (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op);

void
AcpiDmMethodFlags (
    ACPI_PARSE_OBJECT       *Op);

void
AcpiDmFieldFlags (
    ACPI_PARSE_OBJECT       *Op);

void
AcpiDmAddressSpace (
    UINT8                   SpaceId);

void
AcpiDmRegionFlags (
    ACPI_PARSE_OBJECT       *Op);

void
AcpiDmMatchOp (
    ACPI_PARSE_OBJECT       *Op);

void
AcpiDmMatchKeyword (
    ACPI_PARSE_OBJECT       *Op);

BOOLEAN
AcpiDmCommaIfListMember (
    ACPI_PARSE_OBJECT       *Op);

void
AcpiDmCommaIfFieldMember (
    ACPI_PARSE_OBJECT       *Op);


/*
 * dmobject
 */

void
AcpiDmDecodeNode (
    ACPI_NAMESPACE_NODE     *Node);

void
AcpiDmDisplayInternalObject (
    ACPI_OPERAND_OBJECT     *ObjDesc,
    ACPI_WALK_STATE         *WalkState);

void
AcpiDmDisplayArguments (
    ACPI_WALK_STATE         *WalkState);

void
AcpiDmDisplayLocals (
    ACPI_WALK_STATE         *WalkState);

void
AcpiDmDumpMethodInfo (
    ACPI_STATUS             Status,
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op);


/*
 * dmbuffer
 */

void
AcpiIsEisaId (
    ACPI_PARSE_OBJECT       *Op);

void
AcpiDmEisaId (
    UINT32                  EncodedId);

BOOLEAN
AcpiDmIsUnicodeBuffer (
    ACPI_PARSE_OBJECT       *Op);

BOOLEAN
AcpiDmIsStringBuffer (
    ACPI_PARSE_OBJECT       *Op);


/*
 * dmresrc
 */

void
AcpiDmDisasmByteList (
    UINT32                  Level,
    UINT8                   *ByteData,
    UINT32                  ByteCount);

void
AcpiDmByteList (
    ACPI_OP_WALK_INFO       *Info,
    ACPI_PARSE_OBJECT       *Op);

void
AcpiDmResourceDescriptor (
    ACPI_OP_WALK_INFO       *Info,
    UINT8                   *ByteData,
    UINT32                  ByteCount);

BOOLEAN
AcpiDmIsResourceDescriptor (
    ACPI_PARSE_OBJECT       *Op);

void
AcpiDmIndent (
    UINT32                  Level);

void
AcpiDmBitList (
    UINT16                  Mask);

void
AcpiDmDecodeAttribute (
    UINT8                   Attribute);

/*
 * dmresrcl
 */

void
AcpiDmIoFlags (
        UINT8               Flags);

void
AcpiDmMemoryFlags (
    UINT8                   Flags,
    UINT8                   SpecificFlags);

void
AcpiDmWordDescriptor (
    ASL_WORD_ADDRESS_DESC   *Resource,
    UINT32                  Length,
    UINT32                  Level);

void
AcpiDmDwordDescriptor (
    ASL_DWORD_ADDRESS_DESC  *Resource,
    UINT32                  Length,
    UINT32                  Level);

void
AcpiDmQwordDescriptor (
    ASL_QWORD_ADDRESS_DESC  *Resource,
    UINT32                  Length,
    UINT32                  Level);

void
AcpiDmMemory24Descriptor (
    ASL_MEMORY_24_DESC      *Resource,
    UINT32                  Length,
    UINT32                  Level);

void
AcpiDmMemory32Descriptor (
    ASL_MEMORY_32_DESC      *Resource,
    UINT32                  Length,
    UINT32                  Level);

void
AcpiDmFixedMem32Descriptor (
    ASL_FIXED_MEMORY_32_DESC *Resource,
    UINT32                  Length,
    UINT32                  Level);

void
AcpiDmGenericRegisterDescriptor (
    ASL_GENERAL_REGISTER_DESC *Resource,
    UINT32                  Length,
    UINT32                  Level);

void
AcpiDmInterruptDescriptor (
    ASL_EXTENDED_XRUPT_DESC *Resource,
    UINT32                  Length,
    UINT32                  Level);

void
AcpiDmVendorLargeDescriptor (
    ASL_LARGE_VENDOR_DESC   *Resource,
    UINT32                  Length,
    UINT32                  Level);


/*
 * dmresrcs
 */

void
AcpiDmIrqDescriptor (
    ASL_IRQ_FORMAT_DESC     *Resource,
    UINT32                  Length,
    UINT32                  Level);

void
AcpiDmDmaDescriptor (
    ASL_DMA_FORMAT_DESC     *Resource,
    UINT32                  Length,
    UINT32                  Level);

void
AcpiDmIoDescriptor (
    ASL_IO_PORT_DESC        *Resource,
    UINT32                  Length,
    UINT32                  Level);

void
AcpiDmFixedIoDescriptor (
    ASL_FIXED_IO_PORT_DESC  *Resource,
    UINT32                  Length,
    UINT32                  Level);

void
AcpiDmStartDependentDescriptor (
    ASL_START_DEPENDENT_DESC *Resource,
    UINT32                  Length,
    UINT32                  Level);

void
AcpiDmEndDependentDescriptor (
    ASL_START_DEPENDENT_DESC *Resource,
    UINT32                  Length,
    UINT32                  Level);

void
AcpiDmVendorSmallDescriptor (
    ASL_SMALL_VENDOR_DESC   *Resource,
    UINT32                  Length,
    UINT32                  Level);


/*
 * dmutils
 */

void
AcpiDmAddToExternalList (
    char                    *Path);

#endif  /* __ACDISASM_H__ */
