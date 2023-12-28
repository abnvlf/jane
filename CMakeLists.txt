cmake_minimum_required(VERSION 2.8)

project(jane C CXX)
set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "choose type of build, option are: Debug Release MinSizeRel" FORCE)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})
set(JANE_VERSION_MAJOR 1)
set(JANE_VERSION_MINOR 0)
set(JANE_VERSION "${JANE_VERSION_MAJOR}.${JANE_VERSION_MINOR}")
message("config jane version ${JANE_VERSION}")

find_package(llvm)
include_directories(${LLVM_INCLUDE_DIR})

include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR})

set(JANE_SOURCES "${CMAKE_SOURCE_DIR}/src/main.cpp" "${CMAKE_SOURCE_DIR}/src/util.cpp" "${CMAKE_SOURCE_DIR}/src/buffer.cpp")

set(CONFIGURE_OUT_FILE "${CMAKE_BINARY_DIR}/config.h")
configure_file("${CMAKE_SOURCE_DIR}/src/config.h.in" ${CONFIGURE_OUT_FILE})

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Wno-unused-variable -Wno-unused-but-set-variable")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wno-unused-variable -Wno-unused-but-set-variable")

set(EXE_CXX_FLAGS "-std=c++11 -Werror -Wall")
set(EXE_C_FLAGS "-std=c11 -Werror -Wall -Wno-error=strict-prototypes -Wno-error=old-style-definition -Wno-error=missing-prototypes")

# Apply CXX flags for C++ sources and C flags for C sources
foreach(source_file IN LISTS JANE_SOURCES)
    get_filename_component(source_extension ${source_file} EXT)
    if (${source_extension} STREQUAL ".cpp" OR ${source_extension} STREQUAL ".cxx" OR ${source_extension} STREQUAL ".cc")
        set_source_files_properties(${source_file} PROPERTIES COMPILE_FLAGS ${EXE_CXX_FLAGS})
    else()
        set_source_files_properties(${source_file} PROPERTIES COMPILE_FLAGS ${EXE_C_FLAGS})
    endif()
endforeach()

add_executable(jane ${JANE_SOURCES})
target_link_libraries(jane LINK_PUBLIC ${LLVM_LIBRARIES})
install(TARGETS jane DESTINATION bin)