/*******************************************************************************
 *
 * Module Name: nsxfeval - Public interfaces to the ACPI subsystem
 *                         ACPI Object evaluation interfaces
 *              $Revision: 1.30 $
 *
 ******************************************************************************/

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


#define __NSXFEVAL_C__

#include <contrib/dev/acpica/acpi.h>
#include <contrib/dev/acpica/acnamesp.h>
#include <contrib/dev/acpica/acinterp.h>


#define _COMPONENT          ACPI_NAMESPACE
        ACPI_MODULE_NAME    ("nsxfeval")


/*******************************************************************************
 *
 * FUNCTION:    AcpiEvaluateObjectTyped
 *
 * PARAMETERS:  Handle              - Object handle (optional)
 *              Pathname            - Object pathname (optional)
 *              ExternalParams      - List of parameters to pass to method,
 *                                    terminated by NULL.  May be NULL
 *                                    if no parameters are being passed.
 *              ReturnBuffer        - Where to put method's return value (if
 *                                    any).  If NULL, no value is returned.
 *              ReturnType          - Expected type of return object
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Find and evaluate the given object, passing the given
 *              parameters if necessary.  One of "Handle" or "Pathname" must
 *              be valid (non-null)
 *
 ******************************************************************************/

ACPI_STATUS
AcpiEvaluateObjectTyped (
    ACPI_HANDLE             Handle,
    ACPI_STRING             Pathname,
    ACPI_OBJECT_LIST        *ExternalParams,
    ACPI_BUFFER             *ReturnBuffer,
    ACPI_OBJECT_TYPE        ReturnType)
{
    ACPI_STATUS             Status;
    BOOLEAN                 MustFree = FALSE;


    ACPI_FUNCTION_TRACE (AcpiEvaluateObjectTyped);


    /* Return buffer must be valid */

    if (!ReturnBuffer)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    if (ReturnBuffer->Length == ACPI_ALLOCATE_BUFFER)
    {
        MustFree = TRUE;
    }

    /* Evaluate the object */

    Status = AcpiEvaluateObject (Handle, Pathname, ExternalParams, ReturnBuffer);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Type ANY means "don't care" */

    if (ReturnType == ACPI_TYPE_ANY)
    {
        return_ACPI_STATUS (AE_OK);
    }

    if (ReturnBuffer->Length == 0)
    {
        /* Error because caller specifically asked for a return value */

        ACPI_ERROR ((AE_INFO, "No return value"));
        return_ACPI_STATUS (AE_NULL_OBJECT);
    }

    /* Examine the object type returned from EvaluateObject */

    if (((ACPI_OBJECT *) ReturnBuffer->Pointer)->Type == ReturnType)
    {
        return_ACPI_STATUS (AE_OK);
    }

    /* Return object type does not match requested type */

    ACPI_ERROR ((AE_INFO,
        "Incorrect return type [%s] requested [%s]",
        AcpiUtGetTypeName (((ACPI_OBJECT *) ReturnBuffer->Pointer)->Type),
        AcpiUtGetTypeName (ReturnType)));

    if (MustFree)
    {
        /* Caller used ACPI_ALLOCATE_BUFFER, free the return buffer */

        AcpiOsFree (ReturnBuffer->Pointer);
        ReturnBuffer->Pointer = NULL;
    }

    ReturnBuffer->Length = 0;
    return_ACPI_STATUS (AE_TYPE);
}

ACPI_EXPORT_SYMBOL (AcpiEvaluateObjectTyped)


/*******************************************************************************
 *
 * FUNCTION:    AcpiEvaluateObject
 *
 * PARAMETERS:  Handle              - Object handle (optional)
 *              Pathname            - Object pathname (optional)
 *              ExternalParams      - List of parameters to pass to method,
 *                                    terminated by NULL.  May be NULL
 *                                    if no parameters are being passed.
 *              ReturnBuffer        - Where to put method's return value (if
 *                                    any).  If NULL, no value is returned.
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Find and evaluate the given object, passing the given
 *              parameters if necessary.  One of "Handle" or "Pathname" must
 *              be valid (non-null)
 *
 ******************************************************************************/

ACPI_STATUS
AcpiEvaluateObject (
    ACPI_HANDLE             Handle,
    ACPI_STRING             Pathname,
    ACPI_OBJECT_LIST        *ExternalParams,
    ACPI_BUFFER             *ReturnBuffer)
{
    ACPI_STATUS             Status;
    ACPI_EVALUATE_INFO      *Info;
    ACPI_SIZE               BufferSpaceNeeded;
    UINT32                  i;


    ACPI_FUNCTION_TRACE (AcpiEvaluateObject);


    /* Allocate and initialize the evaluation information block */

    Info = ACPI_ALLOCATE_ZEROED (sizeof (ACPI_EVALUATE_INFO));
    if (!Info)
    {
        return_ACPI_STATUS (AE_NO_MEMORY);
    }

    Info->Pathname = Pathname;
    Info->ParameterType = ACPI_PARAM_ARGS;

    /* Convert and validate the device handle */

    Info->PrefixNode = AcpiNsMapHandleToNode (Handle);
    if (!Info->PrefixNode)
    {
        Status = AE_BAD_PARAMETER;
        goto Cleanup;
    }

    /*
     * If there are parameters to be passed to a control method, the external
     * objects must all be converted to internal objects
     */
    if (ExternalParams && ExternalParams->Count)
    {
        /*
         * Allocate a new parameter block for the internal objects
         * Add 1 to count to allow for null terminated internal list
         */
        Info->Parameters = ACPI_ALLOCATE_ZEROED (
            ((ACPI_SIZE) ExternalParams->Count + 1) * sizeof (void *));
        if (!Info->Parameters)
        {
            Status = AE_NO_MEMORY;
            goto Cleanup;
        }

        /* Convert each external object in the list to an internal object */

        for (i = 0; i < ExternalParams->Count; i++)
        {
            Status = AcpiUtCopyEobjectToIobject (
                        &ExternalParams->Pointer[i], &Info->Parameters[i]);
            if (ACPI_FAILURE (Status))
            {
                goto Cleanup;
            }
        }
        Info->Parameters[ExternalParams->Count] = NULL;
    }

    /*
     * Three major cases:
     * 1) Fully qualified pathname
     * 2) No handle, not fully qualified pathname (error)
     * 3) Valid handle
     */
    if ((Pathname) &&
        (AcpiNsValidRootPrefix (Pathname[0])))
    {
        /* The path is fully qualified, just evaluate by name */

        Info->PrefixNode = NULL;
        Status = AcpiNsEvaluate (Info);
    }
    else if (!Handle)
    {
        /*
         * A handle is optional iff a fully qualified pathname is specified.
         * Since we've already handled fully qualified names above, this is
         * an error
         */
        if (!Pathname)
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_INFO,
                "Both Handle and Pathname are NULL"));
        }
        else
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_INFO,
                "Null Handle with relative pathname [%s]", Pathname));
        }

        Status = AE_BAD_PARAMETER;
    }
    else
    {
        /* We have a namespace a node and a possible relative path */

        Status = AcpiNsEvaluate (Info);
    }

    /*
     * If we are expecting a return value, and all went well above,
     * copy the return value to an external object.
     */
    if (ReturnBuffer)
    {
        if (!Info->ReturnObject)
        {
            ReturnBuffer->Length = 0;
        }
        else
        {
            if (ACPI_GET_DESCRIPTOR_TYPE (Info->ReturnObject) ==
                ACPI_DESC_TYPE_NAMED)
            {
                /*
                 * If we received a NS Node as a return object, this means that
                 * the object we are evaluating has nothing interesting to
                 * return (such as a mutex, etc.)  We return an error because
                 * these types are essentially unsupported by this interface.
                 * We don't check up front because this makes it easier to add
                 * support for various types at a later date if necessary.
                 */
                Status = AE_TYPE;
                Info->ReturnObject = NULL;   /* No need to delete a NS Node */
                ReturnBuffer->Length = 0;
            }

            if (ACPI_SUCCESS (Status))
            {
                /* Get the size of the returned object */

                Status = AcpiUtGetObjectSize (Info->ReturnObject,
                            &BufferSpaceNeeded);
                if (ACPI_SUCCESS (Status))
                {
                    /* Validate/Allocate/Clear caller buffer */

                    Status = AcpiUtInitializeBuffer (ReturnBuffer,
                                BufferSpaceNeeded);
                    if (ACPI_FAILURE (Status))
                    {
                        /*
                         * Caller's buffer is too small or a new one can't
                         * be allocated
                         */
                        ACPI_DEBUG_PRINT ((ACPI_DB_INFO,
                            "Needed buffer size %X, %s\n",
                            (UINT32) BufferSpaceNeeded,
                            AcpiFormatException (Status)));
                    }
                    else
                    {
                        /* We have enough space for the object, build it */

                        Status = AcpiUtCopyIobjectToEobject (Info->ReturnObject,
                                    ReturnBuffer);
                    }
                }
            }
        }
    }

    if (Info->ReturnObject)
    {
        /*
         * Delete the internal return object. NOTE: Interpreter must be
         * locked to avoid race condition.
         */
        AcpiExEnterInterpreter ();

        /* Remove one reference on the return object (should delete it) */

        AcpiUtRemoveReference (Info->ReturnObject);
        AcpiExExitInterpreter ();
    }


Cleanup:

    /* Free the input parameter list (if we created one) */

    if (Info->Parameters)
    {
        /* Free the allocated parameter block */

        AcpiUtDeleteInternalObjectList (Info->Parameters);
    }

    ACPI_FREE (Info);
    return_ACPI_STATUS (Status);
}

ACPI_EXPORT_SYMBOL (AcpiEvaluateObject)


/*******************************************************************************
 *
 * FUNCTION:    AcpiWalkNamespace
 *
 * PARAMETERS:  Type                - ACPI_OBJECT_TYPE to search for
 *              StartObject         - Handle in namespace where search begins
 *              MaxDepth            - Depth to which search is to reach
 *              UserFunction        - Called when an object of "Type" is found
 *              Context             - Passed to user function
 *              ReturnValue         - Location where return value of
 *                                    UserFunction is put if terminated early
 *
 * RETURNS      Return value from the UserFunction if terminated early.
 *              Otherwise, returns NULL.
 *
 * DESCRIPTION: Performs a modified depth-first walk of the namespace tree,
 *              starting (and ending) at the object specified by StartHandle.
 *              The UserFunction is called whenever an object that matches
 *              the type parameter is found.  If the user function returns
 *              a non-zero value, the search is terminated immediately and this
 *              value is returned to the caller.
 *
 *              The point of this procedure is to provide a generic namespace
 *              walk routine that can be called from multiple places to
 *              provide multiple services;  the User Function can be tailored
 *              to each task, whether it is a print function, a compare
 *              function, etc.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiWalkNamespace (
    ACPI_OBJECT_TYPE        Type,
    ACPI_HANDLE             StartObject,
    UINT32                  MaxDepth,
    ACPI_WALK_CALLBACK      UserFunction,
    void                    *Context,
    void                    **ReturnValue)
{
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE (AcpiWalkNamespace);


    /* Parameter validation */

    if ((Type > ACPI_TYPE_LOCAL_MAX) ||
        (!MaxDepth)                  ||
        (!UserFunction))
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    /*
     * Lock the namespace around the walk.
     * The namespace will be unlocked/locked around each call
     * to the user function - since this function
     * must be allowed to make Acpi calls itself.
     */
    Status = AcpiUtAcquireMutex (ACPI_MTX_NAMESPACE);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    Status = AcpiNsWalkNamespace (Type, StartObject, MaxDepth,
                    ACPI_NS_WALK_UNLOCK,
                    UserFunction, Context, ReturnValue);

    (void) AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE);
    return_ACPI_STATUS (Status);
}

ACPI_EXPORT_SYMBOL (AcpiWalkNamespace)


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsGetDeviceCallback
 *
 * PARAMETERS:  Callback from AcpiGetDevice
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Takes callbacks from WalkNamespace and filters out all non-
 *              present devices, or if they specified a HID, it filters based
 *              on that.
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiNsGetDeviceCallback (
    ACPI_HANDLE             ObjHandle,
    UINT32                  NestingLevel,
    void                    *Context,
    void                    **ReturnValue)
{
    ACPI_GET_DEVICES_INFO   *Info = Context;
    ACPI_STATUS             Status;
    ACPI_NAMESPACE_NODE     *Node;
    UINT32                  Flags;
    ACPI_DEVICE_ID          Hid;
    ACPI_COMPATIBLE_ID_LIST *Cid;
    ACPI_NATIVE_UINT        i;


    Status = AcpiUtAcquireMutex (ACPI_MTX_NAMESPACE);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    Node = AcpiNsMapHandleToNode (ObjHandle);
    Status = AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    if (!Node)
    {
        return (AE_BAD_PARAMETER);
    }

    /* Run _STA to determine if device is present */

    Status = AcpiUtExecute_STA (Node, &Flags);
    if (ACPI_FAILURE (Status))
    {
        return (AE_CTRL_DEPTH);
    }

    if (!(Flags & ACPI_STA_DEVICE_PRESENT))
    {
        /* Don't examine children of the device if not present */

        return (AE_CTRL_DEPTH);
    }

    /* Filter based on device HID & CID */

    if (Info->Hid != NULL)
    {
        Status = AcpiUtExecute_HID (Node, &Hid);
        if (Status == AE_NOT_FOUND)
        {
            return (AE_OK);
        }
        else if (ACPI_FAILURE (Status))
        {
            return (AE_CTRL_DEPTH);
        }

        if (ACPI_STRNCMP (Hid.Value, Info->Hid, sizeof (Hid.Value)) != 0)
        {
            /* Get the list of Compatible IDs */

            Status = AcpiUtExecute_CID (Node, &Cid);
            if (Status == AE_NOT_FOUND)
            {
                return (AE_OK);
            }
            else if (ACPI_FAILURE (Status))
            {
                return (AE_CTRL_DEPTH);
            }

            /* Walk the CID list */

            for (i = 0; i < Cid->Count; i++)
            {
                if (ACPI_STRNCMP (Cid->Id[i].Value, Info->Hid,
                                        sizeof (ACPI_COMPATIBLE_ID)) != 0)
                {
                    ACPI_FREE (Cid);
                    return (AE_OK);
                }
            }
            ACPI_FREE (Cid);
        }
    }

    Status = Info->UserFunction (ObjHandle, NestingLevel, Info->Context,
                ReturnValue);
    return (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiGetDevices
 *
 * PARAMETERS:  HID                 - HID to search for. Can be NULL.
 *              UserFunction        - Called when a matching object is found
 *              Context             - Passed to user function
 *              ReturnValue         - Location where return value of
 *                                    UserFunction is put if terminated early
 *
 * RETURNS      Return value from the UserFunction if terminated early.
 *              Otherwise, returns NULL.
 *
 * DESCRIPTION: Performs a modified depth-first walk of the namespace tree,
 *              starting (and ending) at the object specified by StartHandle.
 *              The UserFunction is called whenever an object of type
 *              Device is found.  If the user function returns
 *              a non-zero value, the search is terminated immediately and this
 *              value is returned to the caller.
 *
 *              This is a wrapper for WalkNamespace, but the callback performs
 *              additional filtering. Please see AcpiGetDeviceCallback.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiGetDevices (
    char                    *HID,
    ACPI_WALK_CALLBACK      UserFunction,
    void                    *Context,
    void                    **ReturnValue)
{
    ACPI_STATUS             Status;
    ACPI_GET_DEVICES_INFO   Info;


    ACPI_FUNCTION_TRACE (AcpiGetDevices);


    /* Parameter validation */

    if (!UserFunction)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    /*
     * We're going to call their callback from OUR callback, so we need
     * to know what it is, and their context parameter.
     */
    Info.Hid          = HID;
    Info.Context      = Context;
    Info.UserFunction = UserFunction;

    /*
     * Lock the namespace around the walk.
     * The namespace will be unlocked/locked around each call
     * to the user function - since this function
     * must be allowed to make Acpi calls itself.
     */
    Status = AcpiUtAcquireMutex (ACPI_MTX_NAMESPACE);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    Status = AcpiNsWalkNamespace (ACPI_TYPE_DEVICE, ACPI_ROOT_OBJECT,
                ACPI_UINT32_MAX, ACPI_NS_WALK_UNLOCK,
                AcpiNsGetDeviceCallback, &Info, ReturnValue);

    (void) AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE);
    return_ACPI_STATUS (Status);
}

ACPI_EXPORT_SYMBOL (AcpiGetDevices)


/*******************************************************************************
 *
 * FUNCTION:    AcpiAttachData
 *
 * PARAMETERS:  ObjHandle           - Namespace node
 *              Handler             - Handler for this attachment
 *              Data                - Pointer to data to be attached
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Attach arbitrary data and handler to a namespace node.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiAttachData (
    ACPI_HANDLE             ObjHandle,
    ACPI_OBJECT_HANDLER     Handler,
    void                    *Data)
{
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_STATUS             Status;


    /* Parameter validation */

    if (!ObjHandle  ||
        !Handler    ||
        !Data)
    {
        return (AE_BAD_PARAMETER);
    }

    Status = AcpiUtAcquireMutex (ACPI_MTX_NAMESPACE);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    /* Convert and validate the handle */

    Node = AcpiNsMapHandleToNode (ObjHandle);
    if (!Node)
    {
        Status = AE_BAD_PARAMETER;
        goto UnlockAndExit;
    }

    Status = AcpiNsAttachData (Node, Handler, Data);

UnlockAndExit:
    (void) AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE);
    return (Status);
}

ACPI_EXPORT_SYMBOL (AcpiAttachData)


/*******************************************************************************
 *
 * FUNCTION:    AcpiDetachData
 *
 * PARAMETERS:  ObjHandle           - Namespace node handle
 *              Handler             - Handler used in call to AcpiAttachData
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Remove data that was previously attached to a node.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDetachData (
    ACPI_HANDLE             ObjHandle,
    ACPI_OBJECT_HANDLER     Handler)
{
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_STATUS             Status;


    /* Parameter validation */

    if (!ObjHandle  ||
        !Handler)
    {
        return (AE_BAD_PARAMETER);
    }

    Status = AcpiUtAcquireMutex (ACPI_MTX_NAMESPACE);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    /* Convert and validate the handle */

    Node = AcpiNsMapHandleToNode (ObjHandle);
    if (!Node)
    {
        Status = AE_BAD_PARAMETER;
        goto UnlockAndExit;
    }

    Status = AcpiNsDetachData (Node, Handler);

UnlockAndExit:
    (void) AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE);
    return (Status);
}

ACPI_EXPORT_SYMBOL (AcpiDetachData)


/*******************************************************************************
 *
 * FUNCTION:    AcpiGetData
 *
 * PARAMETERS:  ObjHandle           - Namespace node
 *              Handler             - Handler used in call to AttachData
 *              Data                - Where the data is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Retrieve data that was previously attached to a namespace node.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiGetData (
    ACPI_HANDLE             ObjHandle,
    ACPI_OBJECT_HANDLER     Handler,
    void                    **Data)
{
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_STATUS             Status;


    /* Parameter validation */

    if (!ObjHandle  ||
        !Handler    ||
        !Data)
    {
        return (AE_BAD_PARAMETER);
    }

    Status = AcpiUtAcquireMutex (ACPI_MTX_NAMESPACE);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    /* Convert and validate the handle */

    Node = AcpiNsMapHandleToNode (ObjHandle);
    if (!Node)
    {
        Status = AE_BAD_PARAMETER;
        goto UnlockAndExit;
    }

    Status = AcpiNsGetAttachedData (Node, Handler, Data);

UnlockAndExit:
    (void) AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE);
    return (Status);
}

ACPI_EXPORT_SYMBOL (AcpiGetData)


