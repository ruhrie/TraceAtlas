add_executable(libDetector libDetector.cpp)

set_target_properties(libDetector PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
target_compile_definitions(libDetector PUBLIC ${LLVM_DEFINITIONS})
target_link_libraries(libDetector ${llvm_libs}  nlohmann_json nlohmann_json::nlohmann_json AtlasUtil)
target_include_directories(libDetector SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})

install(TARGETS libDetector RUNTIME DESTINATION bin)

add_executable(kernelHasher KernelHasher.cpp)

set_target_properties(kernelHasher PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
target_compile_definitions(kernelHasher PUBLIC ${LLVM_DEFINITIONS})
target_link_libraries(kernelHasher ${llvm_libs} nlohmann_json nlohmann_json::nlohmann_json)
target_include_directories(kernelHasher SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})

install(TARGETS kernelHasher RUNTIME DESTINATION bin)

add_executable(dagExtractor DagExtractor.cpp)

set_target_properties(dagExtractor PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
target_compile_definitions(dagExtractor PUBLIC ${LLVM_DEFINITIONS})
target_link_libraries(dagExtractor ${llvm_libs} nlohmann_json nlohmann_json::nlohmann_json  AtlasUtil)
target_include_directories(dagExtractor SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})

install(TARGETS dagExtractor RUNTIME DESTINATION bin)

add_executable(sizeEmitter SizeEmitter.cpp)

set_target_properties(sizeEmitter PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
target_compile_definitions(sizeEmitter PUBLIC ${LLVM_DEFINITIONS})
target_link_libraries(sizeEmitter ${llvm_libs} nlohmann_json nlohmann_json::nlohmann_json)
target_include_directories(sizeEmitter SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})

install(TARGETS sizeEmitter RUNTIME DESTINATION bin)

add_executable(bow Bow.cpp)
target_link_libraries(bow ${llvm_libs} nlohmann_json nlohmann_json::nlohmann_json AtlasUtil)
target_include_directories(bow SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_definitions(bow PRIVATE ${LLVM_DEFINITIONS})

set_target_properties(bow
	PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin" 
)

install(TARGETS bow RUNTIME DESTINATION bin)

add_executable(KernelVerifier KernelVerifier.cpp)
target_link_libraries(KernelVerifier ${llvm_libs} nlohmann_json nlohmann_json::nlohmann_json AtlasUtil libtik)
target_include_directories(KernelVerifier SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_definitions(KernelVerifier PRIVATE ${LLVM_DEFINITIONS})

set_target_properties(KernelVerifier
	PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin" 
)

install(TARGETS KernelVerifier RUNTIME DESTINATION bin)

add_executable(JR JR.cpp)
target_link_libraries(JR ${llvm_libs} nlohmann_json nlohmann_json::nlohmann_json AtlasUtil)
target_include_directories(JR SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_definitions(JR PRIVATE ${LLVM_DEFINITIONS})

set_target_properties(JR
	PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin" 
)

install(TARGETS JR RUNTIME DESTINATION bin)