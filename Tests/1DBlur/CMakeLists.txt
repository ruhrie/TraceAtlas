add_executable(1DBlur 1DBlur.c)
set_target_properties(1DBlur PROPERTIES LINK_FLAGS "-Wl,--plugin-opt=emit-llvm")
target_compile_options(1DBlur PRIVATE "-flto")
InjectTracer(1DBlur)

add_test(NAME 1DBlur_Trace COMMAND 1DBlur-trace)
set_tests_properties(1DBlur_Trace PROPERTIES 
    FIXTURES_SETUP 1dblur_trace_fixutre
    )

add_test(NAME 1DBlur_cartographer COMMAND cartographer -i ${CMAKE_CURRENT_BINARY_DIR}/raw.trc -b $<TARGET_FILE:1DBlur> -k ${CMAKE_CURRENT_BINARY_DIR}/kernel.json)
set_tests_properties(1DBlur_cartographer PROPERTIES 
    DEPENDS 1DBlur_Trace
    FIXTURES_REQUIRED 1dblur_trace_fixutre
    FIXTURES_SETUP 1dblur_cartographer_fixutre
    )

add_test(NAME 1DBlur_tik COMMAND tik -j ${CMAKE_CURRENT_BINARY_DIR}/kernel.json -o ${CMAKE_CURRENT_BINARY_DIR}/tik.bc $<TARGET_FILE:1DBlur>)
set_tests_properties(1DBlur_tik PROPERTIES 
    DEPENDS 1DBlur_cartographer
    FIXTURES_REQUIRED  1dblur_cartographer_fixutre
    )

add_test(NAME 1DBlur_dag COMMAND dagExtractor -t ${CMAKE_CURRENT_BINARY_DIR}/raw.trc -o ${CMAKE_CURRENT_BINARY_DIR}/dag.json -k ${CMAKE_CURRENT_BINARY_DIR}/kernel.json)
set_tests_properties(1DBlur_dag PROPERTIES 
    DEPENDS 1DBlur_cartographer
    FIXTURES_REQUIRED 1dblur_cartographer_fixutre
    )
