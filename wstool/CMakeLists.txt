file(GLOB SOURCES "*.cpp")
add_executable(wstool ${SOURCES})

set_target_properties(wstool
	PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin" 
)
target_link_libraries(wstool PRIVATE ${ZLIB_LIBRARY} ${llvm_libs} nlohmann_json nlohmann_json::nlohmann_json AtlasUtil spdlog::spdlog spdlog::spdlog_header_only)
target_include_directories(wstool SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_include_directories(wstool PRIVATE "include/")
target_compile_options(wstool PRIVATE -Wall -Wextra -Wconversion -Werror)
install(TARGETS wstool RUNTIME DESTINATION bin)
