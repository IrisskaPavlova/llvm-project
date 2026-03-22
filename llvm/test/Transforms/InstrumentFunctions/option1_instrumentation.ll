; RUN: opt -load-pass-plugin=%pass -passes=instrument-functions -S %s 2>&1 | FileCheck %s

define void @test_function() {
entry:
  ret void
}

define i32 @main() {
entry:
  call void @test_function()
  ret i32 0
}

; CHECK: define void @test_function() {
; CHECK: entry:
; CHECK:   call void @instrument_start()
; CHECK:   call void @instrument_end()
; CHECK:   ret void
; CHECK: }

; CHECK: define i32 @main() {
; CHECK: entry:
; CHECK:   call void @instrument_start()
; CHECK:   call void @test_function()
; CHECK:   call void @instrument_end()
; CHECK:   ret i32 0
; CHECK: }

; CHECK: declare void @instrument_start()
; CHECK: declare void @instrument_end()
