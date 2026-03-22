// RUN: %clang -fsyntax-only -Xclang -load -Xclang /home/irisska/llvm-project/build/lib/clang/23/lib/InstrumentFunctionsPlugin.so -Xclang -add-plugin -Xclang instrument-functions -Xclang -plugin-arg-instrument-functions -Xclang option3 %s 2>&1 | FileCheck %s

double sum(int a, float b) {
    return a + b;
}

int mul(float a, float b) {
    return a + sum(a, b);
}

// CHECK: === Lab1 Plugin - Option 3 ===
// CHECK:   float -> double
// CHECK:   int -> float
// CHECK:   double -> int
// CHECK:   float -> double
// CHECK:   float -> int
// CHECK: === Implicit Casts Summary ===
// CHECK: Function 'mul': 4 casts
// CHECK: Function 'sum': 2 casts
// CHECK: ====================
