/*******************************************************************************
 *
 * Module Name: utownerid - Support for Table/Method Owner IDs
 *
 ******************************************************************************/

/******************************************************************************
 *
 * Copyright (c) 1999 - 2026, Intel Corp.
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0-only
 *
 *****************************************************************************/

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/acnamesp.h>


#define _COMPONENT          ACPI_UTILITIES
        ACPI_MODULE_NAME    ("utownerid")


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtAllocateOwnerId
 *
 * PARAMETERS:  OwnerId         - Where the new owner ID is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Allocate a table or method owner ID. The owner ID is used to
 *              track objects created by the table or method, to be deleted
 *              when the method exits or the table is unloaded.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiUtAllocateOwnerId (
    ACPI_OWNER_ID           *OwnerId)
{
    UINT32                  i;
    UINT32                  j;
    UINT32                  k;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE (UtAllocateOwnerId);


    /* Guard against multiple allocations of ID to the same location */

    if (*OwnerId)
    {
        ACPI_ERROR ((AE_INFO,
            "Owner ID [0x%3.3X] already exists", *OwnerId));
        return_ACPI_STATUS (AE_ALREADY_EXISTS);
    }

    /* Mutex for the global ID mask */

    Status = AcpiUtAcquireMutex (ACPI_MTX_CACHES);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /*
     * Find a free owner ID, cycle through all possible IDs on repeated
     * allocations. (ACPI_NUM_OWNERID_MASKS + 1) because first index
     * may have to be scanned twice.
     */
    for (i = 0, j = AcpiGbl_LastOwnerIdIndex;
         i < (ACPI_NUM_OWNERID_MASKS + 1);
         i++, j++)
    {
        if (j >= ACPI_NUM_OWNERID_MASKS)
        {
            j = 0;  /* Wraparound to start of mask array */
        }

        for (k = AcpiGbl_NextOwnerIdOffset; k < 32; k++)
        {
            if (AcpiGbl_OwnerIdMask[j] == ACPI_UINT32_MAX)
            {
                /* There are no free IDs in this mask */

                break;
            }

            /*
             * Note: the UINT32 cast ensures that 1 is stored as a unsigned
             * integer. Omitting the cast may result in 1 being stored as an
             * int. Some compilers or runtime error detection may flag this as
             * an error.
             */
            if (!(AcpiGbl_OwnerIdMask[j] & ((UINT32) 1 << k)))
            {
                /*
                 * Found a free ID. The actual ID is the bit index plus one,
                 * making zero an invalid Owner ID. Save this as the last ID
                 * allocated and update the global ID mask.
                 */
                AcpiGbl_OwnerIdMask[j] |= ((UINT32) 1 << k);

                AcpiGbl_LastOwnerIdIndex = (UINT8) j;
                AcpiGbl_NextOwnerIdOffset = (UINT8) (k + 1);

                /*
                 * Construct encoded ID from the index and bit position
                 *
                 * Note: Last [j].k (bit 4095) is never used and is marked
                 * permanently allocated (prevents +1 overflow)
                 */
                *OwnerId = (ACPI_OWNER_ID) ((k + 1) + ACPI_MUL_32 (j));

                ACPI_DEBUG_PRINT ((ACPI_DB_VALUES,
                    "Allocated OwnerId: 0x%3.3X\n", (unsigned int) *OwnerId));
                goto Exit;
            }
        }

        AcpiGbl_NextOwnerIdOffset = 0;
    }

    /*
     * All OwnerIds have been allocated. This typically should
     * not happen since the IDs are reused after deallocation. The IDs are
     * allocated upon table load (one per table) and method execution, and
     * they are released when a table is unloaded or a method completes
     * execution.
     *
     * If this error happens, there may be very deep nesting of invoked
     * control methods, or there may be a bug where the IDs are not released.
     */
    Status = AE_OWNER_ID_LIMIT;
    ACPI_ERROR ((AE_INFO,
        "Could not allocate new OwnerId (4095 max), AE_OWNER_ID_LIMIT"));

Exit:
    (void) AcpiUtReleaseMutex (ACPI_MTX_CACHES);
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtReleaseOwnerId
 *
 * PARAMETERS:  OwnerIdPtr          - Pointer to a previously allocated OwnerID
 *
 * RETURN:      None. No error is returned because we are either exiting a
 *              control method or unloading a table. Either way, we would
 *              ignore any error anyway.
 *
 * DESCRIPTION: Release a table or method owner ID. Valid IDs are 1 - 255
 *
 ******************************************************************************/

void
AcpiUtReleaseOwnerId (
    ACPI_OWNER_ID           *OwnerIdPtr)
{
    ACPI_OWNER_ID           OwnerId = *OwnerIdPtr;
    ACPI_STATUS             Status;
    UINT32                  Index;
    UINT32                  Bit;


    ACPI_FUNCTION_TRACE_U32 (UtReleaseOwnerId, OwnerId);


    /* Always clear the input OwnerId (zero is an invalid ID) */

    *OwnerIdPtr = 0;

    /* Zero is not a valid OwnerID */

    if (OwnerId == 0)
    {
        ACPI_ERROR ((AE_INFO, "Invalid OwnerId: 0x%3.3X", OwnerId));
        return_VOID;
    }

    /* Mutex for the global ID mask */

    Status = AcpiUtAcquireMutex (ACPI_MTX_CACHES);
    if (ACPI_FAILURE (Status))
    {
        return_VOID;
    }

    /* Normalize the ID to zero */

    OwnerId--;

    /* Decode ID to index/offset pair */

    Index = ACPI_DIV_32 (OwnerId);
    Bit = (UINT32) 1 << ACPI_MOD_32 (OwnerId);

    /* Free the owner ID only if it is valid */

    if (AcpiGbl_OwnerIdMask[Index] & Bit)
    {
        AcpiGbl_OwnerIdMask[Index] ^= Bit;
    }
    else
    {
        ACPI_ERROR ((AE_INFO,
            "Attempted release of non-allocated OwnerId: 0x%3.3X", OwnerId + 1));
    }

    (void) AcpiUtReleaseMutex (ACPI_MTX_CACHES);
    return_VOID;
}
