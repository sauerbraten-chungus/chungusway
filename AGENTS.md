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
| `src/logging.h` | Structured UTC logger with `LOG_LEVEL` filtering |
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

- **Match stats flow**: Receives `PLAYERINFO_ALL` (starts a pending stats report with the expected player set) → receives individual `PLAYERINFO` packets (membership-checked: stats for chungids not in the expected set are dropped) → when all expected players reported, stats are handed to a detached thread for `RecordMatchStats` to chungusdb (entry erased before sending; `pending_reports` is mutex-guarded). Reports older than 60 s are swept from the ENet loop tick: partial stats are sent, empty ones dropped — a lost packet can't park a report forever. Naming note: a "pending report" is a match whose stats are still being assembled at intermission — chungusway has no notion of a match being played
- **Verification code flow**: Chungustrator streams codes together with the target game server's `game_server_host:game_server_port` (allocated per match) → chungusway retries the ENet connect (12 × 5 s window, ~60 s budget, covers container boot) and forwards the codes; on exhaustion it logs and drops without affecting the process
- **Shutdown flow**: Game server sends ENet shutdown packet → chungusway pushes `GameServerShutdown` message on outgoing gRPC stream → chungustrator receives and cleans up containers
- **Threading**: ENet thread + gRPC thread, shared state via mutex + condition variable
- `CHUNGUSDB_URL` configures the ChungusDB gRPC address (default `localhost:50052`); its port must match chungusdb's gRPC listener
- **Logging**: stderr lines use `[UTC timestamp][chungusway][level] event=<name> key=value...`. `LOG_LEVEL` defaults to `INFO`; `DEBUG` enables packet, expected-player, and retry detail. Match/stat events carry `container_id` and `chungid`; verification-code values are never logged.
- Proto files are pre-compiled; regenerate manually if `.proto` changes
- No tests
