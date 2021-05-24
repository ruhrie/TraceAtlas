# TraceAtlas

TraceAtlas is a program analysis toolchain. It uses the LLVM api to profile programs dynamically and extract coarse-grained concurrent tasks automatically. 

![Unit Tests](https://github.com/ruhrie/TraceAtlas/workflows/Unit%20Tests/badge.svg)

## Building

TraceAtlas requires a few of libraries:
* [LLVM-9](https://llvm.org/)
* [papi](https://icl.utk.edu/papi/)
* [nlohmann-json](https://github.com/nlohmann/json)
* [zlib](https://www.zlib.net/)
* [spdlog](https://github.com/gabime/spdlog)

To build, simply create a build directory and run cmake (at least 3.13) with your build tool of choice. Then, test your install with the `test` target. Doxygen documentation is also available under the `doc` target (it is not built by default).

## Profiling & Cartographer

The profiling tool and cartographer are the main tools within TraceAtlas. To use them, follow these steps:

1. Compile to bitcode: `clang -flto -fuse-ld=lld -Wl,--plugin-opt=emit-llvm $(ARCHIVES) input.c -o output.bc`
2. Inject our profiler: `opt -load {PATH_TO_ATLASPASSES} -Markov output.bc -o opt.bc`
3. Compile to binary: `clang++ -fuse-ld=lld -lz -lpapi -lpthread $(SHARED_OBJECTS) {$PATH_TO_ATLASBACKEND} opt.bc -o profile.native`
4. Run your executable: `MARKOV_FILE=profile.bin BLOCK_FILE=BlockInfo_profile.json ./profile.native`
5. Segment the program: `./install/bin/newCartographer -i profile.bin -bi BlockInfo_profile.json -o kernel_profile.json`

$(ARCHIVES) should be a variable that contains all static LLVM bitcode libraries your application can link against. This step contains all code that will be profiled i.e. the profiler only observes LLVM IR bitcode. $(SHARED_OBJECTS) enumerates all dynamic links that are required by the target program (for example, any dependencies that are not available in LLVM IR). There are two output files from the resulting executable: `MARKOV_FILE` which specifies the name of the resultant profile (default is `markov.bin`) and `BLOCK_FILE` which specifies the Json output file (contains information about the profile, default is `BlockInfo.json`). These two output files feed the cartographer.

Cartographer (step 5) is our program segmenter. It exploits cycles within the control flow to parse an input profile into its concurrent tasks. We define a kernel to be a concurrent task. Call cartographer with the input profile specified by `-i`, the input BlockInfo.json file with `-bi` and the output kernel file with `-o`. The result is a dictionary containing information about the profile of the program, the kernels within the program, and statistics about those kernels.

## Known profiler bug
There is a memory problem with the profiling tool when allocating memory for its backend data structures. It has only been observed when profiling programs that use OpenCV and Microsoft SEAL. This error can manifest as
* a segfault before the main() function occurs
* a segfault after main returns

Both types should occur when calling constructors/destructors on C++ STL containers (specifically been observed with std::map, std::unordered_map, std::vector). Please submit an issue if you ever encounter this bug yourself with as much information as your debugger can give you.
