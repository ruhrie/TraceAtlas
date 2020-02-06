add_executable(bubbleSort bubbleSort.c)

set_target_properties(bubbleSort PROPERTIES LINK_FLAGS "-Wl,--plugin-opt=emit-llvm")
target_compile_options(bubbleSort PRIVATE "-flto")
InjectTracer(bubbleSort)

add_test(NAME bubbleSort_Trace COMMAND bubbleSort-trace 512)
set_tests_properties(bubbleSort_Trace PROPERTIES 
    FIXTURES_SETUP bubblesort_trace_fixutre
    )

add_test(NAME bubbleSort_cartographer COMMAND cartographer -i ${CMAKE_CURRENT_BINARY_DIR}/raw.trc -b $<TARGET_FILE:bubbleSort> -k ${CMAKE_CURRENT_BINARY_DIR}/kernel.json)
set_tests_properties(bubbleSort_cartographer PROPERTIES 
    DEPENDS bubbleSort_Trace
    FIXTURES_REQUIRED bubblesort_trace_fixutre
    FIXTURES_SETUP bubblesort_cartographer_fixutre
    )

add_test(NAME bubbleSort_tik COMMAND tik -j ${CMAKE_CURRENT_BINARY_DIR}/kernel.json -o ${CMAKE_CURRENT_BINARY_DIR}/tik.bc $<TARGET_FILE:bubbleSort>)
set_tests_properties(bubbleSort_tik PROPERTIES 
    DEPENDS bubbleSort_cartographer
    FIXTURES_REQUIRED  bubblesort_cartographer_fixutre
    )

add_test(NAME bubbleSort_dag COMMAND dagExtractor -t ${CMAKE_CURRENT_BINARY_DIR}/raw.trc -o ${CMAKE_CURRENT_BINARY_DIR}/dag.json -k ${CMAKE_CURRENT_BINARY_DIR}/kernel.json)
set_tests_properties(bubbleSort_dag PROPERTIES 
    DEPENDS bubbleSort_cartographer
    FIXTURES_REQUIRED bubblesort_cartographer_fixutre
    )