add_executable(workingSet WorkingSetAnalysis.cpp WorkingSet.cpp)

set_target_properties(workingSet PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
target_compile_definitions(workingSet PUBLIC ${LLVM_DEFINITIONS})
target_link_libraries(workingSet ${llvm_libs}  AtlasUtil)

install(TARGETS workingSet RUNTIME DESTINATION bin)