/******************************************************************************
 *
 * Module Name: pswalk - Parser routines to walk parsed op tree(s)
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
#include <contrib/dev/acpica/include/acparser.h>

#define _COMPONENT          ACPI_PARSER
        ACPI_MODULE_NAME    ("pswalk")


/*******************************************************************************
 *
 * FUNCTION:    AcpiPsDeleteParseTree
 *
 * PARAMETERS:  SubtreeRoot         - Root of tree (or subtree) to delete
 *
 * RETURN:      None
 *
 * DESCRIPTION: Delete a portion of or an entire parse tree.
 *
 ******************************************************************************/

#include <contrib/dev/acpica/include/amlcode.h>

void
AcpiPsDeleteParseTree (
    ACPI_PARSE_OBJECT       *SubtreeRoot)
{
    ACPI_PARSE_OBJECT       *Op = SubtreeRoot;
    ACPI_PARSE_OBJECT       *Next = NULL;
    ACPI_PARSE_OBJECT       *Parent = NULL;
    UINT32                  Level = 0;


    ACPI_FUNCTION_TRACE_PTR (PsDeleteParseTree, SubtreeRoot);

    ACPI_DEBUG_PRINT ((ACPI_DB_PARSE_TREES,
        " root %p\n", SubtreeRoot));

    /* Visit all nodes in the subtree */

    while (Op)
    {
        if (Op != Parent)
        {
            /* This is the descending case */

            if (ACPI_IS_DEBUG_ENABLED (ACPI_LV_PARSE_TREES, _COMPONENT))
            {
                /* This debug option will print the entire parse tree */

                AcpiOsPrintf ("        %*s%s %p", (Level * 4), "",
                    AcpiPsGetOpcodeName (Op->Common.AmlOpcode), Op);

                if (Op->Named.AmlOpcode == AML_INT_NAMEPATH_OP)
                {
                    AcpiOsPrintf ("    %4.4s", Op->Common.Value.String);
                }
                if (Op->Named.AmlOpcode == AML_STRING_OP)
                {
                    AcpiOsPrintf ("    %s", Op->Common.Value.String);
                }
                AcpiOsPrintf ("\n");
            }

            /* Look for an argument or child of the current op */

            Next = AcpiPsGetArg (Op, 0);
            if (Next)
            {
                /* Still going downward in tree (Op is not completed yet) */

                Op = Next;
                Level++;
                continue;
            }
        }

        /* No more children, this Op is complete. */

        Next = Op->Common.Next;
        Parent = Op->Common.Parent;

        AcpiPsFreeOp (Op);

        /* If we are back to the starting point, the walk is complete. */

        if (Op == SubtreeRoot)
        {
            return_VOID;
        }

        if (Next)
        {
            Op = Next;
        }
        else
        {
            Level--;
            Op = Parent;
        }
    }

    return_VOID;
}
