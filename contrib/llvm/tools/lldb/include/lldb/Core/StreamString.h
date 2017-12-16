//===-- StreamString.h ------------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef liblldb_StreamString_h_
#define liblldb_StreamString_h_

#include <string>

#include "lldb/Core/Stream.h"

namespace lldb_private {

class StreamString : public Stream
{
public:
    StreamString ();

    StreamString (uint32_t flags,
                  uint32_t addr_size,
                  lldb::ByteOrder byte_order);

    virtual
    ~StreamString ();

    virtual void
    Flush ();

    virtual size_t
    Write (const void *s, size_t length);

    void
    Clear();

    bool
    Empty() const;

    const char *
    GetData () const;

    size_t
    GetSize() const;

    std::string &
    GetString();

    const std::string &
    GetString() const;

    void
    FillLastLineToColumn (uint32_t column, char fill_char);

protected:
    std::string m_packet;

};

} // namespace lldb_private
#endif  // #ifndef liblldb_StreamString_h_
