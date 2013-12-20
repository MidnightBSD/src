// RUN: %clang_cc1 -std=c++11 -fsyntax-only -verify %s

// rdar://13784901

struct S0 {
  int x;
  static const int test0 = __alignof__(x); // expected-error {{invalid application of 'alignof' to a field of a class still being defined}}
  static const int test1 = __alignof__(S0::x); // expected-error {{invalid application of 'alignof' to a field of a class still being defined}}
  auto test2() -> char(&)[__alignof__(x)]; // expected-error {{invalid application of 'alignof' to a field of a class still being defined}}
};

struct S1; // expected-note 5 {{forward declaration}}
extern S1 s1;
const int test3 = __alignof__(s1); // expected-error {{invalid application of 'alignof' to an incomplete type 'S1'}}

struct S2 {
  S2();
  S1 &s;
  int x;

  int test4 = __alignof__(x); // ok
  int test5 = __alignof__(s); // expected-error {{invalid application of 'alignof' to an incomplete type 'S1'}}
};

const int test6 = __alignof__(S2::x);
const int test7 = __alignof__(S2::s); // expected-error {{invalid application of 'alignof' to an incomplete type 'S1'}}

// Arguably, these should fail like the S1 cases do: the alignment of
// 's2.x' should depend on the alignment of both x-within-S2 and
// s2-within-S3 and thus require 'S3' to be complete.  If we start
// doing the appropriate recursive walk to do that, we should make
// sure that these cases don't explode.
struct S3 {
  S2 s2;

  static const int test8 = __alignof__(s2.x);
  static const int test9 = __alignof__(s2.s); // expected-error {{invalid application of 'alignof' to an incomplete type 'S1'}}
  auto test10() -> char(&)[__alignof__(s2.x)];
  static const int test11 = __alignof__(S3::s2.x);
  static const int test12 = __alignof__(S3::s2.s); // expected-error {{invalid application of 'alignof' to an incomplete type 'S1'}}
  auto test13() -> char(&)[__alignof__(s2.x)];
};

// Same reasoning as S3.
struct S4 {
  union {
    int x;
  };
  static const int test0 = __alignof__(x);
  static const int test1 = __alignof__(S0::x);
  auto test2() -> char(&)[__alignof__(x)];
};
