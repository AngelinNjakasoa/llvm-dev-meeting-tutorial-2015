// RUN: clang %s -O2 -S -emit-llvm -o %t.ll
// RUN: opt -load %bindir/XorZero/LLVMXorZero${MOD_EXT} -XorZero %t.ll -S -o %t0.ll
// RUN: FileCheck %s < %t0.ll
// RUN: clang %t0.ll -o %t0
// RUN: %t0

#include <stdio.h>

int foo(int x){
  return 42;
}

int main() {

  //CHECK: xor
  //CHECK-NEXT: store
  //CHECK-NEXT: load
  foo(2 - 2);
  return 0;
}

