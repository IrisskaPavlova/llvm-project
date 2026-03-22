// RUN: %clang -fsyntax-only -Xclang -load -Xclang /home/irisska/llvm-project/build/lib/clang/23/lib/InstrumentFunctionsPlugin.so -Xclang -add-plugin -Xclang instrument-functions -Xclang -plugin-arg-instrument-functions -Xclang option1 %s 2>&1 | FileCheck %s

struct Human {
    unsigned age;
    unsigned height;
    virtual void sleep() = 0;
    virtual void eat() = 0;
};

struct Engineer : Human {
    unsigned salary;
    void sleep() override { }
    void eat() override { }
    void work() { }
};

// CHECK: === Lab1 Plugin - Option 1 ===
// CHECK: Human
// CHECK:   Fields:
// CHECK:     - age : unsigned int (public)
// CHECK:     - height : unsigned int (public)
// CHECK:   Methods:
// CHECK:     - sleep() virtual pure
// CHECK:     - eat() virtual pure
// CHECK: Engineer
// CHECK:   inherits from Human
// CHECK:   Fields:
// CHECK:     - salary : unsigned int (public)
// CHECK:   Methods:
// CHECK:     - sleep() virtual
// CHECK:     - eat() virtual
// CHECK:     - work()
// CHECK: ====================
