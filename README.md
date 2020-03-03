# TraceAtlas

TraceAtlas is a tool that generates a dynamic application trace from source code and extracts kernels from it. There is a paper currently under review by TPDS that goes into depth upon this process. Its title is "Automated Parallel Kernel Extraction from Dynamic Application Traces." It is available at [arxiv](https://arxiv.org/abs/2001.09995). If you use this work in your research please cite this paper.

Tik is a tool included within TraceAtlas that converts the detected kernels into executable code. It will be included in a future publication.

![Unit Tests](https://github.com/ruhrie/TraceAtlas/workflows/Unit%20Tests/badge.svg)

## Building

TraceAtlas requires a few of libraries:
* [LLVM-9](https://llvm.org/)
* [papi](https://icl.utk.edu/papi/)
* [nlohmann-json](https://github.com/nlohmann/json)
* [zlib](https://www.zlib.net/)
* [spdlog](https://github.com/gabime/spdlog)

The json library, spdlog, and indicators are expected to be installed via [vcpkg](https://github.com/Microsoft/vcpkg) which is available as a submodule of the repo. 

To build, simply create a build directory and run cmake (at least 3.10) against it with your build tool of choice. A couple of small unit tests are created. These can be disabled with the `ENABLE_TESTING` option. Note that they do not work properly on the release version due to optimization of the inputs. Doxygen documentation is also available under the `doc` target. It is not built by default.

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

Tik is a work in progress to extract kernels from the source code. It currently has provisional support for simple kernels, but more complex structures are still a work in progress. The current limitations are:

* No multithreading
* No exception handling
* No phi nodes at the beginning of a block that depend on multiple kernel entrances

The status of tik is written to the log. Info indicates a successful action, a warning is something that is likely failing to execute properly, an error is an unrecoverable kernel error, and critical occurs when the generated tik module in invalid.

Currently the performance of tik is lower than desired, but no accelerations have occured yet and are simply a copying of the source code with an additional overhead injected by us to simplify analysis.

## Utilities

Various utilities are available as binaries. Feel free to use them, but they were written to solve a particular problem and are probably not useful to you.