cmake_minimum_required(VERSION 3.13)
project(TraceAtlas VERSION 1.0.0)

SET(CMAKE_CXX_STANDARD 17)
SET(CMAKE_C_STANDARD 11)

find_package(LLVM 9 REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
llvm_map_components_to_libnames(llvm_libs support core irreader bitreader bitwriter transformutils)

#use the llvm toolchain
#we fix these ourselves since we don't support gcc anyway
#I want windows support, but passes on windows are tricky anyway so that is a secondary goal
set(CMAKE_C_COMPILER ${LLVM_INSTALL_PREFIX}/bin/clang)
set(CMAKE_CXX_COMPILER ${LLVM_INSTALL_PREFIX}/bin/clang++)
set(CMAKE_LINKER ${LLVM_INSTALL_PREFIX}/bin/ld.lld)
set(CMAKE_AR ${LLVM_INSTALL_PREFIX}/bin/llvm-ar)
set(CMAKE_ADDR2LINE ${LLVM_INSTALL_PREFIX}/bin/llvm-addr2line)
set(CMAKE_NM ${LLVM_INSTALL_PREFIX}/bin/llvm-nm)
set(CMAKE_OBJCOPY ${LLVM_INSTALL_PREFIX}/bin/llvm-objcopy)
set(CMAKE_OBJDUMP ${LLVM_INSTALL_PREFIX}/bin/llvm-objdump)
set(CMAKE_RANLIB ${LLVM_INSTALL_PREFIX}/bin/llvm-ranlib)
set(CMAKE_READELF ${LLVM_INSTALL_PREFIX}/bin/llvm-readelf)
set(CMAKE_STRIP ${LLVM_INSTALL_PREFIX}/bin/llvm-strip)

#other dependencies, provided by vcpkg but you can get them elsewhere as long as you have a cmake file
find_package(ZLIB REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED) #nlohmann_json
find_package(spdlog CONFIG REQUIRED) #spdlog
find_package(indicators CONFIG REQUIRED) #indicators

#clang-tidy options
option(ENABLE_LINTER "Run linter" OFF)
if(${ENABLE_LINTER})
    find_program(TIDY NAMES clang-tidy-9 clang-tidy)
    if(${TIDY} STREQUAL "TIDY-NOTFOUND")
        message(FATAL_ERROR "clang-tidy not found! Necessary for linting!")
    else()
        message(STATUS "Found clang-tidy in ${TIDY}")
    endif()
    
    set(CMAKE_CXX_CLANG_TIDY 
        ${TIDY};
        --warnings-as-errors=*;
        --checks=readability-*,-readability-magic-numbers,performance-*,portability-*,modernize-*,-modernize-use-trailing-return-type,-readability-uppercase-literal-suffix;
        )
    set(CMAKE_C_CLANG_TIDY 
        ${TIDY};
        --warnings-as-errors=*;
        --checks=readability-*,-readability-magic-numbers,performance-*,portability-*,-clang-analyzer-security.insecureAPI.*,-readability-uppercase-literal-suffix;
        )
    add_compile_options(-Werror)
endif()

if(WIN32)
    add_compile_definitions(_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS)
endif()

add_subdirectory("AtlasUtil")
add_subdirectory("cartographer")
add_subdirectory("TraceInfrastructure")
add_subdirectory("tik")
add_subdirectory("deat")
add_subdirectory("kar")
add_subdirectory("Utilities")
add_subdirectory("hydra")

#responsible for injecting the tracer. Fairly fragile so be careful
function(InjectTracer tar)
    target_compile_options(${tar} PRIVATE "-flto" "-g")
    set_target_properties(${tar} PROPERTIES LINK_FLAGS "-fuse-ld=lld -Wl,--plugin-opt=emit-llvm")
    add_custom_command(OUTPUT opt.bc
        COMMAND ${LLVM_INSTALL_PREFIX}/bin/opt -load $<TARGET_FILE:AtlasPasses> -Markov $<TARGET_FILE:${tar}> -o opt.bc
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

#our unit tests
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

#stolen from https://github.com/ttroy50/cmake-examples/blob/master/04-static-analysis/clang-format/cmake/modules/clang-format.cmake
#used to run clang-format on the entire source tree
find_program(FORMAT NAMES clang-format-9 clang-format)
if(${FORMAT} STREQUAL "FORMAT-NOTFOUND")
    message(WARNING "clang-format not found. Disabling formatting target")
else()
    message(STATUS "Found clang-format in ${FORMAT}")
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
        COMMAND ${FORMAT}
        -style=file
        -i
        ${ALL_SOURCE_FILES}
    )
endif()