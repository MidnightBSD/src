/******************************************************************************
 *
 * Module Name: dsinit - Object initialization namespace walk
 *              $Revision: 1.1.1.1 $
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

#define __DSINIT_C__

#include <contrib/dev/acpica/acpi.h>
#include <contrib/dev/acpica/acdispat.h>
#include <contrib/dev/acpica/acnamesp.h>

#define _COMPONENT          ACPI_DISPATCHER
        ACPI_MODULE_NAME    ("dsinit")


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsInitOneObject
 *
 * PARAMETERS:  ObjHandle       - Node
 *              Level           - Current nesting level
 *              Context         - Points to a init info struct
 *              ReturnValue     - Not used
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Callback from AcpiWalkNamespace.  Invoked for every object
 *              within the namespace.
 *
 *              Currently, the only objects that require initialization are:
 *              1) Methods
 *              2) Operation Regions
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDsInitOneObject (
    ACPI_HANDLE             ObjHandle,
    UINT32                  Level,
    void                    *Context,
    void                    **ReturnValue)
{
    ACPI_OBJECT_TYPE        Type;
    ACPI_STATUS             Status;
    ACPI_INIT_WALK_INFO     *Info = (ACPI_INIT_WALK_INFO *) Context;


    ACPI_FUNCTION_NAME ("DsInitOneObject");


    /*
     * We are only interested in objects owned by the table that
     * was just loaded
     */
    if (((ACPI_NAMESPACE_NODE *) ObjHandle)->OwnerId !=
            Info->TableDesc->TableId)
    {
        return (AE_OK);
    }

    Info->ObjectCount++;

    /* And even then, we are only interested in a few object types */

    Type = AcpiNsGetType (ObjHandle);

    switch (Type)
    {
    case ACPI_TYPE_REGION:

        Status = AcpiDsInitializeRegion (ObjHandle);
        if (ACPI_FAILURE (Status))
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Region %p [%4.4s] - Init failure, %s\n",
                ObjHandle, AcpiUtGetNodeName (ObjHandle),
                AcpiFormatException (Status)));
        }

        Info->OpRegionCount++;
        break;


    case ACPI_TYPE_METHOD:

        Info->MethodCount++;

        /* Print a dot for each method unless we are going to print the entire pathname */

        if (!(AcpiDbgLevel & ACPI_LV_INIT_NAMES))
        {
            ACPI_DEBUG_PRINT_RAW ((ACPI_DB_INIT, "."));
        }

        /*
         * Set the execution data width (32 or 64) based upon the
         * revision number of the parent ACPI table.
         * TBD: This is really for possible future support of integer width
         * on a per-table basis. Currently, we just use a global for the width.
         */
        if (Info->TableDesc->Pointer->Revision == 1)
        {
            ((ACPI_NAMESPACE_NODE *) ObjHandle)->Flags |= ANOBJ_DATA_WIDTH_32;
        }

        /*
         * Always parse methods to detect errors, we will delete
         * the parse tree below
         */
        Status = AcpiDsParseMethod (ObjHandle);
        if (ACPI_FAILURE (Status))
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Method %p [%4.4s] - parse failure, %s\n",
                ObjHandle, AcpiUtGetNodeName (ObjHandle),
                AcpiFormatException (Status)));

            /* This parse failed, but we will continue parsing more methods */

            break;
        }

        /*
         * Delete the parse tree.  We simply re-parse the method
         * for every execution since there isn't much overhead
         */
        AcpiNsDeleteNamespaceSubtree (ObjHandle);
        AcpiNsDeleteNamespaceByOwner (((ACPI_NAMESPACE_NODE *) ObjHandle)->Object->Method.OwningId);
        break;


    case ACPI_TYPE_DEVICE:

        Info->DeviceCount++;
        break;


    default:
        break;
    }

    /*
     * We ignore errors from above, and always return OK, since
     * we don't want to abort the walk on a single error.
     */
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsInitializeObjects
 *
 * PARAMETERS:  TableDesc       - Descriptor for parent ACPI table
 *              StartNode       - Root of subtree to be initialized.
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Walk the namespace starting at "StartNode" and perform any
 *              necessary initialization on the objects found therein
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDsInitializeObjects (
    ACPI_TABLE_DESC         *TableDesc,
    ACPI_NAMESPACE_NODE     *StartNode)
{
    ACPI_STATUS             Status;
    ACPI_INIT_WALK_INFO     Info;


    ACPI_FUNCTION_TRACE ("DsInitializeObjects");


    ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH,
        "**** Starting initialization of namespace objects ****\n"));
    ACPI_DEBUG_PRINT_RAW ((ACPI_DB_INIT, "Parsing all Control Methods:"));

    Info.MethodCount    = 0;
    Info.OpRegionCount  = 0;
    Info.ObjectCount    = 0;
    Info.DeviceCount    = 0;
    Info.TableDesc      = TableDesc;

    /* Walk entire namespace from the supplied root */

    Status = AcpiWalkNamespace (ACPI_TYPE_ANY, StartNode, ACPI_UINT32_MAX,
                    AcpiDsInitOneObject, &Info, NULL);
    if (ACPI_FAILURE (Status))
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "WalkNamespace failed, %s\n",
            AcpiFormatException (Status)));
    }

    ACPI_DEBUG_PRINT_RAW ((ACPI_DB_INIT,
        "\nTable [%4.4s](id %4.4X) - %hd Objects with %hd Devices %hd Methods %hd Regions\n",
        TableDesc->Pointer->Signature, TableDesc->TableId, Info.ObjectCount,
        Info.DeviceCount, Info.MethodCount, Info.OpRegionCount));

    ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH,
        "%hd Methods, %hd Regions\n", Info.MethodCount, Info.OpRegionCount));

    return_ACPI_STATUS (AE_OK);
}


