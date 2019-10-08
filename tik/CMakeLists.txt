file(GLOB SOURCES "*.cpp")

add_executable(tik ${SOURCES})

set_target_properties(tik
	PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin" 
)

target_link_libraries(tik PRIVATE ${llvm_libs})
target_compile_definitions(tik PRIVATE ${LLVM_DEFINITIONS})
target_include_directories(tik PRIVATE ${LLVM_INCLUDE_DIRS} "include" ${DEP_INC})

option(BUILD_TIK_APPS "Build tik test applications" OFF)

if(${BUILD_TIK_APPS})
    add_subdirectory(TestApplications)
endif()