; ModuleID = '/mnt/nobackup-11/bwilli46/TraceAtlas/tik/TestApplications/1DBlur/1DBlur.ll'
source_filename = "/mnt/nobackup-11/bwilli46/TraceAtlas/tik/TestApplications/1DBlur/1DBlur.ll"

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

define void @Kernel_0() {
Init:
  %0 = alloca i32*, align 8
  %1 = alloca i32, align 4
  br label %Loop

Body:                                             ; preds = %Loop, %Loop
  %2 = call i32 @rand() #0
  %3 = call i32 @MemoryRead(i32 0)
  %4 = inttoptr i32 %3 to i32**
  %5 = load i32*, i32** %4
  %6 = call i32 @MemoryRead(i32 1)
  %7 = inttoptr i32 %6 to i32*
  %8 = load i32, i32* %7
  %9 = sext i32 %15 to i64
  %10 = getelementptr inbounds i32, i32* %14, i64 %16
  store i32* %10, i32** @2
  %11 = call i32 @MemoryWrite(i32 1)
  %12 = inttoptr i32 %11 to i32*
  store i32 %2, i32* %12
  %13 = call i32 @MemoryRead(i32 1)
  %14 = inttoptr i32 %13 to i32*
  %15 = load i32, i32* %14
  %16 = add nsw i32 %18, 1
  %17 = call i32 @MemoryWrite(i32 0)
  %18 = inttoptr i32 %17 to i32*
  store i32 %16, i32* %18
  br label %Loop

Exit:                                             ; preds = %Loop, %Loop
  ret void

Loop:                                             ; preds = %Body, %Init, %Body, %Init
  %19 = load i32, i32* %3, align 4
  %20 = icmp slt i32 %11, 1024
  br i1 %12, label %Body, label %Exit
}

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
  %2 = alloca i32*, align 8
  %3 = alloca i32, align 4
  br label %Loop

Body:                                             ; No predecessors!
  %4 = call i32 @rand() #0
  %5 = call i32 @MemoryRead(i32 0)
  %6 = inttoptr i32 %5 to i32**
  %7 = load i32*, i32** %6
  %8 = call i32 @MemoryRead(i32 1)
  %9 = inttoptr i32 %8 to i32*
  %10 = load i32, i32* %9
  %11 = sext i32 %8 to i64
  %12 = getelementptr inbounds i32, i32* %5, i64 %9
  store i32* %12, i32** @2
  %13 = call i32 @MemoryWrite(i32 1)
  %14 = inttoptr i32 %13 to i32*
  store i32 %4, i32* %14
  %15 = call i32 @MemoryRead(i32 1)
  %16 = inttoptr i32 %15 to i32*
  %17 = load i32, i32* %16
  %18 = add nsw i32 %15, 1
  %19 = call i32 @MemoryWrite(i32 0)
  %20 = inttoptr i32 %19 to i32*
  store i32 %18, i32* %20
  br label %Loop

Exit:                                             ; No predecessors!
  ret void

Loop:                                             ; No predecessors!
  %21 = load i32, i32* %1, align 4
  %22 = icmp slt i32 %19, 1024
  br i1 %20, label %Body, label %Exit
}

define void @Kernel_1() {
Init:
  %0 = alloca i32*, align 8
  %1 = alloca i32, align 4
  %2 = alloca i32*, align 8
  br label %Loop

Body:                                             ; preds = %Loop, %Loop
  %3 = call i32 @MemoryRead.1(i32 0)
  %4 = inttoptr i32 %3 to i32**
  %5 = load i32*, i32** %4
  %6 = call i32 @MemoryRead.1(i32 2)
  %7 = inttoptr i32 %6 to i32*
  %8 = load i32, i32* %7
  %9 = sub nsw i32 %23, 1
  %10 = sext i32 %24 to i64
  %11 = getelementptr inbounds i32, i32* %22, i64 %25
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
  %21 = sext i32 %29 to i64
  %22 = getelementptr inbounds i32, i32* %28, i64 %30
  store i32* %22, i32** @7
  %23 = call i32 @MemoryRead.1(i32 4)
  %24 = inttoptr i32 %23 to i32*
  %25 = load i32, i32* %24
  %26 = add nsw i32 %27, %32
  %27 = call i32 @MemoryRead.1(i32 0)
  %28 = inttoptr i32 %27 to i32**
  %29 = load i32*, i32** %28
  %30 = call i32 @MemoryRead.1(i32 2)
  %31 = inttoptr i32 %30 to i32*
  %32 = load i32, i32* %31
  %33 = add nsw i32 %35, 1
  %34 = sext i32 %36 to i64
  %35 = getelementptr inbounds i32, i32* %34, i64 %37
  store i32* %35, i32** @8
  %36 = call i32 @MemoryRead.1(i32 5)
  %37 = inttoptr i32 %36 to i32*
  %38 = load i32, i32* %37
  %39 = add nsw i32 %33, %39
  %40 = call i32 @MemoryRead.1(i32 1)
  %41 = inttoptr i32 %40 to i32**
  %42 = load i32*, i32** %41
  %43 = call i32 @MemoryRead.1(i32 2)
  %44 = inttoptr i32 %43 to i32*
  %45 = load i32, i32* %44
  %46 = sub nsw i32 %42, 1
  %47 = sext i32 %43 to i64
  %48 = getelementptr inbounds i32, i32* %41, i64 %44
  store i32* %48, i32** @9
  %49 = call i32 @MemoryWrite.2(i32 1)
  %50 = inttoptr i32 %49 to i32*
  store i32 %39, i32* %50
  %51 = call i32 @MemoryRead.1(i32 2)
  %52 = inttoptr i32 %51 to i32*
  %53 = load i32, i32* %52
  %54 = add nsw i32 %46, 1
  %55 = call i32 @MemoryWrite.2(i32 0)
  %56 = inttoptr i32 %55 to i32*
  store i32 %54, i32* %56
  br label %Loop

Exit:                                             ; preds = %Loop, %Loop
  ret void

Loop:                                             ; preds = %Body, %Init, %Body, %Init
  %57 = load i32, i32* %4, align 4
  %58 = icmp slt i32 %20, 1023
  br i1 %21, label %Body, label %Exit
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
  %3 = alloca i32*, align 8
  %4 = alloca i32, align 4
  %5 = alloca i32*, align 8
  br label %Loop

Body:                                             ; No predecessors!
  %6 = call i32 @MemoryRead.1(i32 0)
  %7 = inttoptr i32 %6 to i32**
  %8 = load i32*, i32** %7
  %9 = call i32 @MemoryRead.1(i32 2)
  %10 = inttoptr i32 %9 to i32*
  %11 = load i32, i32* %10
  %12 = sub nsw i32 %8, 1
  %13 = sext i32 %9 to i64
  %14 = getelementptr inbounds i32, i32* %5, i64 %10
  store i32* %14, i32** @6
  %15 = call i32 @MemoryRead.1(i32 3)
  %16 = inttoptr i32 %15 to i32*
  %17 = load i32, i32* %16
  %18 = call i32 @MemoryRead.1(i32 0)
  %19 = inttoptr i32 %18 to i32**
  %20 = load i32*, i32** %19
  %21 = call i32 @MemoryRead.1(i32 2)
  %22 = inttoptr i32 %21 to i32*
  %23 = load i32, i32* %22
  %24 = sext i32 %20 to i64
  %25 = getelementptr inbounds i32, i32* %17, i64 %21
  store i32* %25, i32** @7
  %26 = call i32 @MemoryRead.1(i32 4)
  %27 = inttoptr i32 %26 to i32*
  %28 = load i32, i32* %27
  %29 = add nsw i32 %14, %25
  %30 = call i32 @MemoryRead.1(i32 0)
  %31 = inttoptr i32 %30 to i32**
  %32 = load i32*, i32** %31
  %33 = call i32 @MemoryRead.1(i32 2)
  %34 = inttoptr i32 %33 to i32*
  %35 = load i32, i32* %34
  %36 = add nsw i32 %32, 1
  %37 = sext i32 %33 to i64
  %38 = getelementptr inbounds i32, i32* %29, i64 %34
  store i32* %38, i32** @8
  %39 = call i32 @MemoryRead.1(i32 5)
  %40 = inttoptr i32 %39 to i32*
  %41 = load i32, i32* %40
  %42 = add nsw i32 %26, %38
  %43 = call i32 @MemoryRead.1(i32 1)
  %44 = inttoptr i32 %43 to i32**
  %45 = load i32*, i32** %44
  %46 = call i32 @MemoryRead.1(i32 2)
  %47 = inttoptr i32 %46 to i32*
  %48 = load i32, i32* %47
  %49 = sub nsw i32 %45, 1
  %50 = sext i32 %46 to i64
  %51 = getelementptr inbounds i32, i32* %42, i64 %47
  store i32* %51, i32** @9
  %52 = call i32 @MemoryWrite.2(i32 1)
  %53 = inttoptr i32 %52 to i32*
  store i32 %42, i32* %53
  %54 = call i32 @MemoryRead.1(i32 2)
  %55 = inttoptr i32 %54 to i32*
  %56 = load i32, i32* %55
  %57 = add nsw i32 %53, 1
  %58 = call i32 @MemoryWrite.2(i32 0)
  %59 = inttoptr i32 %58 to i32*
  store i32 %57, i32* %59
  br label %Loop

Exit:                                             ; No predecessors!
  ret void

Loop:                                             ; No predecessors!
  %60 = load i32, i32* %1, align 4
  %61 = icmp slt i32 %57, 1023
  br i1 %58, label %Body, label %Exit
}

; Function Attrs: nounwind
declare i32 @rand() #0

attributes #0 = { nounwind }
