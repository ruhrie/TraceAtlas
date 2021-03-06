add_executable(bubbleSort bubbleSort.c)
target_include_directories(bubbleSort PRIVATE ${CMAKE_SOURCE_DIR}/TraceInfrastructure/include/)
InjectTracer(bubbleSort)

add_test(NAME bubbleSort_Trace COMMAND bubbleSort-trace 512)

add_test(NAME bubbleSort_cartographer COMMAND cartographer -L -i ${CMAKE_CURRENT_BINARY_DIR}/raw.trc -b $<TARGET_FILE:bubbleSort> -k ${CMAKE_CURRENT_BINARY_DIR}/kernel.json -p ${CMAKE_CURRENT_BINARY_DIR}/pig.json)
set_tests_properties(bubbleSort_cartographer PROPERTIES DEPENDS bubbleSort_Trace)

add_test(NAME bubbleSort_tik COMMAND tik -j ${CMAKE_CURRENT_BINARY_DIR}/kernel.json -o ${CMAKE_CURRENT_BINARY_DIR}/tik.bc $<TARGET_FILE:bubbleSort> -S)
set_tests_properties(bubbleSort_tik PROPERTIES DEPENDS bubbleSort_cartographer)

add_test(NAME bubbleSort_dag COMMAND dagExtractor -t ${CMAKE_CURRENT_BINARY_DIR}/raw.trc -o ${CMAKE_CURRENT_BINARY_DIR}/dag.json -k ${CMAKE_CURRENT_BINARY_DIR}/kernel.json)
set_tests_properties(bubbleSort_dag PROPERTIES DEPENDS bubbleSort_cartographer)