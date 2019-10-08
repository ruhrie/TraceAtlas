add_executable(libDetector libDetector.cpp)

set_target_properties(libDetector PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
target_compile_definitions(libDetector PUBLIC ${LLVM_DEFINITIONS})
target_link_libraries(libDetector ${llvm_libs})
target_include_directories(libDetector PRIVATE ${DEP_INC} ${LLVM_INCLUDE_DIRS})

install(TARGETS libDetector RUNTIME DESTINATION bin)

add_executable(kernelHasher KernelHasher.cpp)

set_target_properties(kernelHasher PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
target_compile_definitions(kernelHasher PUBLIC ${LLVM_DEFINITIONS})
target_link_libraries(kernelHasher ${llvm_libs})
target_include_directories(kernelHasher PRIVATE ${DEP_INC} ${LLVM_INCLUDE_DIRS})

install(TARGETS kernelHasher RUNTIME DESTINATION bin)

add_executable(dagExtractor DagExtractor.cpp)

set_target_properties(dagExtractor PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
target_compile_definitions(dagExtractor PUBLIC ${LLVM_DEFINITIONS})
target_link_libraries(dagExtractor ${llvm_libs})
target_include_directories(dagExtractor PRIVATE ${DEP_INC} ${LLVM_INCLUDE_DIRS})

install(TARGETS dagExtractor RUNTIME DESTINATION bin)

add_executable(sizeEmitter SizeEmitter.cpp)

set_target_properties(sizeEmitter PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
target_compile_definitions(sizeEmitter PUBLIC ${LLVM_DEFINITIONS})
target_link_libraries(sizeEmitter ${llvm_libs})
target_include_directories(sizeEmitter PRIVATE ${DEP_INC} ${LLVM_INCLUDE_DIRS})

install(TARGETS sizeEmitter RUNTIME DESTINATION bin)

add_executable(bow Bow.cpp)
target_link_libraries(bow ${llvm_libs})
target_include_directories(bow PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_definitions(bow PRIVATE ${LLVM_DEFINITIONS})
target_include_directories(bow PRIVATE  ${DEP_INC})

set_target_properties(bow
	PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin" 
)

install(TARGETS bow RUNTIME DESTINATION bin)