add_library(AtlasUtil INTERFACE)

target_compile_definitions(AtlasUtil INTERFACE ${LLVM_DEFINITIONS})
target_include_directories(AtlasUtil SYSTEM INTERFACE ${LLVM_INCLUDE_DIRS})
target_include_directories(AtlasUtil INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(AtlasUtil INTERFACE spdlog::spdlog_header_only indicators::indicators ZLIB::ZLIB)