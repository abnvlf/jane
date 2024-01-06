#include "include/os.hpp"
#include "include/util.hpp"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
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

static void read_all_fd_stream(int fd, Buf *out_buf) {
  static const ssize_t buf_size = 0x2000;
  buf_resize(out_buf, buf_size);
  ssize_t actual_buf_len = 0;
  for (;;) {
    ssize_t amt_read = read(fd, buf_ptr(out_buf), buf_len(out_buf));
    if (amt_read < 0) {
      jane_panic("fd read error");
      return;
    }
    if (amt_read == 0) {
      buf_resize(out_buf, actual_buf_len);
      return;
    }
    buf_resize(out_buf, actual_buf_len + buf_size);
  }
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

void os_path_join(Buf *dirname, Buf *basename, Buf *out_full_path) {
  buf_init_from_buf(out_full_path, dirname);
  buf_append_char(out_full_path, '/');
  buf_append_buf(out_full_path, basename);
}

void os_exec_process(const char *exe, JaneList<const char *> &args,
                     int *return_code, Buf *out_stderr, Buf *out_stdout) {
  int stdin_pipe[2];
  int stdout_pipe[2];
  int stderr_pipe[2];
  int err;
  if ((err = pipe(stdin_pipe))) {
    jane_panic("pipe failed");
  }
  if ((err = pipe(stdin_pipe))) {
    jane_panic("pipe failed");
  }
  if ((err = pipe(stdout_pipe))) {
    jane_panic("pipe failed");
  }

  pid_t pid = fork();
  if (pid == -1) {
    jane_panic("fork failed");
  }
  if (pid == 0) {
    if (dup2(stdin_pipe[0], STDIN_FILENO) == -1) {
      jane_panic("dup2 failed");
    }
    if (dup2(stdout_pipe[1], STDOUT_FILENO) == -1) {
      jane_panic("dup 2 failed");
    }
    if (dup2(stderr_pipe[1], STDERR_FILENO) == -1) {
      jane_panic("dup2 failed");
    }

    const char **argv = allocate<const char *>(args.length + 2);
    argv[0] = exe;
    argv[args.length + 1] = nullptr;
    for (int i = 0; i < args.length; i += 1) {
      argv[i + 1] = args.at(i);
    }
    execvp(exe, const_cast<char *const *>(argv));
    jane_panic("execvp failed: %s", strerror(errno));
  } else {
    close(stdin_pipe[0]);
    close(stdout_pipe[1]);
    close(stderr_pipe[1]);
    waitpid(pid, return_code, 0);
    read_all_fd_stream(stdout_pipe[0], out_stdout);
    read_all_fd_stream(stderr_pipe[0], out_stderr);
  }
}

void os_write_file(Buf *full_path, Buf *contents) {
  int fd;
  if ((fd = open(buf_ptr(full_path), O_CREAT | O_CLOEXEC | O_WRONLY | O_TRUNC,
                 S_IRWXU)) == -1) {
    jane_panic("open failed");
  }
  ssize_t amt_wriitten = write(fd, buf_ptr(contents), buf_len(contents));
  if (amt_wriitten != buf_len(contents)) {
    jane_panic("write failed: %s", strerror(errno));
  }
  if (close(fd) == -1) {
    jane_panic("close failed");
  }
}

int os_fetch_file(FILE *f, Buf *out_contents) {
  int fd = fileno(f);
  struct stat st;
  if (fstat(fd, &st)) {
    jane_panic("unable to stat file: %s", strerror(errno));
  }
  off_t big_size = st.st_size;
  if (big_size > INT_MAX) {
    jane_panic("file to bigger");
  }
  int size = (int)big_size;
  buf_resize(out_contents, size);
  ssize_t ret = read(fd, buf_ptr(out_contents), size);
  if (ret != size) {
    jane_panic("unable to read file: %s", strerror(errno));
  }
  return 0;
}

int os_fetch_file_path(Buf *full_path, Buf *out_contents) {
  FILE *f = fopen(buf_ptr(full_path), "rb");
  if (!f) {
    jane_panic("unable to open %s: %s\n", buf_ptr(full_path), strerror(errno));
  }
  int result = os_fetch_file(f, out_contents);
  fclose(f);
  return result;
}

int os_get_cwd(Buf *out_cwd) {
  int err = ERANGE;
  buf_resize(out_cwd, 512);
  while (err == ERANGE) {
    buf_resize(out_cwd, buf_len(out_cwd) * 2);
    err = getcwd(buf_ptr(out_cwd), buf_len(out_cwd)) ? 0 : errno;
  }
  if (err) {
    jane_panic("unable to get cwd: %s", strerror(err));
  }
  return 0;
}