add_executable(2DConv 2DConv.c)
InjectTracer(2DConv)

add_test(NAME 2DConv_Trace COMMAND 2DConv-trace)

add_test(NAME 2DConv_cartographer COMMAND cartographer -i ${CMAKE_CURRENT_BINARY_DIR}/raw.trc -b $<TARGET_FILE:2DConv> -k ${CMAKE_CURRENT_BINARY_DIR}/kernel.json -p ${CMAKE_CURRENT_BINARY_DIR}/pig.json)
set_tests_properties(2DConv_cartographer PROPERTIES DEPENDS 2DConv_Trace)

add_test(NAME 2DConv_tik COMMAND tik -j ${CMAKE_CURRENT_BINARY_DIR}/kernel.json -o ${CMAKE_CURRENT_BINARY_DIR}/tik.bc $<TARGET_FILE:2DConv>)
set_tests_properties(2DConv_tik PROPERTIES DEPENDS 2DConv_cartographer)

add_test(NAME 2DConv_dag COMMAND dagExtractor -t ${CMAKE_CURRENT_BINARY_DIR}/raw.trc -o ${CMAKE_CURRENT_BINARY_DIR}/dag.json -k ${CMAKE_CURRENT_BINARY_DIR}/kernel.json)
set_tests_properties(2DConv_dag PROPERTIES DEPENDS 2DConv_cartographer)

add_test(NAME 2DConv_WS COMMAND workingSet -i ${CMAKE_CURRENT_BINARY_DIR}/raw.trc -k ${CMAKE_CURRENT_BINARY_DIR}/kernel.json -o ${CMAKE_CURRENT_BINARY_DIR}/WS.json)
set_tests_properties(2DConv_WS PROPERTIES DEPENDS 2DConv_cartographer)

add_test(NAME 2DConv_ts COMMAND tikSwap -g -t ${CMAKE_CURRENT_BINARY_DIR}/tik.bc -b $<TARGET_FILE:2DConv> -o ${CMAKE_CURRENT_BINARY_DIR}/tikSwap.bc)
set_tests_properties(2DConv_ts PROPERTIES DEPENDS 2DConv_tik)

add_test(NAME 2DConv_swap COMMAND ${CMAKE_CXX_COMPILER} ${CMAKE_CURRENT_BINARY_DIR}/tik.bc ${CMAKE_CURRENT_BINARY_DIR}/tikSwap.bc -o ${CMAKE_CURRENT_BINARY_DIR}/swap.exec)
set_tests_properties(2DConv_swap PROPERTIES DEPENDS 2DConv_ts)

add_test(NAME 2DConvSwap_run COMMAND ${CMAKE_CURRENT_BINARY_DIR}/swap.exec)
set_tests_properties(2DConvSwap_run PROPERTIES DEPENDS 2DConv_swap)