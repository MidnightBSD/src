/******************************************************************************
 *
 * Module Name: osgendbg - Generic debugger command signalling
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
#include <contrib/dev/acpica/include/acdebug.h>


#define _COMPONENT          ACPI_CA_DEBUGGER
        ACPI_MODULE_NAME    ("osgendbg")


/* Local prototypes */

static void
AcpiDbRunRemoteDebugger (
    char                    *BatchBuffer);


static ACPI_MUTEX           AcpiGbl_DbCommandReady;
static ACPI_MUTEX           AcpiGbl_DbCommandComplete;
static BOOLEAN              AcpiGbl_DbCommandSignalsInitialized = FALSE;


/******************************************************************************
 *
 * FUNCTION:    AcpiDbRunRemoteDebugger
 *
 * PARAMETERS:  BatchBuffer         - Buffer containing commands running in
 *                                    the batch mode
 *
 * RETURN:      None
 *
 * DESCRIPTION: Run multi-threading debugger remotely
 *
 *****************************************************************************/

static void
AcpiDbRunRemoteDebugger (
    char                    *BatchBuffer)
{
    ACPI_STATUS             Status;
    char                    *Ptr = BatchBuffer;
    char                    *Cmd = Ptr;


    while (!AcpiGbl_DbTerminateLoop)
    {
        if (BatchBuffer)
        {
            if (*Ptr)
            {
                while (*Ptr)
                {
                    if (*Ptr == ',')
                    {
                        /* Convert commas to spaces */
                        *Ptr = ' ';
                    }
                    else if (*Ptr == ';')
                    {
                        *Ptr = '\0';
                        continue;
                    }

                    Ptr++;
                }

                AcpiUtSafeStrncpy (AcpiGbl_DbLineBuf, Cmd, ACPI_DB_LINE_BUFFER_SIZE);
                Ptr++;
                Cmd = Ptr;
            }
            else
            {
                return;
            }
        }
        else
        {
            /* Force output to console until a command is entered */

            AcpiDbSetOutputDestination (ACPI_DB_CONSOLE_OUTPUT);

            /* Different prompt if method is executing */

            if (!AcpiGbl_MethodExecuting)
            {
                AcpiOsPrintf ("%1c ", ACPI_DEBUGGER_COMMAND_PROMPT);
            }
            else
            {
                AcpiOsPrintf ("%1c ", ACPI_DEBUGGER_EXECUTE_PROMPT);
            }

            /* Get the user input line */

            Status = AcpiOsGetLine (AcpiGbl_DbLineBuf,
                ACPI_DB_LINE_BUFFER_SIZE, NULL);
            if (ACPI_FAILURE (Status))
            {
                return;
            }
        }

        /*
         * Signal the debug thread that we have a command to execute,
         * and wait for the command to complete.
         */
        AcpiOsReleaseMutex (AcpiGbl_DbCommandReady);

        Status = AcpiOsAcquireMutex (AcpiGbl_DbCommandComplete,
            ACPI_WAIT_FOREVER);
        if (ACPI_FAILURE (Status))
        {
            return;
        }
    }
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsWaitCommandReady
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Negotiate with the debugger foreground thread (the user
 *              thread) to wait the readiness of a command.
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsWaitCommandReady (
    void)
{
    ACPI_STATUS             Status = AE_OK;


    if (AcpiGbl_DebuggerConfiguration == DEBUGGER_MULTI_THREADED)
    {
        Status = AE_TIME;

        while (Status == AE_TIME)
        {
            if (AcpiGbl_DbTerminateLoop)
            {
                Status = AE_CTRL_TERMINATE;
            }
            else
            {
                Status = AcpiOsAcquireMutex (AcpiGbl_DbCommandReady, 1000);
            }
        }
    }
    else
    {
        /* Force output to console until a command is entered */

        AcpiDbSetOutputDestination (ACPI_DB_CONSOLE_OUTPUT);

        /* Different prompt if method is executing */

        if (!AcpiGbl_MethodExecuting)
        {
            AcpiOsPrintf ("%1c ", ACPI_DEBUGGER_COMMAND_PROMPT);
        }
        else
        {
            AcpiOsPrintf ("%1c ", ACPI_DEBUGGER_EXECUTE_PROMPT);
        }

        /* Get the user input line */

        Status = AcpiOsGetLine (AcpiGbl_DbLineBuf,
            ACPI_DB_LINE_BUFFER_SIZE, NULL);
    }

    if (ACPI_FAILURE (Status) && Status != AE_CTRL_TERMINATE)
    {
        ACPI_EXCEPTION ((AE_INFO, Status,
            "While parsing/handling command line"));
    }
    return (Status);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsNotifyCommandComplete
 *
 * PARAMETERS:  void
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Negotiate with the debugger foreground thread (the user
 *              thread) to notify the completion of a command.
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsNotifyCommandComplete (
    void)
{

    if (AcpiGbl_DebuggerConfiguration == DEBUGGER_MULTI_THREADED)
    {
        AcpiOsReleaseMutex (AcpiGbl_DbCommandComplete);
    }
    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsInitializeDebugger
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Initialize OSPM specific part of the debugger
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsInitializeDebugger (
    void)
{
    ACPI_STATUS             Status;


    /* Create command signals */

    Status = AcpiOsCreateMutex (&AcpiGbl_DbCommandReady);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }
    Status = AcpiOsCreateMutex (&AcpiGbl_DbCommandComplete);
    if (ACPI_FAILURE (Status))
    {
        goto ErrorReady;
    }

    /* Initialize the states of the command signals */

    Status = AcpiOsAcquireMutex (AcpiGbl_DbCommandComplete,
        ACPI_WAIT_FOREVER);
    if (ACPI_FAILURE (Status))
    {
        goto ErrorComplete;
    }
    Status = AcpiOsAcquireMutex (AcpiGbl_DbCommandReady,
        ACPI_WAIT_FOREVER);
    if (ACPI_FAILURE (Status))
    {
        goto ErrorComplete;
    }

    AcpiGbl_DbCommandSignalsInitialized = TRUE;
    return (Status);

ErrorComplete:
    AcpiOsDeleteMutex (AcpiGbl_DbCommandComplete);
ErrorReady:
    AcpiOsDeleteMutex (AcpiGbl_DbCommandReady);
    return (Status);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsTerminateDebugger
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Terminate signals used by the multi-threading debugger
 *
 *****************************************************************************/

void
AcpiOsTerminateDebugger (
    void)
{

    if (AcpiGbl_DbCommandSignalsInitialized)
    {
        AcpiOsDeleteMutex (AcpiGbl_DbCommandReady);
        AcpiOsDeleteMutex (AcpiGbl_DbCommandComplete);
    }
}


/******************************************************************************
 *
 * FUNCTION:    AcpiRunDebugger
 *
 * PARAMETERS:  BatchBuffer         - Buffer containing commands running in
 *                                    the batch mode
 *
 * RETURN:      None
 *
 * DESCRIPTION: Run a local/remote debugger
 *
 *****************************************************************************/

void
AcpiRunDebugger (
    char                    *BatchBuffer)
{
    /* Check for single or multithreaded debug */

    if (AcpiGbl_DebuggerConfiguration & DEBUGGER_MULTI_THREADED)
    {
        AcpiDbRunRemoteDebugger (BatchBuffer);
    }
    else
    {
        AcpiDbUserCommands ();
    }
}

ACPI_EXPORT_SYMBOL (AcpiRunDebugger)
