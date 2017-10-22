//===- ObjectFile.cpp - File format independent object file -----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines a file format independent ObjectFile class.
//
//===----------------------------------------------------------------------===//

#include "llvm/Object/ObjectFile.h"
#include "llvm/ADT/OwningPtr.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/system_error.h"

using namespace llvm;
using namespace object;

void ObjectFile::anchor() { }

ObjectFile::ObjectFile(unsigned int Type, MemoryBuffer *source)
  : Binary(Type, source) {
}

error_code ObjectFile::getSymbolAlignment(DataRefImpl DRI,
                                          uint32_t &Result) const {
  Result = 0;
  return object_error::success;
}

ObjectFile *ObjectFile::createObjectFile(MemoryBuffer *Object) {
  if (!Object || Object->getBufferSize() < 64)
    return 0;
  sys::LLVMFileType type = sys::IdentifyFileType(Object->getBufferStart(),
                                static_cast<unsigned>(Object->getBufferSize()));
  switch (type) {
    case sys::Unknown_FileType:
      return 0;
    case sys::ELF_Relocatable_FileType:
    case sys::ELF_Executable_FileType:
    case sys::ELF_SharedObject_FileType:
    case sys::ELF_Core_FileType:
      return createELFObjectFile(Object);
    case sys::Mach_O_Object_FileType:
    case sys::Mach_O_Executable_FileType:
    case sys::Mach_O_FixedVirtualMemorySharedLib_FileType:
    case sys::Mach_O_Core_FileType:
    case sys::Mach_O_PreloadExecutable_FileType:
    case sys::Mach_O_DynamicallyLinkedSharedLib_FileType:
    case sys::Mach_O_DynamicLinker_FileType:
    case sys::Mach_O_Bundle_FileType:
    case sys::Mach_O_DynamicallyLinkedSharedLibStub_FileType:
    case sys::Mach_O_DSYMCompanion_FileType:
      return createMachOObjectFile(Object);
    case sys::COFF_FileType:
      return createCOFFObjectFile(Object);
    default:
      llvm_unreachable("Unexpected Object File Type");
  }
}

ObjectFile *ObjectFile::createObjectFile(StringRef ObjectPath) {
  OwningPtr<MemoryBuffer> File;
  if (MemoryBuffer::getFile(ObjectPath, File))
    return NULL;
  return createObjectFile(File.take());
}
