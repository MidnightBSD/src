/*******************************************************************************
 *
 * Module Name: utxfmutex - external AML mutex access functions
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
        ACPI_MODULE_NAME    ("utxfmutex")


/* Local prototypes */

static ACPI_STATUS
AcpiUtGetMutexObject (
    ACPI_HANDLE             Handle,
    ACPI_STRING             Pathname,
    ACPI_OPERAND_OBJECT     **RetObj);


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtGetMutexObject
 *
 * PARAMETERS:  Handle              - Mutex or prefix handle (optional)
 *              Pathname            - Mutex pathname (optional)
 *              RetObj              - Where the mutex object is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Get an AML mutex object. The mutex node is pointed to by
 *              Handle:Pathname. Either Handle or Pathname can be NULL, but
 *              not both.
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiUtGetMutexObject (
    ACPI_HANDLE             Handle,
    ACPI_STRING             Pathname,
    ACPI_OPERAND_OBJECT     **RetObj)
{
    ACPI_NAMESPACE_NODE     *MutexNode;
    ACPI_OPERAND_OBJECT     *MutexObj;
    ACPI_STATUS             Status;


    /* Parameter validation */

    if (!RetObj || (!Handle && !Pathname))
    {
        return (AE_BAD_PARAMETER);
    }

    /* Get a the namespace node for the mutex */

    MutexNode = Handle;
    if (Pathname != NULL)
    {
        Status = AcpiGetHandle (
            Handle, Pathname, ACPI_CAST_PTR (ACPI_HANDLE, &MutexNode));
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }
    }

    /* Ensure that we actually have a Mutex object */

    if (!MutexNode ||
        (MutexNode->Type != ACPI_TYPE_MUTEX))
    {
        return (AE_TYPE);
    }

    /* Get the low-level mutex object */

    MutexObj = AcpiNsGetAttachedObject (MutexNode);
    if (!MutexObj)
    {
        return (AE_NULL_OBJECT);
    }

    *RetObj = MutexObj;
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiAcquireMutex
 *
 * PARAMETERS:  Handle              - Mutex or prefix handle (optional)
 *              Pathname            - Mutex pathname (optional)
 *              Timeout             - Max time to wait for the lock (millisec)
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Acquire an AML mutex. This is a device driver interface to
 *              AML mutex objects, and allows for transaction locking between
 *              drivers and AML code. The mutex node is pointed to by
 *              Handle:Pathname. Either Handle or Pathname can be NULL, but
 *              not both.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiAcquireMutex (
    ACPI_HANDLE             Handle,
    ACPI_STRING             Pathname,
    UINT16                  Timeout)
{
    ACPI_STATUS             Status;
    ACPI_OPERAND_OBJECT     *MutexObj;


    /* Get the low-level mutex associated with Handle:Pathname */

    Status = AcpiUtGetMutexObject (Handle, Pathname, &MutexObj);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    /* Acquire the OS mutex */

    Status = AcpiOsAcquireMutex (MutexObj->Mutex.OsMutex, Timeout);
    return (Status);
}

ACPI_EXPORT_SYMBOL (AcpiAcquireMutex)


/*******************************************************************************
 *
 * FUNCTION:    AcpiReleaseMutex
 *
 * PARAMETERS:  Handle              - Mutex or prefix handle (optional)
 *              Pathname            - Mutex pathname (optional)
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Release an AML mutex. This is a device driver interface to
 *              AML mutex objects, and allows for transaction locking between
 *              drivers and AML code. The mutex node is pointed to by
 *              Handle:Pathname. Either Handle or Pathname can be NULL, but
 *              not both.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiReleaseMutex (
    ACPI_HANDLE             Handle,
    ACPI_STRING             Pathname)
{
    ACPI_STATUS             Status;
    ACPI_OPERAND_OBJECT     *MutexObj;


    /* Get the low-level mutex associated with Handle:Pathname */

    Status = AcpiUtGetMutexObject (Handle, Pathname, &MutexObj);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    /* Release the OS mutex */

    AcpiOsReleaseMutex (MutexObj->Mutex.OsMutex);
    return (AE_OK);
}

ACPI_EXPORT_SYMBOL (AcpiReleaseMutex)
