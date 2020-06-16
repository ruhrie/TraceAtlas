add_executable(workingSet main.cpp WorkingSet.cpp)

set_target_properties(workingSet PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
target_compile_definitions(workingSet PUBLIC ${LLVM_DEFINITIONS})
target_link_libraries(workingSet ${llvm_libs}  nlohmann_json nlohmann_json::nlohmann_json AtlasUtil)
target_include_directories(workingSet SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})

install(TARGETS workingSet RUNTIME DESTINATION bin)
