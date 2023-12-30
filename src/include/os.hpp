#ifndef JANE_OS
#define JANE_OS

#include "buffer.hpp"
#include "list.hpp"

void os_spawn_process(const char *exe, JaneList<const char *> &args,
                      bool detached);
void os_path_split(Buf *full_path, Buf *out_dirname, Buf *out_basename);

#endif // JANE_OS