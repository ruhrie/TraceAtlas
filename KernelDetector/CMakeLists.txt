file(GLOB SOURCES "*.cpp")
add_executable(KernelParser ${SOURCES})

set_target_properties(KernelParser
	PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin" 
)
target_link_libraries(KernelParser PRIVATE ${ZLIB_LIBRARY} ${llvm_libs} nlohmann_json nlohmann_json::nlohmann_json)
target_include_directories(KernelParser PRIVATE ${LLVM_INCLUDE_DIRS} )
