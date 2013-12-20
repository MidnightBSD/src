// RUN: rm -rf %t
// RUN: %clang_cc1 -x objective-c -fmodules-cache-path=%t -fmodules -I %S/Inputs/normal-module-map %s -verify
#include "Umbrella/umbrella_sub.h"

int getUmbrella() { 
  return umbrella + umbrella_sub; 
}

@import Umbrella2;

#include "a1.h"
#include "b1.h"
#include "nested/nested2.h"

int test() {
  return a1 + b1 + nested2;
}

@import nested_umbrella.a;

int testNestedUmbrellaA() {
  return nested_umbrella_a;
}

int testNestedUmbrellaBFail() {
  return nested_umbrella_b;
  // expected-error@-1{{use of undeclared identifier 'nested_umbrella_b'; did you mean 'nested_umbrella_a'?}}
  // expected-note@Inputs/normal-module-map/nested_umbrella/a.h:1{{'nested_umbrella_a' declared here}}
}

@import nested_umbrella.b;

int testNestedUmbrellaB() {
  return nested_umbrella_b;
}

@import nested_umbrella.a_extras;

@import nested_umbrella._1;

@import nested_umbrella.decltype_;

int testSanitizedName() {
  return extra_a + one + decltype_val;
}
