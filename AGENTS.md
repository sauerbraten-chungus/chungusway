# AGENTS.md — chungusway

## Overview

C++ bridge between game servers (ENet) and the backend platform (gRPC). Forwards verification codes to game servers, collects match stats, and relays lifecycle events.

- **Language**: C++20 (gRPC + ENet)
- **Ports**: 50051 (gRPC server), 30000 (ENet listener)
- **Build**: CMake — `nix` preset (nixpkgs deps, default for local dev) or `vcpkg` preset
- **Binary name**: `chungustrator-enet` (legacy naming)
- **Status**: Active

## Protocols

### gRPC Service (port 50051) — `ChungusService`

chungusway **hosts** this service; chungustrator dials in (at `CHUNGUSWAY_URL`, default `http://127.0.0.1:50051`) and opens the stream.

| RPC | Type | Purpose |
|-----|------|---------|
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
| `RecordMatchStats` | Forward aggregated match stats |

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

Generated proto stubs (`.pb.h`/`.pb.cc`/`.grpc.pb.*`) are **gitignored** — regenerate them before the first cmake build (`just protos` from `chungusroot/`, or the protoc commands below).

```bash
# from the nix devshell: nix develop ./chungusroot#chungusway (or the default fullShell)
cd proto
protoc --grpc_out=. --plugin=protoc-gen-grpc=$(which grpc_cpp_plugin) chungustrator_enet_streaming.proto chungusway_chungusdb.proto
protoc --cpp_out=. chungustrator_enet_streaming.proto chungusway_chungusdb.proto
cd ..
cmake --preset nix      # or: cmake --preset vcpkg (requires VCPKG_ROOT)
cmake --build build-nix
./build-nix/chungustrator-enet
```

## Architecture Notes

- **Match stats flow**: Receives `PLAYERINFO_ALL` (registers expected players) → receives individual `PLAYERINFO` packets → when all players reported, spawns detached thread to `RecordMatchStats` to chungusdb
- **Verification code flow**: Chungustrator streams codes together with the target game server's `game_server_host:game_server_port` (allocated per match) → chungusway retries the ENet connect (12 × 5 s window, ~60 s budget, covers container boot) and forwards the codes; on exhaustion it logs and drops without affecting the process
- **Shutdown flow**: Game server sends ENet shutdown packet → chungusway pushes `GameServerShutdown` message on outgoing gRPC stream → chungustrator receives and cleans up containers
- **Threading**: ENet thread + gRPC thread, shared state via mutex + condition variable
- Hardcoded address: ChungusDB at `localhost:50052` — needs env var config (game server address now arrives per-request from chungustrator)
- Proto files are pre-compiled; regenerate manually if `.proto` changes
- No tests
