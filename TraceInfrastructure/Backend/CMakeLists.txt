set(SOURCES BackendTrace.c)
if(NOT WIN32)
    list(APPEND SOURCES BackendPapi.c)
endif()
add_library(AtlasBackend STATIC ${SOURCES})
set_target_properties(
    AtlasBackend PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    OUTPUT_NAME AtlasBackend
)

target_link_libraries(AtlasBackend ${llvm_libs} ZLIB::ZLIB)
target_include_directories(AtlasBackend PUBLIC ${TRACE_INC})
if(WIN32)
    target_compile_options(AtlasBackend PRIVATE -W3 -Wextra -Wconversion)
else()
    target_compile_options(AtlasBackend PRIVATE -Wall -Wextra -Wconversion)
endif()

install(TARGETS AtlasBackend
    ARCHIVE DESTINATION lib
)

file(GLOB GR ${TRACE_INC}/Backend/*.h)
install (FILES ${GR} DESTINATION "include/Backend")