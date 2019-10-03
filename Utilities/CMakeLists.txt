add_executable(libDetector libDetector.cpp)

set_target_properties(libDetector PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
target_compile_definitions(libDetector PUBLIC ${LLVM_DEFINITIONS})
target_link_libraries(libDetector ${llvm_libs})
target_include_directories(libDetector PRIVATE ${DEP_INC} ${LLVM_INCLUDE_DIRS})

install(TARGETS libDetector RUNTIME DESTINATION bin)