add_executable(bubbleSort bubbleSort.c)
target_include_directories(bubbleSort PRIVATE ${CMAKE_SOURCE_DIR}/TraceInfrastructure/include/)
InjectTracer(bubbleSort)

add_test(NAME bubbleSort_Trace COMMAND bubbleSort-trace 512)