set(DEP_INC "${CMAKE_CURRENT_SOURCE_DIR}/include" PARENT_SCOPE)

find_package(Doxygen)
if(DOXYGEN_FOUND)
    configure_file(DoxygenLayout.xml ${CMAKE_CURRENT_BINARY_DIR}/DoxygenLayout.xml @ONLY)
    configure_file(Doxyfile ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
    add_custom_target(doc COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
endif(DOXYGEN_FOUND)