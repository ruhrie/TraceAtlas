cmake_minimum_required(VERSION 3.10)
project(DashTracer)
find_package(ZLIB REQUIRED)
SET (CMAKE_CXX_STANDARD 17)

find_package(LLVM 8 REQUIRED CONFIG)
add_executable(KernelParser "EncodeDetect.cpp" "EncodeExtract.cpp" "main.cpp")

target_include_directories(KernelParser PRIVATE {DEP_INC} )
target_link_libraries(KernelParser PRIVATE ${ZLIB_LIBRARY})
