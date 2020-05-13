add_executable(Recurse Recurse.c)
InjectTracer(Recurse)

add_test(NAME Recurse_Trace COMMAND Recurse-trace)

add_test(NAME Recurse_cartographer COMMAND cartographer ${CMAKE_CURRENT_BINARY_DIR}/raw.trc.0 -b $<TARGET_FILE:Recurse> -k ${CMAKE_CURRENT_BINARY_DIR}/kernel.json -p ${CMAKE_CURRENT_BINARY_DIR}/pig.json)
set_tests_properties(Recurse_cartographer PROPERTIES DEPENDS Recurse_Trace)

add_test(NAME Recurse_tik COMMAND tik -j ${CMAKE_CURRENT_BINARY_DIR}/kernel.json -o ${CMAKE_CURRENT_BINARY_DIR}/tik.bc $<TARGET_FILE:Recurse>)
set_tests_properties(Recurse_tik PROPERTIES DEPENDS Recurse_cartographer)

add_test(NAME Recurse_dag COMMAND dagExtractor -t ${CMAKE_CURRENT_BINARY_DIR}/raw.trc -o ${CMAKE_CURRENT_BINARY_DIR}/dag.json -k ${CMAKE_CURRENT_BINARY_DIR}/kernel.json)
set_tests_properties(Recurse_dag PROPERTIES DEPENDS Recurse_cartographer)