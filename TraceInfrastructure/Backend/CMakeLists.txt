add_library(DashTracer STATIC BackendTrace.c BackendPapi.c)
set_target_properties(
    DashTracer PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    OUTPUT_NAME DashTracer
)

target_link_libraries(DashTracer ${llvm_libs} ${ZLIB_LIBRARIES})
target_include_directories(DashTracer PRIVATE ${ZLIB_INCLUDE_DIRS} ${TRACE_INC})
install(TARGETS DashTracer
    ARCHIVE DESTINATION lib
)