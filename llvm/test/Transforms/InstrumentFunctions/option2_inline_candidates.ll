; RUN: opt -load-pass-plugin=%pass -passes=inline-candidates -disable-output %s 2>&1 | FileCheck %s

define void @test_function() {
entry:
  ret void
}

define i32 @main() {
entry:
  call void @test_function()
  ret i32 0
}

; CHECK: [Option 2] Inline candidate: test_function in main
