; ModuleID = '2DConv.o'
source_filename = "2DConv.c"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

@.str = private unnamed_addr constant [9 x i8] c"Success\0A\00", align 1

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @main() #0 {
  %1 = alloca i32, align 4
  %2 = alloca i32*, align 8
  %3 = alloca i32*, align 8
  %4 = alloca i32, align 4
  %5 = alloca i32, align 4
  %6 = alloca i32, align 4
  %7 = alloca i32, align 4
  %8 = alloca i32, align 4
  %9 = alloca i32, align 4
  %10 = alloca i32, align 4
  store i32 0, i32* %1, align 4
  %11 = call noalias i8* @malloc(i64 4194304) #3
  %12 = bitcast i8* %11 to i32*
  store i32* %12, i32** %2, align 8
  %13 = call noalias i8* @malloc(i64 4186116) #3
  %14 = bitcast i8* %13 to i32*
  store i32* %14, i32** %3, align 8
  store i32 1, i32* %4, align 4
  store i32 -1, i32* %5, align 4
  store i32 -1, i32* %6, align 4
  store i32 1, i32* %7, align 4
  %15 = call i64 @time(i64* null) #3
  %16 = trunc i64 %15 to i32
  call void @srand(i32 %16) #3
  store i32 0, i32* %8, align 4
  br label %17

; <label>:17:                                     ; preds = %26, %0
  %18 = load i32, i32* %8, align 4
  %19 = icmp slt i32 %18, 1048576
  br i1 %19, label %20, label %29

; <label>:20:                                     ; preds = %17
  %21 = call i32 @rand() #3
  %22 = load i32*, i32** %2, align 8
  %23 = load i32, i32* %8, align 4
  %24 = sext i32 %23 to i64
  %25 = getelementptr inbounds i32, i32* %22, i64 %24
  store i32 %21, i32* %25, align 4
  br label %26

; <label>:26:                                     ; preds = %20
  %27 = load i32, i32* %8, align 4
  %28 = add nsw i32 %27, 1
  store i32 %28, i32* %8, align 4
  br label %17

; <label>:29:                                     ; preds = %17
  store i32 0, i32* %9, align 4
  br label %30

; <label>:30:                                     ; preds = %96, %29
  %31 = load i32, i32* %9, align 4
  %32 = icmp slt i32 %31, 1023
  br i1 %32, label %33, label %99

; <label>:33:                                     ; preds = %30
  store i32 0, i32* %10, align 4
  br label %34

; <label>:34:                                     ; preds = %92, %33
  %35 = load i32, i32* %10, align 4
  %36 = icmp slt i32 %35, 1023
  br i1 %36, label %37, label %95

; <label>:37:                                     ; preds = %34
  %38 = load i32, i32* %4, align 4
  %39 = load i32*, i32** %2, align 8
  %40 = load i32, i32* %10, align 4
  %41 = load i32, i32* %9, align 4
  %42 = mul nsw i32 %41, 1024
  %43 = add nsw i32 %40, %42
  %44 = sext i32 %43 to i64
  %45 = getelementptr inbounds i32, i32* %39, i64 %44
  %46 = load i32, i32* %45, align 4
  %47 = mul nsw i32 %38, %46
  %48 = load i32, i32* %5, align 4
  %49 = load i32*, i32** %2, align 8
  %50 = load i32, i32* %10, align 4
  %51 = add nsw i32 %50, 1
  %52 = load i32, i32* %9, align 4
  %53 = mul nsw i32 %52, 1024
  %54 = add nsw i32 %51, %53
  %55 = sext i32 %54 to i64
  %56 = getelementptr inbounds i32, i32* %49, i64 %55
  %57 = load i32, i32* %56, align 4
  %58 = mul nsw i32 %48, %57
  %59 = add nsw i32 %47, %58
  %60 = load i32, i32* %6, align 4
  %61 = load i32*, i32** %2, align 8
  %62 = load i32, i32* %10, align 4
  %63 = load i32, i32* %9, align 4
  %64 = add nsw i32 %63, 1
  %65 = mul nsw i32 %64, 1024
  %66 = add nsw i32 %62, %65
  %67 = sext i32 %66 to i64
  %68 = getelementptr inbounds i32, i32* %61, i64 %67
  %69 = load i32, i32* %68, align 4
  %70 = mul nsw i32 %60, %69
  %71 = add nsw i32 %59, %70
  %72 = load i32, i32* %7, align 4
  %73 = load i32*, i32** %2, align 8
  %74 = load i32, i32* %10, align 4
  %75 = add nsw i32 %74, 1
  %76 = load i32, i32* %9, align 4
  %77 = add nsw i32 %76, 1
  %78 = mul nsw i32 %77, 1024
  %79 = add nsw i32 %75, %78
  %80 = sext i32 %79 to i64
  %81 = getelementptr inbounds i32, i32* %73, i64 %80
  %82 = load i32, i32* %81, align 4
  %83 = mul nsw i32 %72, %82
  %84 = add nsw i32 %71, %83
  %85 = load i32*, i32** %3, align 8
  %86 = load i32, i32* %9, align 4
  %87 = mul nsw i32 %86, 1023
  %88 = load i32, i32* %10, align 4
  %89 = add nsw i32 %87, %88
  %90 = sext i32 %89 to i64
  %91 = getelementptr inbounds i32, i32* %85, i64 %90
  store i32 %84, i32* %91, align 4
  br label %92

; <label>:92:                                     ; preds = %37
  %93 = load i32, i32* %10, align 4
  %94 = add nsw i32 %93, 1
  store i32 %94, i32* %10, align 4
  br label %34

; <label>:95:                                     ; preds = %34
  br label %96

; <label>:96:                                     ; preds = %95
  %97 = load i32, i32* %9, align 4
  %98 = add nsw i32 %97, 1
  store i32 %98, i32* %9, align 4
  br label %30

; <label>:99:                                     ; preds = %30
  %100 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([9 x i8], [9 x i8]* @.str, i32 0, i32 0))
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

attributes #0 = { noinline nounwind optnone uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { nounwind "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #2 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #3 = { nounwind }

!llvm.module.flags = !{!0, !1, !2}
!llvm.ident = !{!3}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 1, !"ThinLTO", i32 0}
!2 = !{i32 1, !"EnableSplitLTOUnit", i32 0}
!3 = !{!"clang version 8.0.1-svn363027-1~exp1~20190611211629.77 (branches/release_80)"}

^0 = module: (path: "2DConv.o", hash: (0, 0, 0, 0, 0))
^1 = gv: (name: "malloc") ; guid = 2336192559129972258
^2 = gv: (name: "time") ; guid = 3946912059654523911
^3 = gv: (name: "rand") ; guid = 7206085285791387956
^4 = gv: (name: "printf") ; guid = 7383291119112528047
^5 = gv: (name: "main", summaries: (function: (module: ^0, flags: (linkage: external, notEligibleToImport: 1, live: 0, dsoLocal: 1), insts: 117, calls: ((callee: ^1), (callee: ^2), (callee: ^6), (callee: ^3), (callee: ^4)), refs: (^7)))) ; guid = 15822663052811949562
^6 = gv: (name: "srand") ; guid = 16361127236386863736
^7 = gv: (name: ".str", summaries: (variable: (module: ^0, flags: (linkage: private, notEligibleToImport: 1, live: 0, dsoLocal: 1), varFlags: (readonly: 1)))) ; guid = 18137602417015618666
