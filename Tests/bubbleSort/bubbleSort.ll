; ModuleID = 'bubbleSort.ll'
source_filename = "ld-temp.o"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

@.str = private unnamed_addr constant [11 x i8] c"\0ASIZE = %d\00", align 1
@.str.1 = private unnamed_addr constant [14 x i8] c"\0ASorting Done\00", align 1
@.str.2 = private unnamed_addr constant [3 x i8] c"%c\00", align 1

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @main(i32, i8**) #0 {
  %3 = alloca i32, align 4
  %4 = alloca i32, align 4
  %5 = alloca i8**, align 8
  %6 = alloca i32, align 4
  %7 = alloca i8*, align 8
  %8 = alloca i64, align 8
  %9 = alloca i32, align 4
  %10 = alloca i32, align 4
  %11 = alloca i32, align 4
  store i32 0, i32* %3, align 4
  store i32 %0, i32* %4, align 4
  store i8** %1, i8*** %5, align 8
  %12 = load i32, i32* %4, align 4
  %13 = icmp sge i32 %12, 1
  br i1 %13, label %14, label %19

14:                                               ; preds = %2
  %15 = load i8**, i8*** %5, align 8
  %16 = getelementptr inbounds i8*, i8** %15, i64 1
  %17 = load i8*, i8** %16, align 8
  %18 = call i32 @atoi(i8* %17) #4
  store i32 %18, i32* %6, align 4
  br label %20

19:                                               ; preds = %2
  store i32 1000, i32* %6, align 4
  br label %20

20:                                               ; preds = %19, %14
  %21 = load i32, i32* %6, align 4
  %22 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([11 x i8], [11 x i8]* @.str, i64 0, i64 0), i32 %21)
  %23 = load i32, i32* %6, align 4
  %24 = zext i32 %23 to i64
  %25 = call i8* @llvm.stacksave()
  store i8* %25, i8** %7, align 8
  %26 = alloca i32, i64 %24, align 16
  store i64 %24, i64* %8, align 8
  %27 = load i32, i32* %6, align 4
  call void @get_input(i32 %27, i32* %26)
  store i32 0, i32* %10, align 4
  br label %28

28:                                               ; preds = %69, %20
  %29 = load i32, i32* %10, align 4
  %30 = load i32, i32* %6, align 4
  %31 = icmp slt i32 %29, %30
  br i1 %31, label %32, label %72

32:                                               ; preds = %28
  %33 = load i32, i32* %10, align 4
  store i32 %33, i32* %11, align 4
  br label %34

34:                                               ; preds = %65, %32
  %35 = load i32, i32* %11, align 4
  %36 = load i32, i32* %6, align 4
  %37 = icmp slt i32 %35, %36
  br i1 %37, label %38, label %68

38:                                               ; preds = %34
  %39 = load i32, i32* %10, align 4
  %40 = sext i32 %39 to i64
  %41 = getelementptr inbounds i32, i32* %26, i64 %40
  %42 = load i32, i32* %41, align 4
  %43 = load i32, i32* %11, align 4
  %44 = sext i32 %43 to i64
  %45 = getelementptr inbounds i32, i32* %26, i64 %44
  %46 = load i32, i32* %45, align 4
  %47 = icmp sgt i32 %42, %46
  br i1 %47, label %48, label %64

48:                                               ; preds = %38
  %49 = load i32, i32* %10, align 4
  %50 = sext i32 %49 to i64
  %51 = getelementptr inbounds i32, i32* %26, i64 %50
  %52 = load i32, i32* %51, align 4
  store i32 %52, i32* %9, align 4
  %53 = load i32, i32* %11, align 4
  %54 = sext i32 %53 to i64
  %55 = getelementptr inbounds i32, i32* %26, i64 %54
  %56 = load i32, i32* %55, align 4
  %57 = load i32, i32* %10, align 4
  %58 = sext i32 %57 to i64
  %59 = getelementptr inbounds i32, i32* %26, i64 %58
  store i32 %56, i32* %59, align 4
  %60 = load i32, i32* %9, align 4
  %61 = load i32, i32* %11, align 4
  %62 = sext i32 %61 to i64
  %63 = getelementptr inbounds i32, i32* %26, i64 %62
  store i32 %60, i32* %63, align 4
  br label %64

64:                                               ; preds = %48, %38
  br label %65

65:                                               ; preds = %64
  %66 = load i32, i32* %11, align 4
  %67 = add nsw i32 %66, 1
  store i32 %67, i32* %11, align 4
  br label %34

68:                                               ; preds = %34
  br label %69

69:                                               ; preds = %68
  %70 = load i32, i32* %10, align 4
  %71 = add nsw i32 %70, 1
  store i32 %71, i32* %10, align 4
  br label %28

72:                                               ; preds = %28
  %73 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([14 x i8], [14 x i8]* @.str.1, i64 0, i64 0))
  store i32 0, i32* %3, align 4
  %74 = load i8*, i8** %7, align 8
  call void @llvm.stackrestore(i8* %74)
  %75 = load i32, i32* %3, align 4
  ret i32 %75
}

; Function Attrs: nounwind readonly
declare dso_local i32 @atoi(i8*) #1

declare dso_local i32 @printf(i8*, ...) #2

; Function Attrs: nounwind
declare i8* @llvm.stacksave() #3

; Function Attrs: nounwind
declare void @llvm.stackrestore(i8*) #3

; Function Attrs: noinline nounwind optnone uwtable
define internal i32 @read_number() #0 {
  %1 = alloca i32, align 4
  %2 = alloca [25 x i8], align 16
  %3 = alloca i8, align 1
  %4 = alloca i32, align 4
  %5 = alloca i32, align 4
  %6 = alloca i8*, align 8
  %7 = alloca i64, align 8
  %8 = alloca i32, align 4
  store i32 0, i32* %1, align 4
  store i32 10, i32* %4, align 4
  store volatile i32 100, i32* %5, align 4
  br label %9

9:                                                ; preds = %21, %0
  %10 = call i32 (i8*, ...) @__isoc99_scanf(i8* getelementptr inbounds ([3 x i8], [3 x i8]* @.str.2, i64 0, i64 0), i8* %3)
  %11 = load i8, i8* %3, align 1
  %12 = load i32, i32* %1, align 4
  %13 = sext i32 %12 to i64
  %14 = getelementptr inbounds [25 x i8], [25 x i8]* %2, i64 0, i64 %13
  store i8 %11, i8* %14, align 1
  %15 = load i32, i32* %1, align 4
  %16 = add nsw i32 %15, 1
  store i32 %16, i32* %1, align 4
  %17 = load i8, i8* %3, align 1
  %18 = sext i8 %17 to i32
  %19 = load i32, i32* %4, align 4
  %20 = sub nsw i32 %18, %19
  store volatile i32 %20, i32* %5, align 4
  br label %21

21:                                               ; preds = %9
  %22 = load volatile i32, i32* %5, align 4
  %23 = icmp ne i32 %22, 0
  br i1 %23, label %9, label %24

24:                                               ; preds = %21
  %25 = load i32, i32* %1, align 4
  %26 = zext i32 %25 to i64
  %27 = call i8* @llvm.stacksave()
  store i8* %27, i8** %6, align 8
  %28 = alloca i8, i64 %26, align 16
  store i64 %26, i64* %7, align 8
  store i32 0, i32* %8, align 4
  br label %29

29:                                               ; preds = %41, %24
  %30 = load i32, i32* %8, align 4
  %31 = load i32, i32* %1, align 4
  %32 = icmp slt i32 %30, %31
  br i1 %32, label %33, label %44

33:                                               ; preds = %29
  %34 = load i32, i32* %8, align 4
  %35 = sext i32 %34 to i64
  %36 = getelementptr inbounds [25 x i8], [25 x i8]* %2, i64 0, i64 %35
  %37 = load i8, i8* %36, align 1
  %38 = load i32, i32* %8, align 4
  %39 = sext i32 %38 to i64
  %40 = getelementptr inbounds i8, i8* %28, i64 %39
  store i8 %37, i8* %40, align 1
  br label %41

41:                                               ; preds = %33
  %42 = load i32, i32* %8, align 4
  %43 = add nsw i32 %42, 1
  store i32 %43, i32* %8, align 4
  br label %29

44:                                               ; preds = %29
  %45 = getelementptr inbounds [25 x i8], [25 x i8]* %2, i64 0, i64 0
  %46 = call i32 @atoi(i8* %45) #4
  %47 = load i8*, i8** %6, align 8
  call void @llvm.stackrestore(i8* %47)
  ret i32 %46
}

declare dso_local i32 @__isoc99_scanf(i8*, ...) #2

; Function Attrs: noinline nounwind optnone uwtable
define internal void @get_input(i32, i32*) #0 {
  %3 = alloca i32, align 4
  %4 = alloca i32*, align 8
  %5 = alloca i32, align 4
  store i32 %0, i32* %3, align 4
  store i32* %1, i32** %4, align 8
  store i32 0, i32* %5, align 4
  br label %6

6:                                                ; preds = %16, %2
  %7 = load i32, i32* %5, align 4
  %8 = load i32, i32* %3, align 4
  %9 = icmp slt i32 %7, %8
  br i1 %9, label %10, label %19

10:                                               ; preds = %6
  %11 = call i32 @read_number()
  %12 = load i32*, i32** %4, align 8
  %13 = load i32, i32* %5, align 4
  %14 = sext i32 %13 to i64
  %15 = getelementptr inbounds i32, i32* %12, i64 %14
  store i32 %11, i32* %15, align 4
  br label %16

16:                                               ; preds = %10
  %17 = load i32, i32* %5, align 4
  %18 = add nsw i32 %17, 1
  store i32 %18, i32* %5, align 4
  br label %6

19:                                               ; preds = %6
  ret void
}

attributes #0 = { noinline nounwind optnone uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { nounwind readonly "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #2 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #3 = { nounwind }
attributes #4 = { nounwind readonly }

!llvm.ident = !{!0, !0}
!llvm.module.flags = !{!1, !2, !3}

!0 = !{!"clang version 9.0.0-2~ubuntu18.04.1 (tags/RELEASE_900/final)"}
!1 = !{i32 1, !"wchar_size", i32 4}
!2 = !{i32 1, !"ThinLTO", i32 0}
!3 = !{i32 1, !"EnableSplitLTOUnit", i32 0}
