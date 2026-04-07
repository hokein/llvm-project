// RUN: rm -rf %t
// RUN: split-file %s %t
// RUN: %clang_cc1 -emit-module -fmodules -fmodule-name=A -x c++ %t/Inputs/module.modulemap -o %t/A.pcm
// RUN: %clang_cc1 -emit-module -fmodules -fmodule-name=B -x c++ %t/Inputs/module.modulemap -o %t/B.pcm
// RUN: %clang_cc1 -fmodules -fimplicit-module-maps -I %t/Inputs -fmodule-file=A=%t/A.pcm -fmodule-file=B=%t/B.pcm %t/test.cpp -print-stats 2>&1 | FileCheck %s

//--- Inputs/module.modulemap
module A {
  header "a.h"
}
module B {
  header "b.h"
}

//--- Inputs/huge.h
#define HUGE_VALUE 42
// Add some padding to make size > 0
char huge_array[1024];

//--- Inputs/a.h
#include "huge.h"
void foo();

//--- Inputs/b.h
#include "huge.h"
void bar();

//--- test.cpp
#include "a.h"
#include "b.h"
void baz() {
  foo();
  bar();
}

// CHECK: *** Source Manager Stats:
// CHECK: loaded SLocEntries allocated
// CHECK: B of SLoc address space used.
// CHECK: {{[1-9][0-9]*}}B of SLoc address space actively saved.
