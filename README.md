# TraceAtlas

## Overview

TraceAtlas is a tool that generates a dynamic application trace from source code and extracts kernels from it. There is a paper currently under review by TPDS that goes into depth upon this process. Its title is "Automated Parallel Kernel Extraction from Dynamic Application Traces."

## Building

TraceAtlas requires a few of libraries:
* [LLVM-9](https://llvm.org/)
* [papi](https://icl.utk.edu/papi/)
* [nlohmann-json](https://github.com/nlohmann/json)
* [zlib](https://www.zlib.net/)

The json library is currently expected to be installed via [vcpkg](https://github.com/Microsoft/vcpkg) and a future update will also move LLVM to vcpkg.

To build, simply create a build directory and run cmake (at least 3.10) against it with your build tool of choice. A couple of small unit tests are created for the tik tool, which is still in heavy development. These can be disabled with the `ENABLE_TESTING` option. Doxygen documentation is also available under the `doc` target. It is not built by default.

## Tracing

Generating a trace occurs as part of an LLVM pass. There are a few steps:

1. Compile to bitcode: `clang -fuse-ld=lld -Wl,--plugin-opt=emit-llvm input.c -o output.bc`
2. Inject our tracer: `opt -load {PATH_TO_ATLASPASSES} output.bc -o opt.bc -EncodedTrace`
3. Compile to binary: `clang++ -fuse-ld=lld -lz -lpapi -lpthread opt.bc -o result.native {PATH_TO_LIBATLASBACKEND}`
4. Run your executable: `./result.native`

It can only trace code that is compiled into bitcode. Shared library code will not be traced and should be linked in step 3. There are two environment variables that are read when generating the trace. The first is `TRACE_NAME` and it will specify the name of the resultant trace. It defaults to `raw.trc`. The second is `TRACE_COMPRESSION` and controls how hard zlib will work to compress the trace. It defaults to 9 (max compression). This trace is then analyzed by cartographer.

## cartographer

Cartographer is our trace analysis tool. To detect kernels simply call it with the input trace file specified by `-i` and the result by `-k`. The probability threshold can be specified by `-t` and the hotcode floor by `-ht`. The result is a dictionary containing kernels and basic block IDs. These IDs can be compared to the source code by running `opt -load {PATH_TO_ATLASPASSES} output.bc -o opt.ll -EncodedAnnotate -S` and looking at the source.

## tik

Tik is a work in progress to extract kernels from the source code. It is not ready for major use. It currently is being developed against function call code. The current requirements are:
* Single depth function calls
* Single entrance kernels
* Single exit kernels
* No multithreading
* No exception handling

## Utilities

Various utilities are available as binaries. Feel free to use them, but they were written to solve a particular problem and are probably not useful to you.