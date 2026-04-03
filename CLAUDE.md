# CLAUDE.md — chungusway

## Overview

C++ bridge between game servers (ENet) and the backend platform (gRPC). Forwards verification codes to game servers, collects match stats, and relays lifecycle events.

- **Language**: C++20 (gRPC + ENet)
- **Ports**: 50051 (gRPC server), 30000 (ENet listener)
- **Build**: CMake + vcpkg
- **Binary name**: `chungustrator-enet` (legacy naming)
- **Status**: Active

## Protocols

### gRPC Service (port 50051) — `ChungusService`

| RPC | Type | Purpose |
|-----|------|---------|
| `SendVerificationCodes` | Unary | Receive codes, forward to game server via ENet |
| `StreamEvents` | Bidirectional | Streaming with chungustrator (codes, pings, shutdowns) |

### ENet Listener (port 30000) — Packet Types

| Flag | Name | Purpose |
|------|------|---------|
| `0` | `SHUTDOWN` | Game server shutdown notification |
| `1` | `CHUNGUS_PLAYERINFO_ALL` | Match init: container ID + expected player list |
| `2` | `CHUNGUS_PLAYERINFO` | Individual player stats (chungid, name, frags, deaths, accuracy, ELO) |

### gRPC Client → ChungusDB (connects to `localhost:50052`)

| RPC | Purpose |
|-----|---------|
| `SendMatchStats` | Forward aggregated match stats |

## Key Files

| File | Purpose |
|------|---------|
| `src/main.cpp` | ENet server loop, packet routing, ChungusDB client init |
| `src/chungus_service.cpp/h` | gRPC service (streaming + unary) |
| `src/match_helper.cpp/h` | Match stats aggregation and forwarding |
| `src/enet_client.cpp/h` | ENet client for sending verification codes |
| `proto/chungustrator_enet_streaming.proto` | Primary gRPC service definition |
| `proto/chungusway_chungusdb.proto` | ChungusDB stats service definition |
| `proto/chungustrator_enet.proto` | Legacy proto (unused) |

## Build

```bash
cmake --preset vcpkg    # requires VCPKG_ROOT env var
cmake --build build
./build/chungustrator-enet
```

**Regenerate proto stubs:**
```bash
protoc --grpc_out=proto --plugin=protoc-gen-grpc=`which grpc_cpp_plugin` proto/<file>.proto
protoc --cpp_out=proto proto/<file>.proto
```

## Architecture Notes

- **Match stats flow**: Receives `PLAYERINFO_ALL` (registers expected players) → receives individual `PLAYERINFO` packets → when all players reported, spawns detached thread to `SendMatchStats` to chungusdb
- **Verification code flow**: Chungustrator sends codes via streaming → chungusway forwards via ENet to game server at `127.0.0.1:28785`
- **Shutdown flow**: Game server sends ENet shutdown packet → chungusway pushes `GameServerShutdown` message on outgoing gRPC stream → chungustrator receives and cleans up containers
- **Threading**: ENet thread + gRPC thread, shared state via mutex + condition variable
- Hardcoded addresses (ChungusDB at `localhost:50052`, game server at `127.0.0.1:28785`) — needs env var config
- Proto files are pre-compiled; regenerate manually if `.proto` changes
- No tests
