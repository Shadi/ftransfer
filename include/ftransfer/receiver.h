#ifndef RECEIVER_H
#define RECEIVER_H

#include <cstdint>
#include <string>

struct RecvOpts {
  uint16_t port;
  std::string output_path;
  bool compressed;
};

void receive_files(const RecvOpts &opts);

#endif // !RECIVER_H
