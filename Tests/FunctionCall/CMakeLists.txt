add_executable(FunctionCall FunctionCall.c)
set_target_properties(FunctionCall PROPERTIES LINK_FLAGS "-Wl,--plugin-opt=emit-llvm")
target_compile_options(FunctionCall PRIVATE "-flto")
InjectTracer(FunctionCall)

add_test(NAME FunctionCall_Trace COMMAND FunctionCall-trace)
set_tests_properties(FunctionCall_Trace PROPERTIES 
    FIXTURES_SETUP functioncall_trace_fixutre
    )

add_test(NAME FunctionCall_cartographer COMMAND cartographer -i ${CMAKE_CURRENT_BINARY_DIR}/raw.trc -b $<TARGET_FILE:FunctionCall> -k ${CMAKE_CURRENT_BINARY_DIR}/kernel.json)
set_tests_properties(FunctionCall_cartographer PROPERTIES 
    DEPENDS FunctionCall_Trace
    FIXTURES_REQUIRED functioncall_trace_fixutre
    FIXTURES_SETUP functioncall_cartographer_fixutre
    )

add_test(NAME FunctionCall_tik COMMAND tik -j ${CMAKE_CURRENT_BINARY_DIR}/kernel.json -o ${CMAKE_CURRENT_BINARY_DIR}/tik.bc $<TARGET_FILE:FunctionCall>)
set_tests_properties(FunctionCall_tik PROPERTIES 
    DEPENDS FunctionCall_cartographer
    FIXTURES_REQUIRED  functioncall_cartographer_fixutre
    )

add_test(NAME FunctionCall_dag COMMAND dagExtractor -t ${CMAKE_CURRENT_BINARY_DIR}/raw.trc -o ${CMAKE_CURRENT_BINARY_DIR}/dag.json -k ${CMAKE_CURRENT_BINARY_DIR}/kernel.json)
set_tests_properties(FunctionCall_dag PROPERTIES 
    DEPENDS FunctionCall_cartographer
    FIXTURES_REQUIRED  functioncall_cartographer_fixutre
    )