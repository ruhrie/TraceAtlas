add_executable(FunctionCall FunctionCall.c)
InjectTracer(FunctionCall)

add_test(NAME FunctionCall_Trace COMMAND FunctionCall-trace)

add_test(NAME FunctionCall_cartographer COMMAND cartographer -i ${CMAKE_CURRENT_BINARY_DIR}/raw.trc -b $<TARGET_FILE:FunctionCall> -k ${CMAKE_CURRENT_BINARY_DIR}/kernel.json -p ${CMAKE_CURRENT_BINARY_DIR}/pig.json)
set_tests_properties(FunctionCall_cartographer PROPERTIES DEPENDS FunctionCall_Trace)

add_test(NAME FunctionCall_tik COMMAND tik -j ${CMAKE_CURRENT_BINARY_DIR}/kernel.json -o ${CMAKE_CURRENT_BINARY_DIR}/tik.bc $<TARGET_FILE:FunctionCall>)
set_tests_properties(FunctionCall_tik PROPERTIES DEPENDS FunctionCall_cartographer)

add_test(NAME FunctionCall_dag COMMAND dagExtractor -t ${CMAKE_CURRENT_BINARY_DIR}/raw.trc -o ${CMAKE_CURRENT_BINARY_DIR}/dag.json -k ${CMAKE_CURRENT_BINARY_DIR}/kernel.json)
set_tests_properties(FunctionCall_dag PROPERTIES DEPENDS FunctionCall_cartographer)

add_test(NAME FunctionCall_WS COMMAND workingSet -i ${CMAKE_CURRENT_BINARY_DIR}/raw.trc -k ${CMAKE_CURRENT_BINARY_DIR}/kernel.json -o ${CMAKE_CURRENT_BINARY_DIR}/WS.json)
set_tests_properties(FunctionCall_WS PROPERTIES DEPENDS FunctionCall_cartographer)

add_test(NAME FunctionCall_ts COMMAND tikSwap -t ${CMAKE_CURRENT_BINARY_DIR}/tik.bc -b $<TARGET_FILE:FunctionCall> -o ${CMAKE_CURRENT_BINARY_DIR}/tikSwap.bc)
set_tests_properties(FunctionCall_ts PROPERTIES DEPENDS FunctionCall_cartographer)

add_test(NAME FunctionCall_swap COMMAND ${CMAKE_CXX_COMPILER} ${CMAKE_CURRENT_BINARY_DIR}/tik.bc ${CMAKE_CURRENT_BINARY_DIR}/tikSwap.bc -o ${CMAKE_CURRENT_BINARY_DIR}/swap.exec)
set_tests_properties(FunctionCall_swap PROPERTIES DEPENDS FunctionCall_ts)

add_test(NAME FunctionCallSwap_run COMMAND ${CMAKE_CURRENT_BINARY_DIR}/swap.exec)
set_tests_properties(FunctionCallSwap_run PROPERTIES DEPENDS FunctionCall_swap)