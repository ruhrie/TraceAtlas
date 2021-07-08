# TraceAtlas

TraceAtlas is a program analysis toolchain. It uses the LLVM api to profile programs dynamically and segment the coarse-grained tasks of the program automatically. 

![Unit Tests](https://github.com/ruhrie/TraceAtlas/workflows/Unit%20Tests/badge.svg)

## Building

TraceAtlas requires a few of libraries:
* [LLVM-9](https://llvm.org/)
* [papi](https://icl.utk.edu/papi/)
* [nlohmann-json](https://github.com/nlohmann/json)
* [zlib](https://www.zlib.net/)
* [spdlog](https://github.com/gabime/spdlog)

To build, simply create a build directory and run cmake (at least 3.13). Test your install with the `test` target. Doxygen documentation is also available under the `doc` target (it is not built by default).

## Profiling & Cartographer

The profiling tool and cartographer are the main tools within TraceAtlas. To use them, follow these steps:

1. Compile to bitcode: `clang -flto -fuse-ld=lld -Wl,--plugin-opt=emit-llvm $(ARCHIVES) input.c -o input.bc`
2. Inject our profiler: `opt -load {PATH_TO_TRACEATLAS_INSTALL}lib/AtlasPasses.so -Markov input.bc -o opt.bc`
3. Compile to binary: `clang++ -fuse-ld=lld -lz -lpapi -lpthread $(SHARED_OBJECTS) {$PATH_TO_TRACEATLAS_INSTALL}lib/libAtlasBackend.a opt.bc -o input.profile`
4. Profile your program: `MARKOV_FILE=profile.bin BLOCK_FILE=BlockInfo_profile.json ./input.profile`
5. Segment the program: `{PATH_TO_TRACEATLAS_INSTALL}bin/newCartographer -i profile.bin -bi BlockInfo_profile.json -b input.bc -o kernel_input.json`

$(ARCHIVES) should be a variable that contains all static LLVM bitcode libraries your application can link against. This step contains all code that will be profiled i.e. the profiler only observes LLVM IR bitcode. $(SHARED_OBJECTS) enumerates all dynamic links that are required by the target program (for example, any dependencies that are not available in LLVM IR). There are two output files from the resulting executable: `MARKOV_FILE` which specifies the name of the resultant profile (default is `markov.bin`) and `BLOCK_FILE` which specifies the Json output file (contains information about the profile, default is `BlockInfo.json`). These two output files feed the cartographer.

Cartographer (step 5) is our program segmenter. It exploits cycles within the control flow to structure an input profile into its concurrent tasks. We define a kernel to be a cycle that has the highest probability of continuing to cycle. Call cartographer with the input profile specified by `-i`, the input BlockInfo.json file with `-bi`, the input LLVM IR bitcode file with `-b` and the output kernel file with `-o`. Use `-h` for the optional flags. The otuput kernel file is a dictionary containing information about the profile of the program, the kernels within the program, and statistics about those kernels.