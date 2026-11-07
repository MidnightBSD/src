/******************************************************************************
 *
 * Module Name: getopt
 *
 *****************************************************************************/

/******************************************************************************
 *
 * Copyright (c) 1999 - 2026, Intel Corp.
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0-only
 *
 *****************************************************************************/

/*
 * ACPICA getopt() implementation
 *
 * Option strings:
 *    "f"       - Option has no arguments
 *    "f:"      - Option requires an argument
 *    "f+"      - Option has an optional argument
 *    "f^"      - Option has optional single-char sub-options
 *    "f|"      - Option has required single-char sub-options
 */

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/acapps.h>

#define ACPI_OPTION_ERROR(msg, badchar) \
    if (AcpiGbl_Opterr) {fprintf (stderr, "%s%c\n", msg, badchar);}


int                 AcpiGbl_Opterr = 1;
int                 AcpiGbl_Optind = 1;
int                 AcpiGbl_SubOptChar = 0;
char                *AcpiGbl_Optarg;

static int          CurrentCharPtr = 1;


/*******************************************************************************
 *
 * FUNCTION:    AcpiGetoptArgument
 *
 * PARAMETERS:  argc, argv          - from main
 *
 * RETURN:      0 if an argument was found, -1 otherwise. Sets AcpiGbl_Optarg
 *              to point to the next argument.
 *
 * DESCRIPTION: Get the next argument. Used to obtain arguments for the
 *              two-character options after the original call to AcpiGetopt.
 *              Note: Either the argument starts at the next character after
 *              the option, or it is pointed to by the next argv entry.
 *              (After call to AcpiGetopt, we need to backup to the previous
 *              argv entry).
 *
 ******************************************************************************/

int
AcpiGetoptArgument (
    int                     argc,
    char                    **argv)
{

    AcpiGbl_Optind--;
    CurrentCharPtr++;

    if (argv[AcpiGbl_Optind][(int) (CurrentCharPtr+1)] != '\0')
    {
        AcpiGbl_Optarg = &argv[AcpiGbl_Optind++][(int) (CurrentCharPtr+1)];
    }
    else if (++AcpiGbl_Optind >= argc)
    {
        ACPI_OPTION_ERROR ("\nOption requires an argument", 0);

        CurrentCharPtr = 1;
        return (-1);
    }
    else
    {
        AcpiGbl_Optarg = argv[AcpiGbl_Optind++];
    }

    CurrentCharPtr = 1;
    return (0);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiGetopt
 *
 * PARAMETERS:  argc, argv          - from main
 *              opts                - options info list
 *
 * RETURN:      Option character or ACPI_OPT_END
 *
 * DESCRIPTION: Get the next option
 *
 ******************************************************************************/

int
AcpiGetopt(
    int                     argc,
    char                    **argv,
    char                    *opts)
{
    int                     CurrentChar;
    char                    *OptsPtr;


    if (CurrentCharPtr == 1)
    {
        if (AcpiGbl_Optind >= argc ||
            argv[AcpiGbl_Optind][0] != '-' ||
            argv[AcpiGbl_Optind][1] == '\0')
        {
            return (ACPI_OPT_END);
        }
        else if (strcmp (argv[AcpiGbl_Optind], "--") == 0)
        {
            AcpiGbl_Optind++;
            return (ACPI_OPT_END);
        }
    }

    /* Get the option */

    CurrentChar = argv[AcpiGbl_Optind][CurrentCharPtr];

    /* Make sure that the option is legal */

    if (CurrentChar == ':' ||
       (OptsPtr = strchr (opts, CurrentChar)) == NULL)
    {
        ACPI_OPTION_ERROR ("Illegal option: -", CurrentChar);

        if (argv[AcpiGbl_Optind][++CurrentCharPtr] == '\0')
        {
            AcpiGbl_Optind++;
            CurrentCharPtr = 1;
        }

        return ('?');
    }

    /* Option requires an argument? */

    if (*++OptsPtr == ':')
    {
        if (argv[AcpiGbl_Optind][(int) (CurrentCharPtr+1)] != '\0')
        {
            AcpiGbl_Optarg = &argv[AcpiGbl_Optind++][(int) (CurrentCharPtr+1)];
        }
        else if (++AcpiGbl_Optind >= argc)
        {
            ACPI_OPTION_ERROR (
                "Option requires an argument: -", CurrentChar);

            CurrentCharPtr = 1;
            return ('?');
        }
        else
        {
            AcpiGbl_Optarg = argv[AcpiGbl_Optind++];
        }

        CurrentCharPtr = 1;
    }

    /* Option has an optional argument? */

    else if (*OptsPtr == '+')
    {
        if (argv[AcpiGbl_Optind][(int) (CurrentCharPtr+1)] != '\0')
        {
            AcpiGbl_Optarg = &argv[AcpiGbl_Optind++][(int) (CurrentCharPtr+1)];
        }
        else if (++AcpiGbl_Optind >= argc)
        {
            AcpiGbl_Optarg = NULL;
        }
        else
        {
            AcpiGbl_Optarg = argv[AcpiGbl_Optind++];
        }

        CurrentCharPtr = 1;
    }

    /* Option has optional single-char arguments? */

    else if (*OptsPtr == '^')
    {
        if (argv[AcpiGbl_Optind][(int) (CurrentCharPtr+1)] != '\0')
        {
            AcpiGbl_Optarg = &argv[AcpiGbl_Optind][(int) (CurrentCharPtr+1)];
        }
        else
        {
            AcpiGbl_Optarg = "^";
        }

        AcpiGbl_SubOptChar = AcpiGbl_Optarg[0];
        AcpiGbl_Optind++;
        CurrentCharPtr = 1;
    }

    /* Option has a required single-char argument? */

    else if (*OptsPtr == '|')
    {
        if (argv[AcpiGbl_Optind][(int) (CurrentCharPtr+1)] != '\0')
        {
            AcpiGbl_Optarg = &argv[AcpiGbl_Optind][(int) (CurrentCharPtr+1)];
        }
        else
        {
            ACPI_OPTION_ERROR (
                "Option requires a single-character suboption: -",
                CurrentChar);

            CurrentCharPtr = 1;
            return ('?');
        }

        AcpiGbl_SubOptChar = AcpiGbl_Optarg[0];
        AcpiGbl_Optind++;
        CurrentCharPtr = 1;
    }

    /* Option with no arguments */

    else
    {
        if (argv[AcpiGbl_Optind][++CurrentCharPtr] == '\0')
        {
            CurrentCharPtr = 1;
            AcpiGbl_Optind++;
        }

        AcpiGbl_Optarg = NULL;
    }

    return (CurrentChar);
}
