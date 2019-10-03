add_executable(libDetector libDetector.cpp)

set_target_properties(libDetector PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
target_compile_definitions(libDetector PUBLIC ${LLVM_DEF})
target_link_libraries(libDetector ${LLVM_LIB} "-lpthread -lncurses -lz") #not sure why these are needed
target_include_directories(libDetector PRIVATE ${DEP_INC} ${LLVM_INC})

install(TARGETS libDetector RUNTIME DESTINATION bin)