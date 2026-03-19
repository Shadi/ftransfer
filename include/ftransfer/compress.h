#ifndef COMPRESS_H
#define COMPRESS_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <sys/types.h>

using ChunkCallback = std::function<bool(const void *data, size_t size)>;

// compress read_cb and pass compressed chuncks to write_cb
// level: zstd compression level(1=fast, 3=default, higher smaller size+slower)
void compress_stream(std::function<ssize_t(void *buf, size_t capacity)> read_cb,
                     ChunkCallback write_cb, int level = 1);

void decompress_stream(
    std::function<ssize_t(void *buf, size_t capacity)> read_cb,
    ChunkCallback write_cb);

#endif // !COMPRESS_H
