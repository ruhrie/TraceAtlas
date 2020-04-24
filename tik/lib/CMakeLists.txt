file(GLOB SOURCES "*.cpp")
add_library(libtik SHARED ${SOURCES})
target_link_libraries(libtik PRIVATE AtlasUtil)
target_link_libraries(libtik INTERFACE ${llvm_libs} spdlog::spdlog_header_only)
target_compile_definitions(libtik PRIVATE ${LLVM_DEFINITIONS})
target_include_directories(libtik SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_include_directories(libtik PUBLIC "include/")
target_compile_options(libtik PRIVATE -Wall -Wextra -Wconversion -fPIC)
set_target_properties(libtik
    PROPERTIES
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)
install(TARGETS libtik LIBRARY DESTINATION lib)

file(GLOB HEADERS include/tik/*.h)
install (FILES ${HEADERS} DESTINATION "include/tik")
