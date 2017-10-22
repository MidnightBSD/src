//===-- FormatNavigator.h ----------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef lldb_FormatNavigator_h_
#define lldb_FormatNavigator_h_

// C Includes
// C++ Includes

// Other libraries and framework includes
#include "clang/AST/DeclCXX.h"
#include "clang/AST/Type.h"
#include "clang/AST/DeclObjC.h"

// Project includes
#include "lldb/lldb-public.h"

#include "lldb/Core/Log.h"
#include "lldb/Core/RegularExpression.h"
#include "lldb/Core/ValueObject.h"

#include "lldb/DataFormatters/FormatClasses.h"

#include "lldb/Symbol/ClangASTContext.h"
#include "lldb/Symbol/ClangASTType.h"

#include "lldb/Target/ObjCLanguageRuntime.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/StackFrame.h"
#include "lldb/Target/TargetList.h"

namespace lldb_private {
    
// this file (and its. cpp) contain the low-level implementation of LLDB Data Visualization
// class DataVisualization is the high-level front-end of this feature
// clients should refer to that class as the entry-point into the data formatters
// unless they have a good reason to bypass it and prefer to use this file's objects directly
class IFormatChangeListener
{
public:
    virtual void
    Changed () = 0;
    
    virtual
    ~IFormatChangeListener () {}
    
    virtual uint32_t
    GetCurrentRevision () = 0;
    
};
    
static inline bool
IsWhitespace (char c)
{
    return ( (c == ' ') || (c == '\t') || (c == '\v') || (c == '\f') );
}

static inline bool
HasPrefix (const char* str1, const char* str2)
{
    return ( ::strstr(str1, str2) == str1 );
}
    
// if the user tries to add formatters for, say, "struct Foo"
// those will not match any type because of the way we strip qualifiers from typenames
// this method looks for the case where the user is adding a "class","struct","enum" or "union" Foo
// and strips the unnecessary qualifier
static inline ConstString
GetValidTypeName_Impl (const ConstString& type)
{
    int strip_len = 0;
    
    if (type == false)
        return type;
    
    const char* type_cstr = type.AsCString();
    
    if ( HasPrefix(type_cstr, "class ") )
        strip_len = 6;
    else if ( HasPrefix(type_cstr, "enum ") )
        strip_len = 5;
    else if ( HasPrefix(type_cstr, "struct ") )
        strip_len = 7;
    else if ( HasPrefix(type_cstr, "union ") )
        strip_len = 6;
    
    if (strip_len == 0)
        return type;
    
    type_cstr += strip_len;
    while (IsWhitespace(*type_cstr) && ++type_cstr)
        ;
    
    return ConstString(type_cstr);
}
    
template<typename KeyType, typename ValueType>
class FormatNavigator;

template<typename KeyType, typename ValueType>
class FormatMap
{
public:

    typedef typename ValueType::SharedPointer ValueSP;
    typedef std::map<KeyType, ValueSP> MapType;
    typedef typename MapType::iterator MapIterator;
    typedef bool(*CallbackType)(void*, KeyType, const ValueSP&);
    
    FormatMap(IFormatChangeListener* lst) :
    m_map(),
    m_map_mutex(Mutex::eMutexTypeRecursive),
    listener(lst)
    {
    }
    
    void
    Add(KeyType name,
        const ValueSP& entry)
    {
        if (listener)
            entry->GetRevision() = listener->GetCurrentRevision();
        else
            entry->GetRevision() = 0;

        Mutex::Locker locker(m_map_mutex);
        m_map[name] = entry;
        if (listener)
            listener->Changed();
    }
    
    bool
    Delete (KeyType name)
    {
        Mutex::Locker locker(m_map_mutex);
        MapIterator iter = m_map.find(name);
        if (iter == m_map.end())
            return false;
        m_map.erase(name);
        if (listener)
            listener->Changed();
        return true;
    }
    
    void
    Clear ()
    {
        Mutex::Locker locker(m_map_mutex);
        m_map.clear();
        if (listener)
            listener->Changed();
    }
    
    bool
    Get(KeyType name,
        ValueSP& entry)
    {
        Mutex::Locker locker(m_map_mutex);
        MapIterator iter = m_map.find(name);
        if (iter == m_map.end())
            return false;
        entry = iter->second;
        return true;
    }
    
    void
    LoopThrough (CallbackType callback, void* param)
    {
        if (callback)
        {
            Mutex::Locker locker(m_map_mutex);
            MapIterator pos, end = m_map.end();
            for (pos = m_map.begin(); pos != end; pos++)
            {
                KeyType type = pos->first;
                if (!callback(param, type, pos->second))
                    break;
            }
        }
    }
    
    uint32_t
    GetCount ()
    {
        return m_map.size();
    }
    
    ValueSP
    GetValueAtIndex (size_t index)
    {
        Mutex::Locker locker(m_map_mutex);
        MapIterator iter = m_map.begin();
        MapIterator end = m_map.end();
        while (index > 0)
        {
            iter++;
            index--;
            if (end == iter)
                return ValueSP();
        }
        return iter->second;
    }
    
    KeyType
    GetKeyAtIndex (size_t index)
    {
        Mutex::Locker locker(m_map_mutex);
        MapIterator iter = m_map.begin();
        MapIterator end = m_map.end();
        while (index > 0)
        {
            iter++;
            index--;
            if (end == iter)
                return KeyType();
        }
        return iter->first;
    }
    
protected:
    MapType m_map;    
    Mutex m_map_mutex;
    IFormatChangeListener* listener;
    
    MapType&
    map ()
    {
        return m_map;
    }
    
    Mutex&
    mutex ()
    {
        return m_map_mutex;
    }
    
    friend class FormatNavigator<KeyType, ValueType>;
    friend class FormatManager;
    
};
    
template<typename KeyType, typename ValueType>
class FormatNavigator
{
protected:
    typedef FormatMap<KeyType,ValueType> BackEndType;
    
public:
    typedef typename BackEndType::MapType MapType;
    typedef typename MapType::iterator MapIterator;
    typedef typename MapType::key_type MapKeyType;
    typedef typename MapType::mapped_type MapValueType;
    typedef typename BackEndType::CallbackType CallbackType;
    typedef typename std::shared_ptr<FormatNavigator<KeyType, ValueType> > SharedPointer;
    
    friend class TypeCategoryImpl;

    FormatNavigator(std::string name,
                    IFormatChangeListener* lst) :
    m_format_map(lst),
    m_name(name),
    m_id_cs(ConstString("id"))
    {
    }
    
    void
    Add (const MapKeyType &type, const MapValueType& entry)
    {
        Add_Impl(type, entry, (KeyType*)NULL);
    }
    
    bool
    Delete (ConstString type)
    {
        return Delete_Impl(type, (KeyType*)NULL);
    }
        
    bool
    Get(ValueObject& valobj,
        MapValueType& entry,
        lldb::DynamicValueType use_dynamic,
        uint32_t* why = NULL)
    {
        uint32_t value = lldb_private::eFormatterChoiceCriterionDirectChoice;
        ClangASTType ast_type(valobj.GetClangType());
        bool ret = Get(valobj, ast_type, entry, use_dynamic, value);
        if (ret)
            entry = MapValueType(entry);
        else
            entry = MapValueType();        
        if (why)
            *why = value;
        return ret;
    }
    
    bool
    Get (ConstString type, MapValueType& entry)
    {
        return Get_Impl(type, entry, (KeyType*)NULL);
    }
    
    bool
    GetExact (ConstString type, MapValueType& entry)
    {
        return GetExact_Impl(type, entry, (KeyType*)NULL);
    }
    
    MapValueType
    GetAtIndex (size_t index)
    {
        return m_format_map.GetValueAtIndex(index);
    }
    
    lldb::TypeNameSpecifierImplSP
    GetTypeNameSpecifierAtIndex (size_t index)
    {
        return GetTypeNameSpecifierAtIndex_Impl(index, (KeyType*)NULL);
    }
    
    void
    Clear ()
    {
        m_format_map.Clear();
    }
    
    void
    LoopThrough (CallbackType callback, void* param)
    {
        m_format_map.LoopThrough(callback,param);
    }
    
    uint32_t
    GetCount ()
    {
        return m_format_map.GetCount();
    }
    
protected:
        
    BackEndType m_format_map;
    
    std::string m_name;
    
    DISALLOW_COPY_AND_ASSIGN(FormatNavigator);
    
    ConstString m_id_cs;
                           
    void
    Add_Impl (const MapKeyType &type, const MapValueType& entry, lldb::RegularExpressionSP *dummy)
    {
       m_format_map.Add(type,entry);
    }

    void Add_Impl (const ConstString &type, const MapValueType& entry, ConstString *dummy)
    {
       m_format_map.Add(GetValidTypeName_Impl(type), entry);
    }

    bool
    Delete_Impl (ConstString type, ConstString *dummy)
    {
       return m_format_map.Delete(type);
    }

    bool
    Delete_Impl (ConstString type, lldb::RegularExpressionSP *dummy)
    {
       Mutex& x_mutex = m_format_map.mutex();
        lldb_private::Mutex::Locker locker(x_mutex);
       MapIterator pos, end = m_format_map.map().end();
       for (pos = m_format_map.map().begin(); pos != end; pos++)
       {
           lldb::RegularExpressionSP regex = pos->first;
           if ( ::strcmp(type.AsCString(),regex->GetText()) == 0)
           {
               m_format_map.map().erase(pos);
               if (m_format_map.listener)
                   m_format_map.listener->Changed();
               return true;
           }
       }
       return false;
    }    

    bool
    Get_Impl (ConstString type, MapValueType& entry, ConstString *dummy)
    {
       return m_format_map.Get(type, entry);
    }

    bool
    GetExact_Impl (ConstString type, MapValueType& entry, ConstString *dummy)
    {
        return Get_Impl(type,entry, (KeyType*)0);
    }
    
    lldb::TypeNameSpecifierImplSP
    GetTypeNameSpecifierAtIndex_Impl (size_t index, ConstString *dummy)
    {
        ConstString key = m_format_map.GetKeyAtIndex(index);
        if (key)
            return lldb::TypeNameSpecifierImplSP(new TypeNameSpecifierImpl(key.AsCString(),
                                                                           false));
        else
            return lldb::TypeNameSpecifierImplSP();
    }
    
    lldb::TypeNameSpecifierImplSP
    GetTypeNameSpecifierAtIndex_Impl (size_t index, lldb::RegularExpressionSP *dummy)
    {
        lldb::RegularExpressionSP regex = m_format_map.GetKeyAtIndex(index);
        if (regex.get() == NULL)
            return lldb::TypeNameSpecifierImplSP();
        return lldb::TypeNameSpecifierImplSP(new TypeNameSpecifierImpl(regex->GetText(),
                                                                       true));
    }

    bool
    Get_Impl (ConstString key, MapValueType& value, lldb::RegularExpressionSP *dummy)
    {
       const char* key_cstr = key.AsCString();
       if (!key_cstr)
           return false;
       Mutex& x_mutex = m_format_map.mutex();
       lldb_private::Mutex::Locker locker(x_mutex);
       MapIterator pos, end = m_format_map.map().end();
       for (pos = m_format_map.map().begin(); pos != end; pos++)
       {
           lldb::RegularExpressionSP regex = pos->first;
           if (regex->Execute(key_cstr))
           {
               value = pos->second;
               return true;
           }
       }
       return false;
    }
    
    bool
    GetExact_Impl (ConstString key, MapValueType& value, lldb::RegularExpressionSP *dummy)
    {
        Mutex& x_mutex = m_format_map.mutex();
        lldb_private::Mutex::Locker locker(x_mutex);
        MapIterator pos, end = m_format_map.map().end();
        for (pos = m_format_map.map().begin(); pos != end; pos++)
        {
            lldb::RegularExpressionSP regex = pos->first;
            if (strcmp(regex->GetText(),key.AsCString()) == 0)
            {
                value = pos->second;
                return true;
            }
        }
        return false;
    }
    
    bool
    Get_BitfieldMatch (ValueObject& valobj,
                       ConstString typeName,
                       MapValueType& entry,
                       uint32_t& reason)
    {
        Log *log(lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_TYPES));
        // for bitfields, append size to the typename so one can custom format them
        StreamString sstring;
        sstring.Printf("%s:%d",typeName.AsCString(),valobj.GetBitfieldBitSize());
        ConstString bitfieldname = ConstString(sstring.GetData());
        if (log)
            log->Printf("[Get_BitfieldMatch] appended bitfield info, final result is %s", bitfieldname.GetCString());
        if (Get(bitfieldname, entry))
        {
            if (log)
                log->Printf("[Get_BitfieldMatch] bitfield direct match found, returning");
            return true;
        }
        else
        {
            reason |= lldb_private::eFormatterChoiceCriterionStrippedBitField;
            if (log)
                log->Printf("[Get_BitfieldMatch] no bitfield direct match");
            return false;
        }
    }
    
    bool Get_ObjC (ValueObject& valobj,
                   MapValueType& entry)
    {
        Log *log(lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_TYPES));
        lldb::ProcessSP process_sp = valobj.GetProcessSP();
        ObjCLanguageRuntime* runtime = process_sp->GetObjCLanguageRuntime();
        if (runtime == NULL)
        {
            if (log)
                log->Printf("[Get_ObjC] no valid ObjC runtime, skipping dynamic");
            return false;
        }
        ObjCLanguageRuntime::ClassDescriptorSP objc_class_sp (runtime->GetClassDescriptor(valobj));
        if (!objc_class_sp)
        {
            if (log)
                log->Printf("[Get_ObjC] invalid ISA, skipping dynamic");
            return false;
        }
        ConstString name (objc_class_sp->GetClassName());
        if (log)
            log->Printf("[Get_ObjC] dynamic type inferred is %s - looking for direct dynamic match", name.GetCString());
        if (Get(name, entry))
        {
            if (log)
                log->Printf("[Get_ObjC] direct dynamic match found, returning");
            return true;
        }
        if (log)
            log->Printf("[Get_ObjC] no dynamic match");
        return false;
    }
    
    bool
    Get_Impl (ValueObject& valobj,
              ClangASTType clang_type,
              MapValueType& entry,
              lldb::DynamicValueType use_dynamic,
              uint32_t& reason)
    {
        Log *log(lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_TYPES));

        if (!clang_type.IsValid())
        {
            if (log)
                log->Printf("[Get_Impl] type is invalid, returning");
            return false;
        }
        
        clang_type = clang_type.RemoveFastQualifiers();

        ConstString typeName(clang_type.GetConstTypeName());
        
        if (valobj.GetBitfieldBitSize() > 0)
        {
            if (Get_BitfieldMatch(valobj, typeName, entry, reason))
                return true;
        }
        
        if (log)
            log->Printf("[Get_Impl] trying to get %s for VO name %s of type %s",
                        m_name.c_str(),
                        valobj.GetName().AsCString(),
                        typeName.AsCString());
        
        if (Get(typeName, entry))
        {
            if (log)
                log->Printf("[Get] direct match found, returning");
            return true;
        }
        if (log)
            log->Printf("[Get_Impl] no direct match");
        
        // strip pointers and references and see if that helps
        if (clang_type.IsReferenceType())
        {
            if (log)
                log->Printf("[Get_Impl] stripping reference");
            if (Get_Impl(valobj, clang_type.GetNonReferenceType(), entry, use_dynamic, reason) && !entry->SkipsReferences())
            {
                reason |= lldb_private::eFormatterChoiceCriterionStrippedPointerReference;
                return true;
            }
        }
        else if (clang_type.IsPointerType())
        {
            if (log)
                log->Printf("[Get_Impl] stripping pointer");
            if (Get_Impl(valobj, clang_type.GetPointeeType(), entry, use_dynamic, reason) && !entry->SkipsPointers())
            {
                reason |= lldb_private::eFormatterChoiceCriterionStrippedPointerReference;
                return true;
            }
        }
        
        bool canBeObjCDynamic = valobj.GetClangType().IsPossibleDynamicType (NULL,
                                                                             false, // no C++
                                                                             true); // yes ObjC
        
        if (canBeObjCDynamic)
        {
            if (use_dynamic != lldb::eNoDynamicValues)
            {
                if (log)
                    log->Printf("[Get_Impl] allowed to figure out dynamic ObjC type");
                if (Get_ObjC(valobj,entry))
                {
                    reason |= lldb_private::eFormatterChoiceCriterionDynamicObjCDiscovery;
                    return true;
                }
            }
            if (log)
                log->Printf("[Get_Impl] dynamic disabled or failed - stripping ObjC pointer");
            if (Get_Impl(valobj, clang_type.GetPointeeType(), entry, use_dynamic, reason) && !entry->SkipsPointers())
            {
                reason |= lldb_private::eFormatterChoiceCriterionStrippedPointerReference;
                return true;
            }
        }
        
        // try to strip typedef chains
        if (clang_type.IsTypedefType())
        {
            if (log)
                log->Printf("[Get_Impl] stripping typedef");
            if ((Get_Impl(valobj, clang_type.GetTypedefedType(), entry, use_dynamic, reason)) && entry->Cascades())
            {
                reason |= lldb_private::eFormatterChoiceCriterionNavigatedTypedefs;
                return true;
            }
        }
        
        // out of luck here
        return false;
    }
    
    // we are separately passing in valobj and type because the valobj is fixed (and is used for ObjC discovery and bitfield size)
    // but the type can change (e.g. stripping pointers, ...)
    bool Get (ValueObject& valobj,
              ClangASTType clang_type,
              MapValueType& entry,
              lldb::DynamicValueType use_dynamic,
              uint32_t& reason)
    {
        Log *log(lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_TYPES));
        
        if (Get_Impl (valobj, clang_type, entry, use_dynamic, reason))
            return true;
        
        // try going to the unqualified type
        do {
            if (log)
                log->Printf("[Get] trying the unqualified type");
            if (!clang_type.IsValid())
                break;

            ClangASTType unqual_clang_ast_type = clang_type.GetFullyUnqualifiedType();
            if (!unqual_clang_ast_type.IsValid())
            {
                if (log)
                    log->Printf("[Get] could not get the unqual_clang_ast_type");
                break;
            }
            if (unqual_clang_ast_type.GetOpaqueQualType() != clang_type.GetOpaqueQualType())
            {
                if (log)
                    log->Printf("[Get] unqualified type is there and is not the same, let's try");
                if (Get_Impl (valobj, unqual_clang_ast_type,entry, use_dynamic, reason))
                    return true;
            }
            else if (log)
                log->Printf("[Get] unqualified type same as original type");
        } while(false);
        
        // if all else fails, go to static type
        if (valobj.IsDynamic())
        {
            if (log)
                log->Printf("[Get] going to static value");
            lldb::ValueObjectSP static_value_sp(valobj.GetStaticValue());
            if (static_value_sp)
            {
                if (log)
                    log->Printf("[Get] has a static value - actually use it");
                if (Get(*static_value_sp.get(), static_value_sp->GetClangType(), entry, use_dynamic, reason))
                {
                    reason |= lldb_private::eFormatterChoiceCriterionWentToStaticValue;
                    return true;
                }
            }
        }
        
        return false;
    }
};

} // namespace lldb_private

#endif	// lldb_FormatNavigator_h_
