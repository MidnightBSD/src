/******************************************************************************
 *
 * Module Name: aslallocate -- Local memory allocation
 *
 *****************************************************************************/

/******************************************************************************
 *
 * Copyright (c) 1999 - 2026, Intel Corp.
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0-only
 *
 *****************************************************************************/

#include <contrib/dev/acpica/compiler/aslcompiler.h>

/*
 * Local heap allocation wrappers. See aslcache.c for allocation from local
 * cache allocations
 */


/*******************************************************************************
 *
 * FUNCTION:    UtLocalCalloc
 *
 * PARAMETERS:  Size                - Bytes to be allocated
 *
 * RETURN:      Pointer to the allocated memory. If this function returns
 *              (the compiler is not aborted), the pointer is guaranteed to
 *              be valid.
 *
 * DESCRIPTION: Allocate zero-initialized memory. The point of this function
 *              is to abort the compile on an allocation failure, on the
 *              assumption that nothing more can be accomplished.
 *
 * NOTE:        For allocation from the local caches, see aslcache.c
 *
 ******************************************************************************/

void *
UtLocalCalloc (
    UINT32                  Size)
{
    void                    *Allocated;


    Allocated = ACPI_ALLOCATE_ZEROED (Size);
    if (!Allocated)
    {
        AslCommonError (ASL_ERROR, ASL_MSG_MEMORY_ALLOCATION,
            AslGbl_CurrentLineNumber, AslGbl_LogicalLineNumber,
            AslGbl_InputByteCount, AslGbl_CurrentColumn,
            AslGbl_Files[ASL_FILE_INPUT].Filename, NULL);

        CmCleanupAndExit ();
        exit (1);
    }

    AslGbl_TotalAllocations++;
    AslGbl_TotalAllocated += Size;
    return (Allocated);
}


/******************************************************************************
 *
 * FUNCTION:    UtExpandLineBuffers
 *
 * PARAMETERS:  None. Updates global line buffer pointers.
 *
 * RETURN:      None. Reallocates the global line buffers
 *
 * DESCRIPTION: Called if the current line buffer becomes filled. Reallocates
 *              all global line buffers and updates AslGbl_LineBufferSize. NOTE:
 *              Also used for the initial allocation of the buffers, when
 *              all of the buffer pointers are NULL. Initial allocations are
 *              of size ASL_DEFAULT_LINE_BUFFER_SIZE
 *
 *****************************************************************************/

void
UtExpandLineBuffers (
    void)
{
    UINT32                  NewSize;


    /* Attempt to double the size of all line buffers */

    NewSize = AslGbl_LineBufferSize * 2;
    if (AslGbl_CurrentLineBuffer)
    {
        DbgPrint (ASL_DEBUG_OUTPUT,
            "Increasing line buffer size from %u to %u\n",
            AslGbl_LineBufferSize, NewSize);
    }

    UtReallocLineBuffers (&AslGbl_CurrentLineBuffer, AslGbl_LineBufferSize, NewSize);
    UtReallocLineBuffers (&AslGbl_MainTokenBuffer, AslGbl_LineBufferSize, NewSize);
    UtReallocLineBuffers (&AslGbl_MacroTokenBuffer, AslGbl_LineBufferSize, NewSize);
    UtReallocLineBuffers (&AslGbl_ExpressionTokenBuffer, AslGbl_LineBufferSize, NewSize);

    AslGbl_LineBufPtr = AslGbl_CurrentLineBuffer;
    AslGbl_LineBufferSize = NewSize;
}


/******************************************************************************
 *
 * FUNCTION:    UtReallocLineBuffers
 *
 * PARAMETERS:  Buffer              - Buffer to realloc
 *              OldSize             - Old size of Buffer
 *              NewSize             - New size of Buffer
 *
 * RETURN:      none
 *
 * DESCRIPTION: Reallocate and initialize Buffer
 *
 *****************************************************************************/

void
UtReallocLineBuffers (
    char                    **Buffer,
    UINT32                  OldSize,
    UINT32                  NewSize)
{

    *Buffer = realloc (*Buffer, NewSize);
    if (*Buffer)
    {
        memset (*Buffer + OldSize, 0, NewSize - OldSize);
        return;
    }

    printf ("Could not increase line buffer size from %u to %u\n",
        OldSize, NewSize);

    AslError (ASL_ERROR, ASL_MSG_BUFFER_ALLOCATION, NULL, NULL);
    AslAbort ();
}


/******************************************************************************
 *
 * FUNCTION:    UtFreeLineBuffers
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Free all line buffers
 *
 *****************************************************************************/

void
UtFreeLineBuffers (
    void)
{
    free (AslGbl_CurrentLineBuffer);

    free (AslGbl_MainTokenBuffer);

    free (AslGbl_MacroTokenBuffer);

    free (AslGbl_MacroTokenReplaceBuffer);

    free (AslGbl_ExpressionTokenBuffer);
}
