/******************************************************************************
 *
 * Module Name: cmfsize - Common get file size function
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
#include <contrib/dev/acpica/include/acapps.h>

#define _COMPONENT          ACPI_TOOLS
        ACPI_MODULE_NAME    ("cmfsize")


/*******************************************************************************
 *
 * FUNCTION:    CmGetFileSize
 *
 * PARAMETERS:  File                    - Open file descriptor
 *
 * RETURN:      File Size. On error, -1 (ACPI_UINT32_MAX)
 *
 * DESCRIPTION: Get the size of a file. Uses seek-to-EOF. File must be open.
 *              Does not disturb the current file pointer.
 *
 ******************************************************************************/

UINT32
CmGetFileSize (
    ACPI_FILE               File)
{
    long                    FileSize;
    long                    CurrentOffset;
    ACPI_STATUS             Status;


    /* Save the current file pointer, seek to EOF to obtain file size */

    CurrentOffset = ftell (File);
    if (CurrentOffset < 0)
    {
        goto OffsetError;
    }

    Status = fseek (File, 0, SEEK_END);
    if (ACPI_FAILURE (Status))
    {
        goto SeekError;
    }

    FileSize = ftell (File);
    if (FileSize < 0)
    {
        goto OffsetError;
    }

    /* Restore original file pointer */

    Status = fseek (File, CurrentOffset, SEEK_SET);
    if (ACPI_FAILURE (Status))
    {
        goto SeekError;
    }

    return ((UINT32) FileSize);


OffsetError:
    fprintf (stderr, "Could not get file offset\n");
    return (ACPI_UINT32_MAX);

SeekError:
    fprintf (stderr, "Could not set file offset\n");
    return (ACPI_UINT32_MAX);
}
