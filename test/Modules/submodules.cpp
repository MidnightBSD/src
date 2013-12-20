// RUN: rm -rf %t
// RUN: %clang_cc1 -x objective-c++ -fmodules-cache-path=%t -fmodules -I %S/Inputs/submodules %s -verify
// FIXME: When we have a syntax for modules in C++, use that.

@import std.vector;

vector<int> vi;

// Note: remove_reference is not visible yet.
remove_reference<int&>::type *int_ptr = 0; // expected-error{{unknown type name 'remove_reference'}} \
// expected-error{{expected unqualified-id}}

@import std.typetraits; // expected-error{{no submodule named 'typetraits' in module 'std'; did you mean 'type_traits'?}}

vector<float> vf;
remove_reference<int&>::type *int_ptr2 = 0;

@import std.vector.compare; // expected-error{{no submodule named 'compare' in module 'std.vector'}}

@import std; // import everything in 'std'

// hash_map still isn't available.
hash_map<int, float> ints_to_floats; // expected-error{{unknown type name 'hash_map'}} \
// expected-error{{expected unqualified-id}}

@import std.hash_map;

hash_map<int, float> ints_to_floats2;

