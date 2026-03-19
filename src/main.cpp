#include "ftransfer/receiver.h"
#include "ftransfer/sender.h"

#include <cstdlib>
#include <iostream>
#include <signal.h>
#include <unistd.h>

static void print_usage(const char *prog) {
  std::cerr << "Usage:\n"
            << "  " << prog << " send   -h HOST -p PORT [-d DIR] [-z LEVEL]\n"
            << "  " << prog << " recv   -p PORT [-d DIR] [-z]\n"
            << "\n"
            << "Options:\n"
            << "  -h HOST    Destination host/IP\n"
            << "  -p PORT    Port number\n"
            << "  -d DIR     Directory to send / extract into (default: .)\n"
            << "  -z [LEVEL] Enable zstd compression (sender: level 1-19, "
               "default 1)\n"
            << "             Receiver: just -z to expect compressed stream\n"
            << "\n"
            << "Examples:\n"
            << "  Receiver:  " << prog << " recv -p 9000 -d ./incoming -z\n"
            << "  Sender:    " << prog
            << " send -h 192.168.1.42 -p 9000 -d ./myfiles -z 3\n";
}

int main(int argc, char *argv[]) {
  signal(SIGPIPE, SIG_IGN);

  if (argc < 2) {
    print_usage(argv[0]);
    return 1;
  }

  std::string mode = argv[1];

  if (mode == "send") {
    SendOpts opts{};
    opts.path = ".";
    opts.zstd_level = 0;

    int opt;
    optind = 2;
    while ((opt = getopt(argc, argv, "h:p:d:z:")) != -1) {
      switch (opt) {
      case 'h':
        opts.host = optarg;
        break;
      case 'p':
        opts.port = static_cast<uint16_t>(std::atoi(optarg));
        break;
      case 'd':
        opts.path = optarg;
        break;
      case 'z':
        opts.zstd_level = std::atoi(optarg);
        break;
      default:
        print_usage(argv[0]);
        return 1;
      }
    }

    if (opts.host.empty() || opts.port == 0) {
      std::cerr << "Error: send requires -h HOST and -p PORT\n\n";
      print_usage(argv[0]);
      return 1;
    }

    if (opts.zstd_level < 0 || opts.zstd_level > 19) {
      std::cerr << "Error: zstd level must be 0-19\n";
      return 1;
    }

    try {
      send_files(opts);
    } catch (const std::exception &e) {
      std::cerr << "[ftransfer] error: " << e.what() << "\n";
      return 1;
    }

  } else if (mode == "recv" || mode == "receive") {
    RecvOpts opts{};
    opts.output_path = ".";
    opts.compressed = false;

    int opt;
    optind = 2;
    while ((opt = getopt(argc, argv, "p:d:z")) != -1) {
      switch (opt) {
      case 'p':
        opts.port = static_cast<uint16_t>(std::atoi(optarg));
        break;
      case 'd':
        opts.output_path = optarg;
        break;
      case 'z':
        opts.compressed = true;
        break;
      default:
        print_usage(argv[0]);
        return 1;
      }
    }

    if (opts.port == 0) {
      std::cerr << "Error: recv requires -p PORT\n\n";
      print_usage(argv[0]);
      return 1;
    }

    try {
      receive_files(opts);
    } catch (const std::exception &e) {
      std::cerr << "[ftransfer] error: " << e.what() << "\n";
      return 1;
    }

  } else {
    std::cerr << "Unknown mode: " << mode << "\n\n";
    print_usage(argv[0]);
    return 1;
  }

  return 0;
}
