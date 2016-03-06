; ModuleID = 'case-7.ll'
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; Function Attrs: nounwind uwtable
define i32 @main() #0 {
entry:
  %retval = alloca i32, align 4
  %x = alloca i32, align 4
  %i = alloca i32, align 4
  %j = alloca i32, align 4
  %n = alloca i32, align 4
  %sum = alloca i32, align 4
  %y = alloca i32, align 4
  store i32 0, i32* %retval, align 4
  store i32 3, i32* %x, align 4
  store i32 0, i32* %sum, align 4
  store i32 0, i32* %i, align 4
  br label %for.cond

for.cond:                                         ; preds = %for.inc4, %entry
  %0 = load i32, i32* %i, align 4
  %1 = load i32, i32* %n, align 4
  %cmp = icmp slt i32 %0, %1
  br i1 %cmp, label %for.body, label %for.end6

for.body:                                         ; preds = %for.cond
  store i32 0, i32* %j, align 4
  br label %for.cond1

for.cond1:                                        ; preds = %for.inc, %for.body
  %2 = load i32, i32* %j, align 4
  %3 = load i32, i32* %n, align 4
  %cmp2 = icmp slt i32 %2, %3
  br i1 %cmp2, label %for.body3, label %for.end

for.body3:                                        ; preds = %for.cond1
  %4 = load i32, i32* %x, align 4
  %5 = load i32, i32* %sum, align 4
  %add = add nsw i32 %5, %4
  store i32 %add, i32* %sum, align 4
  br label %for.inc

for.inc:                                          ; preds = %for.body3
  %6 = load i32, i32* %j, align 4
  %inc = add nsw i32 %6, 1
  store i32 %inc, i32* %j, align 4
  br label %for.cond1

for.end:                                          ; preds = %for.cond1
  br label %for.inc4

for.inc4:                                         ; preds = %for.end
  %7 = load i32, i32* %i, align 4
  %inc5 = add nsw i32 %7, 1
  store i32 %inc5, i32* %i, align 4
  br label %for.cond

for.end6:                                         ; preds = %for.cond
  store i32 4, i32* %x, align 4
  %8 = load i32, i32* %n, align 4
  store i32 %8, i32* %y, align 4
  store i32 0, i32* %i, align 4
  br label %for.cond7

for.cond7:                                        ; preds = %for.inc17, %for.end6
  %9 = load i32, i32* %i, align 4
  %10 = load i32, i32* %n, align 4
  %cmp8 = icmp slt i32 %9, %10
  br i1 %cmp8, label %for.body9, label %for.end19

for.body9:                                        ; preds = %for.cond7
  store i32 0, i32* %j, align 4
  br label %for.cond10

for.cond10:                                       ; preds = %for.inc14, %for.body9
  %11 = load i32, i32* %j, align 4
  %12 = load i32, i32* %n, align 4
  %cmp11 = icmp slt i32 %11, %12
  br i1 %cmp11, label %for.body12, label %for.end16

for.body12:                                       ; preds = %for.cond10
  %13 = load i32, i32* %x, align 4
  %14 = load i32, i32* %sum, align 4
  %add13 = add nsw i32 %14, %13
  store i32 %add13, i32* %sum, align 4
  br label %for.inc14

for.inc14:                                        ; preds = %for.body12
  %15 = load i32, i32* %j, align 4
  %inc15 = add nsw i32 %15, 1
  store i32 %inc15, i32* %j, align 4
  br label %for.cond10

for.end16:                                        ; preds = %for.cond10
  %16 = load i32, i32* %y, align 4
  store i32 %16, i32* %x, align 4
  br label %for.inc17

for.inc17:                                        ; preds = %for.end16
  %17 = load i32, i32* %i, align 4
  %inc18 = add nsw i32 %17, 1
  store i32 %inc18, i32* %i, align 4
  br label %for.cond7

for.end19:                                        ; preds = %for.cond7
  %18 = load i32, i32* %retval, align 4
  ret i32 %18
}

attributes #0 = { nounwind uwtable "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.ident = !{!0}

!0 = !{!"clang version 3.9.0 (trunk 259334) (llvm/trunk 259306)"}
