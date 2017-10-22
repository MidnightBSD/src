//===-- AttributesImpl.h - Attributes Internals -----------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines various helper methods and classes used by LLVMContextImpl
// for creating and managing attributes.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_ATTRIBUTESIMPL_H
#define LLVM_ATTRIBUTESIMPL_H

#include "llvm/Attributes.h"
#include "llvm/ADT/FoldingSet.h"

namespace llvm {

class AttributesImpl : public FoldingSetNode {
  uint64_t Bits;                // FIXME: We will be expanding this.
public:
  AttributesImpl(uint64_t bits) : Bits(bits) {}

  bool hasAttribute(uint64_t A) const;

  bool hasAttributes() const;
  bool hasAttributes(const Attributes &A) const;

  uint64_t getAlignment() const;
  uint64_t getStackAlignment() const;

  uint64_t Raw() const { return Bits; } // FIXME: Remove.

  static uint64_t getAttrMask(uint64_t Val);

  void Profile(FoldingSetNodeID &ID) const {
    Profile(ID, Bits);
  }
  static void Profile(FoldingSetNodeID &ID, uint64_t Bits) {
    ID.AddInteger(Bits);
  }
};

class AttributeListImpl : public FoldingSetNode {
  // AttributesList is uniqued, these should not be publicly available.
  void operator=(const AttributeListImpl &) LLVM_DELETED_FUNCTION;
  AttributeListImpl(const AttributeListImpl &) LLVM_DELETED_FUNCTION;
public:
  SmallVector<AttributeWithIndex, 4> Attrs;

  AttributeListImpl(ArrayRef<AttributeWithIndex> attrs)
    : Attrs(attrs.begin(), attrs.end()) {}

  void Profile(FoldingSetNodeID &ID) const {
    Profile(ID, Attrs);
  }
  static void Profile(FoldingSetNodeID &ID, ArrayRef<AttributeWithIndex> Attrs){
    for (unsigned i = 0, e = Attrs.size(); i != e; ++i) {
      ID.AddInteger(Attrs[i].Attrs.Raw());
      ID.AddInteger(Attrs[i].Index);
    }
  }
};

} // end llvm namespace

#endif
