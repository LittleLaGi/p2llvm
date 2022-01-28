; ModuleID = './test/basic_cases/sample-code/expression.c'
source_filename = "./test/basic_cases/sample-code/expression.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

@gc = dso_local global i32 2, align 4
@gv = common dso_local global i32 0, align 4
@.str = private unnamed_addr constant [4 x i8] c"%d\0A\00", align 1

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @main() #0 {
  %1 = alloca i32, align 4
  %2 = alloca i32, align 4
  %3 = alloca i32, align 4
  store i32 0, i32* %1, align 4
  store i32 2, i32* %3, align 4
  store i32 2, i32* @gv, align 4
  store i32 2, i32* %2, align 4
  %4 = load i32, i32* %3, align 4
  %5 = load i32, i32* @gv, align 4
  %6 = add nsw i32 %4, %5
  %7 = load i32, i32* @gc, align 4
  %8 = add nsw i32 %6, %7
  %9 = load i32, i32* %2, align 4
  %10 = add nsw i32 %8, %9
  store i32 %10, i32* @gv, align 4
  %11 = load i32, i32* %3, align 4
  %12 = load i32, i32* @gv, align 4
  %13 = mul nsw i32 %11, %12
  %14 = load i32, i32* @gc, align 4
  %15 = mul nsw i32 %13, %14
  %16 = load i32, i32* %2, align 4
  %17 = mul nsw i32 %15, %16
  store i32 %17, i32* %2, align 4
  %18 = load i32, i32* @gv, align 4
  %19 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str, i64 0, i64 0), i32 %18)
  %20 = load i32, i32* %2, align 4
  %21 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str, i64 0, i64 0), i32 %20)
  %22 = load i32, i32* %3, align 4
  %23 = load i32, i32* @gv, align 4
  %24 = load i32, i32* @gc, align 4
  %25 = add nsw i32 %23, %24
  %26 = load i32, i32* %2, align 4
  %27 = mul nsw i32 %25, %26
  %28 = add nsw i32 %22, %27
  store i32 %28, i32* @gv, align 4
  %29 = load i32, i32* %3, align 4
  %30 = load i32, i32* @gv, align 4
  %31 = load i32, i32* @gc, align 4
  %32 = load i32, i32* %2, align 4
  %33 = load i32, i32* %3, align 4
  %34 = load i32, i32* @gv, align 4
  %35 = load i32, i32* @gc, align 4
  %36 = load i32, i32* %2, align 4
  %37 = load i32, i32* %3, align 4
  %38 = add nsw i32 %36, %37
  %39 = add nsw i32 %35, %38
  %40 = add nsw i32 %34, %39
  %41 = add nsw i32 %33, %40
  %42 = add nsw i32 %32, %41
  %43 = add nsw i32 %31, %42
  %44 = add nsw i32 %30, %43
  %45 = add nsw i32 %29, %44
  store i32 %45, i32* %2, align 4
  %46 = load i32, i32* @gv, align 4
  %47 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str, i64 0, i64 0), i32 %46)
  %48 = load i32, i32* %2, align 4
  %49 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str, i64 0, i64 0), i32 %48)
  ret i32 0
}

declare dso_local i32 @printf(i8*, ...) #1

attributes #0 = { noinline nounwind optnone uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="all" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="all" "less-precise-fpmad"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 10.0.0-4ubuntu1 "}
