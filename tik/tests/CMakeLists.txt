add_subdirectory(2DConv)
add_subdirectory(1DBlur)
add_subdirectory(1DCondition)
add_subdirectory(FunctionCall)

add_test(NAME tik_1DBlur_json COMMAND tik -j ${CMAKE_CURRENT_SOURCE_DIR}/1DBlur/kernel.json ${CMAKE_CURRENT_SOURCE_DIR}/1DBlur/1DBlur.ll -o 1DBlur.json -f=JSON )
add_test(NAME tik_1DBlur_llvm COMMAND tik -j ${CMAKE_CURRENT_SOURCE_DIR}/1DBlur/kernel.json ${CMAKE_CURRENT_SOURCE_DIR}/1DBlur/1DBlur.ll -o 1DBlur.bc -f=LLVM )

add_test(NAME tik_1DCondition_json COMMAND tik -j ${CMAKE_CURRENT_SOURCE_DIR}/1DCondition/kernel.json ${CMAKE_CURRENT_SOURCE_DIR}/1DCondition/1DCondition.ll -o 1DCondition.json -f=JSON )
add_test(NAME tik_1DCondition_llvm COMMAND tik -j ${CMAKE_CURRENT_SOURCE_DIR}/1DCondition/kernel.json ${CMAKE_CURRENT_SOURCE_DIR}/1DCondition/1DCondition.ll -o 1DCondition.bc -f=LLVM )

add_test(NAME tik_2DConv_json COMMAND tik -j ${CMAKE_CURRENT_SOURCE_DIR}/2DConv/kernel.json ${CMAKE_CURRENT_SOURCE_DIR}/2DConv/2DConv.ll -o 2DConv.json -f=JSON )
add_test(NAME tik_2DConv_llvm COMMAND tik -j ${CMAKE_CURRENT_SOURCE_DIR}/2DConv/kernel.json ${CMAKE_CURRENT_SOURCE_DIR}/2DConv/2DConv.ll -o 2DConv.bc -f=LLVM )

add_test(NAME tik_FunctionCall_json COMMAND tik -j ${CMAKE_CURRENT_SOURCE_DIR}/FunctionCall/kernel.json ${CMAKE_CURRENT_SOURCE_DIR}/FunctionCall/FunctionCall.ll -o FunctionCall.json -f=JSON )
add_test(NAME tik_FunctionCall_llvm COMMAND tik -j ${CMAKE_CURRENT_SOURCE_DIR}/FunctionCall/kernel.json ${CMAKE_CURRENT_SOURCE_DIR}/FunctionCall/FunctionCall.ll -o FunctionCall.bc -f=LLVM )