; ModuleID = 'Recurse.bc'
source_filename = "Recurse.c"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

@.str = private unnamed_addr constant [9 x i8] c"Success\0A\00", align 1

; Function Attrs: noinline nounwind optnone uwtable
define dso_local void @Recurse(i32*, i32*, i32) #0 {
BB_UID_0:
  %3 = alloca i32*, align 8
  %4 = alloca i32*, align 8
  %5 = alloca i32, align 4
  store i32* %0, i32** %3, align 8
  store i32* %1, i32** %4, align 8
  store i32 %2, i32* %5, align 4
  %6 = load i32, i32* %5, align 4
  %7 = icmp ne i32 %6, 0
  br i1 %7, label %BB_UID_1, label %BB_UID_2

BB_UID_1:                                         ; preds = %BB_UID_0
  %8 = load i32*, i32** %3, align 8
  %9 = load i32, i32* %5, align 4
  %10 = sub nsw i32 %9, 1
  %11 = sext i32 %10 to i64
  %12 = getelementptr inbounds i32, i32* %8, i64 %11
  %13 = load i32, i32* %12, align 4
  %14 = load i32*, i32** %3, align 8
  %15 = load i32, i32* %5, align 4
  %16 = sext i32 %15 to i64
  %17 = getelementptr inbounds i32, i32* %14, i64 %16
  %18 = load i32, i32* %17, align 4
  %19 = add nsw i32 %13, %18
  %20 = load i32*, i32** %3, align 8
  %21 = load i32, i32* %5, align 4
  %22 = add nsw i32 %21, 1
  %23 = sext i32 %22 to i64
  %24 = getelementptr inbounds i32, i32* %20, i64 %23
  %25 = load i32, i32* %24, align 4
  %26 = add nsw i32 %19, %25
  %27 = load i32*, i32** %4, align 8
  %28 = load i32, i32* %5, align 4
  %29 = sext i32 %28 to i64
  %30 = getelementptr inbounds i32, i32* %27, i64 %29
  store i32 %26, i32* %30, align 4
  br label %BB_UID_2

BB_UID_2:                                         ; preds = %BB_UID_1, %BB_UID_0
  %31 = load i32, i32* %5, align 4
  %32 = icmp ne i32 %31, 1023
  br i1 %32, label %BB_UID_3, label %BB_UID_4

BB_UID_3:                                         ; preds = %BB_UID_2
  %33 = load i32*, i32** %3, align 8
  %34 = load i32*, i32** %4, align 8
  %35 = load i32, i32* %5, align 4
  %36 = add nsw i32 %35, 1
  store i32 %36, i32* %5, align 4
  call void @Recurse(i32* %33, i32* %34, i32 %36)
  br label %BB_UID_4

BB_UID_4:                                         ; preds = %BB_UID_3, %BB_UID_2
  ret void
}

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @main() #0 {
BB_UID_5:
  %0 = alloca i32, align 4
  %1 = alloca i32*, align 8
  %2 = alloca i32*, align 8
  %3 = alloca i32, align 4
  %4 = alloca i32, align 4
  store i32 0, i32* %0, align 4
  %5 = call noalias i8* @malloc(i64 4096) #3
  %6 = bitcast i8* %5 to i32*
  store i32* %6, i32** %1, align 8
  %7 = call noalias i8* @malloc(i64 4088) #3
  %8 = bitcast i8* %7 to i32*
  store i32* %8, i32** %2, align 8
  %9 = call i64 @time(i64* null) #3
  %10 = trunc i64 %9 to i32
  call void @srand(i32 %10) #3
  store i32 0, i32* %3, align 4
  br label %BB_UID_6

BB_UID_6:                                         ; preds = %BB_UID_8, %BB_UID_5
  %11 = load i32, i32* %3, align 4
  %12 = icmp slt i32 %11, 1024
  br i1 %12, label %BB_UID_7, label %BB_UID_9

BB_UID_7:                                         ; preds = %BB_UID_6
  %13 = call i32 @rand() #3
  %14 = load i32*, i32** %1, align 8
  %15 = load i32, i32* %3, align 4
  %16 = sext i32 %15 to i64
  %17 = getelementptr inbounds i32, i32* %14, i64 %16
  store i32 %13, i32* %17, align 4
  br label %BB_UID_8

BB_UID_8:                                         ; preds = %BB_UID_7
  %18 = load i32, i32* %3, align 4
  %19 = add nsw i32 %18, 1
  store i32 %19, i32* %3, align 4
  br label %BB_UID_6

BB_UID_9:                                         ; preds = %BB_UID_6
  store i32 0, i32* %4, align 4
  %20 = load i32*, i32** %1, align 8
  %21 = load i32*, i32** %2, align 8
  %22 = load i32, i32* %4, align 4
  call void @Recurse(i32* %20, i32* %21, i32 %22)
  %23 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([9 x i8], [9 x i8]* @.str, i64 0, i64 0))
  ret i32 0
}

; Function Attrs: nounwind
declare dso_local noalias i8* @malloc(i64) #1

; Function Attrs: nounwind
declare dso_local void @srand(i32) #1

; Function Attrs: nounwind
declare dso_local i64 @time(i64*) #1

; Function Attrs: nounwind
declare dso_local i32 @rand() #1

declare dso_local i32 @printf(i8*, ...) #2

attributes #0 = { noinline nounwind optnone uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { nounwind "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #2 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #3 = { nounwind }

!llvm.module.flags = !{!0, !1, !2}
!llvm.ident = !{!3}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 1, !"ThinLTO", i32 0}
!2 = !{i32 1, !"EnableSplitLTOUnit", i32 0}
!3 = !{!"clang version 9.0.1-+20191211102125+c1a0a213378-1~exp1~20191211212701.104 "}
