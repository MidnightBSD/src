//===-- DataBufferMemoryMap.cpp ---------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//


#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#ifdef _WIN32
#include "lldb/Host/windows/windows.h"
#else
#include <sys/mman.h>
#endif

#include "lldb/Core/DataBufferMemoryMap.h"
#include "lldb/Core/Error.h"
#include "lldb/Host/File.h"
#include "lldb/Host/FileSpec.h"
#include "lldb/Host/Host.h"
#include "lldb/Core/Log.h"
#include "lldb/lldb-private-log.h"

using namespace lldb;
using namespace lldb_private;

//----------------------------------------------------------------------
// Default Constructor
//----------------------------------------------------------------------
DataBufferMemoryMap::DataBufferMemoryMap() :
    m_mmap_addr(NULL),
    m_mmap_size(0),
    m_data(NULL),
    m_size(0)
{
}

//----------------------------------------------------------------------
// Virtual destructor since this class inherits from a pure virtual
// base class.
//----------------------------------------------------------------------
DataBufferMemoryMap::~DataBufferMemoryMap()
{
    Clear();
}

//----------------------------------------------------------------------
// Return a pointer to the bytes owned by this object, or NULL if
// the object contains no bytes.
//----------------------------------------------------------------------
uint8_t *
DataBufferMemoryMap::GetBytes()
{
    return m_data;
}

//----------------------------------------------------------------------
// Return a const pointer to the bytes owned by this object, or NULL
// if the object contains no bytes.
//----------------------------------------------------------------------
const uint8_t *
DataBufferMemoryMap::GetBytes() const
{
    return m_data;
}

//----------------------------------------------------------------------
// Return the number of bytes this object currently contains.
//----------------------------------------------------------------------
uint64_t
DataBufferMemoryMap::GetByteSize() const
{
    return m_size;
}

//----------------------------------------------------------------------
// Reverts this object to an empty state by unmapping any memory
// that is currently owned.
//----------------------------------------------------------------------
void
DataBufferMemoryMap::Clear()
{
    if (m_mmap_addr != NULL)
    {
        Log *log(lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_MMAP));
        if (log)
            log->Printf("DataBufferMemoryMap::Clear() m_mmap_addr = %p, m_mmap_size = %zu", m_mmap_addr, m_mmap_size);
#ifdef _WIN32
        UnmapViewOfFile(m_mmap_addr);
#else
        ::munmap((void *)m_mmap_addr, m_mmap_size);
#endif
        m_mmap_addr = NULL;
        m_mmap_size = 0;
        m_data = NULL;
        m_size = 0;
    }
}

//----------------------------------------------------------------------
// Memory map "length" bytes from "file" starting "offset"
// bytes into the file. If "length" is set to SIZE_MAX, then
// map as many bytes as possible.
//
// Returns the number of bytes mapped starting from the requested
// offset.
//----------------------------------------------------------------------
size_t
DataBufferMemoryMap::MemoryMapFromFileSpec (const FileSpec* filespec,
                                            lldb::offset_t offset,
                                            lldb::offset_t length,
                                            bool writeable)
{
    if (filespec != NULL)
    {
        Log *log(lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_MMAP));
        if (log)
        {
            log->Printf("DataBufferMemoryMap::MemoryMapFromFileSpec(file=\"%s\", offset=0x%" PRIx64 ", length=0x%" PRIx64 ", writeable=%i",
                        filespec->GetPath().c_str(),
                        offset,
                        length,
                        writeable);
        }
        char path[PATH_MAX];
        if (filespec->GetPath(path, sizeof(path)))
        {
            uint32_t options = File::eOpenOptionRead;
            if (writeable)
                options |= File::eOpenOptionWrite;

            File file;
            Error error (file.Open(path, options));
            if (error.Success())
            {
                const bool fd_is_file = true;
                return MemoryMapFromFileDescriptor (file.GetDescriptor(), offset, length, writeable, fd_is_file);
            }
        }
    }
    // We should only get here if there was an error
    Clear();
    return 0;
}


#ifdef _WIN32
static size_t win32memmapalignment = 0;
void LoadWin32MemMapAlignment ()
{
  SYSTEM_INFO data;
  GetSystemInfo(&data);
  win32memmapalignment = data.dwAllocationGranularity;
}
#endif

//----------------------------------------------------------------------
// The file descriptor FD is assumed to already be opened as read only
// and the STAT structure is assumed to a valid pointer and already
// containing valid data from a call to stat().
//
// Memory map FILE_LENGTH bytes in FILE starting FILE_OFFSET bytes into
// the file. If FILE_LENGTH is set to SIZE_MAX, then map as many bytes
// as possible.
//
// RETURNS
//  Number of bytes mapped starting from the requested offset.
//----------------------------------------------------------------------
size_t
DataBufferMemoryMap::MemoryMapFromFileDescriptor (int fd, 
                                                  lldb::offset_t offset, 
                                                  lldb::offset_t length,
                                                  bool writeable,
                                                  bool fd_is_file)
{
    Clear();
    if (fd >= 0)
    {
        Log *log(lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_MMAP|LIBLLDB_LOG_VERBOSE));
        if (log)
        {
#ifdef _WIN32
            log->Printf("DataBufferMemoryMap::MemoryMapFromFileSpec(fd=%p, offset=0x%" PRIx64 ", length=0x%" PRIx64 ", writeable=%i, fd_is_file=%i)",
#else
            log->Printf("DataBufferMemoryMap::MemoryMapFromFileSpec(fd=%i, offset=0x%" PRIx64 ", length=0x%" PRIx64 ", writeable=%i, fd_is_file=%i)",
#endif
                        fd,
                        offset,
                        length,
                        writeable,
                        fd_is_file);
        }
#ifdef _WIN32
        HANDLE handle = (HANDLE)_get_osfhandle(fd);
        DWORD file_size_low, file_size_high;
        file_size_low = GetFileSize(handle, &file_size_high);
        const size_t file_size = (file_size_high << 32) | file_size_low;
        const size_t max_bytes_available = file_size - offset;
        if (length == SIZE_MAX)
        {
            length = max_bytes_available;
        }
        else if (length > max_bytes_available)
        {
            // Cap the length if too much data was requested
            length = max_bytes_available;
        }

        if (length > 0)
        {
            HANDLE fileMapping = CreateFileMapping(handle, NULL, writeable ? PAGE_READWRITE : PAGE_READONLY, file_size_high, file_size_low, NULL);
            if (fileMapping != NULL)
            {
                if (win32memmapalignment == 0) LoadWin32MemMapAlignment();
                lldb::offset_t realoffset = offset;
                lldb::offset_t delta = 0;
                if (realoffset % win32memmapalignment != 0) {
                  realoffset = realoffset / win32memmapalignment * win32memmapalignment;
                  delta = offset - realoffset;
	              }

                LPVOID data = MapViewOfFile(fileMapping, writeable ? FILE_MAP_WRITE : FILE_MAP_READ, 0, realoffset, length + delta);
                m_mmap_addr = (uint8_t *)data;
                if (!data) {
                  Error error; 
                  error.SetErrorToErrno ();
                } else {
                  m_data = m_mmap_addr + delta;
                  m_size = length;
                }
                CloseHandle(fileMapping);
            }
        }
#else
        struct stat stat;
        if (::fstat(fd, &stat) == 0)
        {
            if (S_ISREG(stat.st_mode) && (stat.st_size > offset))
            {
                const size_t max_bytes_available = stat.st_size - offset;
                if (length == SIZE_MAX)
                {
                    length = max_bytes_available;
                }
                else if (length > max_bytes_available)
                {
                    // Cap the length if too much data was requested
                    length = max_bytes_available;
                }

                if (length > 0)
                {
                    int prot = PROT_READ;
                    if (writeable)
                        prot |= PROT_WRITE;

                    int flags = MAP_PRIVATE;
                    if (fd_is_file)
                        flags |= MAP_FILE;

                    m_mmap_addr = (uint8_t *)::mmap(NULL, length, prot, flags, fd, offset);
                    Error error;

                    if (m_mmap_addr == (void*)-1)
                    {
                        error.SetErrorToErrno ();
                        if (error.GetError() == EINVAL)
                        {
                            // We may still have a shot at memory mapping if we align things correctly
                            size_t page_offset = offset % Host::GetPageSize();
                            if (page_offset != 0)
                            {
                                m_mmap_addr = (uint8_t *)::mmap(NULL, length + page_offset, prot, flags, fd, offset - page_offset);
                                if (m_mmap_addr == (void*)-1)
                                {
                                    // Failed to map file
                                    m_mmap_addr = NULL;
                                }
                                else if (m_mmap_addr != NULL)
                                {
                                    // We recovered and were able to memory map
                                    // after we aligned things to page boundaries

                                    // Save the actual mmap'ed size
                                    m_mmap_size = length + page_offset;
                                    // Our data is at an offset into the the mapped data
                                    m_data = m_mmap_addr + page_offset;
                                    // Our pretend size is the size that was requestd
                                    m_size = length;
                                }
                            }
                        }
                        if (error.GetError() == ENOMEM)
                        {
                           error.SetErrorStringWithFormat("could not allocate %" PRId64 " bytes of memory to mmap in file", (uint64_t) length);
                        }
                    }
                    else
                    {
                        // We were able to map the requested data in one chunk
                        // where our mmap and actual data are the same.
                        m_mmap_size = length;
                        m_data = m_mmap_addr;
                        m_size = length;
                    }
                    
                    if (log)
                    {
                        log->Printf("DataBufferMemoryMap::MemoryMapFromFileSpec() m_mmap_addr = %p, m_mmap_size = %zu, error = %s",
                                    m_mmap_addr, m_mmap_size, error.AsCString());
                    }
                }
            }
        }
#endif
    }
    return GetByteSize ();
}
