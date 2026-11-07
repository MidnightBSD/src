/******************************************************************************
 *
 * Module Name: nsload - namespace loading/expanding/contracting procedures
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
#include <contrib/dev/acpica/include/acnamesp.h>
#include <contrib/dev/acpica/include/acdispat.h>
#include <contrib/dev/acpica/include/actables.h>
#include <contrib/dev/acpica/include/acinterp.h>


#define _COMPONENT          ACPI_NAMESPACE
        ACPI_MODULE_NAME    ("nsload")

/* Local prototypes */

#ifdef ACPI_FUTURE_IMPLEMENTATION
ACPI_STATUS
AcpiNsUnloadNamespace (
    ACPI_HANDLE             Handle);

static ACPI_STATUS
AcpiNsDeleteSubtree (
    ACPI_HANDLE             StartHandle);
#endif


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsLoadTable
 *
 * PARAMETERS:  TableIndex      - Index for table to be loaded
 *              Node            - Owning NS node
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Load one ACPI table into the namespace
 *
 ******************************************************************************/

ACPI_STATUS
AcpiNsLoadTable (
    UINT32                  TableIndex,
    ACPI_NAMESPACE_NODE     *Node)
{
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE (NsLoadTable);


    /* If table already loaded into namespace, just return */

    if (AcpiTbIsTableLoaded (TableIndex))
    {
        Status = AE_ALREADY_EXISTS;
        goto Unlock;
    }

    ACPI_DEBUG_PRINT ((ACPI_DB_INFO,
        "**** Loading table into namespace ****\n"));

    Status = AcpiTbAllocateOwnerId (TableIndex);
    if (ACPI_FAILURE (Status))
    {
        goto Unlock;
    }

    /*
     * Parse the table and load the namespace with all named
     * objects found within. Control methods are NOT parsed
     * at this time. In fact, the control methods cannot be
     * parsed until the entire namespace is loaded, because
     * if a control method makes a forward reference (call)
     * to another control method, we can't continue parsing
     * because we don't know how many arguments to parse next!
     */
    Status = AcpiNsParseTable (TableIndex, Node);
    if (ACPI_SUCCESS (Status))
    {
        AcpiTbSetTableLoadedFlag (TableIndex, TRUE);
    }
    else
    {
        /*
         * On error, delete any namespace objects created by this table.
         * We cannot initialize these objects, so delete them. There are
         * a couple of especially bad cases:
         * AE_ALREADY_EXISTS - namespace collision.
         * AE_NOT_FOUND - the target of a Scope operator does not
         * exist. This target of Scope must already exist in the
         * namespace, as per the ACPI specification.
         */
        AcpiNsDeleteNamespaceByOwner (
            AcpiGbl_RootTableList.Tables[TableIndex].OwnerId);

        AcpiTbReleaseOwnerId (TableIndex);
        return_ACPI_STATUS (Status);
    }

Unlock:
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /*
     * Now we can parse the control methods. We always parse
     * them here for a sanity check, and if configured for
     * just-in-time parsing, we delete the control method
     * parse trees.
     */
    ACPI_DEBUG_PRINT ((ACPI_DB_INFO,
        "**** Begin Table Object Initialization\n"));

    AcpiExEnterInterpreter ();
    Status = AcpiDsInitializeObjects (TableIndex, Node);
    AcpiExExitInterpreter ();

    ACPI_DEBUG_PRINT ((ACPI_DB_INFO,
        "**** Completed Table Object Initialization\n"));

    return_ACPI_STATUS (Status);
}


#ifdef ACPI_OBSOLETE_FUNCTIONS
/*******************************************************************************
 *
 * FUNCTION:    AcpiLoadNamespace
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Load the name space from what ever is pointed to by DSDT.
 *              (DSDT points to either the BIOS or a buffer.)
 *
 ******************************************************************************/

ACPI_STATUS
AcpiNsLoadNamespace (
    void)
{
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE (AcpiLoadNameSpace);


    /* There must be at least a DSDT installed */

    if (AcpiGbl_DSDT == NULL)
    {
        ACPI_ERROR ((AE_INFO, "DSDT is not in memory"));
        return_ACPI_STATUS (AE_NO_ACPI_TABLES);
    }

    /*
     * Load the namespace. The DSDT is required,
     * but the SSDT and PSDT tables are optional.
     */
    Status = AcpiNsLoadTableByType (ACPI_TABLE_ID_DSDT);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Ignore exceptions from these */

    (void) AcpiNsLoadTableByType (ACPI_TABLE_ID_SSDT);
    (void) AcpiNsLoadTableByType (ACPI_TABLE_ID_PSDT);

    ACPI_DEBUG_PRINT_RAW ((ACPI_DB_INIT,
        "ACPI Namespace successfully loaded at root %p\n",
        AcpiGbl_RootNode));

    return_ACPI_STATUS (Status);
}
#endif

#ifdef ACPI_FUTURE_IMPLEMENTATION
/*******************************************************************************
 *
 * FUNCTION:    AcpiNsDeleteSubtree
 *
 * PARAMETERS:  StartHandle         - Handle in namespace where search begins
 *
 * RETURNS      Status
 *
 * DESCRIPTION: Walks the namespace starting at the given handle and deletes
 *              all objects, entries, and scopes in the entire subtree.
 *
 *              Namespace/Interpreter should be locked or the subsystem should
 *              be in shutdown before this routine is called.
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiNsDeleteSubtree (
    ACPI_HANDLE             StartHandle)
{
    ACPI_STATUS             Status;
    ACPI_HANDLE             ChildHandle;
    ACPI_HANDLE             ParentHandle;
    ACPI_HANDLE             NextChildHandle;
    ACPI_HANDLE             Dummy;
    UINT32                  Level;


    ACPI_FUNCTION_TRACE (NsDeleteSubtree);


    ParentHandle = StartHandle;
    ChildHandle = NULL;
    Level = 1;

    /*
     * Traverse the tree of objects until we bubble back up
     * to where we started.
     */
    while (Level > 0)
    {
        /* Attempt to get the next object in this scope */

        Status = AcpiGetNextObject (ACPI_TYPE_ANY, ParentHandle,
            ChildHandle, &NextChildHandle);

        ChildHandle = NextChildHandle;

        /* Did we get a new object? */

        if (ACPI_SUCCESS (Status))
        {
            /* Check if this object has any children */

            if (ACPI_SUCCESS (AcpiGetNextObject (ACPI_TYPE_ANY, ChildHandle,
                NULL, &Dummy)))
            {
                /*
                 * There is at least one child of this object,
                 * visit the object
                 */
                Level++;
                ParentHandle = ChildHandle;
                ChildHandle  = NULL;
            }
        }
        else
        {
            /*
             * No more children in this object, go back up to
             * the object's parent
             */
            Level--;

            /* Delete all children now */

            AcpiNsDeleteChildren (ChildHandle);

            ChildHandle = ParentHandle;
            Status = AcpiGetParent (ParentHandle, &ParentHandle);
            if (ACPI_FAILURE (Status))
            {
                return_ACPI_STATUS (Status);
            }
        }
    }

    /* Now delete the starting object, and we are done */

    AcpiNsRemoveNode (ChildHandle);
    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 *  FUNCTION:       AcpiNsUnloadNameSpace
 *
 *  PARAMETERS:     Handle          - Root of namespace subtree to be deleted
 *
 *  RETURN:         Status
 *
 *  DESCRIPTION:    Shrinks the namespace, typically in response to an undocking
 *                  event. Deletes an entire subtree starting from (and
 *                  including) the given handle.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiNsUnloadNamespace (
    ACPI_HANDLE             Handle)
{
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE (NsUnloadNameSpace);


    /* Parameter validation */

    if (!AcpiGbl_RootNode)
    {
        return_ACPI_STATUS (AE_NO_NAMESPACE);
    }

    if (!Handle)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    /* This function does the real work */

    Status = AcpiNsDeleteSubtree (Handle);
    return_ACPI_STATUS (Status);
}
#endif
