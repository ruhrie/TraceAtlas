add_library(tikBackend STATIC Backend.c)
set_target_properties(
    tikBackend PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    OUTPUT_NAME tikBackend 
)

target_link_libraries(tikBackend ${llvm_libs} ZLIB::ZLIB)
target_include_directories(tikBackend PUBLIC ${TRACE_INC})
install(TARGETS tikBackend
    ARCHIVE DESTINATION lib
)

file(GLOB GR ${TRACE_INC}/Backend/*.h)
install (FILES ${GR} DESTINATION "include/Backend")
