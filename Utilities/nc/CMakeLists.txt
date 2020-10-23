add_executable(nc nc.cpp)
target_link_libraries(nc PRIVATE ${llvm_libs} nlohmann_json nlohmann_json::nlohmann_json AtlasUtil)