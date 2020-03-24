cmake_minimum_required(VERSION 3.10)
project(TraceAtlas VERSION 0.2.1)

SET(CMAKE_CXX_STANDARD 17)
SET(CMAKE_C_STANDARD 11)

find_package(LLVM 9 REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
llvm_map_components_to_libnames(llvm_libs  support core irreader bitreader bitwriter transformutils)

find_package(ZLIB REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED) #nlohmann_json
find_package(spdlog CONFIG REQUIRED) #spdlog
find_package(indica CONFIG REQUIRED) #indicators

function(MakeIR target)
    set_target_properties(LINK_OPTIONS "-Wl,--plugin-opt=emit-llvm")
endfunction()

set(CMAKE_EXE_LINKER_FLAGS "-fuse-ld=lld-9")

add_subdirectory("AtlasUtil")
add_subdirectory("cartographer")
add_subdirectory("TraceInfrastructure")
add_subdirectory("tik")
add_subdirectory("Utilities")
add_subdirectory("wstool")

function(InjectTracer tar)
    add_custom_command(OUTPUT opt.bc
        COMMAND opt-9 -load $<TARGET_FILE:AtlasPasses> -EncodedTrace $<TARGET_FILE:${tar}> -o opt.bc
        DEPENDS $<TARGET_FILE:${tar}>
    )
    set_source_files_properties(
        opt.bc
        PROPERTIES
        EXTERNAL_OBJECT true
        GENERATED true
    )
    add_executable(${tar}-trace opt.bc)
    set_target_properties(${tar}-trace PROPERTIES LINKER_LANGUAGE CXX)
    target_link_libraries(${tar}-trace PRIVATE AtlasBackend)
endfunction()

option(ENABLE_TESTING "Build tests" ON)
option(ENABLE_TESTING_LONG "Build tests long" OFF)
if(${ENABLE_TESTING})
    enable_testing()
    add_subdirectory(Tests)
endif()

find_package(Doxygen)
if(DOXYGEN_FOUND)
    configure_file(DoxygenLayout.xml ${CMAKE_CURRENT_BINARY_DIR}/DoxygenLayout.xml @ONLY)
    configure_file(Doxyfile ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
    add_custom_target(doc COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
endif(DOXYGEN_FOUND)
