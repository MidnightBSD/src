/******************************************************************************
 *
 * Module Name: psxface - Parser external interfaces
 *              $Revision: 1.93 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2007, Intel Corp.
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

#define __PSXFACE_C__

#include <contrib/dev/acpica/acpi.h>
#include <contrib/dev/acpica/acparser.h>
#include <contrib/dev/acpica/acdispat.h>
#include <contrib/dev/acpica/acinterp.h>


#define _COMPONENT          ACPI_PARSER
        ACPI_MODULE_NAME    ("psxface")

/* Local Prototypes */

static void
AcpiPsStartTrace (
    ACPI_EVALUATE_INFO      *Info);

static void
AcpiPsStopTrace (
    ACPI_EVALUATE_INFO      *Info);

static void
AcpiPsUpdateParameterList (
    ACPI_EVALUATE_INFO      *Info,
    UINT16                  Action);


/*******************************************************************************
 *
 * FUNCTION:    AcpiDebugTrace
 *
 * PARAMETERS:  MethodName      - Valid ACPI name string
 *              DebugLevel      - Optional level mask. 0 to use default
 *              DebugLayer      - Optional layer mask. 0 to use default
 *              Flags           - bit 1: one shot(1) or persistent(0)
 *
 * RETURN:      Status
 *
 * DESCRIPTION: External interface to enable debug tracing during control
 *              method execution
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDebugTrace (
    char                    *Name,
    UINT32                  DebugLevel,
    UINT32                  DebugLayer,
    UINT32                  Flags)
{
    ACPI_STATUS             Status;


    Status = AcpiUtAcquireMutex (ACPI_MTX_NAMESPACE);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    /* TBDs: Validate name, allow full path or just nameseg */

    AcpiGbl_TraceMethodName = *ACPI_CAST_PTR (UINT32, Name);
    AcpiGbl_TraceFlags = Flags;

    if (DebugLevel)
    {
        AcpiGbl_TraceDbgLevel = DebugLevel;
    }
    if (DebugLayer)
    {
        AcpiGbl_TraceDbgLayer = DebugLayer;
    }

    (void) AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE);
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiPsStartTrace
 *
 * PARAMETERS:  Info        - Method info struct
 *
 * RETURN:      None
 *
 * DESCRIPTION: Start control method execution trace
 *
 ******************************************************************************/

static void
AcpiPsStartTrace (
    ACPI_EVALUATE_INFO      *Info)
{
    ACPI_STATUS             Status;


    ACPI_FUNCTION_ENTRY ();


    Status = AcpiUtAcquireMutex (ACPI_MTX_NAMESPACE);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    if ((!AcpiGbl_TraceMethodName) ||
        (AcpiGbl_TraceMethodName != Info->ResolvedNode->Name.Integer))
    {
        goto Exit;
    }

    AcpiGbl_OriginalDbgLevel = AcpiDbgLevel;
    AcpiGbl_OriginalDbgLayer = AcpiDbgLayer;

    AcpiDbgLevel = 0x00FFFFFF;
    AcpiDbgLayer = ACPI_UINT32_MAX;

    if (AcpiGbl_TraceDbgLevel)
    {
        AcpiDbgLevel = AcpiGbl_TraceDbgLevel;
    }
    if (AcpiGbl_TraceDbgLayer)
    {
        AcpiDbgLayer = AcpiGbl_TraceDbgLayer;
    }


Exit:
    (void) AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiPsStopTrace
 *
 * PARAMETERS:  Info        - Method info struct
 *
 * RETURN:      None
 *
 * DESCRIPTION: Stop control method execution trace
 *
 ******************************************************************************/

static void
AcpiPsStopTrace (
    ACPI_EVALUATE_INFO      *Info)
{
    ACPI_STATUS             Status;


    ACPI_FUNCTION_ENTRY ();


    Status = AcpiUtAcquireMutex (ACPI_MTX_NAMESPACE);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    if ((!AcpiGbl_TraceMethodName) ||
        (AcpiGbl_TraceMethodName != Info->ResolvedNode->Name.Integer))
    {
        goto Exit;
    }

    /* Disable further tracing if type is one-shot */

    if (AcpiGbl_TraceFlags & 1)
    {
        AcpiGbl_TraceMethodName = 0;
        AcpiGbl_TraceDbgLevel = 0;
        AcpiGbl_TraceDbgLayer = 0;
    }

    AcpiDbgLevel = AcpiGbl_OriginalDbgLevel;
    AcpiDbgLayer = AcpiGbl_OriginalDbgLayer;

Exit:
    (void) AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiPsExecuteMethod
 *
 * PARAMETERS:  Info            - Method info block, contains:
 *                  Node            - Method Node to execute
 *                  ObjDesc         - Method object
 *                  Parameters      - List of parameters to pass to the method,
 *                                    terminated by NULL. Params itself may be
 *                                    NULL if no parameters are being passed.
 *                  ReturnObject    - Where to put method's return value (if
 *                                    any). If NULL, no value is returned.
 *                  ParameterType   - Type of Parameter list
 *                  ReturnObject    - Where to put method's return value (if
 *                                    any). If NULL, no value is returned.
 *                  PassNumber      - Parse or execute pass
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Execute a control method
 *
 ******************************************************************************/

ACPI_STATUS
AcpiPsExecuteMethod (
    ACPI_EVALUATE_INFO      *Info)
{
    ACPI_STATUS             Status;
    ACPI_PARSE_OBJECT       *Op;
    ACPI_WALK_STATE         *WalkState;


    ACPI_FUNCTION_TRACE (PsExecuteMethod);


    /* Validate the Info and method Node */

    if (!Info || !Info->ResolvedNode)
    {
        return_ACPI_STATUS (AE_NULL_ENTRY);
    }

    /* Init for new method, wait on concurrency semaphore */

    Status = AcpiDsBeginMethodExecution (Info->ResolvedNode, Info->ObjDesc, NULL);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /*
     * The caller "owns" the parameters, so give each one an extra reference
     */
    AcpiPsUpdateParameterList (Info, REF_INCREMENT);

    /* Begin tracing if requested */

    AcpiPsStartTrace (Info);

    /*
     * Execute the method. Performs parse simultaneously
     */
    ACPI_DEBUG_PRINT ((ACPI_DB_PARSE,
        "**** Begin Method Parse/Execute [%4.4s] **** Node=%p Obj=%p\n",
        Info->ResolvedNode->Name.Ascii, Info->ResolvedNode, Info->ObjDesc));

    /* Create and init a Root Node */

    Op = AcpiPsCreateScopeOp ();
    if (!Op)
    {
        Status = AE_NO_MEMORY;
        goto Cleanup;
    }

    /* Create and initialize a new walk state */

    Info->PassNumber = ACPI_IMODE_EXECUTE;
    WalkState = AcpiDsCreateWalkState (
                    Info->ObjDesc->Method.OwnerId, NULL, NULL, NULL);
    if (!WalkState)
    {
        Status = AE_NO_MEMORY;
        goto Cleanup;
    }

    Status = AcpiDsInitAmlWalk (WalkState, Op, Info->ResolvedNode,
                Info->ObjDesc->Method.AmlStart,
                Info->ObjDesc->Method.AmlLength, Info, Info->PassNumber);
    if (ACPI_FAILURE (Status))
    {
        AcpiDsDeleteWalkState (WalkState);
        goto Cleanup;
    }

    /* Parse the AML */

    Status = AcpiPsParseAml (WalkState);

    /* WalkState was deleted by ParseAml */

Cleanup:
    AcpiPsDeleteParseTree (Op);

    /* End optional tracing */

    AcpiPsStopTrace (Info);

    /* Take away the extra reference that we gave the parameters above */

    AcpiPsUpdateParameterList (Info, REF_DECREMENT);

    /* Exit now if error above */

    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /*
     * If the method has returned an object, signal this to the caller with
     * a control exception code
     */
    if (Info->ReturnObject)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_PARSE, "Method returned ObjDesc=%p\n",
            Info->ReturnObject));
        ACPI_DUMP_STACK_ENTRY (Info->ReturnObject);

        Status = AE_CTRL_RETURN_VALUE;
    }

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiPsUpdateParameterList
 *
 * PARAMETERS:  Info            - See ACPI_EVALUATE_INFO
 *                                (Used: ParameterType and Parameters)
 *              Action          - Add or Remove reference
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Update reference count on all method parameter objects
 *
 ******************************************************************************/

static void
AcpiPsUpdateParameterList (
    ACPI_EVALUATE_INFO      *Info,
    UINT16                  Action)
{
    ACPI_NATIVE_UINT        i;


    if ((Info->ParameterType == ACPI_PARAM_ARGS) &&
        (Info->Parameters))
    {
        /* Update reference count for each parameter */

        for (i = 0; Info->Parameters[i]; i++)
        {
            /* Ignore errors, just do them all */

            (void) AcpiUtUpdateObjectReference (Info->Parameters[i], Action);
        }
    }
}


