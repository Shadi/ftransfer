# ftransfer

An experimental network file transfer tool with optional zstd streaming compression.

Built as an experiment. Outperforms scp on compressible data by avoiding encryption overhead. Only use on trusted networks.

## Benchmarks

Tested over LAN.

### Compressible data: 244MB text file

| Method | Time | Throughput |
|--------|------|------------|
| scp | 12.6s | ~19 MB/s |
| rsync | 12.7s | ~19 MB/s |
| rsync -z | 0.6s | ~407 MB/s* |
| ssh + tar (raw) | 10.9s | ~22 MB/s |
| ssh + tar + zstd 3 | 0.5s | ~488 MB/s* |
| ftransfer (raw) | 13.6s | ~18 MB/s |
| **ftransfer (zstd 3)** | **0.3s** | **~813 MB/s*** |

### Compressible data: 1GB text file

| Method | Time | Throughput |
|--------|------|------------|
| scp | 53.1s | ~19 MB/s |
| rsync | 49.2s | ~21 MB/s |
| rsync -z | 1.0s | ~1023 MB/s* |
| ssh + tar (raw) | 57.0s | ~18 MB/s |
| ssh + tar + zstd 3 | 1.0s | ~1023 MB/s* |
| ftransfer (raw) | 57.3s | ~18 MB/s |
| **ftransfer (zstd 3)** | **0.5s** | **~2046 MB/s*** |
| **ftransfer (zstd 7)** | **0.7s** | **~1461 MB/s*** |

### Incompressible data: 222MB MOV video

| Method | Time | Throughput |
|--------|------|------------|
| scp | 10.8s | ~21 MB/s |
| rsync | 10.6s | ~21 MB/s |
| rsync -z | 9.9s | ~22 MB/s |
| ssh + tar (raw) | 9.6s | ~23 MB/s |
| ssh + tar + zstd 3 | 10.5s | ~21 MB/s |
| ftransfer (raw) | 12.2s | ~18 MB/s |
| ftransfer (zstd 3) | 12.2s** | ~18 MB/s |

\* Effective throughput — actual bytes on the wire are much less due to compression.

\** Video is already compressed; zstd adds CPU overhead with no size reduction.


## Build

```bash
cmake -B build
cmake --build build
```

Requires `libzstd` and a C++23 compiler.

## Usage

```
ftransfer send -h HOST -p PORT [-d DIR] [-z LEVEL]
ftransfer recv -p PORT [-d DIR] [-z]
```

**Receiver** (start first):
```bash
ftransfer recv -p 9000 -d ./incoming        # raw
ftransfer recv -p 9000 -d ./incoming -z     # compressed
```

**Sender**:
```bash
ftransfer send -h 192.168.1.42 -p 9000 -d ./myfiles          # raw
ftransfer send -h 192.168.1.42 -p 9000 -d ./myfiles -z 3     # zstd level 3
```

### Options

| Flag | Description |
|------|-------------|
| `-h HOST` | Destination host/IP (send only) |
| `-p PORT` | Port number |
| `-d DIR` | Directory to send / extract into (default: `.`) |
| `-z [LEVEL]` | Enable zstd compression. Sender: level 1-19 (default 1). Receiver: flag only. |
