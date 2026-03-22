; RUN: opt -load-pass-plugin=%pass -passes=mul-to-shift -S %s 2>&1 | FileCheck %s

define i32 @test_mul(i32 %a) {
entry:
  %mul1 = mul i32 %a, 2
  %mul2 = mul i32 %a, 4
  %mul3 = mul i32 %a, 8
  %mul4 = mul i32 %a, 16
  %result = add i32 %mul1, %mul2
  %result2 = add i32 %result, %mul3
  %result3 = add i32 %result2, %mul4
  ret i32 %result3
}

; CHECK: define i32 @test_mul(i32 %a) {
; CHECK: entry:
; CHECK-NOT: mul i32 %a, 2
; CHECK-NOT: mul i32 %a, 4
; CHECK-NOT: mul i32 %a, 8
; CHECK-NOT: mul i32 %a, 16
; CHECK:   %shift = shl i32 %a, 1
; CHECK:   %shift1 = shl i32 %a, 2
; CHECK:   %shift2 = shl i32 %a, 3
; CHECK:   %shift3 = shl i32 %a, 4
; CHECK:   %result = add i32 %shift, %shift1
; CHECK:   %result2 = add i32 %result, %shift2
; CHECK:   %result3 = add i32 %result2, %shift3
; CHECK:   ret i32 %result3
; CHECK: }
