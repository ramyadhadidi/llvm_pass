; ModuleID = 'loop_opt.bc'
target datalayout = "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64-s0:64:64-f80:128:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

define i32 @main() nounwind uwtable {
entry:
  %retval = alloca i32, align 4
  %a = alloca [100 x i32], align 16
  %i = alloca i32, align 4
  store i32 0, i32* %retval
  store i32 0, i32* %i, align 4
  %0 = load i32* %i
  %1 = sext i32 %0 to i64
  %2 = icmp slt i64 %1, 0
  br i1 %2, label %trap, label %3

; <label>:3                                       ; preds = %entry
  br label %for.cond

for.cond:                                         ; preds = %for.inc, %3
  %4 = load i32* %i, align 4
  %cmp = icmp slt i32 %4, 100
  br i1 %cmp, label %for.body, label %for.end

for.body:                                         ; preds = %for.cond
  %5 = load i32* %i, align 4
  %idxprom = sext i32 %5 to i64
  %arrayidx.idx = mul i64 %idxprom, 4
  %arrayidx.offs = add i64 %arrayidx.idx, 0
  %6 = add i64 0, %arrayidx.offs
  %arrayidx = getelementptr inbounds [100 x i32]* %a, i32 0, i64 %idxprom
  %7 = icmp ult i64 400, %6
  br i1 %7, label %trap, label %8

; <label>:8                                       ; preds = %for.body
  store i32 10, i32* %arrayidx, align 4
  br label %for.inc

for.inc:                                          ; preds = %8
  %9 = load i32* %i, align 4
  %inc = add nsw i32 %9, 1
  store i32 %inc, i32* %i, align 4
  br label %for.cond

for.end:                                          ; preds = %for.cond
  ret i32 0

trap:                                             ; preds = %for.body, %entry
  call void @llvm.trap() noreturn nounwind
  unreachable
}

declare void @llvm.trap() noreturn nounwind
