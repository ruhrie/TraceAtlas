add_executable(matmul main.c)
target_include_directories(matmul PRIVATE ${CMAKE_SOURCE_DIR}/TraceInfrastructure/include/)
InjectTracer(matmul)

add_test(NAME matmul_trace COMMAND matmul-trace)
set_tests_properties(matmul_trace PROPERTIES 
    FIXTURES_SETUP matmul_trace_fixture
    )

add_test(NAME matmul_cartographer COMMAND cartographer -i ${CMAKE_CURRENT_BINARY_DIR}/raw.trc -b $<TARGET_FILE:matmul> -k ${CMAKE_CURRENT_BINARY_DIR}/kernel.json -p ${CMAKE_CURRENT_BINARY_DIR}/pig.json)
set_tests_properties(matmul_cartographer PROPERTIES 
    DEPENDS matmul_trace
    FIXTURES_REQUIRED matmul_trace_fixture
    FIXTURES_SETUP matmul_cartographer_fixture
    )

add_test(NAME matmul_tik COMMAND tik -j ${CMAKE_CURRENT_BINARY_DIR}/kernel.json -o ${CMAKE_CURRENT_BINARY_DIR}/tik.bc $<TARGET_FILE:matmul>)
set_tests_properties(matmul_tik PROPERTIES 
    DEPENDS matmul_cartographer
    FIXTURES_REQUIRED matmul_cartographer_fixture
    )

add_test(NAME matmul_dag COMMAND dagExtractor -t ${CMAKE_CURRENT_BINARY_DIR}/raw.trc -o ${CMAKE_CURRENT_BINARY_DIR}/dag.json -k ${CMAKE_CURRENT_BINARY_DIR}/kernel.json)
set_tests_properties(matmul_dag PROPERTIES 
    DEPENDS matmul_cartographer
    FIXTURES_REQUIRED  matmul_cartographer_fixture
    )


add_test(NAME matmul_WS COMMAND workingSet -i ${CMAKE_CURRENT_BINARY_DIR}/raw.trc -k ${CMAKE_CURRENT_BINARY_DIR}/kernel.json -o ${CMAKE_CURRENT_BINARY_DIR}/WS.json)
set_tests_properties(matmul_WS PROPERTIES DEPENDS matmul_cartographer)

add_test(NAME matmul_ts COMMAND tikSwap -t ${CMAKE_CURRENT_BINARY_DIR}/tik.bc -b $<TARGET_FILE:matmul> -o ${CMAKE_CURRENT_BINARY_DIR}/tikSwap.bc)
set_tests_properties(matmul_ts PROPERTIES DEPENDS matmul_cartographer)

add_test(NAME matmul_swap COMMAND clang-9 ${CMAKE_CURRENT_BINARY_DIR}/tik.bc ${CMAKE_CURRENT_BINARY_DIR}/tikSwap.bc -o ${CMAKE_CURRENT_BINARY_DIR}/swap.exec)
set_tests_properties(matmul_swap PROPERTIES DEPENDS matmul_ts)

add_test(NAME matmulSwap_run COMMAND ${CMAKE_CURRENT_BINARY_DIR}/swap.exec)
set_tests_properties(matmulSwap_run PROPERTIES DEPENDS matmul_swap)