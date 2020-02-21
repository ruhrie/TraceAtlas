add_executable(1DCondition 1DCondition.c)
set_target_properties(1DCondition PROPERTIES LINK_FLAGS "-Wl,--plugin-opt=emit-llvm")
target_compile_options(1DCondition PRIVATE "-flto")
InjectTracer(1DCondition)

add_test(NAME 1DCondition_Trace COMMAND 1DCondition-trace)

add_test(NAME 1DCondition_cartographer COMMAND cartographer -i ${CMAKE_CURRENT_BINARY_DIR}/raw.trc -b $<TARGET_FILE:1DCondition> -k ${CMAKE_CURRENT_BINARY_DIR}/kernel.json -p ${CMAKE_CURRENT_BINARY_DIR}/pig.json)
set_tests_properties(1DCondition_cartographer PROPERTIES DEPENDS 1DCondition_Trace)

add_test(NAME 1DCondition_tik COMMAND tik -j ${CMAKE_CURRENT_BINARY_DIR}/kernel.json -o ${CMAKE_CURRENT_BINARY_DIR}/tik.bc $<TARGET_FILE:1DCondition>)
set_tests_properties(1DCondition_tik PROPERTIES DEPENDS 1DCondition_cartographer)

add_test(NAME 1DCondition_dag COMMAND dagExtractor -t ${CMAKE_CURRENT_BINARY_DIR}/raw.trc -o ${CMAKE_CURRENT_BINARY_DIR}/dag.json -k ${CMAKE_CURRENT_BINARY_DIR}/kernel.json)
set_tests_properties(1DCondition_dag PROPERTIES DEPENDS 1DCondition_cartographer)