add_library(AtlasBackend STATIC BackendTrace.cpp BackendPapi.c)
set_target_properties(
    AtlasBackend PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    OUTPUT_NAME AtlasBackend
)

target_link_libraries(AtlasBackend ${llvm_libs} ZLIB::ZLIB)
target_include_directories(AtlasBackend PUBLIC ${TRACE_INC})
target_compile_options(AtlasBackend PRIVATE -Wall  -Wextra -Wconversion)

install(TARGETS AtlasBackend
    ARCHIVE DESTINATION lib
)

file(GLOB GR ${TRACE_INC}/Backend/*.h)
install (FILES ${GR} DESTINATION "include/Backend")