; ModuleID = '/home/ruhrie/TraceAtlas/tik/TestApplications/1DBlur/1DBlur.ll'
source_filename = "/home/ruhrie/TraceAtlas/tik/TestApplications/1DBlur/1DBlur.ll"

@0 = global i32** null
@1 = global i32* null
@2 = global i32* null
@3 = global i32** null
@4 = global i32** null
@5 = global i32* null
@6 = global i32* null
@7 = global i32* null
@8 = global i32* null
@9 = global i32* null

define i32 @MemoryRead(i32) {
entry:
  %1 = load i32**, i32*** @0
  %2 = ptrtoint i32** %1 to i32
  %3 = load i32*, i32** @1
  %4 = ptrtoint i32* %3 to i32
  %5 = icmp eq i32 %0, 1
  %6 = select i1 %5, i32 %4, i32 %2
  ret i32 %6
}

define i32 @MemoryWrite(i32) {
entry:
  %1 = load i32*, i32** @1
  %2 = ptrtoint i32* %1 to i32
  %3 = load i32*, i32** @2
  %4 = ptrtoint i32* %3 to i32
  %5 = icmp eq i32 %0, 1
  %6 = select i1 %5, i32 %4, i32 %2
  ret i32 %6
}

define i32 @Kernel_0_Reformatted(i32**, i32*) {
Init:
  store i32* %1, i32** @1
  store i32** %0, i32*** @0
  br label %Loop

Body:                                             ; preds = %Loop
  %2 = call i32 @rand() #0
  %3 = call i32 @MemoryRead(i32 0)
  %4 = inttoptr i32 %3 to i32**
  %5 = load i32*, i32** %4
  %6 = call i32 @MemoryRead(i32 1)
  %7 = inttoptr i32 %6 to i32*
  %8 = load i32, i32* %7
  %9 = sext i32 %6 to i64
  %10 = getelementptr inbounds i32, i32* %3, i64 %7
  store i32* %10, i32** @2
  %11 = call i32 @MemoryWrite(i32 1)
  %12 = inttoptr i32 %11 to i32*
  store i32 %2, i32* %12
  %13 = call i32 @MemoryRead(i32 1)
  %14 = inttoptr i32 %13 to i32*
  %15 = load i32, i32* %14
  %16 = add nsw i32 %13, 1
  %17 = call i32 @MemoryWrite(i32 0)
  %18 = inttoptr i32 %17 to i32*
  store i32 %16, i32* %18
  br label %Loop

Exit:                                             ; preds = %Loop
  ret void

Loop:                                             ; preds = %Body, %Init
  %19 = load i32, i32* <badref>, align 4
  %20 = icmp slt i32 %17, 1024
  br i1 %18, label %Body, label %Exit
}

define i32 @MemoryRead.1(i32) {
entry:
  %1 = load i32**, i32*** @3
  %2 = ptrtoint i32** %1 to i32
  %3 = load i32**, i32*** @4
  %4 = ptrtoint i32** %3 to i32
  %5 = icmp eq i32 %0, 1
  %6 = select i1 %5, i32 %4, i32 %2
  %7 = load i32*, i32** @5
  %8 = ptrtoint i32* %7 to i32
  %9 = icmp eq i32 %0, 2
  %10 = select i1 %9, i32 %8, i32 %6
  %11 = load i32*, i32** @6
  %12 = ptrtoint i32* %11 to i32
  %13 = icmp eq i32 %0, 3
  %14 = select i1 %13, i32 %12, i32 %10
  %15 = load i32*, i32** @7
  %16 = ptrtoint i32* %15 to i32
  %17 = icmp eq i32 %0, 4
  %18 = select i1 %17, i32 %16, i32 %14
  %19 = load i32*, i32** @8
  %20 = ptrtoint i32* %19 to i32
  %21 = icmp eq i32 %0, 5
  %22 = select i1 %21, i32 %20, i32 %18
  ret i32 %22
}

define i32 @MemoryWrite.2(i32) {
entry:
  %1 = load i32*, i32** @5
  %2 = ptrtoint i32* %1 to i32
  %3 = load i32*, i32** @9
  %4 = ptrtoint i32* %3 to i32
  %5 = icmp eq i32 %0, 1
  %6 = select i1 %5, i32 %4, i32 %2
  ret i32 %6
}

define i32 @Kernel_1_Reformatted(i32**, i32*, i32**) {
Init:
  store i32** %0, i32*** @3
  store i32** %2, i32*** @4
  store i32* %1, i32** @5
  br label %Loop

Body:                                             ; preds = %Loop
  %3 = call i32 @MemoryRead.1(i32 0)
  %4 = inttoptr i32 %3 to i32**
  %5 = load i32*, i32** %4
  %6 = call i32 @MemoryRead.1(i32 2)
  %7 = inttoptr i32 %6 to i32*
  %8 = load i32, i32* %7
  %9 = sub nsw i32 %5, 1
  %10 = sext i32 %6 to i64
  %11 = getelementptr inbounds i32, i32* %2, i64 %7
  store i32* %11, i32** @6
  %12 = call i32 @MemoryRead.1(i32 3)
  %13 = inttoptr i32 %12 to i32*
  %14 = load i32, i32* %13
  %15 = call i32 @MemoryRead.1(i32 0)
  %16 = inttoptr i32 %15 to i32**
  %17 = load i32*, i32** %16
  %18 = call i32 @MemoryRead.1(i32 2)
  %19 = inttoptr i32 %18 to i32*
  %20 = load i32, i32* %19
  %21 = sext i32 %17 to i64
  %22 = getelementptr inbounds i32, i32* %14, i64 %18
  store i32* %22, i32** @7
  %23 = call i32 @MemoryRead.1(i32 4)
  %24 = inttoptr i32 %23 to i32*
  %25 = load i32, i32* %24
  %26 = add nsw i32 %11, %22
  %27 = call i32 @MemoryRead.1(i32 0)
  %28 = inttoptr i32 %27 to i32**
  %29 = load i32*, i32** %28
  %30 = call i32 @MemoryRead.1(i32 2)
  %31 = inttoptr i32 %30 to i32*
  %32 = load i32, i32* %31
  %33 = add nsw i32 %29, 1
  %34 = sext i32 %30 to i64
  %35 = getelementptr inbounds i32, i32* %26, i64 %31
  store i32* %35, i32** @8
  %36 = call i32 @MemoryRead.1(i32 5)
  %37 = inttoptr i32 %36 to i32*
  %38 = load i32, i32* %37
  %39 = add nsw i32 %23, %35
  %40 = call i32 @MemoryRead.1(i32 1)
  %41 = inttoptr i32 %40 to i32**
  %42 = load i32*, i32** %41
  %43 = call i32 @MemoryRead.1(i32 2)
  %44 = inttoptr i32 %43 to i32*
  %45 = load i32, i32* %44
  %46 = sub nsw i32 %42, 1
  %47 = sext i32 %43 to i64
  %48 = getelementptr inbounds i32, i32* %39, i64 %44
  store i32* %48, i32** @9
  %49 = call i32 @MemoryWrite.2(i32 1)
  %50 = inttoptr i32 %49 to i32*
  store i32 %39, i32* %50
  %51 = call i32 @MemoryRead.1(i32 2)
  %52 = inttoptr i32 %51 to i32*
  %53 = load i32, i32* %52
  %54 = add nsw i32 %50, 1
  %55 = call i32 @MemoryWrite.2(i32 0)
  %56 = inttoptr i32 %55 to i32*
  store i32 %54, i32* %56
  br label %Loop

Exit:                                             ; preds = %Loop
  ret void

Loop:                                             ; preds = %Body, %Init
  %57 = load i32, i32* <badref>, align 4
  %58 = icmp slt i32 %54, 1023
  br i1 %55, label %Body, label %Exit
}

; Function Attrs: nounwind
declare i32 @rand() #0

attributes #0 = { nounwind }
