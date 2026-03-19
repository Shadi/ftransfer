#ifndef SENDER_H
#define SENDER_H

#include <cstdint>
#include <string>

struct SendOpts {
  std::string host;
  uint16_t port;
  std::string path;
  int zstd_level; // compression level 0-10
};

void send_files(const SendOpts &opts);

#endif
