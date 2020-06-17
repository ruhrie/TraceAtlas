file(GLOB SOURCES "*.cpp")
add_executable(cartographer ${SOURCES})

set_target_properties(cartographer
	PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin" 
)
target_link_libraries(cartographer PRIVATE ${ZLIB_LIBRARY} ${llvm_libs} nlohmann_json nlohmann_json::nlohmann_json AtlasUtil spdlog::spdlog spdlog::spdlog_header_only libtik)
target_include_directories(cartographer PRIVATE "include/")
target_include_directories(cartographer SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
if(WIN32)
	target_compile_options(cartographer PRIVATE -W3 -Wextra -Wconversion)
else()
	target_compile_options(cartographer PRIVATE -Wall -Wextra -Wconversion)
endif()


install(TARGETS cartographer RUNTIME DESTINATION bin)
