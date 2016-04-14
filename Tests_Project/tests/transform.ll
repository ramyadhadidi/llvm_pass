; ModuleID = 'transform.bc'
target datalayout = "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64-s0:64:64-f80:128:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@count = global i32 0, align 4
@_ZZ4mainE3arr = private unnamed_addr constant [5 x i32] [i32 1, i32 2, i32 3, i32 4, i32 5], align 16

define i32 @main(i32 %argc, i8** nocapture %argv) nounwind uwtable readnone {
entry:
  %0 = zext i32 %argc to i64
  %vla = alloca i32, i64 %0, align 16
  %idxprom = sext i32 %argc to i64
  %arrayidx.idx = mul i64 %idxprom, 4
  %arrayidx.offs = add i64 %arrayidx.idx, 0
  %1 = add i64 0, %arrayidx.offs
  %arrayidx = getelementptr inbounds [5 x i32]* @_ZZ4mainE3arr, i64 0, i64 %idxprom
  %2 = load i32* %arrayidx, align 4, !tbaa !0
  %cmp26 = icmp sgt i32 %argc, 0
  br i1 %cmp26, label %for.body, label %for.end

for.body:                                         ; preds = %for.body, %entry
  %indvars.iv = phi i64 [ %indvars.iv.next, %for.body ], [ 0, %entry ]
  %arrayidx6.idx = mul i64 %indvars.iv, 4
  %arrayidx6.offs = add i64 %arrayidx6.idx, 0
  %3 = add i64 0, %arrayidx6.offs
  %arrayidx6 = getelementptr inbounds [5 x i32]* @_ZZ4mainE3arr, i64 0, i64 %indvars.iv
  %4 = load i32* %arrayidx6, align 4, !tbaa !0
  %add7 = add nsw i32 %4, %argc
  %arrayidx9.idx = mul i64 %indvars.iv, 4
  %arrayidx9.offs = add i64 %arrayidx9.idx, 0
  %5 = add i64 0, %arrayidx9.offs
  %arrayidx9 = getelementptr inbounds i32* %vla, i64 %indvars.iv
  store i32 %add7, i32* %arrayidx9, align 4, !tbaa !0
  %indvars.iv.next = add i64 %indvars.iv, 1
  %lftr.wideiv = trunc i64 %indvars.iv.next to i32
  %exitcond = icmp eq i32 %lftr.wideiv, %argc
  br i1 %exitcond, label %for.end, label %for.body

for.end:                                          ; preds = %for.body, %entry
  %sub13 = add nsw i32 %argc, -1
  %idxprom14 = sext i32 %sub13 to i64
  %arrayidx15 = getelementptr inbounds i32* %vla, i64 %idxprom14
  %6 = load i32* %arrayidx15, align 4, !tbaa !0
  %add16 = add nsw i32 %6, %2
  ret i32 %add16
}

!0 = metadata !{metadata !"int", metadata !1}
!1 = metadata !{metadata !"omnipotent char", metadata !2}
!2 = metadata !{metadata !"Simple C/C++ TBAA"}
