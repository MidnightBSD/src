/*******************************************************************************
 *
 * Module Name: nseval - Object evaluation, includes control method execution
 *              $Revision: 1.144 $
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

#define __NSEVAL_C__

#include <contrib/dev/acpica/acpi.h>
#include <contrib/dev/acpica/acparser.h>
#include <contrib/dev/acpica/acinterp.h>
#include <contrib/dev/acpica/acnamesp.h>


#define _COMPONENT          ACPI_NAMESPACE
        ACPI_MODULE_NAME    ("nseval")


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsEvaluate
 *
 * PARAMETERS:  Info            - Evaluation info block, contains:
 *                  PrefixNode      - Prefix or Method/Object Node to execute
 *                  Pathname        - Name of method to execute, If NULL, the
 *                                    Node is the object to execute
 *                  Parameters      - List of parameters to pass to the method,
 *                                    terminated by NULL. Params itself may be
 *                                    NULL if no parameters are being passed.
 *                  ReturnObject    - Where to put method's return value (if
 *                                    any). If NULL, no value is returned.
 *                  ParameterType   - Type of Parameter list
 *                  ReturnObject    - Where to put method's return value (if
 *                                    any). If NULL, no value is returned.
 *                  Flags           - ACPI_IGNORE_RETURN_VALUE to delete return
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Execute a control method or return the current value of an
 *              ACPI namespace object.
 *
 * MUTEX:       Locks interpreter
 *
 ******************************************************************************/

ACPI_STATUS
AcpiNsEvaluate (
    ACPI_EVALUATE_INFO      *Info)
{
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE (NsEvaluate);


    if (!Info)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    /* Initialize the return value to an invalid object */

    Info->ReturnObject = NULL;

    /*
     * Get the actual namespace node for the target object. Handles these cases:
     *
     * 1) Null node, Pathname (absolute path)
     * 2) Node, Pathname (path relative to Node)
     * 3) Node, Null Pathname
     */
    Status = AcpiNsGetNode (Info->PrefixNode, Info->Pathname,
                ACPI_NS_NO_UPSEARCH, &Info->ResolvedNode);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /*
     * For a method alias, we must grab the actual method node so that proper
     * scoping context will be established before execution.
     */
    if (AcpiNsGetType (Info->ResolvedNode) == ACPI_TYPE_LOCAL_METHOD_ALIAS)
    {
        Info->ResolvedNode =
            ACPI_CAST_PTR (ACPI_NAMESPACE_NODE, Info->ResolvedNode->Object);
    }

    ACPI_DEBUG_PRINT ((ACPI_DB_NAMES, "%s [%p] Value %p\n", Info->Pathname,
        Info->ResolvedNode, AcpiNsGetAttachedObject (Info->ResolvedNode)));

    /*
     * Two major cases here:
     *
     * 1) The object is a control method -- execute it
     * 2) The object is not a method -- just return it's current value
     */
    if (AcpiNsGetType (Info->ResolvedNode) == ACPI_TYPE_METHOD)
    {
        /*
         * 1) Object is a control method - execute it
         */

        /* Verify that there is a method object associated with this node */

        Info->ObjDesc = AcpiNsGetAttachedObject (Info->ResolvedNode);
        if (!Info->ObjDesc)
        {
            ACPI_ERROR ((AE_INFO, "Control method has no attached sub-object"));
            return_ACPI_STATUS (AE_NULL_OBJECT);
        }

        ACPI_DUMP_PATHNAME (Info->ResolvedNode, "Execute Method:",
            ACPI_LV_INFO, _COMPONENT);

        ACPI_DEBUG_PRINT ((ACPI_DB_EXEC,
            "Method at AML address %p Length %X\n",
            Info->ObjDesc->Method.AmlStart + 1,
            Info->ObjDesc->Method.AmlLength - 1));

        /*
         * Any namespace deletion must acquire both the namespace and
         * interpreter locks to ensure that no thread is using the portion of
         * the namespace that is being deleted.
         *
         * Execute the method via the interpreter. The interpreter is locked
         * here before calling into the AML parser
         */
        AcpiExEnterInterpreter ();
        Status = AcpiPsExecuteMethod (Info);
        AcpiExExitInterpreter ();
    }
    else
    {
        /*
         * 2) Object is not a method, return its current value
         */

        /*
         * Objects require additional resolution steps (e.g., the Node may be
         * a field that must be read, etc.) -- we can't just grab the object
         * out of the node.
         *
         * Use ResolveNodeToValue() to get the associated value.
         *
         * NOTE: we can get away with passing in NULL for a walk state because
         * ResolvedNode is guaranteed to not be a reference to either a method
         * local or a method argument (because this interface is never called
         * from a running method.)
         *
         * Even though we do not directly invoke the interpreter for object
         * resolution, we must lock it because we could access an opregion.
         * The opregion access code assumes that the interpreter is locked.
         */
        AcpiExEnterInterpreter ();

        /* Function has a strange interface */

        Status = AcpiExResolveNodeToValue (&Info->ResolvedNode, NULL);
        AcpiExExitInterpreter ();

        /*
         * If AcpiExResolveNodeToValue() succeeded, the return value was placed
         * in ResolvedNode.
         */
        if (ACPI_SUCCESS (Status))
        {
            Status = AE_CTRL_RETURN_VALUE;
            Info->ReturnObject =
                ACPI_CAST_PTR (ACPI_OPERAND_OBJECT, Info->ResolvedNode);

            ACPI_DEBUG_PRINT ((ACPI_DB_NAMES, "Returning object %p [%s]\n",
                Info->ReturnObject,
                AcpiUtGetObjectTypeName (Info->ReturnObject)));
        }
    }

    /*
     * Check if there is a return value that must be dealt with
     */
    if (Status == AE_CTRL_RETURN_VALUE)
    {
        /* If caller does not want the return value, delete it */

        if (Info->Flags & ACPI_IGNORE_RETURN_VALUE)
        {
            AcpiUtRemoveReference (Info->ReturnObject);
            Info->ReturnObject = NULL;
        }

        /* Map AE_CTRL_RETURN_VALUE to AE_OK, we are done with it */

        Status = AE_OK;
    }

    ACPI_DEBUG_PRINT ((ACPI_DB_NAMES,
        "*** Completed evaluation of object %s ***\n", Info->Pathname));

    /*
     * Namespace was unlocked by the handling AcpiNs* function, so we
     * just return
     */
    return_ACPI_STATUS (Status);
}

