; ModuleID = 'case-8.c'
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
  %8 = load i32, i32* %x, align 4
  %cmp7 = icmp ne i32 %8, 3
  br i1 %cmp7, label %if.then, label %if.end

if.then:                                          ; preds = %for.end6
  store i32 4, i32* %x, align 4
  br label %if.end

if.end:                                           ; preds = %if.then, %for.end6
  store i32 0, i32* %i, align 4
  br label %for.cond8

for.cond8:                                        ; preds = %for.inc18, %if.end
  %9 = load i32, i32* %i, align 4
  %10 = load i32, i32* %n, align 4
  %cmp9 = icmp slt i32 %9, %10
  br i1 %cmp9, label %for.body10, label %for.end20

for.body10:                                       ; preds = %for.cond8
  store i32 0, i32* %j, align 4
  br label %for.cond11

for.cond11:                                       ; preds = %for.inc15, %for.body10
  %11 = load i32, i32* %j, align 4
  %12 = load i32, i32* %n, align 4
  %cmp12 = icmp slt i32 %11, %12
  br i1 %cmp12, label %for.body13, label %for.end17

for.body13:                                       ; preds = %for.cond11
  %13 = load i32, i32* %x, align 4
  %14 = load i32, i32* %sum, align 4
  %add14 = add nsw i32 %14, %13
  store i32 %add14, i32* %sum, align 4
  br label %for.inc15

for.inc15:                                        ; preds = %for.body13
  %15 = load i32, i32* %j, align 4
  %inc16 = add nsw i32 %15, 1
  store i32 %inc16, i32* %j, align 4
  br label %for.cond11

for.end17:                                        ; preds = %for.cond11
  br label %for.inc18

for.inc18:                                        ; preds = %for.end17
  %16 = load i32, i32* %i, align 4
  %inc19 = add nsw i32 %16, 1
  store i32 %inc19, i32* %i, align 4
  br label %for.cond8

for.end20:                                        ; preds = %for.cond8
  %17 = load i32, i32* %retval, align 4
  ret i32 %17
}

attributes #0 = { nounwind uwtable "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.ident = !{!0}

!0 = !{!"clang version 3.9.0 (trunk 259334) (llvm/trunk 259306)"}
