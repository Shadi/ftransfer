#include "ftransfer/receiver.h"
#include "ftransfer/compress.h"
#include "ftransfer/net.h"

#include <cerrno>
#include <cstring>
#include <iostream>
#include <stdexcept>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static int spawn_tar_extract(const std::string &output_dir, pid_t &child_pid) {
  // Ensure output dir exists
  mkdir(output_dir.c_str(), 0755);

  int pipefd[2];
  if (pipe2(pipefd, O_CLOEXEC) < 0) {
    throw std::runtime_error(std::string("tar pipe2: ") + strerror(errno));
  }

  fcntl(pipefd[1], F_SETPIPE_SZ, static_cast<int>(SOCK_BUF_SIZE));

  pid_t pid = fork();
  if (pid < 0) {
    close(pipefd[0]);
    close(pipefd[1]);
    throw std::runtime_error(std::string("fork: ") + strerror(errno));
  }

  if (pid == 0) {
    // Child: redirect stdin from pipe read end
    close(pipefd[1]);
    dup2(pipefd[0], STDIN_FILENO);
    close(pipefd[0]);

    execlp("tar", "tar", "xvf", "-", "-C", output_dir.c_str(), nullptr);
    perror("execlp tar");
    _exit(1);
  }

  // Parent
  close(pipefd[0]);
  child_pid = pid;
  return pipefd[1];
}

static void socket_to_pipe(int sock_fd, int pipe_fd) {
  char buf[IO_BUF_SIZE];
  size_t total = 0;

  while (true) {
    ssize_t n = read(sock_fd, buf, sizeof(buf));
    if (n > 0) {
      if (write_all(pipe_fd, buf, static_cast<size_t>(n)) < 0) {
        throw std::runtime_error(std::string("write to pipe: ") +
                                 strerror(errno));
      }
      total += static_cast<size_t>(n);
    } else if (n == 0) {
      break;
    } else {
      if (errno == EINTR) // ignore interrupt
        continue;
      throw std::runtime_error(std::string("socket read: ") + strerror(errno));
    }
  }

  std::cerr << "[ftransfer] received " << (total / (1024 * 1024))
            << " MB (raw)\n";
}

void receive_files(const RecvOpts &opts) {
  std::cerr << "[ftransfer] listening on port " << opts.port << " ...\n";
  int listen_fd = create_listener(opts.port);
  int client_fd = accept_connection(listen_fd);

  std::cerr << "[ftransfer] receiving into " << opts.output_path << " ...\n";

  pid_t tar_pid;
  int tar_fd = spawn_tar_extract(opts.output_path, tar_pid);

  try {
    if (opts.compressed) {
      std::cerr << "[ftransfer] decompression: zstd\n";

      size_t total_recv = 0;

      decompress_stream(
          [&](void *buf, size_t cap) -> ssize_t {
            while (true) {
              ssize_t n = read(client_fd, buf, cap);
              if (n >= 0) {
                total_recv += static_cast<size_t>(n);
                return n;
              }
              if (errno == EINTR)
                continue;
              return -1;
            }
          },
          // write callback: write decompressed data to tar pipe
          [&](const void *data, size_t size) -> bool {
            return write_all(tar_fd, data, size) >= 0;
          });

      std::cerr << "[ftransfer] received " << (total_recv / (1024 * 1024))
                << " MB (compressed)\n";

    } else {
      // Uncompressed: socket -> tar via splice (zero-copy)
      std::cerr << "[ftransfer] compression: none\n";
      socket_to_pipe(client_fd, tar_fd);
    }
  } catch (...) {
    close(tar_fd);
    close(client_fd);
    throw;
  }

  close(tar_fd);
  close(client_fd);

  // Wait for tar to finish extracting
  int status;
  waitpid(tar_pid, &status, 0);

  std::cerr << "[ftransfer] done.\n";
}
