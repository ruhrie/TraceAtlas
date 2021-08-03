add_executable(SharedFunction SharedFunction.c)
InjectTracer(SharedFunction)
InjectInstance(SharedFunction)

add_test(NAME SharedFunction_Trace COMMAND SharedFunction-trace)

add_test(NAME SharedFunction_cartographer COMMAND newCartographer -i ${CMAKE_CURRENT_BINARY_DIR}/markov.bin -b $<TARGET_FILE:SharedFunction> -bi ${CMAKE_CURRENT_BINARY_DIR}/BlockInfo.json -d dot.dot -o ${CMAKE_CURRENT_BINARY_DIR}/kernel.json)
set_tests_properties(SharedFunction_cartographer PROPERTIES DEPENDS SharedFunction_Trace)

add_test(NAME SharedFunction_DAG_Trace COMMAND SharedFunction-instance-trace)

add_test(NAME SharedFunction_tik COMMAND tik -j ${CMAKE_CURRENT_BINARY_DIR}/kernel.json -o ${CMAKE_CURRENT_BINARY_DIR}/tik.bc $<TARGET_FILE:SharedFunction>)
set_tests_properties(SharedFunction_tik PROPERTIES DEPENDS SharedFunction_cartographer)

#add_test(NAME SharedFunction_dag COMMAND dagExtractor -t ${CMAKE_CURRENT_BINARY_DIR}/raw.trc -o ${CMAKE_CURRENT_BINARY_DIR}/dag.json -k ${CMAKE_CURRENT_BINARY_DIR}/kernel.json)
#set_tests_properties(SharedFunction_dag PROPERTIES DEPENDS SharedFunction_cartographer)

#add_test(NAME SharedFunction_WS COMMAND workingSet -i ${CMAKE_CURRENT_BINARY_DIR}/raw.trc -k ${CMAKE_CURRENT_BINARY_DIR}/kernel.json -o ${CMAKE_CURRENT_BINARY_DIR}/WS.json)
#set_tests_properties(SharedFunction_WS PROPERTIES DEPENDS SharedFunction_cartographer)

add_test(NAME SharedFunction_ts COMMAND tikSwap -g -t ${CMAKE_CURRENT_BINARY_DIR}/tik.bc -b $<TARGET_FILE:SharedFunction> -o ${CMAKE_CURRENT_BINARY_DIR}/tikSwap.bc)
set_tests_properties(SharedFunction_ts PROPERTIES DEPENDS SharedFunction_tik)

add_test(NAME SharedFunction_swap COMMAND ${CMAKE_CXX_COMPILER} ${CMAKE_CURRENT_BINARY_DIR}/tik.bc ${CMAKE_CURRENT_BINARY_DIR}/tikSwap.bc -o ${CMAKE_CURRENT_BINARY_DIR}/swap.exec)
set_tests_properties(SharedFunction_swap PROPERTIES DEPENDS SharedFunction_ts)

add_test(NAME SharedFunctionSwap_run COMMAND ${CMAKE_CURRENT_BINARY_DIR}/swap.exec)
set_tests_properties(SharedFunctionSwap_run PROPERTIES DEPENDS SharedFunction_swap)