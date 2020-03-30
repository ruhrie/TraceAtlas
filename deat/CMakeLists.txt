file(GLOB SOURCES "*.cpp")
add_executable(deat ${SOURCES})

set_target_properties(deat
	PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin" 
)

target_link_libraries(deat PRIVATE ${llvm_libs} nlohmann_json nlohmann_json::nlohmann_json spdlog::spdlog spdlog::spdlog_header_only AtlasUtil)
target_compile_definitions(deat PRIVATE ${LLVM_DEFINITIONS})

#install(TARGETS deat RUNTIME DESTINATION bin)