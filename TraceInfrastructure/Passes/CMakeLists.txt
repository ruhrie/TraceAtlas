
add_library(AtlasPasses MODULE Trace.cpp Annotate.cpp TraceIO.cpp CommandArgs.cpp PapiExport.cpp PapiIO.cpp Functions.cpp AddLibrary.cpp)
target_link_libraries(AtlasPasses PRIVATE nlohmann_json nlohmann_json::nlohmann_json AtlasUtil)
target_compile_definitions(AtlasPasses PRIVATE ${LLVM_DEFINITIONS})
target_compile_options(AtlasPasses PRIVATE -Wall  -Wextra  -Wconversion -Werror)
set_target_properties(AtlasPasses
    PROPERTIES
    PREFIX ""
	LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

target_include_directories(AtlasPasses PRIVATE ${TRACE_INC})
target_include_directories(AtlasPasses SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
install(TARGETS AtlasPasses
    LIBRARY DESTINATION lib
)