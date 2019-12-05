cmake_minimum_required(VERSION 3.10)
project(DashTracer)
find_package(ZLIB REQUIRED)
SET (CMAKE_CXX_STANDARD 17)


add_executable(KernelParser "EncodeDetect.cpp" "EncodeExtract.cpp" "main.cpp")
target_link_libraries(KernelParser PRIVATE ${ZLIB_LIBRARY})
