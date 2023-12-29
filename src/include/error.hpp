#ifndef JANE_ERROR
#define JANE_ERROR

enum Error {
  ErrorNoMem,
  ErrorInvalidFormat,
};

const char *err_str(int err);

#endif // JANE_ERROR