; ModuleID = 'test.bc'
target datalayout = "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64-s0:64:64-f80:128:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@_ZZ4mainE3arr = private unnamed_addr constant [5 x i32] [i32 1, i32 2, i32 3, i32 4, i32 5], align 16

define i32 @main(i32 %argc, i8** nocapture %argv) nounwind uwtable readnone {
entry:
  %idxprom6 = sext i32 %argc to i64
  %arrayidx7 = getelementptr inbounds [5 x i32]* @_ZZ4mainE3arr, i64 0, i64 %idxprom6
  %0 = load i32* %arrayidx7, align 4, !tbaa !0
  %add8 = add nsw i32 %argc, 1
  %add9 = add nsw i32 %argc, 2
  %idxprom10 = sext i32 %add9 to i64
  %arrayidx11 = getelementptr inbounds [5 x i32]* @_ZZ4mainE3arr, i64 0, i64 %idxprom10
  %1 = load i32* %arrayidx11, align 4, !tbaa !0
  %idxprom13 = sext i32 %add8 to i64
  %arrayidx14 = getelementptr inbounds [5 x i32]* @_ZZ4mainE3arr, i64 0, i64 %idxprom13
  %2 = load i32* %arrayidx14, align 4, !tbaa !0
  %add16 = add i32 %argc, 7
  %add17 = add i32 %add16, %0
  %add18 = add i32 %add17, %1
  %add19 = add i32 %add18, %2
  ret i32 %add19
}

!0 = metadata !{metadata !"int", metadata !1}
!1 = metadata !{metadata !"omnipotent char", metadata !2}
!2 = metadata !{metadata !"Simple C/C++ TBAA"}
