/******************************************************************************
 *
 * Module Name: utlock - Reader/Writer lock interfaces
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


#define _COMPONENT          ACPI_UTILITIES
        ACPI_MODULE_NAME    ("utlock")


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtCreateRwLock
 *              AcpiUtDeleteRwLock
 *
 * PARAMETERS:  Lock                - Pointer to a valid RW lock
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Reader/writer lock creation and deletion interfaces.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiUtCreateRwLock (
    ACPI_RW_LOCK            *Lock)
{
    ACPI_STATUS             Status;


    Lock->NumReaders = 0;
    Status = AcpiOsCreateMutex (&Lock->ReaderMutex);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    Status = AcpiOsCreateMutex (&Lock->WriterMutex);
    return (Status);
}


void
AcpiUtDeleteRwLock (
    ACPI_RW_LOCK            *Lock)
{

    AcpiOsDeleteMutex (Lock->ReaderMutex);
    AcpiOsDeleteMutex (Lock->WriterMutex);

    Lock->NumReaders = 0;
    Lock->ReaderMutex = NULL;
    Lock->WriterMutex = NULL;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtAcquireReadLock
 *              AcpiUtReleaseReadLock
 *
 * PARAMETERS:  Lock                - Pointer to a valid RW lock
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Reader interfaces for reader/writer locks. On acquisition,
 *              only the first reader acquires the write mutex. On release,
 *              only the last reader releases the write mutex. Although this
 *              algorithm can in theory starve writers, this should not be a
 *              problem with ACPICA since the subsystem is infrequently used
 *              in comparison to (for example) an I/O system.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiUtAcquireReadLock (
    ACPI_RW_LOCK            *Lock)
{
    ACPI_STATUS             Status;


    Status = AcpiOsAcquireMutex (Lock->ReaderMutex, ACPI_WAIT_FOREVER);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    /* Acquire the write lock only for the first reader */

    Lock->NumReaders++;
    if (Lock->NumReaders == 1)
    {
        Status = AcpiOsAcquireMutex (Lock->WriterMutex, ACPI_WAIT_FOREVER);
    }

    AcpiOsReleaseMutex (Lock->ReaderMutex);
    return (Status);
}


ACPI_STATUS
AcpiUtReleaseReadLock (
    ACPI_RW_LOCK            *Lock)
{
    ACPI_STATUS             Status;


    Status = AcpiOsAcquireMutex (Lock->ReaderMutex, ACPI_WAIT_FOREVER);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    /* Release the write lock only for the very last reader */

    Lock->NumReaders--;
    if (Lock->NumReaders == 0)
    {
        AcpiOsReleaseMutex (Lock->WriterMutex);
    }

    AcpiOsReleaseMutex (Lock->ReaderMutex);
    return (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtAcquireWriteLock
 *              AcpiUtReleaseWriteLock
 *
 * PARAMETERS:  Lock                - Pointer to a valid RW lock
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Writer interfaces for reader/writer locks. Simply acquire or
 *              release the writer mutex associated with the lock. Acquisition
 *              of the lock is fully exclusive and will block all readers and
 *              writers until it is released.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiUtAcquireWriteLock (
    ACPI_RW_LOCK            *Lock)
{
    ACPI_STATUS             Status;


    Status = AcpiOsAcquireMutex (Lock->WriterMutex, ACPI_WAIT_FOREVER);
    return (Status);
}


void
AcpiUtReleaseWriteLock (
    ACPI_RW_LOCK            *Lock)
{

    AcpiOsReleaseMutex (Lock->WriterMutex);
}
