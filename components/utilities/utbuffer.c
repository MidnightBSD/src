/******************************************************************************
 *
 * Module Name: utbuffer - Buffer dump routines
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
        ACPI_MODULE_NAME    ("utbuffer")


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtDumpBuffer
 *
 * PARAMETERS:  Buffer              - Buffer to dump
 *              Count               - Amount to dump, in bytes
 *              Display             - BYTE, WORD, DWORD, or QWORD display:
 *                                      DB_BYTE_DISPLAY
 *                                      DB_WORD_DISPLAY
 *                                      DB_DWORD_DISPLAY
 *                                      DB_QWORD_DISPLAY
 *              BaseOffset          - Beginning buffer offset (display only)
 *
 * RETURN:      None
 *
 * DESCRIPTION: Generic dump buffer in both hex and ascii.
 *
 ******************************************************************************/

void
AcpiUtDumpBuffer (
    UINT8                   *Buffer,
    UINT32                  Count,
    UINT32                  Display,
    UINT32                  BaseOffset)
{
    UINT32                  i = 0;
    UINT32                  j;
    UINT32                  Temp32;
    UINT8                   BufChar;
    UINT32                  DisplayDataOnly = Display & DB_DISPLAY_DATA_ONLY;


    Display &= ~DB_DISPLAY_DATA_ONLY;
    if (!Buffer)
    {
        AcpiOsPrintf ("Null Buffer Pointer in DumpBuffer!\n");
        return;
    }

    if ((Count < 4) || (Count & 0x01))
    {
        Display = DB_BYTE_DISPLAY;
    }

    /* Nasty little dump buffer routine! */

    while (i < Count)
    {
        /* Print current offset */

        if (!DisplayDataOnly)
        {
            AcpiOsPrintf ("%8.4X: ", (BaseOffset + i));
        }

        /* Print 16 hex chars */

        for (j = 0; j < 16;)
        {
            if (i + j >= Count)
            {
                /* Dump fill spaces */

                AcpiOsPrintf ("%*s", ((Display * 2) + 1), " ");
                j += Display;
                continue;
            }

            switch (Display)
            {
            case DB_BYTE_DISPLAY:
            default:    /* Default is BYTE display */

                AcpiOsPrintf ("%02X ", Buffer[(ACPI_SIZE) i + j]);
                break;

            case DB_WORD_DISPLAY:

                ACPI_MOVE_16_TO_32 (&Temp32, &Buffer[(ACPI_SIZE) i + j]);
                AcpiOsPrintf ("%04X ", Temp32);
                break;

            case DB_DWORD_DISPLAY:

                ACPI_MOVE_32_TO_32 (&Temp32, &Buffer[(ACPI_SIZE) i + j]);
                AcpiOsPrintf ("%08X ", Temp32);
                break;

            case DB_QWORD_DISPLAY:

                ACPI_MOVE_32_TO_32 (&Temp32, &Buffer[(ACPI_SIZE) i + j]);
                AcpiOsPrintf ("%08X", Temp32);

                ACPI_MOVE_32_TO_32 (&Temp32, &Buffer[(ACPI_SIZE) i + j + 4]);
                AcpiOsPrintf ("%08X ", Temp32);
                break;
            }

            j += Display;
        }

        /*
         * Print the ASCII equivalent characters but watch out for the bad
         * unprintable ones (printable chars are 0x20 through 0x7E)
         */
        if (!DisplayDataOnly)
        {
            AcpiOsPrintf (" ");
            for (j = 0; j < 16; j++)
            {
                if (i + j >= Count)
                {
                    AcpiOsPrintf ("\n");
                    return;
                }

                /*
                 * Add comment characters so rest of line is ignored when
                 * compiled
                 */
                if (j == 0)
                {
                    AcpiOsPrintf ("// ");
                }

                BufChar = Buffer[(ACPI_SIZE) i + j];
                if (isprint (BufChar))
                {
                    AcpiOsPrintf ("%c", BufChar);
                }
                else
                {
                    AcpiOsPrintf (".");
                }
            }

            /* Done with that line. */

            AcpiOsPrintf ("\n");
        }
        i += 16;
    }

    return;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtDebugDumpBuffer
 *
 * PARAMETERS:  Buffer              - Buffer to dump
 *              Count               - Amount to dump, in bytes
 *              Display             - BYTE, WORD, DWORD, or QWORD display:
 *                                      DB_BYTE_DISPLAY
 *                                      DB_WORD_DISPLAY
 *                                      DB_DWORD_DISPLAY
 *                                      DB_QWORD_DISPLAY
 *              ComponentID         - Caller's component ID
 *
 * RETURN:      None
 *
 * DESCRIPTION: Generic dump buffer in both hex and ascii.
 *
 ******************************************************************************/

void
AcpiUtDebugDumpBuffer (
    UINT8                   *Buffer,
    UINT32                  Count,
    UINT32                  Display,
    UINT32                  ComponentId)
{

    /* Only dump the buffer if tracing is enabled */

    if (!((ACPI_LV_TABLES & AcpiDbgLevel) &&
        (ComponentId & AcpiDbgLayer)))
    {
        return;
    }

    AcpiUtDumpBuffer (Buffer, Count, Display, 0);
}


#ifdef ACPI_APPLICATION
/*******************************************************************************
 *
 * FUNCTION:    AcpiUtDumpBufferToFile
 *
 * PARAMETERS:  File                - File descriptor
 *              Buffer              - Buffer to dump
 *              Count               - Amount to dump, in bytes
 *              Display             - BYTE, WORD, DWORD, or QWORD display:
 *                                      DB_BYTE_DISPLAY
 *                                      DB_WORD_DISPLAY
 *                                      DB_DWORD_DISPLAY
 *                                      DB_QWORD_DISPLAY
 *              BaseOffset          - Beginning buffer offset (display only)
 *
 * RETURN:      None
 *
 * DESCRIPTION: Generic dump buffer in both hex and ascii to a file.
 *
 ******************************************************************************/

void
AcpiUtDumpBufferToFile (
    ACPI_FILE               File,
    UINT8                   *Buffer,
    UINT32                  Count,
    UINT32                  Display,
    UINT32                  BaseOffset)
{
    UINT32                  i = 0;
    UINT32                  j;
    UINT32                  Temp32;
    UINT8                   BufChar;


    if (!Buffer)
    {
        fprintf (File, "Null Buffer Pointer in DumpBuffer!\n");
        return;
    }

    if ((Count < 4) || (Count & 0x01))
    {
        Display = DB_BYTE_DISPLAY;
    }

    /* Nasty little dump buffer routine! */

    while (i < Count)
    {
        /* Print current offset */

        fprintf (File, "%8.4X: ", (BaseOffset + i));

        /* Print 16 hex chars */

        for (j = 0; j < 16;)
        {
            if (i + j >= Count)
            {
                /* Dump fill spaces */

                fprintf (File, "%*s", ((Display * 2) + 1), " ");
                j += Display;
                continue;
            }

            switch (Display)
            {
            case DB_BYTE_DISPLAY:
            default:    /* Default is BYTE display */

                fprintf (File, "%02X ", Buffer[(ACPI_SIZE) i + j]);
                break;

            case DB_WORD_DISPLAY:

                ACPI_MOVE_16_TO_32 (&Temp32, &Buffer[(ACPI_SIZE) i + j]);
                fprintf (File, "%04X ", Temp32);
                break;

            case DB_DWORD_DISPLAY:

                ACPI_MOVE_32_TO_32 (&Temp32, &Buffer[(ACPI_SIZE) i + j]);
                fprintf (File, "%08X ", Temp32);
                break;

            case DB_QWORD_DISPLAY:

                ACPI_MOVE_32_TO_32 (&Temp32, &Buffer[(ACPI_SIZE) i + j]);
                fprintf (File, "%08X", Temp32);

                ACPI_MOVE_32_TO_32 (&Temp32, &Buffer[(ACPI_SIZE) i + j + 4]);
                fprintf (File, "%08X ", Temp32);
                break;
            }

            j += Display;
        }

        /*
         * Print the ASCII equivalent characters but watch out for the bad
         * unprintable ones (printable chars are 0x20 through 0x7E)
         */
        fprintf (File, " ");
        for (j = 0; j < 16; j++)
        {
            if (i + j >= Count)
            {
                fprintf (File, "\n");
                return;
            }

            BufChar = Buffer[(ACPI_SIZE) i + j];
            if (isprint (BufChar))
            {
                fprintf (File, "%c", BufChar);
            }
            else
            {
                fprintf (File, ".");
            }
        }

        /* Done with that line. */

        fprintf (File, "\n");
        i += 16;
    }

    return;
}
#endif
