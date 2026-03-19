#include "ftransfer/sender.h"
#include "ftransfer/compress.h"
#include "ftransfer/net.h"

#include <cerrno>
#include <cstring>
#include <iostream>
#include <stdexcept>

#include <fcntl.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

// Todo: vmsplice() or io_uring
static void pipe_to_socket(int pipe_fd, int sock_fd) {
  char buf[IO_BUF_SIZE];
  size_t total = 0;

  while (true) {
    ssize_t n = read(pipe_fd, buf, sizeof(buf));
    if (n > 0) {
      if (write_all(sock_fd, buf, static_cast<size_t>(n)) < 0) {
        throw std::runtime_error(std::string("write to socket error: ") +
                                 strerror(errno));
      }
      total += static_cast<size_t>(n);
    } else if (n == 0) {
      break;
    } else {
      if (errno == EINTR) // ignore interrupt
        continue;
      throw std::runtime_error(std::string("read: ") + strerror(errno));
    }
  }

  std::cerr << "sent " << (total / (1024 * 1024)) << " MB (raw)\n";
}

// Returns pid via out param so caller can waitpid.
static int spawn_tar(const std::string &path, pid_t &child_pid) {
  int pipefd[2];
  if (pipe2(pipefd, O_CLOEXEC) < 0)
    throw std::runtime_error(std::string("swpan_tar pipe2: ") +
                             strerror(errno));

  fcntl(pipefd[0], F_SETPIPE_SZ, static_cast<int>(SOCK_BUF_SIZE));

  pid_t pid = fork();
  if (pid < 0) {
    close(pipefd[0]);
    close(pipefd[1]);
    throw std::runtime_error(std::string("tar fork: ") + strerror(errno));
  }

  // child write to pipe, parent read
  if (pid == 0) {
    // Child: redirect stdout to pipe write end
    close(pipefd[0]);
    dup2(pipefd[1], STDOUT_FILENO);
    close(pipefd[1]);

    execlp("tar", "tar", "cf", "-", "-C", path.c_str(), ".", nullptr);
    perror("execlp tar");
    _exit(1);
  }

  // Parent
  close(pipefd[1]);
  child_pid = pid;
  return pipefd[0];
}

void send_files(const SendOpts &opts) {
  struct stat st;
  if (stat(opts.path.c_str(), &st) < 0) {
    throw std::runtime_error("path not found: " + opts.path);
  }

  std::cerr << "connecting to " << opts.host << ":" << opts.port << " ...\n";
  int sockfd = connect_to(opts.host, opts.port);
  std::cerr << "connected. sending " << opts.path << " ...\n";

  pid_t tar_pid;
  int tar_fd = spawn_tar(opts.path, tar_pid);

  try {
    if (opts.zstd_level > 0) {
      // Compressed path: tar -> zstd compress -> socket
      std::cerr << "[ftransfer] compression: zstd level " << opts.zstd_level
                << "\n";

      size_t total_sent = 0;

      compress_stream(
          // read callback: read from tar pipe
          [&](void *buf, size_t cap) -> ssize_t {
            while (true) {
              ssize_t n = read(tar_fd, buf, cap);
              if (n >= 0)
                return n;
              if (errno == EINTR)
                continue;
              return -1;
            }
          },
          // write callback: write compressed data to socket
          [&](const void *data, size_t size) -> bool {
            if (write_all(sockfd, data, size) < 0)
              return false;
            total_sent += size;
            return true;
          },
          opts.zstd_level);

      std::cerr << "[ftransfer] sent " << (total_sent / (1024 * 1024))
                << " MB (compressed)\n";

    } else {
      // Uncompressed: tar pipe -> socket via splice (zero-copy)
      std::cerr << "[ftransfer] compression: none (raw tar)\n";
      pipe_to_socket(tar_fd, sockfd);
    }
  } catch (...) {
    close(tar_fd);
    close(sockfd);
    throw;
  }

  close(tar_fd);
  close(sockfd);

  // Wait for tar to finish
  int status;
  waitpid(tar_pid, &status, 0);

  std::cerr << "[ftransfer] done.\n";
}
