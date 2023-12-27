find_path(LLVM_INCLUDE_DIR NAMES llvm-c/Core.h)
find_program(LLVM_CONFIG_EXE llvm-config)
execute_process(COMMAND ${LLVM_CONFIG_EXE} --libs OUTPUT_VARIABLE LLVM_LIBRARIES OUTPUT_STRIP_TRAILING_WHITESPACE)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LLVM DEFAULT_MSG LLVM_LIBRARIES LLVM_INCLUDE_DIR)

mark_as_advanced(LLVM_INCLUDE_DIR LLVM_LIBRARIES)