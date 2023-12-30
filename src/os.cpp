#include "include/os.hpp"
#include "include/util.hpp"

#include <errno.h>
#include <unistd.h>

void os_spawn_process(const char *exe, JaneList<const char *> &args,
                      bool detached) {
  pid_t pid = fork();
  if (pid == -1) {
    jane_panic("fork failed");
  }
  if (pid != 0) {
    return;
  }
  if (detached) {
    if (setsid() == -1) {
      jane_panic("process detach failed");
    }
  }
  const char **argv = allocate<const char *>(args.length + 2);
  argv[0] = exe;
  argv[args.length + 1] = nullptr;
  for (int i = 0; i < args.length; i += 1) {
    argv[i + 1] = args.at(i);
  }
  execvp(exe, const_cast<char *const *>(argv));
  jane_panic("execvp failed: %s", strerror(errno));
}

void os_path_split(Buf *full_path, Buf *out_dirname, Buf *out_basename) {
  int last_index = buf_len(full_path) - 1;
  if (last_index >= 0 && buf_ptr(full_path)[last_index] == '/') {
    last_index -= 1;
  }
  for (int i = last_index; i >= 0; i -= 1) {
    uint8_t c = buf_ptr(full_path)[i];
    if (c == '/') {
      buf_init_from_mem(out_dirname, buf_ptr(full_path), i);
      buf_init_from_mem(out_basename, buf_ptr(full_path) + i + 1,
                        buf_len(full_path) - (i + 1));
      return;
    }
  }
  buf_init_from_mem(out_dirname, ".", 1);
  buf_init_from_buf(out_basename, full_path);
}