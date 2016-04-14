; ModuleID = 'test4_local.bc'
target datalayout = "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64-s0:64:64-f80:128:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@_ZZ4mainE3arr = private unnamed_addr constant [5 x i32] [i32 1, i32 2, i32 3, i32 4, i32 5], align 16

define i32 @main(i32 %argc, i8** nocapture %argv) nounwind uwtable readnone {
entry:
  %idxprom = sext i32 %argc to i64
  %arrayidx = getelementptr inbounds [5 x i32]* @_ZZ4mainE3arr, i64 0, i64 %idxprom
  %0 = load i32* %arrayidx, align 4, !tbaa !0
  %add = add nsw i32 %argc, 1
  %idxprom1 = sext i32 %add to i64
  %arrayidx2 = getelementptr inbounds [5 x i32]* @_ZZ4mainE3arr, i64 0, i64 %idxprom1
  %1 = load i32* %arrayidx2, align 4, !tbaa !0
  %add3 = add nsw i32 %argc, 2
  %idxprom4 = sext i32 %add3 to i64
  %arrayidx5 = getelementptr inbounds [5 x i32]* @_ZZ4mainE3arr, i64 0, i64 %idxprom4
  %2 = load i32* %arrayidx5, align 4, !tbaa !0
  %add6 = add nsw i32 %1, %0
  %add7 = add nsw i32 %add6, %2
  ret i32 %add7
}

!0 = metadata !{metadata !"int", metadata !1}
!1 = metadata !{metadata !"omnipotent char", metadata !2}
!2 = metadata !{metadata !"Simple C/C++ TBAA"}
