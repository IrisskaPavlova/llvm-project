// RUN: %clang -fsyntax-only -Xclang -load -Xclang /home/irisska/llvm-project/build/lib/clang/23/lib/InstrumentFunctionsPlugin.so -Xclang -add-plugin -Xclang instrument-functions -Xclang -plugin-arg-instrument-functions -Xclang option2 %s 2>&1 | FileCheck %s

int foo(int a, int b, int c) {
    double value = 0.0;
    return a + b;
}

// CHECK: === Lab1 Plugin - Option 2 ===
// CHECK: Unused: c in function foo
// CHECK: ====================
