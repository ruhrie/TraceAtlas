
add_library(tikPasses MODULE Trace.cpp CommandArgs.cpp PapiExport.cpp PapiIO.cpp Functions.cpp AddLibrary.cpp ${CMAKE_SOURCE_DIR}/AtlasUtil/Annotate.cpp)
target_link_libraries(tikPasses PRIVATE nlohmann_json nlohmann_json::nlohmann_json)
target_compile_definitions(tikPasses PRIVATE ${LLVM_DEFINITIONS})
set_target_properties(tikPasses
    PROPERTIES
    PREFIX ""
	LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

target_include_directories(tikPasses PRIVATE ${LLVM_INCLUDE_DIRS} ${TRACE_INC} ${CMAKE_SOURCE_DIR}/AtlasUtil/include)
install(TARGETS tikPasses
    LIBRARY DESTINATION lib
)
