//===-- TypeFormat.h ----------------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef lldb_TypeFormat_h_
#define lldb_TypeFormat_h_

// C Includes

// C++ Includes
#include <string>

// Other libraries and framework includes

// Project includes
#include "lldb/lldb-public.h"
#include "lldb/lldb-enumerations.h"

#include "lldb/Core/ValueObject.h"

namespace lldb_private {
    class TypeFormatImpl
    {
    public:
        class Flags
        {
        public:
            
            Flags () :
            m_flags (lldb::eTypeOptionCascade)
            {}
            
            Flags (const Flags& other) :
            m_flags (other.m_flags)
            {}
            
            Flags (uint32_t value) :
            m_flags (value)
            {}
            
            Flags&
            operator = (const Flags& rhs)
            {
                if (&rhs != this)
                    m_flags = rhs.m_flags;
                
                return *this;
            }
            
            Flags&
            operator = (const uint32_t& rhs)
            {
                m_flags = rhs;
                return *this;
            }
            
            Flags&
            Clear()
            {
                m_flags = 0;
                return *this;
            }
            
            bool
            GetCascades () const
            {
                return (m_flags & lldb::eTypeOptionCascade) == lldb::eTypeOptionCascade;
            }
            
            Flags&
            SetCascades (bool value = true)
            {
                if (value)
                    m_flags |= lldb::eTypeOptionCascade;
                else
                    m_flags &= ~lldb::eTypeOptionCascade;
                return *this;
            }
            
            bool
            GetSkipPointers () const
            {
                return (m_flags & lldb::eTypeOptionSkipPointers) == lldb::eTypeOptionSkipPointers;
            }
            
            Flags&
            SetSkipPointers (bool value = true)
            {
                if (value)
                    m_flags |= lldb::eTypeOptionSkipPointers;
                else
                    m_flags &= ~lldb::eTypeOptionSkipPointers;
                return *this;
            }
            
            bool
            GetSkipReferences () const
            {
                return (m_flags & lldb::eTypeOptionSkipReferences) == lldb::eTypeOptionSkipReferences;
            }
            
            Flags&
            SetSkipReferences (bool value = true)
            {
                if (value)
                    m_flags |= lldb::eTypeOptionSkipReferences;
                else
                    m_flags &= ~lldb::eTypeOptionSkipReferences;
                return *this;
            }
            
            uint32_t
            GetValue ()
            {
                return m_flags;
            }
            
            void
            SetValue (uint32_t value)
            {
                m_flags = value;
            }
            
        private:
            uint32_t m_flags;
        };
        
        TypeFormatImpl (const Flags& flags = Flags());
        
        typedef std::shared_ptr<TypeFormatImpl> SharedPointer;
        typedef bool(*ValueCallback)(void*, ConstString, const lldb::TypeFormatImplSP&);
        
        virtual ~TypeFormatImpl () = default;
        
        bool
        Cascades () const
        {
            return m_flags.GetCascades();
        }
        bool
        SkipsPointers () const
        {
            return m_flags.GetSkipPointers();
        }
        bool
        SkipsReferences () const
        {
            return m_flags.GetSkipReferences();
        }
        
        void
        SetCascades (bool value)
        {
            m_flags.SetCascades(value);
        }
        
        void
        SetSkipsPointers (bool value)
        {
            m_flags.SetSkipPointers(value);
        }
        
        void
        SetSkipsReferences (bool value)
        {
            m_flags.SetSkipReferences(value);
        }

        uint32_t
        GetOptions ()
        {
            return m_flags.GetValue();
        }
        
        void
        SetOptions (uint32_t value)
        {
            m_flags.SetValue(value);
        }
        
        uint32_t&
        GetRevision ()
        {
            return m_my_revision;
        }
        
        enum class Type
        {
            eTypeUnknown,
            eTypeFormat,
            eTypeEnum
        };
        
        virtual Type
        GetType ()
        {
            return Type::eTypeUnknown;
        }
        
        // we are using a ValueObject* instead of a ValueObjectSP because we do not need to hold on to this for
        // extended periods of time and we trust the ValueObject to stay around for as long as it is required
        // for us to generate its value
        virtual bool
        FormatObject (ValueObject *valobj,
                      std::string& dest) const = 0;
        
        virtual std::string
        GetDescription() = 0;
        
    protected:
        Flags m_flags;
        uint32_t m_my_revision;
        
    private:
        DISALLOW_COPY_AND_ASSIGN(TypeFormatImpl);
    };
    
    class TypeFormatImpl_Format : public TypeFormatImpl
    {
    public:
        TypeFormatImpl_Format (lldb::Format f = lldb::eFormatInvalid,
                               const TypeFormatImpl::Flags& flags = Flags());
        
        typedef std::shared_ptr<TypeFormatImpl_Format> SharedPointer;
        typedef bool(*ValueCallback)(void*, ConstString, const TypeFormatImpl_Format::SharedPointer&);
        
        virtual ~TypeFormatImpl_Format () = default;
        
        lldb::Format
        GetFormat () const
        {
            return m_format;
        }
        
        void
        SetFormat (lldb::Format fmt)
        {
            m_format = fmt;
        }
        
        virtual TypeFormatImpl::Type
        GetType ()
        {
            return TypeFormatImpl::Type::eTypeFormat;
        }
        
        virtual bool
        FormatObject (ValueObject *valobj,
                      std::string& dest) const;
        
        virtual std::string
        GetDescription();
        
    protected:
        lldb::Format m_format;
        
    private:
        DISALLOW_COPY_AND_ASSIGN(TypeFormatImpl_Format);
    };
    
    class TypeFormatImpl_EnumType : public TypeFormatImpl
    {
    public:
        TypeFormatImpl_EnumType (ConstString type_name = ConstString(""),
                                 const TypeFormatImpl::Flags& flags = Flags());
        
        typedef std::shared_ptr<TypeFormatImpl_EnumType> SharedPointer;
        typedef bool(*ValueCallback)(void*, ConstString, const TypeFormatImpl_EnumType::SharedPointer&);
        
        ~TypeFormatImpl_EnumType () = default;
        
        ConstString
        GetTypeName ()
        {
            return m_enum_type;
        }
        
        void
        SetTypeName (ConstString enum_type)
        {
            m_enum_type = enum_type;
        }
        
        virtual TypeFormatImpl::Type
        GetType ()
        {
            return TypeFormatImpl::Type::eTypeEnum;
        }
        
        virtual bool
        FormatObject (ValueObject *valobj,
                      std::string& dest) const;
        
        virtual std::string
        GetDescription();
        
    protected:
        ConstString m_enum_type;
        mutable std::map<void*,ClangASTType> m_types;
        
    private:
        DISALLOW_COPY_AND_ASSIGN(TypeFormatImpl_EnumType);
    };
} // namespace lldb_private

#endif	// lldb_TypeFormat_h_
