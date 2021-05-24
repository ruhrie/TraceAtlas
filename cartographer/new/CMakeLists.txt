if(${CMAKE_BUILD_TYPE} STREQUAL "DEBUG")
	message(STATUS "Setting DEBUG macro")
	add_compile_definitions(DEBUG)
elseif(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
	message(STATUS "Setting DEBUG macro")
	add_compile_definitions(DEBUG)
endif()

file(GLOB SOURCES "*.cpp")
add_executable(newCartographer ${SOURCES})

set_target_properties(newCartographer
	PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin" 
)
target_link_libraries(newCartographer PRIVATE ${ZLIB_LIBRARY} ${llvm_libs} nlohmann_json nlohmann_json::nlohmann_json AtlasUtil spdlog::spdlog spdlog::spdlog_header_only)
target_include_directories(newCartographer PRIVATE "inc")
target_include_directories(newCartographer SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
if(WIN32)
	target_compile_options(newCartographer PRIVATE -W3 -Wextra -Wconversion)
else()
	target_compile_options(newCartographer PRIVATE -Wall -Wextra -Wconversion)
endif()


install(TARGETS newCartographer RUNTIME DESTINATION bin)
