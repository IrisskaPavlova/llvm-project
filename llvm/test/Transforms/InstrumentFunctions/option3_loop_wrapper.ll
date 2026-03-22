; RUN: opt -load-pass-plugin=%pass -passes=loop-wrapper -S %s 2>&1 | FileCheck %s

define void @test_loops() {
entry:
  br label %loop

loop:
  %i = phi i32 [ 0, %entry ], [ %next, %loop ]
  %next = add i32 %i, 1
  %cond = icmp slt i32 %next, 10
  br i1 %cond, label %loop, label %exit

exit:
  ret void
}

; CHECK: define void @test_loops()
; CHECK: call void @loop_start()
; CHECK: call void @loop_end()
; CHECK: declare void @loop_start()
; CHECK: declare void @loop_end()
