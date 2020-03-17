file(GLOB SOURCES "*.cpp")
add_executable(cartographer ${SOURCES})

set_target_properties(cartographer
	PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin" 
)
target_link_libraries(cartographer PRIVATE ${ZLIB_LIBRARY} ${llvm_libs} nlohmann_json nlohmann_json::nlohmann_json AtlasUtil spdlog::spdlog spdlog::spdlog_header_only)
target_include_directories(cartographer PRIVATE ${LLVM_INCLUDE_DIRS} "include/")

install(TARGETS cartographer RUNTIME DESTINATION bin)
