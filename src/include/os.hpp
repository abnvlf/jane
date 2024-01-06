#ifndef JANE_OS
#define JANE_OS

#include "buffer.hpp"
#include "list.hpp"

#include <stdio.h>

void os_spawn_process(const char *exe, JaneList<const char *> &args,
                      bool detached);
void os_path_split(Buf *full_path, Buf *out_dirname, Buf *out_basename);
void os_exec_process(const char *exe, JaneList<const char *> &args,
                     int *return_code, Buf *out_stderr, Buf *out_stdout);
void os_path_join(Buf *dirname, Buf *basename, Buf *out_full_path);
void os_write_file(Buf *full_path, Buf *contents);
int os_fetch_file(FILE *file_buf, Buf *out_contents);
int os_fetch_file_path(Buf *full_path, Buf *out_contents);
int os_get_cwd(Buf *out_cwd);

#endif // JANE_OS