// Check handling -mhard-float / -msoft-float / -mfloat-abi options
// when build for MIPS platforms.
//
// Default
// RUN: %clang -c %s -### -o %t.o 2>&1 \
// RUN:     -target mips-linux-gnu \
// RUN:   | FileCheck --check-prefix=CHECK-DEF %s
// CHECK-DEF: "-mfloat-abi" "hard"
//
// -mhard-float
// RUN: %clang -c %s -### -o %t.o 2>&1 \
// RUN:     -target mips-linux-gnu -mhard-float \
// RUN:   | FileCheck --check-prefix=CHECK-HARD %s
// CHECK-HARD: "-mfloat-abi" "hard"
//
// -msoft-float
// RUN: %clang -c %s -### -o %t.o 2>&1 \
// RUN:     -target mips-linux-gnu -msoft-float \
// RUN:   | FileCheck --check-prefix=CHECK-SOFT %s
// CHECK-SOFT: "-msoft-float"
// CHECK-SOFT: "-mfloat-abi" "soft"
// CHECK-SOFT: "-target-feature" "+soft-float"
//
// -mfloat-abi=hard
// RUN: %clang -c %s -### -o %t.o 2>&1 \
// RUN:     -target mips-linux-gnu -mfloat-abi=hard \
// RUN:   | FileCheck --check-prefix=CHECK-ABI-HARD %s
// CHECK-ABI-HARD: "-mfloat-abi" "hard"
//
// -mfloat-abi=soft
// RUN: %clang -c %s -### -o %t.o 2>&1 \
// RUN:     -target mips-linux-gnu -mfloat-abi=soft \
// RUN:   | FileCheck --check-prefix=CHECK-ABI-SOFT %s
// CHECK-ABI-SOFT: "-msoft-float"
// CHECK-ABI-SOFT: "-mfloat-abi" "soft"
// CHECK-ABI-SOFT: "-target-feature" "+soft-float"
//
// -mdouble-float
// RUN: %clang -c %s -### -o %t.o 2>&1 \
// RUN:     -target mips-linux-gnu -msingle-float -mdouble-float \
// RUN:   | FileCheck --check-prefix=CHECK-ABI-DOUBLE %s
// CHECK-ABI-DOUBLE: "-mfloat-abi" "hard"
// CHECK-ABI-DOUBLE-NOT: "+single-float"
//
// -msingle-float
// RUN: %clang -c %s -### -o %t.o 2>&1 \
// RUN:     -target mips-linux-gnu -mdouble-float -msingle-float \
// RUN:   | FileCheck --check-prefix=CHECK-ABI-SINGLE %s
// CHECK-ABI-SINGLE: "-mfloat-abi" "hard"
// CHECK-ABI-SINGLE: "-target-feature" "+single-float"
//
// -msoft-float -msingle-float
// RUN: %clang -c %s -### -o %t.o 2>&1 \
// RUN:     -target mips-linux-gnu -msoft-float -msingle-float \
// RUN:   | FileCheck --check-prefix=CHECK-ABI-SOFT-SINGLE %s
// CHECK-ABI-SOFT-SINGLE: "-mfloat-abi" "soft"
// CHECK-ABI-SOFT-SINGLE: "-target-feature" "+single-float"
//
// Default -mips16
// RUN: %clang -c %s -### -o %t.o 2>&1 \
// RUN:     -target mips-linux-gnu -mips16 \
// RUN:   | FileCheck --check-prefix=CHECK-DEF-MIPS16 %s
// CHECK-DEF-MIPS16: "-mfloat-abi" "soft"
// CHECK-DEF-MIPS16: "-mllvm" "-mips16-hard-float"
//
// -mhard-float -mips16
// RUN: %clang -c %s -### -o %t.o 2>&1 \
// RUN:     -target mips-linux-gnu -mhard-float -mips16 \
// RUN:   | FileCheck --check-prefix=CHECK-HARD-MIPS16 %s
// CHECK-HARD-MIPS16: "-msoft-float"
// CHECK-HARD-MIPS16: "-mfloat-abi" "soft"
// CHECK-HARD-MIPS16: "-target-feature" "+soft-float"
// CHECK-HARD-MIPS16: "-mllvm" "-mips16-hard-float"
//
// -msoft-float -mips16
// RUN: %clang -c %s -### -o %t.o 2>&1 \
// RUN:     -target mips-linux-gnu -msoft-float -mips16 \
// RUN:   | FileCheck --check-prefix=CHECK-SOFT-MIPS16 %s
// CHECK-SOFT-MIPS16: "-msoft-float"
// CHECK-SOFT-MIPS16: "-mfloat-abi" "soft"
// CHECK-SOFT-MIPS16: "-target-feature" "+soft-float"
//
// -mfloat-abi=hard -mips16
// RUN: %clang -c %s -### -o %t.o 2>&1 \
// RUN:     -target mips-linux-gnu -mfloat-abi=hard -mips16 \
// RUN:   | FileCheck --check-prefix=CHECK-ABI-HARD-MIPS16 %s
// CHECK-ABI-HARD-MIPS16: "-msoft-float"
// CHECK-ABI-HARD-MIPS16: "-mfloat-abi" "soft"
// CHECK-ABI-HARD-MIPS16: "-target-feature" "+soft-float"
// CHECK-ABI-HARD-MIPS16: "-mllvm" "-mips16-hard-float"
//
// -mfloat-abi=soft -mips16
// RUN: %clang -c %s -### -o %t.o 2>&1 \
// RUN:     -target mips-linux-gnu -mfloat-abi=soft -mips16 \
// RUN:   | FileCheck --check-prefix=CHECK-ABI-SOFT-MIPS16 %s
// CHECK-ABI-SOFT-MIPS16: "-msoft-float"
// CHECK-ABI-SOFT-MIPS16: "-mfloat-abi" "soft"
// CHECK-ABI-SOFT-MIPS16: "-target-feature" "+soft-float"
