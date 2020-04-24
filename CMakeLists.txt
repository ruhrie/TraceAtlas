cmake_minimum_required(VERSION 3.12)
project(TraceAtlas VERSION 0.2.2)

SET(CMAKE_CXX_STANDARD 20)
SET(CMAKE_C_STANDARD 11)

find_package(LLVM 9 REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
llvm_map_components_to_libnames(llvm_libs  support core irreader bitreader bitwriter transformutils)

find_package(ZLIB REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED) #nlohmann_json
find_package(spdlog CONFIG REQUIRED) #spdlog
find_package(indicators CONFIG REQUIRED) #indicators

option(ENABLE_LINTER_ERROR "Run linter with errors" OFF)
if(${ENABLE_LINTER_ERROR})
    set(CMAKE_CXX_CLANG_TIDY 
        clang-tidy-9;
        --warnings-as-errors=*;
        --checks=readability-*,-readability-magic-numbers,performance-*,portability-*,modernize-*,-modernize-use-trailing-return-type;
        )
    add_compile_options(-Werror)
else()
    set(CMAKE_CXX_CLANG_TIDY 
        clang-tidy-9;
        --checks=readability-*,-readability-magic-numbers,performance-*,portability-*,modernize-*,-modernize-use-trailing-return-type;
        )
endif()

add_subdirectory("AtlasUtil")
add_subdirectory("cartographer")
add_subdirectory("TraceInfrastructure")
add_subdirectory("tik")
add_subdirectory("deat")
add_subdirectory("Utilities")

function(InjectTracer tar)
    set_target_properties(${tar} PROPERTIES LINK_OPTIONS "-Wl,--plugin-opt=emit-llvm")
    target_compile_options(${tar} PRIVATE "-flto")
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

#stollen from https://github.com/ttroy50/cmake-examples/blob/master/04-static-analysis/clang-format/cmake/modules/clang-format.cmake
set(CLANG_FORMAT_EXCLUDE_PATTERNS "build" "vcpkg")
file(GLOB_RECURSE ALL_SOURCE_FILES *.cpp *.h *.c)
foreach (SOURCE_FILE ${ALL_SOURCE_FILES})
    foreach (EXCLUDE_PATTERN ${CLANG_FORMAT_EXCLUDE_PATTERNS})
        string(FIND ${SOURCE_FILE} ${EXCLUDE_PATTERN} PROJECT_TRDPARTY_DIR_FOUND)
        if (NOT ${PROJECT_TRDPARTY_DIR_FOUND} EQUAL -1)
            list(REMOVE_ITEM ALL_SOURCE_FILES ${SOURCE_FILE})
        endif ()
    endforeach()
endforeach ()

add_custom_target(
    format
    COMMAND /usr/bin/clang-format-9
    -style=file
    -i
    ${ALL_SOURCE_FILES}
)