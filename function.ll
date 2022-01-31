source_filename = "./test/basic_cases/test-cases/function.p"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

declare i32 @printf(i8*, ...)

@.str = private unnamed_addr constant [4 x i8] c"%d\0A\00", align 1
@gv = global i32 0, align 4
@gc = global i32 2, align 4

define i32 @product(i32 %0, i32 %1) {
  %3 = alloca i32, align 4 ; allocate a
  %4 = alloca i32, align 4 ; allocate b
  store i32 %1, i32* %4, align 4
  store i32 %0, i32* %3, align 4
  %5 = alloca i32, align 4 ; allocate result
  %6 = load i32, i32* %3, align 4
  %7 = load i32, i32* %4, align 4
  %8 = mul nsw i32 %7, %6
  store i32 %8, i32* %5, align 4 ; store to %result
  %9 = load i32, i32* %5, align 4
  ret i32 %9
}

define i32 @sum(i32 %0, i32 %1) {
  %3 = alloca i32, align 4 ; allocate a
  %4 = alloca i32, align 4 ; allocate b
  store i32 %1, i32* %4, align 4
  store i32 %0, i32* %3, align 4
  %5 = alloca i32, align 4 ; allocate result
  %6 = load i32, i32* %3, align 4
  %7 = load i32, i32* %4, align 4
  %8 = add nsw i32 %7, %6
  store i32 %8, i32* %5, align 4 ; store to %result
  %9 = load i32, i32* %5, align 4
  ret i32 %9
}

define i32 @dot(i32 %0, i32 %1, i32 %2, i32 %3) {
  %5 = alloca i32, align 4 ; allocate x1
  %6 = alloca i32, align 4 ; allocate y1
  %7 = alloca i32, align 4 ; allocate x2
  %8 = alloca i32, align 4 ; allocate y2
  store i32 %3, i32* %8, align 4
  store i32 %2, i32* %7, align 4
  store i32 %1, i32* %6, align 4
  store i32 %0, i32* %5, align 4
  %9 = alloca i32, align 4 ; allocate result
  %10 = load i32, i32* %5, align 4
  %11 = load i32, i32* %6, align 4
  %12 = call i32 @product(i32 %10, i32 %11)
  %13 = load i32, i32* %7, align 4
  %14 = load i32, i32* %8, align 4
  %15 = call i32 @product(i32 %13, i32 %14)
  %16 = call i32 @sum(i32 %12, i32 %15)
  store i32 %16, i32* %9, align 4 ; store to %result
  %17 = load i32, i32* %9, align 4
  ret i32 %17
}

define i32 @main() {
  %1 = alloca i32, align 4 ; allocate lv
  %2 = alloca i32, align 4 ; allocate lc
  store i32 2, i32* %2, align 4 ; store to %lc
  store i32 2, i32* @gv, align 4 ; store to @gv
  store i32 2, i32* %1, align 4 ; store to %lv
  %3 = load i32, i32* @gv, align 4
  %4 = load i32, i32* @gc, align 4
  %5 = call i32 @product(i32 %3, i32 %4)
  store i32 %5, i32* @gv, align 4 ; store to @gv
  %6 = load i32, i32* @gv, align 4
  %7 = load i32, i32* %1, align 4
  %8 = load i32, i32* %2, align 4
  %9 = call i32 @product(i32 %7, i32 %8)
  %10 = add nsw i32 %9, %6
  store i32 %10, i32* %1, align 4 ; store to %lv
  %11 = load i32, i32* @gv, align 4
  %12 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str, i64 0, i64 0), i32 %11)
  %13 = load i32, i32* %1, align 4
  %14 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str, i64 0, i64 0), i32 %13)
  %15 = load i32, i32* @gv, align 4
  %16 = load i32, i32* @gc, align 4
  %17 = load i32, i32* %1, align 4
  %18 = load i32, i32* %2, align 4
  %19 = call i32 @dot(i32 %15, i32 %16, i32 %17, i32 %18)
  store i32 %19, i32* @gv, align 4 ; store to @gv
  %20 = load i32, i32* @gv, align 4
  %21 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str, i64 0, i64 0), i32 %20)

  ret i32 0
}
