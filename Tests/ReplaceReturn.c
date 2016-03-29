// RUN: clang %s -O2 -S -emit-llvm -o %t.ll
// RUN: opt -load %bindir/XorZero/LLVMXorZero${MOD_EXT} -XorZero %t.ll -S -o %t0.ll
// RUN: FileCheck %s < %t0.ll
// RUN: clang %t0.ll -o %t0
// RUN: %t0

#include <stdio.h>

int foo(){
  // CHECK: alloca
  // CHECK-NEXT: load
  // CHECK-NEXT: xor
  // CHECK-NEXT: store
  // CHECK-NEXT: load
  // CHECK-NEXT: ret
  return 0;
}

int main() {
  return 0;
}
