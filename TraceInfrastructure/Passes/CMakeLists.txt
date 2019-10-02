add_library(DashPasses MODULE Trace.cpp Annotate.cpp TraceIO.cpp CommandArgs.cpp PapiExport.cpp PapiIO.cpp Functions.cpp AddLibrary.cpp)

set_target_properties(DashPasses
    PROPERTIES
    PREFIX ""
	LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

if(WIN32)
    target_link_libraries(DashPasses ${llvm_libs})
endif()

target_include_directories(DashPasses PRIVATE ${LLVM_INC} ${TRACE_INC})
install(TARGETS DashPasses
    LIBRARY DESTINATION lib
)
