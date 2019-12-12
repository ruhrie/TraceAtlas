cmake_minimum_required(VERSION 3.5)
project(TraceAtlas VERSION 0.1.0)

SET (CMAKE_CXX_STANDARD 17)

find_package(LLVM 9 REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
llvm_map_components_to_libnames(llvm_libs  support core irreader bitreader bitwriter transformutils)

find_package(ZLIB REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
option(ENABLE_TESTING "Build tests" ON)
if(${ENABLE_TESTING})
    enable_testing()
endif()

add_subdirectory("Dependencies")
add_subdirectory("TraceInfrastructure")
add_subdirectory("Utilities")
add_subdirectory("tik")
