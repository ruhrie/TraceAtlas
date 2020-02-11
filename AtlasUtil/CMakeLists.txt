add_library(AtlasUtil STATIC Print.cpp Annotate.cpp)

target_link_libraries(AtlasUtil PRIVATE ${llvm_libs})

set_target_properties(AtlasUtil PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
target_compile_definitions(AtlasUtil PUBLIC ${LLVM_DEFINITIONS})
target_include_directories(AtlasUtil PRIVATE ${LLVM_INCLUDE_DIRS})
target_include_directories(AtlasUtil PUBLIC include)

install(TARGETS AtlasUtil ARCHIVE DESTINATION lib)