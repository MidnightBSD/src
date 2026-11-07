/******************************************************************************
 *
 * Module Name: aslprune - Parse tree prune utility
 *
 *****************************************************************************/

/******************************************************************************
 *
 * Copyright (c) 1999 - 2026, Intel Corp.
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0-only
 *
 *****************************************************************************/

#include <contrib/dev/acpica/compiler/aslcompiler.h>
#include "aslcompiler.y.h"
#include <contrib/dev/acpica/include/acapps.h>

#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslprune")


/* Local prototypes */

static ACPI_STATUS
PrTreePruneWalk (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);

static void
PrPrintObjectAtLevel (
    UINT32                  Level,
    const char              *ObjectName);


/* Structure used for the pruning parse tree walk */

typedef struct acpi_prune_info
{
    UINT32                  PruneLevel;
    UINT16                  ParseOpcode;
    UINT16                  Count;

} ACPI_PRUNE_INFO;


/*******************************************************************************
 *
 * FUNCTION:    AslPruneParseTree
 *
 * PARAMETERS:  PruneDepth              - Number of levels to prune
 *              Type                    - Prune type (Device, Method, etc.)
 *
 * RETURN:      None
 *
 * DESCRIPTION: Prune off one or more levels of the ASL parse tree
 *
 ******************************************************************************/

void
AslPruneParseTree (
    UINT32                  PruneDepth,
    UINT32                  Type)
{
    ACPI_PRUNE_INFO         PruneObj;


    PruneObj.PruneLevel = PruneDepth;
    PruneObj.Count = 0;

    switch (Type)
    {
    case 0:
        PruneObj.ParseOpcode = (UINT16) PARSEOP_DEVICE;
        break;

    case 1:
        PruneObj.ParseOpcode = (UINT16) PARSEOP_METHOD;
        break;

    case 2:
        PruneObj.ParseOpcode = (UINT16) PARSEOP_IF;
        break;

    default:
        AcpiOsPrintf ("Unsupported type: %u\n", Type);
        return;
    }

    AcpiOsPrintf ("Pruning parse tree, from depth %u\n",
        PruneDepth);

    AcpiOsPrintf ("\nRemoving Objects:\n");

    TrWalkParseTree (AslGbl_ParseTreeRoot, ASL_WALK_VISIT_DOWNWARD,
        PrTreePruneWalk, NULL, ACPI_CAST_PTR (void, &PruneObj));

    AcpiOsPrintf ("\n%u Total Objects Removed\n", PruneObj.Count);
}


/*******************************************************************************
 *
 * FUNCTION:    PrPrintObjectAtLevel
 *
 * PARAMETERS:  Level                   - Current nesting level
 *              ObjectName              - ACPI name for the object
 *
 * RETURN:      None
 *
 * DESCRIPTION: Print object name with indent
 *
 ******************************************************************************/

static void
PrPrintObjectAtLevel (
    UINT32                  Level,
    const char              *ObjectName)
{
    UINT32                  i;


    for (i = 0; i < Level; i++)
    {
        AcpiOsPrintf ("  ");
    }

    AcpiOsPrintf ("[%s] at Level [%u]\n", ObjectName, Level);
}


/*******************************************************************************
 *
 * FUNCTION:    PrTreePruneWalk
 *
 * PARAMETERS:  Parse tree walk callback
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Prune off one or more levels of the ASL parse tree
 *
 * Current objects that can be pruned are: Devices, Methods, and If/Else
 * blocks.
 *
 ******************************************************************************/

static ACPI_STATUS
PrTreePruneWalk (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{
    ACPI_PRUNE_INFO         *PruneObj = (ACPI_PRUNE_INFO *) Context;


    /* We only care about objects below the Prune Level threshold */

    if (Level <= PruneObj->PruneLevel)
    {
        return (AE_OK);
    }

    if ((Op->Asl.ParseOpcode != PruneObj->ParseOpcode) &&
       !(Op->Asl.ParseOpcode == PARSEOP_ELSE &&
             PruneObj->ParseOpcode == PARSEOP_IF))
    {
        return (AE_OK);
    }

    switch (Op->Asl.ParseOpcode)
    {
    case PARSEOP_METHOD:

        AcpiOsPrintf ("Method");
        PrPrintObjectAtLevel (Level, Op->Asl.Child->Asl.Value.Name);
        Op->Asl.Child->Asl.Next->Asl.Next->Asl.Next->Asl.Next->Asl.Next->Asl.Next = NULL;
        PruneObj->Count++;
        break;

    case PARSEOP_DEVICE:

        AcpiOsPrintf ("Device");
        PrPrintObjectAtLevel (Level, Op->Asl.Child->Asl.Value.Name);
        Op->Asl.Child->Asl.Next = NULL;
        PruneObj->Count++;
        break;

    case PARSEOP_IF:
    case PARSEOP_ELSE:

        if (Op->Asl.ParseOpcode == PARSEOP_ELSE)
        {
            PrPrintObjectAtLevel(Level, "Else");
            Op->Asl.Child = NULL;
        }
        else
        {
            PrPrintObjectAtLevel(Level, "If");
            Op->Asl.Child->Asl.Next = NULL;
        }

        PruneObj->Count++;
        break;

    default:

        break;
    }

    return (AE_OK);
}
