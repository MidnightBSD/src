/******************************************************************************
 *
 * Module Name: dsmethod - Parser/Interpreter interface - control method parsing
 *              $Revision: 1.136 $
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

#define __DSMETHOD_C__

#include <contrib/dev/acpica/acpi.h>
#include <contrib/dev/acpica/acparser.h>
#include <contrib/dev/acpica/amlcode.h>
#include <contrib/dev/acpica/acdispat.h>
#include <contrib/dev/acpica/acinterp.h>
#include <contrib/dev/acpica/acnamesp.h>
#include <contrib/dev/acpica/acdisasm.h>


#define _COMPONENT          ACPI_DISPATCHER
        ACPI_MODULE_NAME    ("dsmethod")

/* Local prototypes */

static ACPI_STATUS
AcpiDsCreateMethodMutex (
    ACPI_OPERAND_OBJECT     *MethodDesc);


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsMethodError
 *
 * PARAMETERS:  Status          - Execution status
 *              WalkState       - Current state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Called on method error. Invoke the global exception handler if
 *              present, dump the method data if the disassembler is configured
 *
 *              Note: Allows the exception handler to change the status code
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDsMethodError (
    ACPI_STATUS             Status,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_FUNCTION_ENTRY ();


    /* Ignore AE_OK and control exception codes */

    if (ACPI_SUCCESS (Status) ||
        (Status & AE_CODE_CONTROL))
    {
        return (Status);
    }

    /* Invoke the global exception handler */

    if (AcpiGbl_ExceptionHandler)
    {
        /* Exit the interpreter, allow handler to execute methods */

        AcpiExExitInterpreter ();

        /*
         * Handler can map the exception code to anything it wants, including
         * AE_OK, in which case the executing method will not be aborted.
         */
        Status = AcpiGbl_ExceptionHandler (Status,
                    WalkState->MethodNode ?
                        WalkState->MethodNode->Name.Integer : 0,
                    WalkState->Opcode, WalkState->AmlOffset, NULL);
        (void) AcpiExEnterInterpreter ();
    }

#ifdef ACPI_DISASSEMBLER
    if (ACPI_FAILURE (Status))
    {
        /* Display method locals/args if disassembler is present */

        AcpiDmDumpMethodInfo (Status, WalkState, WalkState->Op);
    }
#endif

    return (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsCreateMethodMutex
 *
 * PARAMETERS:  ObjDesc             - The method object
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create a mutex object for a serialized control method
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiDsCreateMethodMutex (
    ACPI_OPERAND_OBJECT     *MethodDesc)
{
    ACPI_OPERAND_OBJECT     *MutexDesc;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE (DsCreateMethodMutex);


    /* Create the new mutex object */

    MutexDesc = AcpiUtCreateInternalObject (ACPI_TYPE_MUTEX);
    if (!MutexDesc)
    {
        return_ACPI_STATUS (AE_NO_MEMORY);
    }

    /* Create the actual OS Mutex */

    Status = AcpiOsCreateMutex (&MutexDesc->Mutex.OsMutex);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    MutexDesc->Mutex.SyncLevel = MethodDesc->Method.SyncLevel;
    MethodDesc->Method.Mutex = MutexDesc;
    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsBeginMethodExecution
 *
 * PARAMETERS:  MethodNode          - Node of the method
 *              ObjDesc             - The method object
 *              WalkState           - current state, NULL if not yet executing
 *                                    a method.
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Prepare a method for execution.  Parses the method if necessary,
 *              increments the thread count, and waits at the method semaphore
 *              for clearance to execute.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDsBeginMethodExecution (
    ACPI_NAMESPACE_NODE     *MethodNode,
    ACPI_OPERAND_OBJECT     *ObjDesc,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_STATUS             Status = AE_OK;


    ACPI_FUNCTION_TRACE_PTR (DsBeginMethodExecution, MethodNode);


    if (!MethodNode)
    {
        return_ACPI_STATUS (AE_NULL_ENTRY);
    }

    /* Prevent wraparound of thread count */

    if (ObjDesc->Method.ThreadCount == ACPI_UINT8_MAX)
    {
        ACPI_ERROR ((AE_INFO,
            "Method reached maximum reentrancy limit (255)"));
        return_ACPI_STATUS (AE_AML_METHOD_LIMIT);
    }

    /*
     * If this method is serialized, we need to acquire the method mutex.
     */
    if (ObjDesc->Method.MethodFlags & AML_METHOD_SERIALIZED)
    {
        /*
         * Create a mutex for the method if it is defined to be Serialized
         * and a mutex has not already been created. We defer the mutex creation
         * until a method is actually executed, to minimize the object count
         */
        if (!ObjDesc->Method.Mutex)
        {
            Status = AcpiDsCreateMethodMutex (ObjDesc);
            if (ACPI_FAILURE (Status))
            {
                return_ACPI_STATUS (Status);
            }
        }

        /*
         * The CurrentSyncLevel (per-thread) must be less than or equal to
         * the sync level of the method. This mechanism provides some
         * deadlock prevention
         *
         * Top-level method invocation has no walk state at this point
         */
        if (WalkState &&
            (WalkState->Thread->CurrentSyncLevel > ObjDesc->Method.Mutex->Mutex.SyncLevel))
        {
            ACPI_ERROR ((AE_INFO,
                "Cannot acquire Mutex for method [%4.4s], current SyncLevel is too large (%d)",
                AcpiUtGetNodeName (MethodNode),
                WalkState->Thread->CurrentSyncLevel));

            return_ACPI_STATUS (AE_AML_MUTEX_ORDER);
        }

        /*
         * Obtain the method mutex if necessary. Do not acquire mutex for a
         * recursive call.
         */
        if (!WalkState ||
            !ObjDesc->Method.Mutex->Mutex.ThreadId ||
            (WalkState->Thread->ThreadId != ObjDesc->Method.Mutex->Mutex.ThreadId))
        {
            /*
             * Acquire the method mutex. This releases the interpreter if we
             * block (and reacquires it before it returns)
             */
            Status = AcpiExSystemWaitMutex (ObjDesc->Method.Mutex->Mutex.OsMutex,
                        ACPI_WAIT_FOREVER);
            if (ACPI_FAILURE (Status))
            {
                return_ACPI_STATUS (Status);
            }

            /* Update the mutex and walk info and save the original SyncLevel */

            if (WalkState)
            {
                ObjDesc->Method.Mutex->Mutex.OriginalSyncLevel =
                    WalkState->Thread->CurrentSyncLevel;

                ObjDesc->Method.Mutex->Mutex.ThreadId = WalkState->Thread->ThreadId;
                WalkState->Thread->CurrentSyncLevel = ObjDesc->Method.SyncLevel;
            }
            else
            {
                ObjDesc->Method.Mutex->Mutex.OriginalSyncLevel =
                    ObjDesc->Method.Mutex->Mutex.SyncLevel;
            }
        }

        /* Always increase acquisition depth */

        ObjDesc->Method.Mutex->Mutex.AcquisitionDepth++;
    }

    /*
     * Allocate an Owner ID for this method, only if this is the first thread
     * to begin concurrent execution. We only need one OwnerId, even if the
     * method is invoked recursively.
     */
    if (!ObjDesc->Method.OwnerId)
    {
        Status = AcpiUtAllocateOwnerId (&ObjDesc->Method.OwnerId);
        if (ACPI_FAILURE (Status))
        {
            goto Cleanup;
        }
    }

    /*
     * Increment the method parse tree thread count since it has been
     * reentered one more time (even if it is the same thread)
     */
    ObjDesc->Method.ThreadCount++;
    return_ACPI_STATUS (Status);


Cleanup:
    /* On error, must release the method mutex (if present) */

    if (ObjDesc->Method.Mutex)
    {
        AcpiOsReleaseMutex (ObjDesc->Method.Mutex->Mutex.OsMutex);
    }
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsCallControlMethod
 *
 * PARAMETERS:  Thread              - Info for this thread
 *              ThisWalkState       - Current walk state
 *              Op                  - Current Op to be walked
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Transfer execution to a called control method
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDsCallControlMethod (
    ACPI_THREAD_STATE       *Thread,
    ACPI_WALK_STATE         *ThisWalkState,
    ACPI_PARSE_OBJECT       *Op)
{
    ACPI_STATUS             Status;
    ACPI_NAMESPACE_NODE     *MethodNode;
    ACPI_WALK_STATE         *NextWalkState = NULL;
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_EVALUATE_INFO      *Info;
    UINT32                  i;


    ACPI_FUNCTION_TRACE_PTR (DsCallControlMethod, ThisWalkState);

    ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH, "Calling method %p, currentstate=%p\n",
        ThisWalkState->PrevOp, ThisWalkState));

    /*
     * Get the namespace entry for the control method we are about to call
     */
    MethodNode = ThisWalkState->MethodCallNode;
    if (!MethodNode)
    {
        return_ACPI_STATUS (AE_NULL_ENTRY);
    }

    ObjDesc = AcpiNsGetAttachedObject (MethodNode);
    if (!ObjDesc)
    {
        return_ACPI_STATUS (AE_NULL_OBJECT);
    }

    /* Init for new method, possibly wait on method mutex */

    Status = AcpiDsBeginMethodExecution (MethodNode, ObjDesc,
                ThisWalkState);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Begin method parse/execution. Create a new walk state */

    NextWalkState = AcpiDsCreateWalkState (ObjDesc->Method.OwnerId,
                        NULL, ObjDesc, Thread);
    if (!NextWalkState)
    {
        Status = AE_NO_MEMORY;
        goto Cleanup;
    }

    /*
     * The resolved arguments were put on the previous walk state's operand
     * stack. Operands on the previous walk state stack always
     * start at index 0. Also, null terminate the list of arguments
     */
    ThisWalkState->Operands [ThisWalkState->NumOperands] = NULL;

    /*
     * Allocate and initialize the evaluation information block
     * TBD: this is somewhat inefficient, should change interface to
     * DsInitAmlWalk. For now, keeps this struct off the CPU stack
     */
    Info = ACPI_ALLOCATE_ZEROED (sizeof (ACPI_EVALUATE_INFO));
    if (!Info)
    {
        return_ACPI_STATUS (AE_NO_MEMORY);
    }

    Info->Parameters = &ThisWalkState->Operands[0];
    Info->ParameterType = ACPI_PARAM_ARGS;

    Status = AcpiDsInitAmlWalk (NextWalkState, NULL, MethodNode,
                ObjDesc->Method.AmlStart, ObjDesc->Method.AmlLength,
                Info, ACPI_IMODE_EXECUTE);

    ACPI_FREE (Info);
    if (ACPI_FAILURE (Status))
    {
        goto Cleanup;
    }

    /*
     * Delete the operands on the previous walkstate operand stack
     * (they were copied to new objects)
     */
    for (i = 0; i < ObjDesc->Method.ParamCount; i++)
    {
        AcpiUtRemoveReference (ThisWalkState->Operands [i]);
        ThisWalkState->Operands [i] = NULL;
    }

    /* Clear the operand stack */

    ThisWalkState->NumOperands = 0;

    ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH,
        "**** Begin nested execution of [%4.4s] **** WalkState=%p\n",
        MethodNode->Name.Ascii, NextWalkState));

    /* Invoke an internal method if necessary */

    if (ObjDesc->Method.MethodFlags & AML_METHOD_INTERNAL_ONLY)
    {
        Status = ObjDesc->Method.Implementation (NextWalkState);
    }

    return_ACPI_STATUS (Status);


Cleanup:

    /* On error, we must terminate the method properly */

    AcpiDsTerminateControlMethod (ObjDesc, NextWalkState);
    if (NextWalkState)
    {
        AcpiDsDeleteWalkState (NextWalkState);
    }

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsRestartControlMethod
 *
 * PARAMETERS:  WalkState           - State for preempted method (caller)
 *              ReturnDesc          - Return value from the called method
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Restart a method that was preempted by another (nested) method
 *              invocation.  Handle the return value (if any) from the callee.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDsRestartControlMethod (
    ACPI_WALK_STATE         *WalkState,
    ACPI_OPERAND_OBJECT     *ReturnDesc)
{
    ACPI_STATUS             Status;
    int                     SameAsImplicitReturn;


    ACPI_FUNCTION_TRACE_PTR (DsRestartControlMethod, WalkState);


    ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH,
        "****Restart [%4.4s] Op %p ReturnValueFromCallee %p\n",
        AcpiUtGetNodeName (WalkState->MethodNode),
        WalkState->MethodCallOp, ReturnDesc));

    ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH,
        "    ReturnFromThisMethodUsed?=%X ResStack %p Walk %p\n",
        WalkState->ReturnUsed,
        WalkState->Results, WalkState));

    /* Did the called method return a value? */

    if (ReturnDesc)
    {
        /* Is the implicit return object the same as the return desc? */

        SameAsImplicitReturn = (WalkState->ImplicitReturnObj == ReturnDesc);

        /* Are we actually going to use the return value? */

        if (WalkState->ReturnUsed)
        {
            /* Save the return value from the previous method */

            Status = AcpiDsResultPush (ReturnDesc, WalkState);
            if (ACPI_FAILURE (Status))
            {
                AcpiUtRemoveReference (ReturnDesc);
                return_ACPI_STATUS (Status);
            }

            /*
             * Save as THIS method's return value in case it is returned
             * immediately to yet another method
             */
            WalkState->ReturnDesc = ReturnDesc;
        }

        /*
         * The following code is the optional support for the so-called
         * "implicit return". Some AML code assumes that the last value of the
         * method is "implicitly" returned to the caller, in the absence of an
         * explicit return value.
         *
         * Just save the last result of the method as the return value.
         *
         * NOTE: this is optional because the ASL language does not actually
         * support this behavior.
         */
        else if (!AcpiDsDoImplicitReturn (ReturnDesc, WalkState, FALSE) ||
                 SameAsImplicitReturn)
        {
            /*
             * Delete the return value if it will not be used by the
             * calling method or remove one reference if the explicit return
             * is the same as the implicit return value.
             */
            AcpiUtRemoveReference (ReturnDesc);
        }
    }

    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsTerminateControlMethod
 *
 * PARAMETERS:  MethodDesc          - Method object
 *              WalkState           - State associated with the method
 *
 * RETURN:      None
 *
 * DESCRIPTION: Terminate a control method.  Delete everything that the method
 *              created, delete all locals and arguments, and delete the parse
 *              tree if requested.
 *
 * MUTEX:       Interpreter is locked
 *
 ******************************************************************************/

void
AcpiDsTerminateControlMethod (
    ACPI_OPERAND_OBJECT     *MethodDesc,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE_PTR (DsTerminateControlMethod, WalkState);


    /* MethodDesc is required, WalkState is optional */

    if (!MethodDesc)
    {
        return_VOID;
    }

    if (WalkState)
    {
        /* Delete all arguments and locals */

        AcpiDsMethodDataDeleteAll (WalkState);
    }

    /*
     * If method is serialized, release the mutex and restore the
     * current sync level for this thread
     */
    if (MethodDesc->Method.Mutex)
    {
        /* Acquisition Depth handles recursive calls */

        MethodDesc->Method.Mutex->Mutex.AcquisitionDepth--;
        if (!MethodDesc->Method.Mutex->Mutex.AcquisitionDepth)
        {
            WalkState->Thread->CurrentSyncLevel =
                MethodDesc->Method.Mutex->Mutex.OriginalSyncLevel;

            AcpiOsReleaseMutex (MethodDesc->Method.Mutex->Mutex.OsMutex);
            MethodDesc->Method.Mutex->Mutex.ThreadId = 0;
        }
    }

    if (WalkState)
    {
        /*
         * Delete any namespace objects created anywhere within
         * the namespace by the execution of this method
         */
        AcpiNsDeleteNamespaceByOwner (MethodDesc->Method.OwnerId);
    }

    /* Decrement the thread count on the method */

    if (MethodDesc->Method.ThreadCount)
    {
        MethodDesc->Method.ThreadCount--;
    }
    else
    {
        ACPI_ERROR ((AE_INFO,
            "Invalid zero thread count in method"));
    }

    /* Are there any other threads currently executing this method? */

    if (MethodDesc->Method.ThreadCount)
    {
        /*
         * Additional threads. Do not release the OwnerId in this case,
         * we immediately reuse it for the next thread executing this method
         */
        ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH,
            "*** Completed execution of one thread, %d threads remaining\n",
            MethodDesc->Method.ThreadCount));
    }
    else
    {
        /* This is the only executing thread for this method */

        /*
         * Support to dynamically change a method from NotSerialized to
         * Serialized if it appears that the method is incorrectly written and
         * does not support multiple thread execution. The best example of this
         * is if such a method creates namespace objects and blocks. A second
         * thread will fail with an AE_ALREADY_EXISTS exception
         *
         * This code is here because we must wait until the last thread exits
         * before creating the synchronization semaphore.
         */
        if ((MethodDesc->Method.MethodFlags & AML_METHOD_SERIALIZED) &&
            (!MethodDesc->Method.Mutex))
        {
            Status = AcpiDsCreateMethodMutex (MethodDesc);
        }

        /* No more threads, we can free the OwnerId */

        AcpiUtReleaseOwnerId (&MethodDesc->Method.OwnerId);
    }

    return_VOID;
}


