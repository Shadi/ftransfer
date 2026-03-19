#ifndef NET_H
#define NET_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <sys/types.h>

constexpr size_t SOCK_BUF_SIZE = 4 * 1024 * 1024;
constexpr size_t IO_BUF_SIZE = 256 * 1024;

int create_listener(uint16_t port);

int accept_connection(int listen_fd);

int connect_to(const std::string &host, uint16_t port);

void tune_socket(int fd);

ssize_t write_all(int fd, const void *buf, size_t len);

#endif // !NET_H
