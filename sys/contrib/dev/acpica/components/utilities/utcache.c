/******************************************************************************
 *
 * Module Name: utcache - local cache allocation routines
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
        ACPI_MODULE_NAME    ("utcache")


#ifdef ACPI_USE_LOCAL_CACHE
/*******************************************************************************
 *
 * FUNCTION:    AcpiOsCreateCache
 *
 * PARAMETERS:  CacheName       - Ascii name for the cache
 *              ObjectSize      - Size of each cached object
 *              MaxDepth        - Maximum depth of the cache (in objects)
 *              ReturnCache     - Where the new cache object is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create a cache object
 *
 ******************************************************************************/

ACPI_STATUS
AcpiOsCreateCache (
    char                    *CacheName,
    UINT16                  ObjectSize,
    UINT16                  MaxDepth,
    ACPI_MEMORY_LIST        **ReturnCache)
{
    ACPI_MEMORY_LIST        *Cache;


    ACPI_FUNCTION_ENTRY ();


    if (!CacheName || !ReturnCache || !ObjectSize)
    {
        return (AE_BAD_PARAMETER);
    }

    /* Create the cache object */

    Cache = AcpiOsAllocate (sizeof (ACPI_MEMORY_LIST));
    if (!Cache)
    {
        return (AE_NO_MEMORY);
    }

    /* Populate the cache object and return it */

    memset (Cache, 0, sizeof (ACPI_MEMORY_LIST));
    Cache->ListName = CacheName;
    Cache->ObjectSize = ObjectSize;
    Cache->MaxDepth = MaxDepth;

    *ReturnCache = Cache;
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiOsPurgeCache
 *
 * PARAMETERS:  Cache           - Handle to cache object
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Free all objects within the requested cache.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiOsPurgeCache (
    ACPI_MEMORY_LIST        *Cache)
{
    void                    *Next;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_ENTRY ();


    if (!Cache)
    {
        return (AE_BAD_PARAMETER);
    }

    Status = AcpiUtAcquireMutex (ACPI_MTX_CACHES);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    /* Walk the list of objects in this cache */

    while (Cache->ListHead)
    {
        /* Delete and unlink one cached state object */

        Next = ACPI_GET_DESCRIPTOR_PTR (Cache->ListHead);
        ACPI_FREE (Cache->ListHead);

        Cache->ListHead = Next;
        Cache->CurrentDepth--;
    }

    (void) AcpiUtReleaseMutex (ACPI_MTX_CACHES);
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiOsDeleteCache
 *
 * PARAMETERS:  Cache           - Handle to cache object
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Free all objects within the requested cache and delete the
 *              cache object.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiOsDeleteCache (
    ACPI_MEMORY_LIST        *Cache)
{
    ACPI_STATUS             Status;


    ACPI_FUNCTION_ENTRY ();


   /* Purge all objects in the cache */

    Status = AcpiOsPurgeCache (Cache);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    /* Now we can delete the cache object */

    AcpiOsFree (Cache);
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiOsReleaseObject
 *
 * PARAMETERS:  Cache       - Handle to cache object
 *              Object      - The object to be released
 *
 * RETURN:      None
 *
 * DESCRIPTION: Release an object to the specified cache. If cache is full,
 *              the object is deleted.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiOsReleaseObject (
    ACPI_MEMORY_LIST        *Cache,
    void                    *Object)
{
    ACPI_STATUS             Status;


    ACPI_FUNCTION_ENTRY ();


    if (!Cache || !Object)
    {
        return (AE_BAD_PARAMETER);
    }

    /* If cache is full, just free this object */

    if (Cache->CurrentDepth >= Cache->MaxDepth)
    {
        ACPI_FREE (Object);
        ACPI_MEM_TRACKING (Cache->TotalFreed++);
    }

    /* Otherwise put this object back into the cache */

    else
    {
        Status = AcpiUtAcquireMutex (ACPI_MTX_CACHES);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        /* Mark the object as cached */

        memset (Object, 0xCA, Cache->ObjectSize);
        ACPI_SET_DESCRIPTOR_TYPE (Object, ACPI_DESC_TYPE_CACHED);

        /* Put the object at the head of the cache list */

        ACPI_SET_DESCRIPTOR_PTR (Object, Cache->ListHead);
        Cache->ListHead = Object;
        Cache->CurrentDepth++;

        (void) AcpiUtReleaseMutex (ACPI_MTX_CACHES);
    }

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiOsAcquireObject
 *
 * PARAMETERS:  Cache           - Handle to cache object
 *
 * RETURN:      the acquired object. NULL on error
 *
 * DESCRIPTION: Get an object from the specified cache. If cache is empty,
 *              the object is allocated.
 *
 ******************************************************************************/

void *
AcpiOsAcquireObject (
    ACPI_MEMORY_LIST        *Cache)
{
    ACPI_STATUS             Status;
    void                    *Object;


    ACPI_FUNCTION_TRACE (OsAcquireObject);


    if (!Cache)
    {
        return_PTR (NULL);
    }

    Status = AcpiUtAcquireMutex (ACPI_MTX_CACHES);
    if (ACPI_FAILURE (Status))
    {
        return_PTR (NULL);
    }

    ACPI_MEM_TRACKING (Cache->Requests++);

    /* Check the cache first */

    if (Cache->ListHead)
    {
        /* There is an object available, use it */

        Object = Cache->ListHead;
        Cache->ListHead = ACPI_GET_DESCRIPTOR_PTR (Object);

        Cache->CurrentDepth--;

        ACPI_MEM_TRACKING (Cache->Hits++);
        ACPI_DEBUG_PRINT_RAW ((ACPI_DB_EXEC,
            "%s: Object %p from %s cache\n",
            ACPI_GET_FUNCTION_NAME, Object, Cache->ListName));

        Status = AcpiUtReleaseMutex (ACPI_MTX_CACHES);
        if (ACPI_FAILURE (Status))
        {
            return_PTR (NULL);
        }

        /* Clear (zero) the previously used Object */

        memset (Object, 0, Cache->ObjectSize);
    }
    else
    {
        /* The cache is empty, create a new object */

#ifdef ACPI_DBG_TRACK_ALLOCATIONS
	ACPI_MEM_TRACKING (Cache->TotalAllocated++);

        if ((Cache->TotalAllocated - Cache->TotalFreed) > Cache->MaxOccupied)
        {
            Cache->MaxOccupied = Cache->TotalAllocated - Cache->TotalFreed;
        }
#endif

        /* Avoid deadlock with ACPI_ALLOCATE_ZEROED */

        Status = AcpiUtReleaseMutex (ACPI_MTX_CACHES);
        if (ACPI_FAILURE (Status))
        {
            return_PTR (NULL);
        }

        Object = ACPI_ALLOCATE_ZEROED (Cache->ObjectSize);
        if (!Object)
        {
            return_PTR (NULL);
        }
    }

    return_PTR (Object);
}
#endif /* ACPI_USE_LOCAL_CACHE */
