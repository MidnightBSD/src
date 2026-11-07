/******************************************************************************
 *
 * Module Name: dttable.c - handling for specific ACPI tables
 *
 *****************************************************************************/

/******************************************************************************
 *
 * Copyright (c) 1999 - 2026, Intel Corp.
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0-only
 *
 *****************************************************************************/

/* Compile routines for the basic ACPI tables */

#include <contrib/dev/acpica/compiler/aslcompiler.h>

#define _COMPONENT          DT_COMPILER
        ACPI_MODULE_NAME    ("dttable")


/******************************************************************************
 *
 * FUNCTION:    DtCompileRsdp
 *
 * PARAMETERS:  PFieldList          - Current field list pointer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile RSDP.
 *
 *****************************************************************************/

ACPI_STATUS
DtCompileRsdp (
    DT_FIELD                **PFieldList)
{
    DT_SUBTABLE             *Subtable;
    ACPI_TABLE_RSDP         *Rsdp;
    ACPI_RSDP_EXTENSION     *RsdpExtension;
    ACPI_STATUS             Status;


    /* Compile the "common" RSDP (ACPI 1.0) */

    Status = DtCompileTable (PFieldList, AcpiDmTableInfoRsdp1,
        &AslGbl_RootTable);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    Rsdp = ACPI_CAST_PTR (ACPI_TABLE_RSDP, AslGbl_RootTable->Buffer);
    DtSetTableChecksum (&Rsdp->Checksum);

    if (Rsdp->Revision > 0)
    {
        /* Compile the "extended" part of the RSDP as a subtable */

        Status = DtCompileTable (PFieldList, AcpiDmTableInfoRsdp2,
            &Subtable);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        DtInsertSubtable (AslGbl_RootTable, Subtable);

        /* Set length and extended checksum for entire RSDP */

        RsdpExtension = ACPI_CAST_PTR (ACPI_RSDP_EXTENSION, Subtable->Buffer);
        RsdpExtension->Length = AslGbl_RootTable->Length + Subtable->Length;
        DtSetTableChecksum (&RsdpExtension->ExtendedChecksum);
    }

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    DtCompileFadt
 *
 * PARAMETERS:  List                - Current field list pointer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile FADT (signature FACP).
 *
 *****************************************************************************/

#define ACPI_XDSDT_LOCATION_IN_LIST         11

ACPI_STATUS
DtCompileFadt (
    void                    **List)
{
    ACPI_STATUS             Status;
    DT_SUBTABLE             *Subtable;
    DT_SUBTABLE             *ParentTable;
    DT_FIELD                **PFieldList = (DT_FIELD **) List;
    DT_FIELD                *DsdtFieldList;
    ACPI_TABLE_FADT         *Table;
    UINT8                   Revision;
    UINT32                  DsdtAddress;
    UINT64                  X_DsdtAddress;
    UINT32                  i;


    /* Get the table revision and 32-bit DSDT Address definition */

    DsdtFieldList = (*PFieldList)->Next;
    Status = DtCompileTable (PFieldList, AcpiDmTableInfoFadt1,
        &Subtable);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    ParentTable = DtPeekSubtable ();
    DtInsertSubtable (ParentTable, Subtable);

    Table = ACPI_CAST_PTR (ACPI_TABLE_FADT, ParentTable->Buffer);
    Revision = Table->Header.Revision;
    DsdtAddress = Table->Dsdt;

    /* FADT version 1 has only 32-bit addresses - error if DSDT address is NULL */

    if ((Revision == 1) && (!DsdtAddress))
    {
        DtError (ASL_ERROR, ASL_MSG_ZERO_VALUE, DsdtFieldList, NULL);
    }

    if (Revision == 2)
    {
        Status = DtCompileTable (PFieldList, AcpiDmTableInfoFadt2,
            &Subtable);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        DtInsertSubtable (ParentTable, Subtable);
    }

    else if (Revision > 2)
    {
        /*
         * Rev 3 and greater have 64-bit addresses (as well as 32-bit).
         * Get the 64-bit DSDT (X_DSDT) Address definition. Note: This
         * appears at field list offset 11 within AcpiDmTableInfoFadt3.
         */
        DsdtFieldList = *PFieldList;
        for (i = 0; i < ACPI_XDSDT_LOCATION_IN_LIST; i++)
        {
            DsdtFieldList = DsdtFieldList->Next;
            if (!DsdtFieldList)
            {
                return (ASL_MSG_BAD_PARSE_TREE);
            }
        }

        Status = DtCompileTable (PFieldList, AcpiDmTableInfoFadt3,
            &Subtable);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        DtInsertSubtable (ParentTable, Subtable);

        Table = ACPI_CAST_PTR (ACPI_TABLE_FADT, ParentTable->Buffer);
        X_DsdtAddress = Table->XDsdt;

        /*
         * Error if both the 32-bit DSDT address and the
         * 64-bit X_DSDT address are zero.
         */
        if ((!X_DsdtAddress) && (!DsdtAddress))
        {
            DtError (ASL_ERROR, ASL_MSG_TWO_ZERO_VALUES, DsdtFieldList, NULL);
        }

        /* Fields specific to FADT Revision 5 (appended to previous) */

        if (Revision >= 5)
        {
            Status = DtCompileTable (PFieldList, AcpiDmTableInfoFadt5,
                &Subtable);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }

            DtInsertSubtable (ParentTable, Subtable);
        }

        /* Fields specific to FADT Revision 6 (appended to previous) */

        if (Revision >= 6)
        {
            Status = DtCompileTable (PFieldList, AcpiDmTableInfoFadt6,
                &Subtable);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }

            DtInsertSubtable (ParentTable, Subtable);
        }
    }

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    DtCompileFacs
 *
 * PARAMETERS:  PFieldList          - Current field list pointer
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compile FACS.
 *
 *****************************************************************************/

ACPI_STATUS
DtCompileFacs (
    DT_FIELD                **PFieldList)
{
    DT_SUBTABLE             *Subtable;
    UINT8                   *ReservedBuffer;
    ACPI_STATUS             Status;
    UINT32                  ReservedSize;


    Status = DtCompileTable (PFieldList, AcpiDmTableInfoFacs,
        &AslGbl_RootTable);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    /* Large FACS reserved area at the end of the table */

    ReservedSize = (UINT32) sizeof (((ACPI_TABLE_FACS *) NULL)->Reserved1);
    ReservedBuffer = UtLocalCalloc (ReservedSize);

    DtCreateSubtable (ReservedBuffer, ReservedSize, &Subtable);

    ACPI_FREE (ReservedBuffer);
    DtInsertSubtable (AslGbl_RootTable, Subtable);
    return (AE_OK);
}
