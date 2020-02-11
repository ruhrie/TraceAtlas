add_executable(2DConv 2DConv.c)
set_target_properties(2DConv PROPERTIES LINK_FLAGS "-Wl,--plugin-opt=emit-llvm")
target_compile_options(2DConv PRIVATE "-flto")
InjectTracer(2DConv)

add_test(NAME 2DConv_Trace COMMAND 2DConv-trace)

add_test(NAME 2DConv_cartographer COMMAND cartographer -i ${CMAKE_CURRENT_BINARY_DIR}/raw.trc -b $<TARGET_FILE:2DConv> -k ${CMAKE_CURRENT_BINARY_DIR}/kernel.json)
set_tests_properties(2DConv_cartographer PROPERTIES DEPENDS 2DConv_Trace)

add_test(NAME 2DConv_tik COMMAND tik -j ${CMAKE_CURRENT_BINARY_DIR}/kernel.json -o ${CMAKE_CURRENT_BINARY_DIR}/tik.bc $<TARGET_FILE:2DConv>)
set_tests_properties(2DConv_tik PROPERTIES DEPENDS 2DConv_cartographer)

add_test(NAME 2DConv_dag COMMAND dagExtractor -t ${CMAKE_CURRENT_BINARY_DIR}/raw.trc -o ${CMAKE_CURRENT_BINARY_DIR}/dag.json -k ${CMAKE_CURRENT_BINARY_DIR}/kernel.json)
set_tests_properties(2DConv_dag PROPERTIES DEPENDS 2DConv_cartographer)