cmake_minimum_required(VERSION 2.8)

project(jane C CXX)
set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "choose type of build, option are: Debug Release MinSizeRel" FORCE)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})
set(JANE_VERSION_MAJOR 1)
set(JANE_VERSION_MINOR 0)
set(JANE_VERSION_PATCH 0)
set(JANE_VERSION "${JANE_VERSION_MAJOR}.${JANE_VERSION_MINOR}.${JANE_VERSION_PATCH}")
message("config jane version ${JANE_VERSION}")

find_package(llvm)
include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBDIRS})

include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}
)

set(JANE_SOURCES
    "${CMAKE_SOURCE_DIR}/src/analyze.cpp"
    "${CMAKE_SOURCE_DIR}/src/buffer.cpp"
    "${CMAKE_SOURCE_DIR}/src/error.cpp"
    "${CMAKE_SOURCE_DIR}/src/main.cpp"
    "${CMAKE_SOURCE_DIR}/src/parser.cpp"
    "${CMAKE_SOURCE_DIR}/src/tokenizer.cpp"
    "${CMAKE_SOURCE_DIR}/src/util.cpp"
    "${CMAKE_SOURCE_DIR}/src/codegen.cpp"
    "${CMAKE_SOURCE_DIR}/src/jane_llvm.cpp"
    "${CMAKE_SOURCE_DIR}/src/os.cpp"
)

set(CONFIGURE_OUT_FILE "${CMAKE_BINARY_DIR}/config.h")
configure_file("${CMAKE_SOURCE_DIR}/src/config.h.in" ${CONFIGURE_OUT_FILE})

include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}
    "${CMAKE_SOURCE_DIR}/src"
)

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Wno-unused-variable -Wno-unused-but-set-variable")

set(EXE_CFLAGS "-std=c++11 -fno-exceptions -fno-rtti -D_GNU_SOURCE -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS -Werror -Wall")

add_executable(jane ${JANE_SOURCES})
set_target_properties(jane PROPERTIES
    COMPILE_FLAGS ${EXE_CFLAGS})
target_link_libraries(jane LINK_PUBLIC
    ${LLVM_LIBRARIES}
)
install(TARGETS jane DESTINATION bin)

# add_executable(parsergenerator ${PARSERGENERATOR_SOURCES})
# set_target_properties(parsergenerator PROPERTIES
#     LINKER_LANGUAGE C
#     COMPILE_FLAGS ${EXE_CFLAGS}
# )

# add_custom_command(
#     OUTPUT ${PARSER_GENERATED_CPP}
#     COMMAND parsergeneratorerator ARGS ${GRAMMAR_TXT} ${PARSER_GENERATED_CPP}
#     DEPENDS ${GRAMMAR_TXT} ${PARSERGENERATOR_SOURCES}
#     WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
# )
