add_library(AtlasPasses MODULE Trace.cpp TraceMem.cpp TraceMemIO.cpp Annotate.cpp TraceIO.cpp CommandArgs.cpp PapiExport.cpp PapiIO.cpp Functions.cpp AddLibrary.cpp)
target_link_libraries(AtlasPasses PRIVATE nlohmann_json nlohmann_json::nlohmann_json AtlasUtil)
target_compile_definitions(AtlasPasses PRIVATE ${LLVM_DEFINITIONS})
if(WIN32)
    target_compile_options(AtlasPasses PRIVATE -W4  -Wextra  -Wconversion)
else()
    target_compile_options(AtlasPasses PRIVATE -Wall  -Wextra  -Wconversion)
endif()
set_target_properties(AtlasPasses
    PROPERTIES
    PREFIX ""
	LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

target_include_directories(AtlasPasses PRIVATE ${TRACE_INC})
install(TARGETS AtlasPasses
    LIBRARY DESTINATION lib
)