
#include "ftransfer/compress.h"
#include <cstddef>
#include <stdexcept>
#include <string>
#include <sys/types.h>
#include <vector>
#include <zstd.h>

void compress_stream(std::function<ssize_t(void *buf, size_t capacity)> read_cb,
                     ChunkCallback write_cb, int level) {
  ZSTD_CStream *cstream = ZSTD_createCStream();
  if (!cstream)
    throw std::runtime_error("ZSTD_createCStream failed");

  size_t ret = ZSTD_initCStream(cstream, level);
  if (ZSTD_isError(ret)) {
    ZSTD_freeCStream(cstream);
    throw std::runtime_error(std::string("ZSTD_initCStream: ") +
                             ZSTD_getErrorName(ret));
  }

  size_t in_buf_size = ZSTD_CStreamInSize();
  size_t out_buf_size = ZSTD_CStreamOutSize();

  std::vector<char> in_buf(in_buf_size);
  std::vector<char> out_buf(out_buf_size);

  bool done = false;

  while (!done) {
    ssize_t nread = read_cb(in_buf.data(), in_buf.size());
    if (nread < 0) {
      throw std::runtime_error("compress: read error");
    }

    bool last_chunk = (nread == 0);

    ZSTD_inBuffer input = {in_buf.data(), static_cast<size_t>(nread), 0};
    if (last_chunk) {
      size_t remaining;
      do {
        ZSTD_outBuffer output = {out_buf.data(), out_buf.size(), 0};

        remaining = ZSTD_endStream(cstream, &output);
        if (ZSTD_isError(remaining)) {
          ZSTD_freeCStream(cstream);
          throw std::runtime_error(std::string("ZSTD_endStream: ") +
                                   ZSTD_getErrorName(remaining));
        }
        if (output.pos > 0 && !write_cb(out_buf.data(), output.pos)) {
          ZSTD_freeCStream(cstream); // log warning
          return;
        }
      } while (remaining > 0);
      done = true;
    } else {
      while (input.pos < input.size) {
        ZSTD_outBuffer output = {out_buf.data(), out_buf.size(), 0};
        ret = ZSTD_compressStream(cstream, &output, &input);
        if (ZSTD_isError(ret)) {
          ZSTD_freeCStream(cstream);
          throw std::runtime_error(std::string("ZSTD_compressStream: ") +
                                   ZSTD_getErrorName(ret));
        }
        if (output.pos > 0 && !write_cb(out_buf.data(), output.pos)) {
          ZSTD_freeCStream(cstream);
          return;
        }
      }
    }
  }
}

void decompress_stream(
    std::function<ssize_t(void *buf, size_t capacity)> read_cb,
    ChunkCallback write_cb) {
  ZSTD_DStream *dstream = ZSTD_createDStream();

  if (!dstream) {
    throw std::runtime_error("ZSTD_createdstream failed");
  }

  size_t ret = ZSTD_initDStream(dstream);
  if (ZSTD_isError(ret)) {
    ZSTD_freeDStream(dstream);
    throw std::runtime_error(std::string("ZSTD_initDStream: ") +
                             ZSTD_getErrorName(ret));
  }

  size_t in_buf_size = ZSTD_DStreamInSize();
  size_t out_buf_size = ZSTD_DStreamOutSize();

  std::vector<char> in_buf(in_buf_size);
  std::vector<char> out_buf(out_buf_size);

  while (true) {
    ssize_t nread = read_cb(in_buf.data(), in_buf.size());
    if (nread < 0) {
      throw std::runtime_error("read chunk error");
    }
    if (nread == 0) {
      break;
    }

    ZSTD_inBuffer input = {in_buf.data(), static_cast<size_t>(nread), 0};

    while (input.pos < input.size) {
      ZSTD_outBuffer output = {out_buf.data(), out_buf.size(), 0};
      ret = ZSTD_decompressStream(dstream, &output, &input);
      if (ZSTD_isError(ret)) {
        ZSTD_freeDStream(dstream);
        throw std::runtime_error(std::string("error in ZSTD_decomressStream ") +
                                 ZSTD_getErrorName(ret));
      }
      if (output.pos > 0 && !write_cb(out_buf.data(), output.pos)) {
        ZSTD_freeDStream(dstream);
        return;
      }
    }
  }
}
