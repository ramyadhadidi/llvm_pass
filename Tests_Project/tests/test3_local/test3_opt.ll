; ModuleID = 'test3_opt.bc'
target datalayout = "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64-s0:64:64-f80:128:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@_ZZ4mainE3arr = private unnamed_addr constant [5 x i32] [i32 1, i32 2, i32 3, i32 4, i32 5], align 16

define i32 @main(i32 %argc, i8** %argv) nounwind uwtable {
entry:
  %retval = alloca i32, align 4
  %argc.addr = alloca i32, align 4
  %argv.addr = alloca i8**, align 8
  %arr = alloca [5 x i32], align 16
  %i = alloca i32, align 4
  %i2 = alloca i32, align 4
  %k = alloca i32, align 4
  %j = alloca i32, align 4
  %l = alloca i32, align 4
  store i32 0, i32* %retval
  store i32 %argc, i32* %argc.addr, align 4
  store i8** %argv, i8*** %argv.addr, align 8
  %0 = bitcast [5 x i32]* %arr to i8*
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* %0, i8* bitcast ([5 x i32]* @_ZZ4mainE3arr to i8*), i64 20, i32 16, i1 false)
  %1 = load i32* %argc.addr, align 4
  store i32 %1, i32* %i, align 4
  %2 = load i32* %i, align 4
  %3 = load i32* %i, align 4
  %add = add nsw i32 %3, 1
  store i32 %add, i32* %i, align 4
  %4 = load i32* %i, align 4
  %add3 = add nsw i32 %4, 1
  store i32 %add3, i32* %j, align 4
  %5 = load i32* %j, align 4
  %idxprom4 = sext i32 %5 to i64
  %6 = icmp sge i64 20, %idxprom4
  br i1 %6, label %trap, label %7

; <label>:7                                       ; preds = %entry
  %idxprom = sext i32 %2 to i64
  %arrayidx = getelementptr inbounds [5 x i32]* %arr, i32 0, i64 %idxprom
  %8 = icmp slt i64 %idxprom, 0
  br i1 %8, label %trap, label %9

; <label>:9                                       ; preds = %7
  %10 = load i32* %arrayidx, align 4
  store i32 %10, i32* %i2, align 4
  %11 = load i32* %i, align 4
  %idxprom1 = sext i32 %11 to i64
  %arrayidx2 = getelementptr inbounds [5 x i32]* %arr, i32 0, i64 %idxprom1
  %12 = load i32* %arrayidx2, align 4
  store i32 %12, i32* %k, align 4
  %arrayidx5 = getelementptr inbounds [5 x i32]* %arr, i32 0, i64 %idxprom4
  %13 = load i32* %arrayidx5, align 4
  store i32 %13, i32* %l, align 4
  ret i32 0

trap:                                             ; preds = %7, %entry
  call void @llvm.trap() noreturn nounwind
  unreachable
}

declare void @llvm.memcpy.p0i8.p0i8.i64(i8* nocapture, i8* nocapture, i64, i32, i1) nounwind

declare void @llvm.trap() noreturn nounwind
