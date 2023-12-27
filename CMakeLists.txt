cmake_minimum_required(VERSION 2.8)

project(jane C)
set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "chose type of build, option are: Debug Release MinSizeRel" FORCE)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})
set(JANE_VERSION_MAJOR 1)
set(JANE_VERSION_MINOR 0)
set(JANE_VERSION "${JANE_VERSION_MAJOR}.${JANE_VERSION_MINOR}")
message("config jane version ${JANE_VERSION}")

find_package(llvm)
include_directories(${LLVM_INCLUDE_DIR})

include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR})

set(JANE_SOURCES "${CMAKE_SOURCE_DIR}/src/main.cpp" "${CMAKE_SOURCE_DIR}/src/util.cpp")

set(CONFIGURE_OUT_FILE "${CMAKE_BINARY_DIR}/config.h")
configure_file("${CMAKE_SOURCE_DIR}/src/config.h.in" ${CONFIGURE_OUT_FILE})

set(CMAKE_CXX_IMPLICIT_LINK_LIBRARIES "")
set(CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES "")

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Wno-unused-variable -Wno-unused-but-set-variable")
set(EXE_CFLAGS "-std=c11 -Werror -Wall -Werror=strict-prototypes -Werror=old-style-definition -Werror=missing-prototypes")
set(TEST_CFLAGS "-std=c99 -Werror -Wall")

add_executable(jane ${JANE_SOURCES})
set_target_properties(jane PROPERTIES LINKER_LANGUAGE C COMPILE_FLAGS ${EXE_CFLAGS})
target_link_libraries(jane LINK_PUBLIC ${LLVM_LIBRARIES})
install(TARGETS jane DESTINATION bin)

