add_library(AtlasUtil INTERFACE)

execute_process(
  COMMAND git log -1 --format=%h
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE COMMIT_ID
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/include/AtlasUtil/Version.h.in ${CMAKE_CURRENT_BINARY_DIR}/include/AtlasUtil/Version.h @ONLY)
target_compile_definitions(AtlasUtil INTERFACE ${LLVM_DEFINITIONS})
target_include_directories(AtlasUtil INTERFACE ${LLVM_INCLUDE_DIRS})
target_include_directories(AtlasUtil INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_BINARY_DIR}/include)
target_link_libraries(AtlasUtil INTERFACE  spdlog::spdlog spdlog::spdlog_header_only indica::indica)