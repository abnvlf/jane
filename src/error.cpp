#include "include/error.hpp"

const char *err_str(int err) {
  switch ((enum Error)err) {
  case ErrorNoMem:
    return "error: out of memory";
  case ErrorInvalidFormat:
    return "error: invalid format";
  }
  return "(invalid error)";
}