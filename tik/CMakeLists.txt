file(GLOB SOURCES "*.cpp")
cmake_minimum_required(VERSION 3.10)
add_executable(tik ${SOURCES})

set_target_properties(tik
	PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin" 
)

target_link_libraries(tik PRIVATE ${llvm_libs} nlohmann_json nlohmann_json::nlohmann_json spdlog::spdlog spdlog::spdlog_header_only AtlasUtil)
target_compile_definitions(tik PRIVATE ${LLVM_DEFINITIONS})
target_include_directories(tik PRIVATE ${LLVM_INCLUDE_DIRS} "include/")

install(TARGETS tik RUNTIME DESTINATION bin)

if(${ENABLE_TESTING})
    add_subdirectory(tests)
endif()

set (TIK_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/include" PARENT_SCOPE)