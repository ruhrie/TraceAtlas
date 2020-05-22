file(GLOB SOURCES "*.cpp")
add_library(libtik STATIC ${SOURCES})
target_link_libraries(libtik PRIVATE AtlasUtil)
target_link_libraries(libtik INTERFACE ${llvm_libs} spdlog::spdlog_header_only)
target_compile_definitions(libtik PRIVATE ${LLVM_DEFINITIONS})
target_include_directories(libtik SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_include_directories(libtik PUBLIC "include/")
if(WIN32)
    target_compile_options(libtik PRIVATE -W3 -Wextra -Wconversion)
else()
    target_compile_options(libtik PRIVATE -Wall -Wextra -Wconversion)
endif()
set_target_properties(libtik
    PROPERTIES
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

install(TARGETS libtik ARCHIVE DESTINATION lib LIBRARY DESTINATION lib)

file(GLOB HEADERS include/tik/*.h)
install (FILES ${HEADERS} DESTINATION "include/tik")
