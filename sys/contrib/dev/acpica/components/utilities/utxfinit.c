/******************************************************************************
 *
 * Module Name: utxfinit - External interfaces for ACPICA initialization
 *
 *****************************************************************************/

/******************************************************************************
 *
 * Copyright (c) 1999 - 2026, Intel Corp.
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0-only
 *
 *****************************************************************************/

#define EXPORT_ACPI_INTERFACES

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/acevents.h>
#include <contrib/dev/acpica/include/acnamesp.h>
#include <contrib/dev/acpica/include/acdebug.h>
#include <contrib/dev/acpica/include/actables.h>

#define _COMPONENT          ACPI_UTILITIES
        ACPI_MODULE_NAME    ("utxfinit")

/* For AcpiExec only */
void
AeDoObjectOverrides (
    void);


/*******************************************************************************
 *
 * FUNCTION:    AcpiInitializeSubsystem
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Initializes all global variables. This is the first function
 *              called, so any early initialization belongs here.
 *
 ******************************************************************************/

ACPI_STATUS ACPI_INIT_FUNCTION
AcpiInitializeSubsystem (
    void)
{
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE (AcpiInitializeSubsystem);


    AcpiGbl_StartupFlags = ACPI_SUBSYSTEM_INITIALIZE;
    ACPI_DEBUG_EXEC (AcpiUtInitStackPtrTrace ());

    /* Initialize the OS-Dependent layer */

    Status = AcpiOsInitialize ();
    if (ACPI_FAILURE (Status))
    {
        ACPI_EXCEPTION ((AE_INFO, Status, "During OSL initialization"));
        return_ACPI_STATUS (Status);
    }

    /* Initialize all globals used by the subsystem */

    Status = AcpiUtInitGlobals ();
    if (ACPI_FAILURE (Status))
    {
        ACPI_EXCEPTION ((AE_INFO, Status, "During initialization of globals"));
        return_ACPI_STATUS (Status);
    }

    /* Create the default mutex objects */

    Status = AcpiUtMutexInitialize ();
    if (ACPI_FAILURE (Status))
    {
        ACPI_EXCEPTION ((AE_INFO, Status, "During Global Mutex creation"));
        return_ACPI_STATUS (Status);
    }

    /*
     * Initialize the namespace manager and
     * the root of the namespace tree
     */
    Status = AcpiNsRootInitialize ();
    if (ACPI_FAILURE (Status))
    {
        ACPI_EXCEPTION ((AE_INFO, Status, "During Namespace initialization"));
        return_ACPI_STATUS (Status);
    }

    /* Initialize the global OSI interfaces list with the static names */

    Status = AcpiUtInitializeInterfaces ();
    if (ACPI_FAILURE (Status))
    {
        ACPI_EXCEPTION ((AE_INFO, Status, "During OSI interfaces initialization"));
        return_ACPI_STATUS (Status);
    }

    return_ACPI_STATUS (AE_OK);
}

ACPI_EXPORT_SYMBOL_INIT (AcpiInitializeSubsystem)


/*******************************************************************************
 *
 * FUNCTION:    AcpiEnableSubsystem
 *
 * PARAMETERS:  Flags               - Init/enable Options
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Completes the subsystem initialization including hardware.
 *              Puts system into ACPI mode if it isn't already.
 *
 ******************************************************************************/

ACPI_STATUS ACPI_INIT_FUNCTION
AcpiEnableSubsystem (
    UINT32                  Flags)
{
    ACPI_STATUS             Status = AE_OK;


    ACPI_FUNCTION_TRACE (AcpiEnableSubsystem);


    /*
     * The early initialization phase is complete. The namespace is loaded,
     * and we can now support address spaces other than Memory, I/O, and
     * PCI_Config.
     */
    AcpiGbl_EarlyInitialization = FALSE;

    /*
     * Obtain a permanent mapping for the FACS. This is required for the
     * Global Lock and the Firmware Waking Vector
     */
    if (!(Flags & ACPI_NO_FACS_INIT))
    {
        Status = AcpiTbInitializeFacs ();
        if (ACPI_FAILURE (Status))
        {
            ACPI_WARNING ((AE_INFO, "Could not map the FACS table"));
            return_ACPI_STATUS (Status);
        }
    }

#if (!ACPI_REDUCED_HARDWARE)

    /* Enable ACPI mode */

    if (!(Flags & ACPI_NO_ACPI_ENABLE))
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "[Init] Going into ACPI mode\n"));

        AcpiGbl_OriginalMode = AcpiHwGetMode();

        Status = AcpiEnable ();
        if (ACPI_FAILURE (Status))
        {
            ACPI_WARNING ((AE_INFO, "AcpiEnable failed"));
            return_ACPI_STATUS (Status);
        }
    }

    /*
     * Initialize ACPI Event handling (Fixed and General Purpose)
     *
     * Note1: We must have the hardware and events initialized before we can
     * execute any control methods safely. Any control method can require
     * ACPI hardware support, so the hardware must be fully initialized before
     * any method execution!
     *
     * Note2: Fixed events are initialized and enabled here. GPEs are
     * initialized, but cannot be enabled until after the hardware is
     * completely initialized (SCI and GlobalLock activated) and the various
     * initialization control methods are run (_REG, _STA, _INI) on the
     * entire namespace.
     */
    if (!(Flags & ACPI_NO_EVENT_INIT))
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_EXEC,
            "[Init] Initializing ACPI events\n"));

        Status = AcpiEvInitializeEvents ();
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }
    }

    /*
     * Install the SCI handler and Global Lock handler. This completes the
     * hardware initialization.
     */
    if (!(Flags & ACPI_NO_HANDLER_INIT))
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_EXEC,
            "[Init] Installing SCI/GL handlers\n"));

        Status = AcpiEvInstallXruptHandlers ();
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }
    }

#endif /* !ACPI_REDUCED_HARDWARE */

    return_ACPI_STATUS (Status);
}

ACPI_EXPORT_SYMBOL_INIT (AcpiEnableSubsystem)


/*******************************************************************************
 *
 * FUNCTION:    AcpiInitializeObjects
 *
 * PARAMETERS:  Flags               - Init/enable Options
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Completes namespace initialization by initializing device
 *              objects and executing AML code for Regions, buffers, etc.
 *
 ******************************************************************************/

ACPI_STATUS ACPI_INIT_FUNCTION
AcpiInitializeObjects (
    UINT32                  Flags)
{
    ACPI_STATUS             Status = AE_OK;


    ACPI_FUNCTION_TRACE (AcpiInitializeObjects);


#ifdef ACPI_OBSOLETE_BEHAVIOR
    /*
     * 05/2019: Removed, initialization now happens at both object
     * creation and table load time
     */

    /*
     * Initialize the objects that remain uninitialized. This
     * runs the executable AML that may be part of the
     * declaration of these objects: OperationRegions, BufferFields,
     * BankFields, Buffers, and Packages.
     */
    if (!(Flags & ACPI_NO_OBJECT_INIT))
    {
        Status = AcpiNsInitializeObjects ();
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }
    }
#endif

    /*
     * Initialize all device/region objects in the namespace. This runs
     * the device _STA and _INI methods and region _REG methods.
     */
    if (!(Flags & (ACPI_NO_DEVICE_INIT | ACPI_NO_ADDRESS_SPACE_INIT)))
    {
        Status = AcpiNsInitializeDevices (Flags);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }
    }

    /*
     * Empty the caches (delete the cached objects) on the assumption that
     * the table load filled them up more than they will be at runtime --
     * thus wasting non-paged memory.
     */
    Status = AcpiPurgeCachedObjects ();

    AcpiGbl_StartupFlags |= ACPI_INITIALIZED_OK;
    return_ACPI_STATUS (Status);
}

ACPI_EXPORT_SYMBOL_INIT (AcpiInitializeObjects)
