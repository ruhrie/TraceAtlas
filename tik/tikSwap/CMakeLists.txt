add_executable(tikSwap tikSwap.cpp)
set_target_properties(tikSwap
	PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin" 
)
target_link_libraries(tikSwap PRIVATE libtik ${llvm_libs} spdlog::spdlog spdlog::spdlog_header_only AtlasUtil)
target_compile_definitions(tikSwap PRIVATE ${LLVM_DEFINITIONS})
target_include_directories(tikSwap SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_include_directories(tikSwap PRIVATE "include/" )
if(WIN32)
    target_compile_options(tikSwap PRIVATE -W3 -Wextra -Wconversion)
else()
    target_compile_options(tikSwap PRIVATE -Wall -Wextra -Wconversion)
endif()
if(${CMAKE_BUILD_TYPE} MATCHES Debug)
    target_compile_definitions(tikSwap PRIVATE DEBUG)
endif()
install(TARGETS tikSwap RUNTIME DESTINATION bin)
