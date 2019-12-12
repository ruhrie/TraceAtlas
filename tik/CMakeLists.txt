file(GLOB SOURCES "*.cpp")
cmake_minimum_required(VERSION 3.10)
add_executable(tik ${SOURCES})

set_target_properties(tik
	PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin" 
)

target_link_libraries(tik PRIVATE ${llvm_libs} nlohmann_json nlohmann_json::nlohmann_json)
target_compile_definitions(tik PRIVATE ${LLVM_DEFINITIONS})
target_include_directories(tik PRIVATE ${LLVM_INCLUDE_DIRS} "include/")

if(${ENABLE_TESTING})
    add_subdirectory(tests)
endif()