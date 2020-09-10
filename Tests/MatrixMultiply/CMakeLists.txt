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
