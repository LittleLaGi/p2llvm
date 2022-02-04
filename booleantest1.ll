source_filename = "./test/bonus_cases/test-cases/booleantest1.p"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

declare i32 @printf(i8*, ...)
declare i32 @__isoc99_scanf(i8*, ...)

@.str = private unnamed_addr constant [4 x i8] c"%d\0A\00", align 1

define i1 @larger(i32 %0, i32 %1) {
  %3 = alloca i32, align 4 ; allocate a
  %4 = alloca i32, align 4 ; allocate b
  store i32 %1, i32* %4, align 4 ; store parameter's value to alloca variable
  store i32 %0, i32* %3, align 4 ; store parameter's value to alloca variable
  %5 = alloca i1, align 4 ; allocate result
  %6 = load i32, i32* %3, align 4
  %7 = load i32, i32* %4, align 4
  %8 = icmp sgt i32 %6, %7
  store i1 %8, i1* %5, align 4 ; store to %result
  %9 = load i1, i1* %5, align 4
  ret i1 %9
}

define i32 @test(i32 %0, i1 %1) {
  %3 = alloca i32, align 4 ; allocate a
  %4 = alloca i1, align 4 ; allocate b
  store i1 %1, i1* %4, align 4 ; store parameter's value to alloca variable
  store i32 %0, i32* %3, align 4 ; store parameter's value to alloca variable
  %5 = load i1, i1* %4, align 4
  ; if-else 
  br i1 %5, label %6, label %8              
6:  ; if
  %7 = load i32, i32* %3, align 4
  ret i32 %7
               
8:  ; else
  ret i32 0
}

define i32 @main() {
  %1 = alloca i32, align 4 ; allocate a
  %2 = alloca i32, align 4 ; allocate b
  store i32 10, i32* %1, align 4 ; store to %a
  store i32 5, i32* %2, align 4 ; store to %b
  %3 = load i32, i32* %1, align 4
  %4 = load i32, i32* %2, align 4
  %5 = call i1 @larger(i32 %3, i32 %4)
  ; if-else 
  br i1 %5, label %6, label %9              
6:  ; if
  %7 = load i32, i32* %1, align 4
  %8 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str, i64 0, i64 0), i32 %7)
  br label %12             
9:  ; else
  %10 = load i32, i32* %2, align 4
  %11 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str, i64 0, i64 0), i32 %10)
  br label %12
12:
  %13 = load i32, i32* %1, align 4
  %14 = call i32 @test(i32 %13, i1 0)
  store i32 %14, i32* %1, align 4 ; store to %a
  %15 = load i32, i32* %1, align 4
  %16 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str, i64 0, i64 0), i32 %15)

  ret i32 0
}