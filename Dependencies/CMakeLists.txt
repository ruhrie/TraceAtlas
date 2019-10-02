set(DEP_INC "${CMAKE_CURRENT_SOURCE_DIR}/include" PARENT_SCOPE)

find_package(LLVM REQUIRED 8 CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

llvm_map_components_to_libnames(llvm_libs support core irreader bitreader bitwriter transformutils)

set(LLVM_DEF "${LLVM_DEFINITIONS}" PARENT_SCOPE)
set(LLVM_INC "${LLVM_INCLUDE_DIRS}" PARENT_SCOPE)
set(LLVM_LIB ${llvm_libs} PARENT_SCOPE)

find_package(ZLIB REQUIRED)

find_package(Doxygen)
if(DOXYGEN_FOUND)
    configure_file(DoxygenLayout.xml ${CMAKE_CURRENT_BINARY_DIR}/DoxygenLayout.xml @ONLY)
    configure_file(Doxyfile ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
    add_custom_target(doc COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
endif(DOXYGEN_FOUND)