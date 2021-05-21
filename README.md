# TraceAtlas

TraceAtlas is a program analysis toolchain. It uses the LLVM api to profile programs dynamically and extract coarse-grained concurrent tasks automatically. 

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

To build, simply create a build directory and run cmake (at least 3.13) against it with your build tool of choice. A couple of small unit tests are created. These can be disabled with the `ENABLE_TESTING` option. Doxygen documentation is also available under the `doc` target. It is not built by default.

## Profiling

Generating a profile occurs as part of an LLVM pass. There are a few steps:

1. Compile to bitcode: `clang -flto -fuse-ld=lld -Wl,--plugin-opt=emit-llvm $(ARCHIVES) input.c -o output.bc`
2. Inject our profiler: `opt -load {PATH_TO_ATLASPASSES} -Markov output.bc -o opt.bc`
3. Compile to binary: `clang++ -fuse-ld=lld -lz -lpapi -lpthread $(SHARED_OBJECTS) {$PATH_TO_ATLASBACKEND} opt.bc -o profile.native`
4. Run your executable: `MARKOV_NAME=profile.bin BLOCK_NAME=BlockInfo_profile.json ./profile.native`
5. Segment the program: `./install/bin/newCartographer -i profile.bin -bi BlockInfo_profile.json -o kernel_profile.json`

$(ARCHIVES) should be a variable that contains all static LLVM bitcode libraries your application can link against. This step contains all code that will be profiled i.e. the profiler only observes LLVM IR bitcode. $(SHARED_OBJECTS) enumerates all dynamic links that are required by the target program (for example, any dependencies that are not available in LLVM IR). There are two output files from the resulting executable: `MARKOV_NAME` which specifies the name of the resultant profile (defaults to `markov.bin`) and `BLOCK_FILE` which specifies the Json output file (contains information about the profile). These two output files feed the cartographer.

## cartographer

Cartographer is our program segmenter. It exploits cycles within the control flow to parse an input profile into its concurrent tasks. We define a kernel to be a concurrent task. Call cartographer with the input profile specified by `-i`, the input BlockInfo.json file with `-bi` and the output kernel file with `-o`. The result is a dictionary containing information about the profile of the program, the kernels within the program, and statistics about those kernels.

## tik

Tik is a work in progress to extract kernels from the source code. It currently has provisional support for simple kernels, but more complex structures are not supported. The current limitations are:

* No multithreading
* No exception handling
* No phi nodes at the beginning of a block that depend on multiple kernel entrances
