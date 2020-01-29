
add_library(AtlasPasses MODULE Trace.cpp TraceMem.cpp TraceMemIO.cpp Annotate.cpp TraceIO.cpp CommandArgs.cpp PapiExport.cpp PapiIO.cpp Functions.cpp AddLibrary.cpp)
target_compile_definitions(AtlasPasses PRIVATE ${LLVM_DEFINITIONS})
set_target_properties(AtlasPasses
    PROPERTIES
    PREFIX ""
	LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

target_include_directories(AtlasPasses PRIVATE ${LLVM_INCLUDE_DIRS} ${TRACE_INC})
install(TARGETS AtlasPasses
    LIBRARY DESTINATION lib
)