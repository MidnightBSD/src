// RUN: cp %s %t-1.cpp
// RUN: cp %s %t-2.cpp
// RUN: clang-format 2>&1 >/dev/null -offset=1 -length=0 %t-1.cpp %t-2.cpp |FileCheck %s
// CHECK: error: "-offset" and "-length" can only be used for single file.

int i ;
