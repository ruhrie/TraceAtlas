add_library(AtlasBackend STATIC BackendTrace.c BackendPapi.c)
set_target_properties(
    AtlasBackend PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    OUTPUT_NAME AtlasBackend
)

target_link_libraries(AtlasBackend ${llvm_libs} ZLIB::ZLIB)
target_include_directories(AtlasBackend PRIVATE ${TRACE_INC})
install(TARGETS AtlasBackend
    ARCHIVE DESTINATION lib
)