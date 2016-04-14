; ModuleID = 'loop_simplify.bc'
target datalayout = "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64-s0:64:64-f80:128:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@_ZZ4mainE3arr = private unnamed_addr constant [5 x i32] [i32 1, i32 2, i32 3, i32 4, i32 5], align 16

define i32 @main(i32 %argc, i8** %argv) nounwind uwtable {
entry:
  %retval = alloca i32, align 4
  %argc.addr = alloca i32, align 4
  %argv.addr = alloca i8**, align 8
  %arr = alloca [5 x i32], align 16
  %x = alloca i32, align 4
  %k = alloca i32, align 4
  %i = alloca i32, align 4
  %j = alloca i32, align 4
  store i32 0, i32* %retval
  store i32 %argc, i32* %argc.addr, align 4
  store i8** %argv, i8*** %argv.addr, align 8
  %0 = bitcast [5 x i32]* %arr to i8*
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* %0, i8* bitcast ([5 x i32]* @_ZZ4mainE3arr to i8*), i64 20, i32 16, i1 false)
  %1 = load i32* %argc.addr, align 4
  store i32 %1, i32* %x, align 4
  store i32 0, i32* %k, align 4
  store i32 0, i32* %i, align 4
  br label %for.cond

for.cond:                                         ; preds = %for.inc10, %entry
  %2 = load i32* %i, align 4
  %3 = load i32* %argc.addr, align 4
  %cmp = icmp slt i32 %2, %3
  br i1 %cmp, label %for.body, label %for.end12

for.body:                                         ; preds = %for.cond
  store i32 0, i32* %j, align 4
  br label %for.cond1

for.cond1:                                        ; preds = %for.inc, %for.body
  %4 = load i32* %j, align 4
  %5 = load i32* %argc.addr, align 4
  %cmp2 = icmp slt i32 %4, %5
  br i1 %cmp2, label %for.body3, label %for.end

for.body3:                                        ; preds = %for.cond1
  %6 = load i32* %x, align 4
  %idxprom = sext i32 %6 to i64
  %arrayidx = getelementptr inbounds [5 x i32]* %arr, i32 0, i64 %idxprom
  %7 = load i32* %arrayidx, align 4
  %8 = load i32* %k, align 4
  %add = add nsw i32 %8, %7
  store i32 %add, i32* %k, align 4
  %9 = load i32* %i, align 4
  %idxprom4 = sext i32 %9 to i64
  %arrayidx5 = getelementptr inbounds [5 x i32]* %arr, i32 0, i64 %idxprom4
  %10 = load i32* %arrayidx5, align 4
  %11 = load i32* %k, align 4
  %add6 = add nsw i32 %11, %10
  store i32 %add6, i32* %k, align 4
  %12 = load i32* %j, align 4
  %idxprom7 = sext i32 %12 to i64
  %arrayidx8 = getelementptr inbounds [5 x i32]* %arr, i32 0, i64 %idxprom7
  %13 = load i32* %arrayidx8, align 4
  %14 = load i32* %k, align 4
  %add9 = add nsw i32 %14, %13
  store i32 %add9, i32* %k, align 4
  br label %for.inc

for.inc:                                          ; preds = %for.body3
  %15 = load i32* %j, align 4
  %inc = add nsw i32 %15, 1
  store i32 %inc, i32* %j, align 4
  br label %for.cond1

for.end:                                          ; preds = %for.cond1
  br label %for.inc10

for.inc10:                                        ; preds = %for.end
  %16 = load i32* %i, align 4
  %inc11 = add nsw i32 %16, 1
  store i32 %inc11, i32* %i, align 4
  br label %for.cond

for.end12:                                        ; preds = %for.cond
  ret i32 0
}

declare void @llvm.memcpy.p0i8.p0i8.i64(i8* nocapture, i8* nocapture, i64, i32, i1) nounwind
