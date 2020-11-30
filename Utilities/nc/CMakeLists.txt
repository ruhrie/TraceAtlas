add_executable(nc nc.cpp Kernel.cpp Dijkstra.cpp GraphTransforms.cpp)
target_include_directories(nc PRIVATE include)
target_link_libraries(nc PRIVATE ${llvm_libs} nlohmann_json nlohmann_json::nlohmann_json AtlasUtil)
set_target_properties(nc PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")