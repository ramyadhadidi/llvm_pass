; ModuleID = 'case-5.ll'
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; Function Attrs: nounwind uwtable
define i32 @main() #0 {
entry:
  %retval = alloca i32, align 4
  %x = alloca i32, align 4
  %n = alloca i32, align 4
  %rval = alloca i32, align 4
  store i32 0, i32* %retval, align 4
  store i32 3, i32* %x, align 4
  br label %while.cond

while.cond:                                       ; preds = %while.body, %entry
  %0 = load i32, i32* %n, align 4
  %cmp = icmp sgt i32 %0, 100
  br i1 %cmp, label %while.body, label %while.end

while.body:                                       ; preds = %while.cond
  %1 = load i32, i32* %x, align 4
  store i32 %1, i32* %rval, align 4
  store i32 4, i32* %x, align 4
  %2 = load i32, i32* %n, align 4
  %dec = add nsw i32 %2, -1
  store i32 %dec, i32* %n, align 4
  store i32 3, i32* %x, align 4
  br label %while.cond

while.end:                                        ; preds = %while.cond
  %3 = load i32, i32* %rval, align 4
  ret i32 %3
}

attributes #0 = { nounwind uwtable "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.ident = !{!0}

!0 = !{!"clang version 3.9.0 (trunk 259334) (llvm/trunk 259306)"}
