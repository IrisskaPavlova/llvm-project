// RUN: %clang -fsyntax-only -Xclang -load -Xclang /home/irisska/llvm-project/build/lib/clang/23/lib/InstrumentFunctionsPlugin.so -Xclang -add-plugin -Xclang instrument-functions -Xclang -plugin-arg-instrument-functions -Xclang option4 %s 2>&1 | FileCheck %s

int var1 = 0;

int foo(int a, int b) {
    static int var2 = 0;
    int var3 = 123;
    ++var2;
    return a + b + var1 + var2 + var3;
}

// CHECK: === Lab1 Plugin - Option 4 ===
// CHECK: Renaming: var1 -> global_var1
// CHECK: Renaming param: a -> param_a
// CHECK: Renaming param: b -> param_b
// CHECK: Renaming: var2 -> static_var2
// CHECK: Renaming: var3 -> local_var3
// CHECK: ====================
