/*******************************************************************************
 *
 * Module Name: utexcep - Exception code support
 *
 ******************************************************************************/

/******************************************************************************
 *
 * Copyright (c) 1999 - 2026, Intel Corp.
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0-only
 *
 *****************************************************************************/

#define EXPORT_ACPI_INTERFACES

#define ACPI_DEFINE_EXCEPTION_TABLE
#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>


#define _COMPONENT          ACPI_UTILITIES
        ACPI_MODULE_NAME    ("utexcep")


/*******************************************************************************
 *
 * FUNCTION:    AcpiFormatException
 *
 * PARAMETERS:  Status              - The ACPI_STATUS code to be formatted
 *
 * RETURN:      A string containing the exception text. A valid pointer is
 *              always returned.
 *
 * DESCRIPTION: This function translates an ACPI exception into an ASCII
 *              string. Returns "unknown status" string for invalid codes.
 *
 ******************************************************************************/

const char *
AcpiFormatException (
    ACPI_STATUS             Status)
{
    const ACPI_EXCEPTION_INFO   *Exception;


    ACPI_FUNCTION_ENTRY ();


    Exception = AcpiUtValidateException (Status);
    if (!Exception)
    {
        /* Exception code was not recognized */

        ACPI_ERROR ((AE_INFO,
            "Unknown exception code: 0x%8.8X", Status));

        return ("UNKNOWN_STATUS_CODE");
    }

    return (Exception->Name);
}

ACPI_EXPORT_SYMBOL (AcpiFormatException)


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtValidateException
 *
 * PARAMETERS:  Status              - The ACPI_STATUS code to be formatted
 *
 * RETURN:      A string containing the exception text. NULL if exception is
 *              not valid.
 *
 * DESCRIPTION: This function validates and translates an ACPI exception into
 *              an ASCII string.
 *
 ******************************************************************************/

const ACPI_EXCEPTION_INFO *
AcpiUtValidateException (
    ACPI_STATUS             Status)
{
    UINT32                      SubStatus;
    const ACPI_EXCEPTION_INFO   *Exception = NULL;


    ACPI_FUNCTION_ENTRY ();


    /*
     * Status is composed of two parts, a "type" and an actual code
     */
    SubStatus = (Status & ~AE_CODE_MASK);

    switch (Status & AE_CODE_MASK)
    {
    case AE_CODE_ENVIRONMENTAL:

        if (SubStatus <= AE_CODE_ENV_MAX)
        {
            Exception = &AcpiGbl_ExceptionNames_Env [SubStatus];
        }
        break;

    case AE_CODE_PROGRAMMER:

        if (SubStatus <= AE_CODE_PGM_MAX)
        {
            Exception = &AcpiGbl_ExceptionNames_Pgm [SubStatus];
        }
        break;

    case AE_CODE_ACPI_TABLES:

        if (SubStatus <= AE_CODE_TBL_MAX)
        {
            Exception = &AcpiGbl_ExceptionNames_Tbl [SubStatus];
        }
        break;

    case AE_CODE_AML:

        if (SubStatus <= AE_CODE_AML_MAX)
        {
            Exception = &AcpiGbl_ExceptionNames_Aml [SubStatus];
        }
        break;

    case AE_CODE_CONTROL:

        if (SubStatus <= AE_CODE_CTRL_MAX)
        {
            Exception = &AcpiGbl_ExceptionNames_Ctrl [SubStatus];
        }
        break;

    default:

        break;
    }

    if (!Exception || !Exception->Name)
    {
        return (NULL);
    }

    return (Exception);
}
