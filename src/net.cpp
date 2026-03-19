#include "ftransfer/net.h"
#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

int create_listener(uint16_t port) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    throw std::runtime_error(std::string("socket error ") + strerror(errno));
  }

  int on = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

  tune_socket(fd);

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = INADDR_ANY;

  if (0 > bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr))) {
    close(fd);
    throw std::runtime_error(std::string("failed to bind: ") + strerror(errno));
  }

  if (listen(fd, 1) < 0) { // receiving from one sender only
    close(fd);
    throw std::runtime_error(std::string("error on listen: ") +
                             strerror(errno));
  }
  return fd;
}

int accept_connection(int listen_fd) {
  sockaddr_in client_addr{};
  socklen_t len = sizeof(client_addr);

  int fd = accept(listen_fd, reinterpret_cast<sockaddr *>(&client_addr), &len);
  if (fd < 0) {
    throw std::runtime_error(std::string("error on socket accept: ") +
                             strerror(errno));
  }

  close(listen_fd); // one at a time ToDo: keep alive and loop?

  tune_socket(fd);

  char host[INET_ADDRSTRLEN];
  if (!inet_ntop(AF_INET, &client_addr.sin_addr, host, sizeof(host))) {
    std::cerr << "error on IP binary convert\n";
  }

  return fd;
}

int connect_to(const std::string &host, uint16_t port) {
  addrinfo hints{};
  hints.ai_family = AF_INET; // IPv4 only
  hints.ai_socktype = SOCK_STREAM;

  std::string port_str = std::to_string(port);
  addrinfo *result = nullptr;

  int err = getaddrinfo(host.c_str(), port_str.c_str(), &hints, &result);
  if (err != 0) {
    throw std::runtime_error(std::string("getaddrinfo: ") + gai_strerror(err));
  }

  int fd = -1;
  for (addrinfo *rp = result; rp; rp = rp->ai_next) {
    fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (fd < 0)
      continue;

    if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0)
      break;

    close(fd);
    fd = -1;
  }

  freeaddrinfo(result);

  if (fd < 0) {
    throw std::runtime_error("connect_to: could not connect to " + host + ":" +
                             port_str);
  }

  tune_socket(fd);
  return fd;
}

void tune_socket(int fd) {
  int buf_size = static_cast<int>(SOCK_BUF_SIZE);
  setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &buf_size, sizeof(buf_size));
  setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &buf_size, sizeof(buf_size));
}

ssize_t write_all(int fd, const void *buf, size_t len) {
  const char *p = static_cast<const char *>(buf);
  size_t remaining = len;

  while (remaining > 0) {
    ssize_t n = write(fd, p, remaining);
    if (n < 0) {
      if (errno == EINTR)
        continue;
      return -1;
    }
    p += n;
    remaining -= static_cast<size_t>(n);
  }

  return static_cast<ssize_t>(len);
}
