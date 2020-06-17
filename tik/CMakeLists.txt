add_subdirectory("lib")
add_executable(tik tik.cpp)
set_target_properties(tik
	PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin" 
)
target_link_libraries(tik PRIVATE libtik nlohmann_json nlohmann_json::nlohmann_json AtlasUtil)
target_compile_definitions(tik PRIVATE ${LLVM_DEFINITIONS})
target_include_directories(tik SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_include_directories(tik PRIVATE "include/")
if(WIN32)
target_compile_options(tik PRIVATE -W3 -Wextra -Wconversion)
else()
    target_compile_options(tik PRIVATE -Wall -Wextra -Wconversion)
endif()
if(${CMAKE_BUILD_TYPE} MATCHES Debug)
    target_compile_definitions(tik PRIVATE DEBUG)
endif()
install(TARGETS tik RUNTIME DESTINATION bin)

set (TIK_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/include" PARENT_SCOPE)

add_subdirectory("tikSwap")
