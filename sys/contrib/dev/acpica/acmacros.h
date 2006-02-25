/******************************************************************************
 *
 * Name: acmacros.h - C macros for the entire subsystem.
 *       $Revision: 1.1.1.1 $
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

#ifndef __ACMACROS_H__
#define __ACMACROS_H__


/*
 * Data manipulation macros
 */
#define ACPI_LOWORD(l)                  ((UINT16)(UINT32)(l))
#define ACPI_HIWORD(l)                  ((UINT16)((((UINT32)(l)) >> 16) & 0xFFFF))
#define ACPI_LOBYTE(l)                  ((UINT8)(UINT16)(l))
#define ACPI_HIBYTE(l)                  ((UINT8)((((UINT16)(l)) >> 8) & 0xFF))

#define ACPI_SET_BIT(target,bit)        ((target) |= (bit))
#define ACPI_CLEAR_BIT(target,bit)      ((target) &= ~(bit))


#if ACPI_MACHINE_WIDTH == 16

/*
 * For 16-bit addresses, we have to assume that the upper 32 bits
 * are zero.
 */
#define ACPI_LODWORD(l)                 ((UINT32)(l))
#define ACPI_HIDWORD(l)                 ((UINT32)(0))

#define ACPI_GET_ADDRESS(a)             ((a).Lo)
#define ACPI_STORE_ADDRESS(a,b)         {(a).Hi=0;(a).Lo=(UINT32)(b);}
#define ACPI_VALID_ADDRESS(a)           ((a).Hi | (a).Lo)

#else
#ifdef ACPI_NO_INTEGER64_SUPPORT
/*
 * ACPI_INTEGER is 32-bits, no 64-bit support on this platform
 */
#define ACPI_LODWORD(l)                 ((UINT32)(l))
#define ACPI_HIDWORD(l)                 ((UINT32)(0))

#define ACPI_GET_ADDRESS(a)             (a)
#define ACPI_STORE_ADDRESS(a,b)         ((a)=(b))
#define ACPI_VALID_ADDRESS(a)           (a)

#else

/*
 * Full 64-bit address/integer on both 32-bit and 64-bit platforms
 */
#define ACPI_LODWORD(l)                 ((UINT32)(UINT64)(l))
#define ACPI_HIDWORD(l)                 ((UINT32)(((*(UINT64_STRUCT *)(void *)(&l))).Hi))

#define ACPI_GET_ADDRESS(a)             (a)
#define ACPI_STORE_ADDRESS(a,b)         ((a)=(ACPI_PHYSICAL_ADDRESS)(b))
#define ACPI_VALID_ADDRESS(a)           (a)
#endif
#endif

/*
 * printf() format helpers
 */

/* Split 64-bit integer into two 32-bit values. Use with %8.8X%8.8X */

#define ACPI_FORMAT_UINT64(i)           ACPI_HIDWORD(i),ACPI_LODWORD(i)

/*
 * Extract a byte of data using a pointer.  Any more than a byte and we
 * get into potential aligment issues -- see the STORE macros below
 */
#define ACPI_GET8(addr)                 (*(UINT8*)(addr))

/* Pointer arithmetic */

#define ACPI_PTR_ADD(t,a,b)             (t *) (void *)((char *)(a) + (ACPI_NATIVE_UINT)(b))
#define ACPI_PTR_DIFF(a,b)              (ACPI_NATIVE_UINT) ((char *)(a) - (char *)(b))

/* Pointer/Integer type conversions */

#define ACPI_TO_POINTER(i)              ACPI_PTR_ADD (void, (void *) NULL,(ACPI_NATIVE_UINT)i)
#define ACPI_TO_INTEGER(p)              ACPI_PTR_DIFF (p,(void *) NULL)
#define ACPI_OFFSET(d,f)                (ACPI_SIZE) ACPI_PTR_DIFF (&(((d *)0)->f),(void *) NULL)
#define ACPI_FADT_OFFSET(f)             ACPI_OFFSET (FADT_DESCRIPTOR, f)

#define ACPI_CAST_PTR(t, p)             ((t *)(void *)(p))
#define ACPI_CAST_INDIRECT_PTR(t, p)    ((t **)(void *)(p))

#if ACPI_MACHINE_WIDTH == 16
#define ACPI_STORE_POINTER(d,s)         ACPI_MOVE_32_TO_32(d,s)
#define ACPI_PHYSADDR_TO_PTR(i)         (void *)(i)
#define ACPI_PTR_TO_PHYSADDR(i)         (UINT32) (char *)(i)
#else
#define ACPI_PHYSADDR_TO_PTR(i)         ACPI_TO_POINTER(i)
#define ACPI_PTR_TO_PHYSADDR(i)         ACPI_TO_INTEGER(i)
#endif

/*
 * Macros for moving data around to/from buffers that are possibly unaligned.
 * If the hardware supports the transfer of unaligned data, just do the store.
 * Otherwise, we have to move one byte at a time.
 */
#ifdef ACPI_BIG_ENDIAN
/*
 * Macros for big-endian machines
 */

/* This macro sets a buffer index, starting from the end of the buffer */

#define ACPI_BUFFER_INDEX(BufLen,BufOffset,ByteGran)  ((BufLen) - (((BufOffset)+1) * (ByteGran)))

/* These macros reverse the bytes during the move, converting little-endian to big endian */

                                                     /* Big Endian      <==        Little Endian */
                                                     /*  Hi...Lo                     Lo...Hi     */
/* 16-bit source, 16/32/64 destination */

#define ACPI_MOVE_16_TO_16(d,s)         {((  UINT8 *)(void *)(d))[0] = ((UINT8 *)(void *)(s))[1];\
                                         ((  UINT8 *)(void *)(d))[1] = ((UINT8 *)(void *)(s))[0];}

#define ACPI_MOVE_16_TO_32(d,s)         {(*(UINT32 *)(void *)(d))=0;\
                                           ((UINT8 *)(void *)(d))[2] = ((UINT8 *)(void *)(s))[1];\
                                           ((UINT8 *)(void *)(d))[3] = ((UINT8 *)(void *)(s))[0];}

#define ACPI_MOVE_16_TO_64(d,s)         {(*(UINT64 *)(void *)(d))=0;\
                                           ((UINT8 *)(void *)(d))[6] = ((UINT8 *)(void *)(s))[1];\
                                           ((UINT8 *)(void *)(d))[7] = ((UINT8 *)(void *)(s))[0];}

/* 32-bit source, 16/32/64 destination */

#define ACPI_MOVE_32_TO_16(d,s)         ACPI_MOVE_16_TO_16(d,s)    /* Truncate to 16 */

#define ACPI_MOVE_32_TO_32(d,s)         {((  UINT8 *)(void *)(d))[0] = ((UINT8 *)(void *)(s))[3];\
                                         ((  UINT8 *)(void *)(d))[1] = ((UINT8 *)(void *)(s))[2];\
                                         ((  UINT8 *)(void *)(d))[2] = ((UINT8 *)(void *)(s))[1];\
                                         ((  UINT8 *)(void *)(d))[3] = ((UINT8 *)(void *)(s))[0];}

#define ACPI_MOVE_32_TO_64(d,s)         {(*(UINT64 *)(void *)(d))=0;\
                                           ((UINT8 *)(void *)(d))[4] = ((UINT8 *)(void *)(s))[3];\
                                           ((UINT8 *)(void *)(d))[5] = ((UINT8 *)(void *)(s))[2];\
                                           ((UINT8 *)(void *)(d))[6] = ((UINT8 *)(void *)(s))[1];\
                                           ((UINT8 *)(void *)(d))[7] = ((UINT8 *)(void *)(s))[0];}

/* 64-bit source, 16/32/64 destination */

#define ACPI_MOVE_64_TO_16(d,s)         ACPI_MOVE_16_TO_16(d,s)    /* Truncate to 16 */

#define ACPI_MOVE_64_TO_32(d,s)         ACPI_MOVE_32_TO_32(d,s)    /* Truncate to 32 */

#define ACPI_MOVE_64_TO_64(d,s)         {((  UINT8 *)(void *)(d))[0] = ((UINT8 *)(void *)(s))[7];\
                                         ((  UINT8 *)(void *)(d))[1] = ((UINT8 *)(void *)(s))[6];\
                                         ((  UINT8 *)(void *)(d))[2] = ((UINT8 *)(void *)(s))[5];\
                                         ((  UINT8 *)(void *)(d))[3] = ((UINT8 *)(void *)(s))[4];\
                                         ((  UINT8 *)(void *)(d))[4] = ((UINT8 *)(void *)(s))[3];\
                                         ((  UINT8 *)(void *)(d))[5] = ((UINT8 *)(void *)(s))[2];\
                                         ((  UINT8 *)(void *)(d))[6] = ((UINT8 *)(void *)(s))[1];\
                                         ((  UINT8 *)(void *)(d))[7] = ((UINT8 *)(void *)(s))[0];}
#else
/*
 * Macros for little-endian machines
 */

/* This macro sets a buffer index, starting from the beginning of the buffer */

#define ACPI_BUFFER_INDEX(BufLen,BufOffset,ByteGran)  (BufOffset)

#ifdef ACPI_MISALIGNED_TRANSFERS

/* The hardware supports unaligned transfers, just do the little-endian move */

#if ACPI_MACHINE_WIDTH == 16

/* No 64-bit integers */
/* 16-bit source, 16/32/64 destination */

#define ACPI_MOVE_16_TO_16(d,s)         *(UINT16 *)(void *)(d) = *(UINT16 *)(void *)(s)
#define ACPI_MOVE_16_TO_32(d,s)         *(UINT32 *)(void *)(d) = *(UINT16 *)(void *)(s)
#define ACPI_MOVE_16_TO_64(d,s)         ACPI_MOVE_16_TO_32(d,s)

/* 32-bit source, 16/32/64 destination */

#define ACPI_MOVE_32_TO_16(d,s)         ACPI_MOVE_16_TO_16(d,s)    /* Truncate to 16 */
#define ACPI_MOVE_32_TO_32(d,s)         *(UINT32 *)(void *)(d) = *(UINT32 *)(void *)(s)
#define ACPI_MOVE_32_TO_64(d,s)         ACPI_MOVE_32_TO_32(d,s)

/* 64-bit source, 16/32/64 destination */

#define ACPI_MOVE_64_TO_16(d,s)         ACPI_MOVE_16_TO_16(d,s)    /* Truncate to 16 */
#define ACPI_MOVE_64_TO_32(d,s)         ACPI_MOVE_32_TO_32(d,s)    /* Truncate to 32 */
#define ACPI_MOVE_64_TO_64(d,s)         ACPI_MOVE_32_TO_32(d,s)

#else
/* 16-bit source, 16/32/64 destination */

#define ACPI_MOVE_16_TO_16(d,s)         *(UINT16 *)(void *)(d) = *(UINT16 *)(void *)(s)
#define ACPI_MOVE_16_TO_32(d,s)         *(UINT32 *)(void *)(d) = *(UINT16 *)(void *)(s)
#define ACPI_MOVE_16_TO_64(d,s)         *(UINT64 *)(void *)(d) = *(UINT16 *)(void *)(s)

/* 32-bit source, 16/32/64 destination */

#define ACPI_MOVE_32_TO_16(d,s)         ACPI_MOVE_16_TO_16(d,s)    /* Truncate to 16 */
#define ACPI_MOVE_32_TO_32(d,s)         *(UINT32 *)(void *)(d) = *(UINT32 *)(void *)(s)
#define ACPI_MOVE_32_TO_64(d,s)         *(UINT64 *)(void *)(d) = *(UINT32 *)(void *)(s)

/* 64-bit source, 16/32/64 destination */

#define ACPI_MOVE_64_TO_16(d,s)         ACPI_MOVE_16_TO_16(d,s)    /* Truncate to 16 */
#define ACPI_MOVE_64_TO_32(d,s)         ACPI_MOVE_32_TO_32(d,s)    /* Truncate to 32 */
#define ACPI_MOVE_64_TO_64(d,s)         *(UINT64 *)(void *)(d) = *(UINT64 *)(void *)(s)
#endif

#else
/*
 * The hardware does not support unaligned transfers.  We must move the
 * data one byte at a time.  These macros work whether the source or
 * the destination (or both) is/are unaligned.  (Little-endian move)
 */

/* 16-bit source, 16/32/64 destination */

#define ACPI_MOVE_16_TO_16(d,s)         {((  UINT8 *)(void *)(d))[0] = ((UINT8 *)(void *)(s))[0];\
                                         ((  UINT8 *)(void *)(d))[1] = ((UINT8 *)(void *)(s))[1];}

#define ACPI_MOVE_16_TO_32(d,s)         {(*(UINT32 *)(void *)(d)) = 0; ACPI_MOVE_16_TO_16(d,s);}
#define ACPI_MOVE_16_TO_64(d,s)         {(*(UINT64 *)(void *)(d)) = 0; ACPI_MOVE_16_TO_16(d,s);}

/* 32-bit source, 16/32/64 destination */

#define ACPI_MOVE_32_TO_16(d,s)         ACPI_MOVE_16_TO_16(d,s)    /* Truncate to 16 */

#define ACPI_MOVE_32_TO_32(d,s)         {((  UINT8 *)(void *)(d))[0] = ((UINT8 *)(void *)(s))[0];\
                                         ((  UINT8 *)(void *)(d))[1] = ((UINT8 *)(void *)(s))[1];\
                                         ((  UINT8 *)(void *)(d))[2] = ((UINT8 *)(void *)(s))[2];\
                                         ((  UINT8 *)(void *)(d))[3] = ((UINT8 *)(void *)(s))[3];}

#define ACPI_MOVE_32_TO_64(d,s)         {(*(UINT64 *)(void *)(d)) = 0; ACPI_MOVE_32_TO_32(d,s);}

/* 64-bit source, 16/32/64 destination */

#define ACPI_MOVE_64_TO_16(d,s)         ACPI_MOVE_16_TO_16(d,s)    /* Truncate to 16 */
#define ACPI_MOVE_64_TO_32(d,s)         ACPI_MOVE_32_TO_32(d,s)    /* Truncate to 32 */
#define ACPI_MOVE_64_TO_64(d,s)         {((  UINT8 *)(void *)(d))[0] = ((UINT8 *)(void *)(s))[0];\
                                         ((  UINT8 *)(void *)(d))[1] = ((UINT8 *)(void *)(s))[1];\
                                         ((  UINT8 *)(void *)(d))[2] = ((UINT8 *)(void *)(s))[2];\
                                         ((  UINT8 *)(void *)(d))[3] = ((UINT8 *)(void *)(s))[3];\
                                         ((  UINT8 *)(void *)(d))[4] = ((UINT8 *)(void *)(s))[4];\
                                         ((  UINT8 *)(void *)(d))[5] = ((UINT8 *)(void *)(s))[5];\
                                         ((  UINT8 *)(void *)(d))[6] = ((UINT8 *)(void *)(s))[6];\
                                         ((  UINT8 *)(void *)(d))[7] = ((UINT8 *)(void *)(s))[7];}
#endif
#endif

/* Macros based on machine integer width */

#if ACPI_MACHINE_WIDTH == 16
#define ACPI_MOVE_SIZE_TO_16(d,s)       ACPI_MOVE_16_TO_16(d,s)

#elif ACPI_MACHINE_WIDTH == 32
#define ACPI_MOVE_SIZE_TO_16(d,s)       ACPI_MOVE_32_TO_16(d,s)

#elif ACPI_MACHINE_WIDTH == 64
#define ACPI_MOVE_SIZE_TO_16(d,s)       ACPI_MOVE_64_TO_16(d,s)

#else
#error unknown ACPI_MACHINE_WIDTH
#endif


/*
 * Fast power-of-two math macros for non-optimized compilers
 */
#define _ACPI_DIV(value,PowerOf2)       ((UINT32) ((value) >> (PowerOf2)))
#define _ACPI_MUL(value,PowerOf2)       ((UINT32) ((value) << (PowerOf2)))
#define _ACPI_MOD(value,Divisor)        ((UINT32) ((value) & ((Divisor) -1)))

#define ACPI_DIV_2(a)                   _ACPI_DIV(a,1)
#define ACPI_MUL_2(a)                   _ACPI_MUL(a,1)
#define ACPI_MOD_2(a)                   _ACPI_MOD(a,2)

#define ACPI_DIV_4(a)                   _ACPI_DIV(a,2)
#define ACPI_MUL_4(a)                   _ACPI_MUL(a,2)
#define ACPI_MOD_4(a)                   _ACPI_MOD(a,4)

#define ACPI_DIV_8(a)                   _ACPI_DIV(a,3)
#define ACPI_MUL_8(a)                   _ACPI_MUL(a,3)
#define ACPI_MOD_8(a)                   _ACPI_MOD(a,8)

#define ACPI_DIV_16(a)                  _ACPI_DIV(a,4)
#define ACPI_MUL_16(a)                  _ACPI_MUL(a,4)
#define ACPI_MOD_16(a)                  _ACPI_MOD(a,16)


/*
 * Rounding macros (Power of two boundaries only)
 */
#define ACPI_ROUND_DOWN(value,boundary)      (((ACPI_NATIVE_UINT)(value)) & (~(((ACPI_NATIVE_UINT) boundary)-1)))
#define ACPI_ROUND_UP(value,boundary)        ((((ACPI_NATIVE_UINT)(value)) + (((ACPI_NATIVE_UINT) boundary)-1)) & (~(((ACPI_NATIVE_UINT) boundary)-1)))

#define ACPI_ROUND_DOWN_TO_32_BITS(a)        ACPI_ROUND_DOWN(a,4)
#define ACPI_ROUND_DOWN_TO_64_BITS(a)        ACPI_ROUND_DOWN(a,8)
#define ACPI_ROUND_DOWN_TO_NATIVE_WORD(a)    ACPI_ROUND_DOWN(a,ALIGNED_ADDRESS_BOUNDARY)

#define ACPI_ROUND_UP_TO_32BITS(a)           ACPI_ROUND_UP(a,4)
#define ACPI_ROUND_UP_TO_64BITS(a)           ACPI_ROUND_UP(a,8)
#define ACPI_ROUND_UP_TO_NATIVE_WORD(a)      ACPI_ROUND_UP(a,ALIGNED_ADDRESS_BOUNDARY)


#define ACPI_ROUND_BITS_UP_TO_BYTES(a)       ACPI_DIV_8((a) + 7)
#define ACPI_ROUND_BITS_DOWN_TO_BYTES(a)     ACPI_DIV_8((a))

#define ACPI_ROUND_UP_TO_1K(a)               (((a) + 1023) >> 10)

/* Generic (non-power-of-two) rounding */

#define ACPI_ROUND_UP_TO(value,boundary)     (((value) + ((boundary)-1)) / (boundary))

/*
 * Bitmask creation
 * Bit positions start at zero.
 * MASK_BITS_ABOVE creates a mask starting AT the position and above
 * MASK_BITS_BELOW creates a mask starting one bit BELOW the position
 */
#define ACPI_MASK_BITS_ABOVE(position)       (~((ACPI_INTEGER_MAX) << ((UINT32) (position))))
#define ACPI_MASK_BITS_BELOW(position)       ((ACPI_INTEGER_MAX) << ((UINT32) (position)))

#define ACPI_IS_OCTAL_DIGIT(d)               (((char)(d) >= '0') && ((char)(d) <= '7'))


/* Bitfields within ACPI registers */

#define ACPI_REGISTER_PREPARE_BITS(Val, Pos, Mask)      ((Val << Pos) & Mask)
#define ACPI_REGISTER_INSERT_VALUE(Reg, Pos, Mask, Val)  Reg = (Reg & (~(Mask))) | ACPI_REGISTER_PREPARE_BITS(Val, Pos, Mask)

/*
 * An ACPI_NAMESPACE_NODE * can appear in some contexts,
 * where a pointer to an ACPI_OPERAND_OBJECT  can also
 * appear.  This macro is used to distinguish them.
 *
 * The "Descriptor" field is the first field in both structures.
 */
#define ACPI_GET_DESCRIPTOR_TYPE(d)     (((ACPI_DESCRIPTOR *)(void *)(d))->DescriptorId)
#define ACPI_SET_DESCRIPTOR_TYPE(d,t)   (((ACPI_DESCRIPTOR *)(void *)(d))->DescriptorId = t)


/* Macro to test the object type */

#define ACPI_GET_OBJECT_TYPE(d)         (((ACPI_OPERAND_OBJECT *)(void *)(d))->Common.Type)

/* Macro to check the table flags for SINGLE or MULTIPLE tables are allowed */

#define ACPI_IS_SINGLE_TABLE(x)         (((x) & 0x01) == ACPI_TABLE_SINGLE ? 1 : 0)

/*
 * Macros for the master AML opcode table
 */
#if defined(ACPI_DISASSEMBLER) || defined (ACPI_DEBUG_OUTPUT)
#define ACPI_OP(Name,PArgs,IArgs,ObjType,Class,Type,Flags)     {Name,(UINT32)(PArgs),(UINT32)(IArgs),(UINT32)(Flags),ObjType,Class,Type}
#else
#define ACPI_OP(Name,PArgs,IArgs,ObjType,Class,Type,Flags)     {(UINT32)(PArgs),(UINT32)(IArgs),(UINT32)(Flags),ObjType,Class,Type}
#endif

#ifdef ACPI_DISASSEMBLER
#define ACPI_DISASM_ONLY_MEMBERS(a)     a;
#else
#define ACPI_DISASM_ONLY_MEMBERS(a)
#endif

#define ARG_TYPE_WIDTH                  5
#define ARG_1(x)                        ((UINT32)(x))
#define ARG_2(x)                        ((UINT32)(x) << (1 * ARG_TYPE_WIDTH))
#define ARG_3(x)                        ((UINT32)(x) << (2 * ARG_TYPE_WIDTH))
#define ARG_4(x)                        ((UINT32)(x) << (3 * ARG_TYPE_WIDTH))
#define ARG_5(x)                        ((UINT32)(x) << (4 * ARG_TYPE_WIDTH))
#define ARG_6(x)                        ((UINT32)(x) << (5 * ARG_TYPE_WIDTH))

#define ARGI_LIST1(a)                   (ARG_1(a))
#define ARGI_LIST2(a,b)                 (ARG_1(b)|ARG_2(a))
#define ARGI_LIST3(a,b,c)               (ARG_1(c)|ARG_2(b)|ARG_3(a))
#define ARGI_LIST4(a,b,c,d)             (ARG_1(d)|ARG_2(c)|ARG_3(b)|ARG_4(a))
#define ARGI_LIST5(a,b,c,d,e)           (ARG_1(e)|ARG_2(d)|ARG_3(c)|ARG_4(b)|ARG_5(a))
#define ARGI_LIST6(a,b,c,d,e,f)         (ARG_1(f)|ARG_2(e)|ARG_3(d)|ARG_4(c)|ARG_5(b)|ARG_6(a))

#define ARGP_LIST1(a)                   (ARG_1(a))
#define ARGP_LIST2(a,b)                 (ARG_1(a)|ARG_2(b))
#define ARGP_LIST3(a,b,c)               (ARG_1(a)|ARG_2(b)|ARG_3(c))
#define ARGP_LIST4(a,b,c,d)             (ARG_1(a)|ARG_2(b)|ARG_3(c)|ARG_4(d))
#define ARGP_LIST5(a,b,c,d,e)           (ARG_1(a)|ARG_2(b)|ARG_3(c)|ARG_4(d)|ARG_5(e))
#define ARGP_LIST6(a,b,c,d,e,f)         (ARG_1(a)|ARG_2(b)|ARG_3(c)|ARG_4(d)|ARG_5(e)|ARG_6(f))

#define GET_CURRENT_ARG_TYPE(List)      (List & ((UINT32) 0x1F))
#define INCREMENT_ARG_LIST(List)        (List >>= ((UINT32) ARG_TYPE_WIDTH))


/*
 * Reporting macros that are never compiled out
 */
#define ACPI_PARAM_LIST(pl)                 pl

/*
 * Error reporting.  These versions add callers module and line#.  Since
 * _THIS_MODULE gets compiled out when ACPI_DEBUG_OUTPUT isn't defined, only
 * use it in debug mode.
 */
#ifdef ACPI_DEBUG_OUTPUT

#define ACPI_REPORT_INFO(fp)                {AcpiUtReportInfo(_THIS_MODULE,__LINE__,_COMPONENT); \
                                                AcpiOsPrintf ACPI_PARAM_LIST(fp);}
#define ACPI_REPORT_ERROR(fp)               {AcpiUtReportError(_THIS_MODULE,__LINE__,_COMPONENT); \
                                                AcpiOsPrintf ACPI_PARAM_LIST(fp);}
#define ACPI_REPORT_WARNING(fp)             {AcpiUtReportWarning(_THIS_MODULE,__LINE__,_COMPONENT); \
                                                AcpiOsPrintf ACPI_PARAM_LIST(fp);}
#define ACPI_REPORT_NSERROR(s,e)            AcpiNsReportError(_THIS_MODULE,__LINE__,_COMPONENT, s, e);

#define ACPI_REPORT_METHOD_ERROR(s,n,p,e)   AcpiNsReportMethodError(_THIS_MODULE,__LINE__,_COMPONENT, s, n, p, e);

#else

#define ACPI_REPORT_INFO(fp)                {AcpiUtReportInfo("ACPI",__LINE__,_COMPONENT); \
                                                AcpiOsPrintf ACPI_PARAM_LIST(fp);}
#define ACPI_REPORT_ERROR(fp)               {AcpiUtReportError("ACPI",__LINE__,_COMPONENT); \
                                                AcpiOsPrintf ACPI_PARAM_LIST(fp);}
#define ACPI_REPORT_WARNING(fp)             {AcpiUtReportWarning("ACPI",__LINE__,_COMPONENT); \
                                                AcpiOsPrintf ACPI_PARAM_LIST(fp);}
#define ACPI_REPORT_NSERROR(s,e)            AcpiNsReportError("ACPI",__LINE__,_COMPONENT, s, e);

#define ACPI_REPORT_METHOD_ERROR(s,n,p,e)   AcpiNsReportMethodError("ACPI",__LINE__,_COMPONENT, s, n, p, e);

#endif

/* Error reporting.  These versions pass thru the module and line# */

#define _ACPI_REPORT_INFO(a,b,c,fp)         {AcpiUtReportInfo(a,b,c); \
                                                AcpiOsPrintf ACPI_PARAM_LIST(fp);}
#define _ACPI_REPORT_ERROR(a,b,c,fp)        {AcpiUtReportError(a,b,c); \
                                                AcpiOsPrintf ACPI_PARAM_LIST(fp);}
#define _ACPI_REPORT_WARNING(a,b,c,fp)      {AcpiUtReportWarning(a,b,c); \
                                                AcpiOsPrintf ACPI_PARAM_LIST(fp);}

/*
 * Debug macros that are conditionally compiled
 */
#ifdef ACPI_DEBUG_OUTPUT

#define ACPI_MODULE_NAME(name)               static char ACPI_UNUSED_VAR *_THIS_MODULE = name;

/*
 * Function entry tracing.
 * The first parameter should be the procedure name as a quoted string.  This is declared
 * as a local string ("_ProcName) so that it can be also used by the function exit macros below.
 */
#define ACPI_FUNCTION_NAME(a)               ACPI_DEBUG_PRINT_INFO _DebugInfo; \
                                                _DebugInfo.ComponentId = _COMPONENT; \
                                                _DebugInfo.ProcName    = a; \
                                                _DebugInfo.ModuleName  = _THIS_MODULE;

#define ACPI_FUNCTION_TRACE(a)              ACPI_FUNCTION_NAME(a) \
                                                AcpiUtTrace(__LINE__,&_DebugInfo)
#define ACPI_FUNCTION_TRACE_PTR(a,b)        ACPI_FUNCTION_NAME(a) \
                                                AcpiUtTracePtr(__LINE__,&_DebugInfo,(void *)b)
#define ACPI_FUNCTION_TRACE_U32(a,b)        ACPI_FUNCTION_NAME(a) \
                                                AcpiUtTraceU32(__LINE__,&_DebugInfo,(UINT32)b)
#define ACPI_FUNCTION_TRACE_STR(a,b)        ACPI_FUNCTION_NAME(a) \
                                                AcpiUtTraceStr(__LINE__,&_DebugInfo,(char *)b)

#define ACPI_FUNCTION_ENTRY()               AcpiUtTrackStackPtr()

/*
 * Function exit tracing.
 * WARNING: These macros include a return statement.  This is usually considered
 * bad form, but having a separate exit macro is very ugly and difficult to maintain.
 * One of the FUNCTION_TRACE macros above must be used in conjunction with these macros
 * so that "_ProcName" is defined.
 */
#ifdef ACPI_USE_DO_WHILE_0
#define ACPI_DO_WHILE0(a)               do a while(0)
#else
#define ACPI_DO_WHILE0(a)               a
#endif

#define return_VOID                     ACPI_DO_WHILE0 ({AcpiUtExit(__LINE__,&_DebugInfo);return;})
#define return_ACPI_STATUS(s)           ACPI_DO_WHILE0 ({AcpiUtStatusExit(__LINE__,&_DebugInfo,(s));return((s));})
#define return_VALUE(s)                 ACPI_DO_WHILE0 ({AcpiUtValueExit(__LINE__,&_DebugInfo,(ACPI_INTEGER)(s));return((s));})
#define return_PTR(s)                   ACPI_DO_WHILE0 ({AcpiUtPtrExit(__LINE__,&_DebugInfo,(UINT8 *)(s));return((s));})

/* Conditional execution */

#define ACPI_DEBUG_EXEC(a)              a
#define ACPI_NORMAL_EXEC(a)

#define ACPI_DEBUG_DEFINE(a)            a;
#define ACPI_DEBUG_ONLY_MEMBERS(a)      a;
#define _VERBOSE_STRUCTURES


/* Stack and buffer dumping */

#define ACPI_DUMP_STACK_ENTRY(a)        AcpiExDumpOperand((a),0)
#define ACPI_DUMP_OPERANDS(a,b,c,d,e)   AcpiExDumpOperands(a,b,c,d,e,_THIS_MODULE,__LINE__)


#define ACPI_DUMP_ENTRY(a,b)            AcpiNsDumpEntry (a,b)
#define ACPI_DUMP_TABLES(a,b)           AcpiNsDumpTables(a,b)
#define ACPI_DUMP_PATHNAME(a,b,c,d)     AcpiNsDumpPathname(a,b,c,d)
#define ACPI_DUMP_RESOURCE_LIST(a)      AcpiRsDumpResourceList(a)
#define ACPI_DUMP_BUFFER(a,b)           AcpiUtDumpBuffer((UINT8 *)a,b,DB_BYTE_DISPLAY,_COMPONENT)
#define ACPI_BREAK_MSG(a)               AcpiOsSignal (ACPI_SIGNAL_BREAKPOINT,(a))


/*
 * Generate INT3 on ACPI_ERROR (Debug only!)
 */
#define ACPI_ERROR_BREAK
#ifdef  ACPI_ERROR_BREAK
#define ACPI_BREAK_ON_ERROR(lvl)        if ((lvl)&ACPI_ERROR) \
                                            AcpiOsSignal(ACPI_SIGNAL_BREAKPOINT,"Fatal error encountered\n")
#else
#define ACPI_BREAK_ON_ERROR(lvl)
#endif

/*
 * Master debug print macros
 * Print iff:
 *    1) Debug print for the current component is enabled
 *    2) Debug error level or trace level for the print statement is enabled
 */
#define ACPI_DEBUG_PRINT(pl)            AcpiUtDebugPrint ACPI_PARAM_LIST(pl)
#define ACPI_DEBUG_PRINT_RAW(pl)        AcpiUtDebugPrintRaw ACPI_PARAM_LIST(pl)


#else
/*
 * This is the non-debug case -- make everything go away,
 * leaving no executable debug code!
 */
#define ACPI_MODULE_NAME(name)
#define _THIS_MODULE ""

#define ACPI_DEBUG_EXEC(a)
#define ACPI_NORMAL_EXEC(a)             a;

#define ACPI_DEBUG_DEFINE(a)
#define ACPI_DEBUG_ONLY_MEMBERS(a)
#define ACPI_FUNCTION_NAME(a)
#define ACPI_FUNCTION_TRACE(a)
#define ACPI_FUNCTION_TRACE_PTR(a,b)
#define ACPI_FUNCTION_TRACE_U32(a,b)
#define ACPI_FUNCTION_TRACE_STR(a,b)
#define ACPI_FUNCTION_EXIT
#define ACPI_FUNCTION_STATUS_EXIT(s)
#define ACPI_FUNCTION_VALUE_EXIT(s)
#define ACPI_FUNCTION_ENTRY()
#define ACPI_DUMP_STACK_ENTRY(a)
#define ACPI_DUMP_OPERANDS(a,b,c,d,e)
#define ACPI_DUMP_ENTRY(a,b)
#define ACPI_DUMP_TABLES(a,b)
#define ACPI_DUMP_PATHNAME(a,b,c,d)
#define ACPI_DUMP_RESOURCE_LIST(a)
#define ACPI_DUMP_BUFFER(a,b)
#define ACPI_DEBUG_PRINT(pl)
#define ACPI_DEBUG_PRINT_RAW(pl)
#define ACPI_BREAK_MSG(a)

#define return_VOID                     return
#define return_ACPI_STATUS(s)           return(s)
#define return_VALUE(s)                 return(s)
#define return_PTR(s)                   return(s)

#endif

/*
 * Some code only gets executed when the debugger is built in.
 * Note that this is entirely independent of whether the
 * DEBUG_PRINT stuff (set by ACPI_DEBUG_OUTPUT) is on, or not.
 */
#ifdef ACPI_DEBUGGER
#define ACPI_DEBUGGER_EXEC(a)           a
#else
#define ACPI_DEBUGGER_EXEC(a)
#endif


/*
 * For 16-bit code, we want to shrink some things even though
 * we are using ACPI_DEBUG_OUTPUT to get the debug output
 */
#if ACPI_MACHINE_WIDTH == 16
#undef ACPI_DEBUG_ONLY_MEMBERS
#undef _VERBOSE_STRUCTURES
#define ACPI_DEBUG_ONLY_MEMBERS(a)
#endif


#ifdef ACPI_DEBUG_OUTPUT
/*
 * 1) Set name to blanks
 * 2) Copy the object name
 */
#define ACPI_ADD_OBJECT_NAME(a,b)       ACPI_MEMSET (a->Common.Name, ' ', sizeof (a->Common.Name));\
                                        ACPI_STRNCPY (a->Common.Name, AcpiGbl_NsTypeNames[b], sizeof (a->Common.Name))
#else

#define ACPI_ADD_OBJECT_NAME(a,b)
#endif


/*
 * Memory allocation tracking (DEBUG ONLY)
 */
#ifndef ACPI_DBG_TRACK_ALLOCATIONS

/* Memory allocation */

#define ACPI_MEM_ALLOCATE(a)            AcpiUtAllocate((ACPI_SIZE)(a),_COMPONENT,_THIS_MODULE,__LINE__)
#define ACPI_MEM_CALLOCATE(a)           AcpiUtCallocate((ACPI_SIZE)(a), _COMPONENT,_THIS_MODULE,__LINE__)
#define ACPI_MEM_FREE(a)                AcpiOsFree(a)
#define ACPI_MEM_TRACKING(a)


#else

/* Memory allocation */

#define ACPI_MEM_ALLOCATE(a)            AcpiUtAllocateAndTrack((ACPI_SIZE)(a),_COMPONENT,_THIS_MODULE,__LINE__)
#define ACPI_MEM_CALLOCATE(a)           AcpiUtCallocateAndTrack((ACPI_SIZE)(a), _COMPONENT,_THIS_MODULE,__LINE__)
#define ACPI_MEM_FREE(a)                AcpiUtFreeAndTrack(a,_COMPONENT,_THIS_MODULE,__LINE__)
#define ACPI_MEM_TRACKING(a)            a

#endif /* ACPI_DBG_TRACK_ALLOCATIONS */

#endif /* ACMACROS_H */
