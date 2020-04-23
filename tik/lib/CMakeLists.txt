add_library(libtik SHARED Backend.cpp Kernel.cpp TikHeader.cpp Util.cpp libtik.cpp KernelLabel.cpp CartographerKernel.cpp)
target_link_libraries(libtik PRIVATE ${llvm_libs} nlohmann_json nlohmann_json::nlohmann_json spdlog::spdlog spdlog::spdlog_header_only AtlasUtil)
target_compile_definitions(libtik PRIVATE ${LLVM_DEFINITIONS})
target_include_directories(libtik SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_include_directories(libtik PUBLIC "include/")
target_compile_options(libtik PRIVATE -Wall -Wextra -Wconversion -fPIC)
set_target_properties(libtik
    PROPERTIES
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)
if(${CMAKE_BUILD_TYPE} MATCHES Debug)
    target_compile_definitions(libtik PRIVATE DEBUG)
endif()
install(TARGETS libtik LIBRARY DESTINATION lib)
