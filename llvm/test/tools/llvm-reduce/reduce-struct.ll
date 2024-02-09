; RUN: llvm-reduce --abort-on-invalid-reduction --delta-passes=struct --test FileCheck --test-arg --check-prefixes=INTERESTING,CHECK --test-arg %s --test-arg --input-file %s -o %t
; RUN: FileCheck -check-prefixes=RESULT,CHECK %s < %t

; CHECK-INTERESTINGNESS: getelementptr


; ModuleID = 'reduced.bc'
target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-macosx14.0.0"

%struct.state_t = type { i32, [64 x i32], i64, i64, i64, [13 x i64], i32, i32, [13 x i32], i32, i32, i32, i32, i32, i32, i32, i64, i64, [64 x %struct.move_x], [64 x i32], [64 x i32], [64 x %struct.anon], i64, i64, i32, [64 x i32], i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, [1000 x i64] }
%struct.move_x = type { i32, i32, i32, i32, i64, i64 }
%struct.anon = type { i32, i32, i32, i32 }

; Function Attrs: strictfp
define i32 @_Z7is_drawP11gamestate_tP7state_t(ptr nocapture %s, i64 %0) #0 {
entry:
  %arrayidx38 = getelementptr %struct.state_t, ptr %s, i64 0, i32 36, i64 %0
  %1 = load i64, ptr %arrayidx38, align 8
  %cmp39 = icmp eq i64 %1, 0
  %. = zext i1 %cmp39 to i32
  ret i32 %.
}

attributes #0 = { strictfp }
