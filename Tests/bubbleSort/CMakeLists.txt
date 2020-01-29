add_executable(bubbleSort bubbleSort.c input.c)
target_include_directories(bubbleSort PRIVATE $ENV{DASH_ROOT}/include/opencv4/)

set_target_properties(bubbleSort PROPERTIES LINK_FLAGS "-Wl,--plugin-opt=emit-llvm")
target_compile_options(bubbleSort PRIVATE "-flto")
InjectTracer(bubbleSort)

add_test(NAME bubbleSort_Trace COMMAND sh -c "$<TARGET_FILE:bubbleSort>-trace 512 < $ENV{DASH_DATA}bubbleSort/input_length512.txt")

add_test(NAME bubbleSort_cartographer COMMAND cartographer -i ${CMAKE_CURRENT_BINARY_DIR}/raw.trc -b $<TARGET_FILE:bubbleSort> -k ${CMAKE_CURRENT_BINARY_DIR}/kernel.json)
set_tests_properties(bubbleSort_cartographer PROPERTIES DEPENDS bubbleSort_Trace)

add_test(NAME bubbleSort_tik COMMAND tik -j ${CMAKE_CURRENT_BINARY_DIR}/kernel.json -o ${CMAKE_CURRENT_BINARY_DIR}/tik.bc $<TARGET_FILE:bubbleSort>)
set_tests_properties(bubbleSort_tik PROPERTIES DEPENDS bubbleSort_cartographer)
