using MyTypeAlias = int;

extern "C" {
  template < typename T > *Allocate() { }
}

// RUN: c-index-test -index-file %s > %t
// RUN: FileCheck %s -input-file=%t

// CHECK: [indexDeclaration]: kind: type-alias | name: MyTypeAlias | {{.*}} | loc: 1:7
