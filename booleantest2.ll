source_filename = "./test/bonus_cases/test-cases/booleantest2.p"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

declare i32 @printf(i8*, ...)
declare i32 @__isoc99_scanf(i8*, ...)

@.str = private unnamed_addr constant [4 x i8] c"%d\0A\00", align 1

define i32 @main() {
  %1 = alloca i32, align 4 ; allocate a
  %2 = alloca i32, align 4 ; allocate b
  store i32 10, i32* %1, align 4 ; store to %a
  store i32 5, i32* %2, align 4 ; store to %b
  %3 = load i32, i32* %1, align 4
  %4 = load i32, i32* %2, align 4
  %5 = icmp sgt i32 %3, %4
  %6 = load i32, i32* %1, align 4
  %7 = icmp sle i32 %6, 10
  %8 = and  i1 %5, %7
  %9 = xor i1 1, 8
  ; if-else 
  br i1 %9, label %10, label %13             
10:  ; if
  %11 = load i32, i32* %1, align 4
  %12 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str, i64 0, i64 0), i32 %11)
  br label %16             
13:  ; else
  %14 = load i32, i32* %2, align 4
  %15 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str, i64 0, i64 0), i32 %14)
  br label %16
16:

  ret i32 0
}