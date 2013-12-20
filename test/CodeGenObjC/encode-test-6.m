// RUN: %clang_cc1 -triple x86_64-apple-darwin -emit-llvm -o %t %s
// RUN: FileCheck < %t %s
// rdar://11777609

typedef struct {} Z;

@interface A
-(void)bar:(Z)a;
-(void)foo:(Z)a : (char*)b : (Z)c : (double) d;
@end

@implementation A
-(void)bar:(Z)a {}
-(void)foo:(Z)a: (char*)b : (Z)c : (double) d {}
@end

// CHECK: internal global [14 x i8] c"v16@0:8{?=}16
// CHECK: internal global [26 x i8] c"v32@0:8{?=}16*16{?=}24d24


// rdar://13190095
@interface NSObject @end

@class BABugExample;
typedef BABugExample BABugExampleRedefinition;

@interface BABugExample : NSObject {
    BABugExampleRedefinition *_property; // .asciz   "^{BABugExample=^{BABugExample}}"
}
@property (copy) BABugExampleRedefinition *property;
@end

@implementation BABugExample
@synthesize property = _property;
@end

// CHECK: internal global [24 x i8] c"^{BABugExample=@}16
