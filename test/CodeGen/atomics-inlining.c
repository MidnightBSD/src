// RUN: %clang_cc1 -triple powerpc-linux-gnu -emit-llvm %s -o - | FileCheck %s -check-prefix=PPC32
// RUN: %clang_cc1 -triple powerpc64-linux-gnu -emit-llvm %s -o - | FileCheck %s -check-prefix=PPC64
// RUN: %clang_cc1 -triple mipsel-linux-gnu -emit-llvm %s -o - | FileCheck %s -check-prefix=MIPS32
// RUN: %clang_cc1 -triple mips64el-linux-gnu -emit-llvm %s -o - | FileCheck %s -check-prefix=MIPS64

unsigned char c1, c2;
unsigned short s1, s2;
unsigned int i1, i2;
unsigned long long ll1, ll2;

enum memory_order {
  memory_order_relaxed,
  memory_order_consume,
  memory_order_acquire,
  memory_order_release,
  memory_order_acq_rel,
  memory_order_seq_cst
};

void test1(void) {
  (void)__atomic_load(&c1, &c2, memory_order_seq_cst);
  (void)__atomic_load(&s1, &s2, memory_order_seq_cst);
  (void)__atomic_load(&i1, &i2, memory_order_seq_cst);
  (void)__atomic_load(&ll1, &ll2, memory_order_seq_cst);

// PPC32: define void @test1
// PPC32: load atomic i8* @c1 seq_cst
// PPC32: load atomic i16* @s1 seq_cst
// PPC32: load atomic i32* @i1 seq_cst
// PPC32: call void @__atomic_load(i32 8, i8* bitcast (i64* @ll1 to i8*)

// PPC64: define void @test1
// PPC64: load atomic i8* @c1 seq_cst
// PPC64: load atomic i16* @s1 seq_cst
// PPC64: load atomic i32* @i1 seq_cst
// PPC64: load atomic i64* @ll1 seq_cst

// MIPS32: define void @test1
// MIPS32: load atomic i8* @c1 seq_cst
// MIPS32: load atomic i16* @s1 seq_cst
// MIPS32: load atomic i32* @i1 seq_cst
// MIPS32: call void @__atomic_load(i32 8, i8* bitcast (i64* @ll1 to i8*)

// MIPS64: define void @test1
// MIPS64: load atomic i8* @c1 seq_cst
// MIPS64: load atomic i16* @s1 seq_cst
// MIPS64: load atomic i32* @i1 seq_cst
// MIPS64: load atomic i64* @ll1 seq_cst
}
