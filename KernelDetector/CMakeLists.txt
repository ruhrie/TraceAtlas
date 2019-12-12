file(GLOB SOURCES "*.cpp")
cmake_minimum_required(VERSION 3.10)
add_executable(KernelParser ${SOURCES})

set_target_properties(KernelParser
	PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin" 
)
target_include_directories(KernelParser PRIVATE ${LLVM_INCLUDE_DIRS} "include/" ${DEP_INC} )
target_link_libraries(KernelParser PRIVATE ${ZLIB_LIBRARY})
