add_executable(1DBlur 1DBlur.c)
InjectTracer(1DBlur)

add_test(NAME 1DBlur_Trace COMMAND 1DBlur-trace)

add_test(NAME 1DBlur_cartographer COMMAND cartographer ${CMAKE_CURRENT_BINARY_DIR}/raw.trc.0 -b $<TARGET_FILE:1DBlur> -k ${CMAKE_CURRENT_BINARY_DIR}/kernel.json -p ${CMAKE_CURRENT_BINARY_DIR}/pig.json)
set_tests_properties(1DBlur_cartographer PROPERTIES DEPENDS 1DBlur_Trace)

add_test(NAME 1DBlur_tik COMMAND tik -j ${CMAKE_CURRENT_BINARY_DIR}/kernel.json -o ${CMAKE_CURRENT_BINARY_DIR}/tik.bc $<TARGET_FILE:1DBlur>)
set_tests_properties(1DBlur_tik PROPERTIES DEPENDS 1DBlur_cartographer)

add_test(NAME 1DBlur_dag COMMAND dagExtractor -t ${CMAKE_CURRENT_BINARY_DIR}/raw.trc -o ${CMAKE_CURRENT_BINARY_DIR}/dag.json -k ${CMAKE_CURRENT_BINARY_DIR}/kernel.json)
set_tests_properties(1DBlur_dag PROPERTIES DEPENDS 1DBlur_cartographer)
