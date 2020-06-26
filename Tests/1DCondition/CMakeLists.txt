add_executable(1DCondition 1DCondition.c)
InjectTracer(1DCondition)

add_test(NAME 1DCondition_Trace COMMAND 1DCondition-trace)

add_test(NAME 1DCondition_cartographer COMMAND cartographer -i ${CMAKE_CURRENT_BINARY_DIR}/raw.trc -b $<TARGET_FILE:1DCondition> -k ${CMAKE_CURRENT_BINARY_DIR}/kernel.json -p ${CMAKE_CURRENT_BINARY_DIR}/pig.json)
set_tests_properties(1DCondition_cartographer PROPERTIES DEPENDS 1DCondition_Trace)

add_test(NAME 1DCondition_tik COMMAND tik -j ${CMAKE_CURRENT_BINARY_DIR}/kernel.json -o ${CMAKE_CURRENT_BINARY_DIR}/tik.bc $<TARGET_FILE:1DCondition>)
set_tests_properties(1DCondition_tik PROPERTIES DEPENDS 1DCondition_cartographer)

add_test(NAME 1DCondition_dag COMMAND dagExtractor -t ${CMAKE_CURRENT_BINARY_DIR}/raw.trc -o ${CMAKE_CURRENT_BINARY_DIR}/dag.json -k ${CMAKE_CURRENT_BINARY_DIR}/kernel.json)
set_tests_properties(1DCondition_dag PROPERTIES DEPENDS 1DCondition_cartographer)

add_test(NAME 1DCondition_WS COMMAND workingSet -i ${CMAKE_CURRENT_BINARY_DIR}/raw.trc -k ${CMAKE_CURRENT_BINARY_DIR}/kernel.json -o ${CMAKE_CURRENT_BINARY_DIR}/WS.json)
set_tests_properties(1DCondition_WS PROPERTIES DEPENDS 1DCondition_cartographer)

add_test(NAME 1DCondition_ts COMMAND tikSwap -t ${CMAKE_CURRENT_BINARY_DIR}/tik.bc -b $<TARGET_FILE:1DCondition> -o ${CMAKE_CURRENT_BINARY_DIR}/tikSwap.bc)
set_tests_properties(1DCondition_ts PROPERTIES DEPENDS 1DCondition_cartographer)

add_test(NAME 1DCondition_swap COMMAND ${CMAKE_CXX_COMPILER} ${CMAKE_CURRENT_BINARY_DIR}/tik.bc ${CMAKE_CURRENT_BINARY_DIR}/tikSwap.bc -o ${CMAKE_CURRENT_BINARY_DIR}/swap.exec)
set_tests_properties(1DCondition_swap PROPERTIES DEPENDS 1DCondition_ts)

add_test(NAME 1DConditionSwap_run COMMAND ${CMAKE_CURRENT_BINARY_DIR}/swap.exec)
set_tests_properties(1DConditionSwap_run PROPERTIES DEPENDS 1DCondition_swap)