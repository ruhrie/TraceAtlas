add_library(DashPasses MODULE Trace.cpp Annotate.cpp TraceIO.cpp CommandArgs.cpp PapiExport.cpp PapiIO.cpp Functions.cpp AddLibrary.cpp)
target_compile_definitions(DashPasses PRIVATE ${LLVM_DEFINITIONS})
set_target_properties(DashPasses
    PROPERTIES
    PREFIX ""
	LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

target_include_directories(DashPasses PRIVATE ${LLVM_INCLUDE_DIRS} ${TRACE_INC})
install(TARGETS DashPasses
    LIBRARY DESTINATION lib
)
