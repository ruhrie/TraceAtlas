cmake_minimum_required(VERSION 3.10)
project(TraceAtlas VERSION 0.1.0)

SET (CMAKE_CXX_STANDARD 17)

find_package(LLVM 9 REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
llvm_map_components_to_libnames(llvm_libs  support core irreader bitreader bitwriter transformutils)

find_package(ZLIB REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
option(ENABLE_TESTING "Build tests" ON)
if(${ENABLE_TESTING})
    enable_testing()
endif()

add_subdirectory("cartographer")
add_subdirectory("TraceInfrastructure")
add_subdirectory("tik")
add_subdirectory("Utilities")

find_package(Doxygen)
if(DOXYGEN_FOUND)
    configure_file(DoxygenLayout.xml ${CMAKE_CURRENT_BINARY_DIR}/DoxygenLayout.xml @ONLY)
    configure_file(Doxyfile ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
    add_custom_target(doc COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
endif(DOXYGEN_FOUND)
