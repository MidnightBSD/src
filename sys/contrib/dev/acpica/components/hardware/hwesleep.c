/******************************************************************************
 *
 * Name: hwesleep.c - ACPI Hardware Sleep/Wake Support functions for the
 *                    extended FADT-V5 sleep registers.
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

#define _COMPONENT          ACPI_HARDWARE
        ACPI_MODULE_NAME    ("hwesleep")


/*******************************************************************************
 *
 * FUNCTION:    AcpiHwExecuteSleepMethod
 *
 * PARAMETERS:  MethodPathname      - Pathname of method to execute
 *              IntegerArgument     - Argument to pass to the method
 *
 * RETURN:      None
 *
 * DESCRIPTION: Execute a sleep/wake related method with one integer argument
 *              and no return value.
 *
 ******************************************************************************/

void
AcpiHwExecuteSleepMethod (
    char                    *MethodPathname,
    UINT32                  IntegerArgument)
{
    ACPI_OBJECT_LIST        ArgList;
    ACPI_OBJECT             Arg;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE (HwExecuteSleepMethod);


    /* One argument, IntegerArgument; No return value expected */

    ArgList.Count = 1;
    ArgList.Pointer = &Arg;
    Arg.Type = ACPI_TYPE_INTEGER;
    Arg.Integer.Value = (UINT64) IntegerArgument;

    Status = AcpiEvaluateObject (NULL, MethodPathname, &ArgList, NULL);
    if (ACPI_FAILURE (Status) && Status != AE_NOT_FOUND)
    {
        ACPI_EXCEPTION ((AE_INFO, Status, "While executing method %s",
            MethodPathname));
    }

    return_VOID;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiHwExtendedSleep
 *
 * PARAMETERS:  SleepState          - Which sleep state to enter
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Enter a system sleep state via the extended FADT sleep
 *              registers (V5 FADT).
 *              THIS FUNCTION MUST BE CALLED WITH INTERRUPTS DISABLED
 *
 ******************************************************************************/

ACPI_STATUS
AcpiHwExtendedSleep (
    UINT8                   SleepState)
{
    ACPI_STATUS             Status;
    UINT8                   SleepControl;
    UINT64                  SleepStatus;


    ACPI_FUNCTION_TRACE (HwExtendedSleep);


    /* Extended sleep registers must be valid */

    if (!AcpiGbl_FADT.SleepControl.Address ||
        !AcpiGbl_FADT.SleepStatus.Address)
    {
        return_ACPI_STATUS (AE_NOT_EXIST);
    }

    /* Clear wake status (WAK_STS) */

    Status = AcpiWrite ((UINT64) ACPI_X_WAKE_STATUS,
        &AcpiGbl_FADT.SleepStatus);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    AcpiGbl_SystemAwakeAndRunning = FALSE;

    /*
     * Set the SLP_TYP and SLP_EN bits.
     *
     * Note: We only use the first value returned by the \_Sx method
     * (AcpiGbl_SleepTypeA) - As per ACPI specification.
     */
    ACPI_DEBUG_PRINT ((ACPI_DB_INIT,
        "Entering sleep state [S%u]\n", SleepState));

    SleepControl = ((AcpiGbl_SleepTypeA << ACPI_X_SLEEP_TYPE_POSITION) &
        ACPI_X_SLEEP_TYPE_MASK) | ACPI_X_SLEEP_ENABLE;

    /* Flush caches, as per ACPI specification */

    if (SleepState < ACPI_STATE_S4)
    {
        ACPI_FLUSH_CPU_CACHE ();
    }

    Status = AcpiOsEnterSleep (SleepState, SleepControl, 0);
    if (Status == AE_CTRL_TERMINATE)
    {
        return_ACPI_STATUS (AE_OK);
    }
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    Status = AcpiWrite ((UINT64) SleepControl, &AcpiGbl_FADT.SleepControl);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Wait for transition back to Working State */

    do
    {
        Status = AcpiRead (&SleepStatus, &AcpiGbl_FADT.SleepStatus);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }

    } while (!(((UINT8) SleepStatus) & ACPI_X_WAKE_STATUS));

    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiHwExtendedWakePrep
 *
 * PARAMETERS:  SleepState          - Which sleep state we just exited
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Perform first part of OS-independent ACPI cleanup after
 *              a sleep. Called with interrupts ENABLED.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiHwExtendedWakePrep (
    UINT8                   SleepState)
{
    UINT8                   SleepTypeValue;


    ACPI_FUNCTION_TRACE (HwExtendedWakePrep);


    if (AcpiGbl_SleepTypeAS0 != ACPI_SLEEP_TYPE_INVALID)
    {
        SleepTypeValue = ((AcpiGbl_SleepTypeAS0 << ACPI_X_SLEEP_TYPE_POSITION) &
            ACPI_X_SLEEP_TYPE_MASK);

        (void) AcpiWrite ((UINT64) (SleepTypeValue | ACPI_X_SLEEP_ENABLE),
            &AcpiGbl_FADT.SleepControl);
    }

    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiHwExtendedWake
 *
 * PARAMETERS:  SleepState          - Which sleep state we just exited
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Perform OS-independent ACPI cleanup after a sleep
 *              Called with interrupts ENABLED.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiHwExtendedWake (
    UINT8                   SleepState)
{
    ACPI_FUNCTION_TRACE (HwExtendedWake);


    /* Ensure EnterSleepStatePrep -> EnterSleepState ordering */

    AcpiGbl_SleepTypeA = ACPI_SLEEP_TYPE_INVALID;

    /* Execute the wake methods */

    AcpiHwExecuteSleepMethod (METHOD_PATHNAME__SST, ACPI_SST_WAKING);
    AcpiHwExecuteSleepMethod (METHOD_PATHNAME__WAK, SleepState);

    /*
     * Some BIOS code assumes that WAK_STS will be cleared on resume
     * and use it to determine whether the system is rebooting or
     * resuming. Clear WAK_STS for compatibility.
     */
    (void) AcpiWrite ((UINT64) ACPI_X_WAKE_STATUS, &AcpiGbl_FADT.SleepStatus);
    AcpiGbl_SystemAwakeAndRunning = TRUE;

    AcpiHwExecuteSleepMethod (METHOD_PATHNAME__SST, ACPI_SST_WORKING);
    return_ACPI_STATUS (AE_OK);
}
