
add_library(AtlasPasses MODULE Trace.cpp Annotate.cpp TraceIO.cpp CommandArgs.cpp PapiExport.cpp PapiIO.cpp Functions.cpp AddLibrary.cpp ${CMAKE_SOURCE_DIR}/AtlasUtil/Annotate.cpp)
target_compile_definitions(AtlasPasses PRIVATE ${LLVM_DEFINITIONS})
set_target_properties(AtlasPasses
    PROPERTIES
    PREFIX ""
	LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

#target_link_libraries(AtlasPasses PRIVATE AtlasUtil)

target_include_directories(AtlasPasses PRIVATE ${LLVM_INCLUDE_DIRS} ${TRACE_INC} ${CMAKE_SOURCE_DIR}/AtlasUtil/include)
install(TARGETS AtlasPasses
    LIBRARY DESTINATION lib
)