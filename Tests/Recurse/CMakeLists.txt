add_executable(Recurse Recurse.c)
set_target_properties(Recurse PROPERTIES LINK_FLAGS "-Wl,--plugin-opt=emit-llvm")
target_compile_options(Recurse PRIVATE "-flto")
InjectTracer(Recurse)

add_test(NAME Recurse_Trace COMMAND Recurse-trace)

add_test(NAME Recurse_cartographer COMMAND cartographer -i ${CMAKE_CURRENT_BINARY_DIR}/raw.trc -b $<TARGET_FILE:Recurse> -k ${CMAKE_CURRENT_BINARY_DIR}/kernel.json)
set_tests_properties(Recurse_cartographer PROPERTIES DEPENDS Recurse_Trace)

add_test(NAME Recurse_tik COMMAND tik -j ${CMAKE_CURRENT_BINARY_DIR}/kernel.json -o ${CMAKE_CURRENT_BINARY_DIR}/tik.bc $<TARGET_FILE:Recurse>)
set_tests_properties(Recurse_tik PROPERTIES DEPENDS Recurse_cartographer)