add_executable(Threads Threads.c)
set_target_properties(Threads PROPERTIES LINK_FLAGS "-Wl,--plugin-opt=emit-llvm")
target_compile_options(Threads PRIVATE "-flto")
InjectTracer(Threads)

target_link_libraries(Threads-trace PRIVATE "-lpthread")
#add_test(NAME Threads_Trace COMMAND Threads-trace)

#add_test(NAME Threads_cartographer COMMAND cartographer -i ${CMAKE_CURRENT_BINARY_DIR}/raw.trc -b $<TARGET_FILE:Threads> -k ${CMAKE_CURRENT_BINARY_DIR}/kernel.json)
#set_tests_properties(Threads_cartographer PROPERTIES DEPENDS Threads_Trace)

#add_test(NAME Threads_tik COMMAND tik -j ${CMAKE_CURRENT_BINARY_DIR}/kernel.json -o ${CMAKE_CURRENT_BINARY_DIR}/tik.bc $<TARGET_FILE:Threads>)
#set_tests_properties(Threads_tik PROPERTIES DEPENDS Threads_cartographer)

#add_test(NAME Threads_dag COMMAND dagExtractor -t ${CMAKE_CURRENT_BINARY_DIR}/raw.trc -o ${CMAKE_CURRENT_BINARY_DIR}/dag.json -k ${CMAKE_CURRENT_BINARY_DIR}/kernel.json)
#set_tests_properties(Threads_dag PROPERTIES DEPENDS Threads_cartographer)
