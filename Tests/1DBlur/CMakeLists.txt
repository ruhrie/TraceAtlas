add_executable(1DBlur 1DBlur.c)
InjectTracer(1DBlur)

add_test(NAME 1DBlur_Trace COMMAND 1DBlur-trace)

add_test(NAME 1DBlur_cartographer COMMAND newCartographer -i ${CMAKE_CURRENT_BINARY_DIR}/markov.bin -b $<TARGET_FILE:1DBlur> -bi ${CMAKE_CURRENT_BINARY_DIR}/BlockInfo.json -o ${CMAKE_CURRENT_BINARY_DIR}/kernel.json)
set_tests_properties(1DBlur_cartographer PROPERTIES DEPENDS 1DBlur_Trace)

add_test(NAME 1DBlur_tik COMMAND tik -j ${CMAKE_CURRENT_BINARY_DIR}/kernel.json -o ${CMAKE_CURRENT_BINARY_DIR}/tik.bc $<TARGET_FILE:1DBlur>)
set_tests_properties(1DBlur_tik PROPERTIES DEPENDS 1DBlur_cartographer)

#add_test(NAME 1DBlur_dag COMMAND dagExtractor -t ${CMAKE_CURRENT_BINARY_DIR}/raw.trc -o ${CMAKE_CURRENT_BINARY_DIR}/dag.json -k ${CMAKE_CURRENT_BINARY_DIR}/kernel.json)
#set_tests_properties(1DBlur_dag PROPERTIES DEPENDS 1DBlur_cartographer)

#add_test(NAME 1DBlur_WS COMMAND workingSet -i ${CMAKE_CURRENT_BINARY_DIR}/raw.trc -k ${CMAKE_CURRENT_BINARY_DIR}/kernel.json -o ${CMAKE_CURRENT_BINARY_DIR}/WS.json)
#set_tests_properties(1DBlur_WS PROPERTIES DEPENDS 1DBlur_cartographer)

add_test(NAME 1DBlur_ts COMMAND tikSwap -g -t ${CMAKE_CURRENT_BINARY_DIR}/tik.bc -b $<TARGET_FILE:1DBlur> -o ${CMAKE_CURRENT_BINARY_DIR}/tikSwap.bc)
set_tests_properties(1DBlur_ts PROPERTIES DEPENDS 1DBlur_tik)

add_test(NAME 1DBlur_swap COMMAND ${CMAKE_CXX_COMPILER} ${CMAKE_CURRENT_BINARY_DIR}/tik.bc ${CMAKE_CURRENT_BINARY_DIR}/tikSwap.bc -o ${CMAKE_CURRENT_BINARY_DIR}/swap.exec)
set_tests_properties(1DBlur_swap PROPERTIES DEPENDS 1DBlur_ts)

add_test(NAME 1DBlurSwap_run COMMAND ${CMAKE_CURRENT_BINARY_DIR}/swap.exec)
set_tests_properties(1DBlurSwap_run PROPERTIES DEPENDS 1DBlur_swap)