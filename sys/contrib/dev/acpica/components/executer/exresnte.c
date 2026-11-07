/******************************************************************************
 *
 * Module Name: exresnte - AML Interpreter object resolution
 *
 *****************************************************************************/

/******************************************************************************
 *
 * Copyright (c) 1999 - 2026, Intel Corp.
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0-only
 *
 *****************************************************************************/

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/acdispat.h>
#include <contrib/dev/acpica/include/acinterp.h>
#include <contrib/dev/acpica/include/acnamesp.h>


#define _COMPONENT          ACPI_EXECUTER
        ACPI_MODULE_NAME    ("exresnte")


/*******************************************************************************
 *
 * FUNCTION:    AcpiExResolveNodeToValue
 *
 * PARAMETERS:  ObjectPtr       - Pointer to a location that contains
 *                                a pointer to a NS node, and will receive a
 *                                pointer to the resolved object.
 *              WalkState       - Current state. Valid only if executing AML
 *                                code. NULL if simply resolving an object
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Resolve a Namespace node to a valued object
 *
 * Note: for some of the data types, the pointer attached to the Node
 * can be either a pointer to an actual internal object or a pointer into the
 * AML stream itself. These types are currently:
 *
 *      ACPI_TYPE_INTEGER
 *      ACPI_TYPE_STRING
 *      ACPI_TYPE_BUFFER
 *      ACPI_TYPE_MUTEX
 *      ACPI_TYPE_PACKAGE
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExResolveNodeToValue (
    ACPI_NAMESPACE_NODE     **ObjectPtr,
    ACPI_WALK_STATE         *WalkState)

{
    ACPI_STATUS             Status = AE_OK;
    ACPI_OPERAND_OBJECT     *SourceDesc;
    ACPI_OPERAND_OBJECT     *ObjDesc = NULL;
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_OBJECT_TYPE        EntryType;


    ACPI_FUNCTION_TRACE (ExResolveNodeToValue);


    /*
     * The stack pointer points to a ACPI_NAMESPACE_NODE (Node). Get the
     * object that is attached to the Node.
     */
    Node = *ObjectPtr;
    SourceDesc = AcpiNsGetAttachedObject (Node);
    EntryType = AcpiNsGetType ((ACPI_HANDLE) Node);

    ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "Entry=%p SourceDesc=%p [%s]\n",
         Node, SourceDesc, AcpiUtGetTypeName (EntryType)));

    if ((EntryType == ACPI_TYPE_LOCAL_ALIAS) ||
        (EntryType == ACPI_TYPE_LOCAL_METHOD_ALIAS))
    {
        /* There is always exactly one level of indirection */

        Node = ACPI_CAST_PTR (ACPI_NAMESPACE_NODE, Node->Object);
        SourceDesc = AcpiNsGetAttachedObject (Node);
        EntryType = AcpiNsGetType ((ACPI_HANDLE) Node);
        *ObjectPtr = Node;
    }

    /*
     * Several object types require no further processing:
     * 1) Device/Thermal objects don't have a "real" subobject, return Node
     * 2) Method locals and arguments have a pseudo-Node
     * 3) 10/2007: Added method type to assist with Package construction.
     */
    if ((EntryType == ACPI_TYPE_DEVICE)  ||
        (EntryType == ACPI_TYPE_THERMAL) ||
        (EntryType == ACPI_TYPE_METHOD)  ||
        (Node->Flags & (ANOBJ_METHOD_ARG | ANOBJ_METHOD_LOCAL)))
    {
        return_ACPI_STATUS (AE_OK);
    }

    if (!SourceDesc)
    {
        ACPI_ERROR ((AE_INFO, "No object attached to node [%4.4s] %p",
            Node->Name.Ascii, Node));
        return_ACPI_STATUS (AE_AML_UNINITIALIZED_NODE);
    }

    /*
     * Action is based on the type of the Node, which indicates the type
     * of the attached object or pointer
     */
    switch (EntryType)
    {
    case ACPI_TYPE_PACKAGE:

        if (SourceDesc->Common.Type != ACPI_TYPE_PACKAGE)
        {
            ACPI_ERROR ((AE_INFO, "Object not a Package, type %s",
                AcpiUtGetObjectTypeName (SourceDesc)));
            return_ACPI_STATUS (AE_AML_OPERAND_TYPE);
        }

        Status = AcpiDsGetPackageArguments (SourceDesc);
        if (ACPI_SUCCESS (Status))
        {
            /* Return an additional reference to the object */

            ObjDesc = SourceDesc;
            AcpiUtAddReference (ObjDesc);
        }
        break;

    case ACPI_TYPE_BUFFER:

        if (SourceDesc->Common.Type != ACPI_TYPE_BUFFER)
        {
            ACPI_ERROR ((AE_INFO, "Object not a Buffer, type %s",
                AcpiUtGetObjectTypeName (SourceDesc)));
            return_ACPI_STATUS (AE_AML_OPERAND_TYPE);
        }

        Status = AcpiDsGetBufferArguments (SourceDesc);
        if (ACPI_SUCCESS (Status))
        {
            /* Return an additional reference to the object */

            ObjDesc = SourceDesc;
            AcpiUtAddReference (ObjDesc);
        }
        break;

    case ACPI_TYPE_STRING:

        if (SourceDesc->Common.Type != ACPI_TYPE_STRING)
        {
            ACPI_ERROR ((AE_INFO, "Object not a String, type %s",
                AcpiUtGetObjectTypeName (SourceDesc)));
            return_ACPI_STATUS (AE_AML_OPERAND_TYPE);
        }

        /* Return an additional reference to the object */

        ObjDesc = SourceDesc;
        AcpiUtAddReference (ObjDesc);
        break;

    case ACPI_TYPE_INTEGER:

        if (SourceDesc->Common.Type != ACPI_TYPE_INTEGER)
        {
            ACPI_ERROR ((AE_INFO, "Object not a Integer, type %s",
                AcpiUtGetObjectTypeName (SourceDesc)));
            return_ACPI_STATUS (AE_AML_OPERAND_TYPE);
        }

        /* Return an additional reference to the object */

        ObjDesc = SourceDesc;
        AcpiUtAddReference (ObjDesc);
        break;

    case ACPI_TYPE_BUFFER_FIELD:
    case ACPI_TYPE_LOCAL_REGION_FIELD:
    case ACPI_TYPE_LOCAL_BANK_FIELD:
    case ACPI_TYPE_LOCAL_INDEX_FIELD:

        ACPI_DEBUG_PRINT ((ACPI_DB_EXEC,
            "FieldRead Node=%p SourceDesc=%p Type=%X\n",
            Node, SourceDesc, EntryType));

        Status = AcpiExReadDataFromField (WalkState, SourceDesc, &ObjDesc);
        break;

    /* For these objects, just return the object attached to the Node */

    case ACPI_TYPE_MUTEX:
    case ACPI_TYPE_POWER:
    case ACPI_TYPE_PROCESSOR:
    case ACPI_TYPE_EVENT:
    case ACPI_TYPE_REGION:

        /* Return an additional reference to the object */

        ObjDesc = SourceDesc;
        AcpiUtAddReference (ObjDesc);
        break;

    /* TYPE_ANY is untyped, and thus there is no object associated with it */

    case ACPI_TYPE_ANY:

        ACPI_ERROR ((AE_INFO,
            "Untyped entry %p, no attached object!", Node));

        return_ACPI_STATUS (AE_AML_OPERAND_TYPE);  /* Cannot be AE_TYPE */

    case ACPI_TYPE_LOCAL_REFERENCE:

        switch (SourceDesc->Reference.Class)
        {
        case ACPI_REFCLASS_TABLE:   /* This is a DdbHandle */
        case ACPI_REFCLASS_REFOF:
        case ACPI_REFCLASS_INDEX:

            /* Return an additional reference to the object */

            ObjDesc = SourceDesc;
            AcpiUtAddReference (ObjDesc);
            break;

        default:

            /* No named references are allowed here */

            ACPI_ERROR ((AE_INFO,
                "Unsupported Reference type 0x%X",
                SourceDesc->Reference.Class));

            return_ACPI_STATUS (AE_AML_OPERAND_TYPE);
        }
        break;

    default:

        /* Default case is for unknown types */

        ACPI_ERROR ((AE_INFO,
            "Node %p - Unknown object type 0x%X",
            Node, EntryType));

        return_ACPI_STATUS (AE_AML_OPERAND_TYPE);

    } /* switch (EntryType) */


    /* Return the object descriptor */

    *ObjectPtr = (void *) ObjDesc;
    return_ACPI_STATUS (Status);
}
