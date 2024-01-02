#ifndef JANE_OS
#define JANE_OS

#include "buffer.hpp"
#include "list.hpp"

void os_spawn_process(const char *exe, JaneList<const char *> &args,
                      bool detached);
void os_path_split(Buf *full_path, Buf *out_dirname, Buf *out_basename);
void os_exec_process(const char *exe, JaneList<const char *> &args,
                     int *return_code, Buf *out_stderr, Buf *out_stdout);
void os_write_file(Buf *full_path, Buf *contents);

#endif // JANE_OS