// Clear out the module cache entirely, so we start from nothing.
// RUN: rm -rf %t

// Parse the file, such that building the module will cause Clang to crash.
// RUN: not env CINDEXTEST_FAILONERROR=1 c-index-test -test-load-source all -fmodules -fmodules-cache-path=%t -Xclang -fdisable-module-hash -I %S/Inputs/Headers -DCRASH %s 2> %t.err
// RUN: FileCheck < %t.err -check-prefix=CHECK-CRASH %s
// CHECK-CRASH: crash-recovery-modules.m:16:9:{16:2-16:14}: fatal error: could not build module 'Crash'

// Parse the file again, without crashing, to make sure that
// subsequent parses do the right thing.
// RUN: env CINDEXTEST_FAILONERROR=1 c-index-test -test-load-source all -fmodules -fmodules-cache-path=%t -Xclang -fdisable-module-hash -I %S/Inputs/Headers %s

// REQUIRES: crash-recovery
// REQUIRES: shell

@import Crash;

void test() {
  const char* error = getCrashString();
}
