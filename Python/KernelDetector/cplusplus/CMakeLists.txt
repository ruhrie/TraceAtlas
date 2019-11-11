cmake_minimum_required(VERSION 3.10)
project(DashTracer)
find_package(ZLIB REQUIRED)
SET (CMAKE_CXX_STANDARD 17)


add_executable(EncodeDetect "EncodeDetect.cpp" "main.cpp")
target_link_libraries(EncodeDetect PRIVATE ${ZLIB_LIBRARY})
