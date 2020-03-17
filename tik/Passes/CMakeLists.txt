
add_library(tikPasses MODULE Trace.cpp Functions.cpp Annotate.cpp CommandArgs.cpp)
target_link_libraries(tikPasses PRIVATE nlohmann_json nlohmann_json::nlohmann_json AtlasUtil)
target_compile_definitions(tikPasses PRIVATE ${LLVM_DEFINITIONS})
set_target_properties(tikPasses
    PROPERTIES
    PREFIX ""
	LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

target_include_directories(tikPasses PRIVATE ${LLVM_INCLUDE_DIRS} ${TRACE_INC})
install(TARGETS tikPasses
    LIBRARY DESTINATION lib
)
