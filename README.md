# Sharded Key-Value Store with Two-Phase Commit

## Project Overview
A distributed key-value store where data is partitioned across 3 shards using consistent hashing (FNV-1a, 150 virtual nodes). Supports single-key operations and multi-key transactions via Two-Phase Commit (2PC).

## Features
- ✅ Consistent hashing with virtual nodes
- ✅ Single-key PUT/GET/DELETE
- ✅ Multi-key transactions with BEGIN/COMMIT/ABORT
- ✅ Two-Phase Commit (2PC) for cross-shard atomicity
- ✅ Single-shard optimization
- ✅ Chaos testing for crash scenarios

## Build Instructions (Windows with MinGW)

```bash
g++ -c common/hash_ring.cpp -o hash_ring.o
g++ -o shard_new.exe shard/shard_server.cpp -lws2_32
g++ -o coordinator_new.exe coordinator/coordinator.cpp hash_ring.o -lws2_32
g++ -o client_new.exe client/client.cpp -lws2_32