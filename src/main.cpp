#include "chungus_service.h"
#include "match_helper.h"
#include <fmt/base.h>
#include <fmt/core.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include <enet/enet.h>
#include <thread>
#include <memory>

// Global MatchHelper for processing game server packets
std::unique_ptr<MatchHelper> match_helper;

void process_packet(uint8_t channel_id, ENetPacket* packet) {
    uint8_t *buffer = packet->data;
    uint8_t flag = *buffer++;

    switch (flag)
    {
        // SHUTDOWN
        case 0: {
            fmt::println("Game server shutdown detected, notifying gRPC clients");
            std::string test = std::string(reinterpret_cast<char*>(buffer));

            // fmt::println("flag: {} string: {} channel: {}", flag, test, channel_id);
            push_shutdown_notification(test, "Game server disconnected");
            break;
        }
        // CHUNGUS_PLAYERINFO_ALL packet
        case 1: {
            fmt::println("detected playerinfo_all");

            // Extract container ID
            std::string container_id = std::string(reinterpret_cast<char*>(buffer));
            buffer += container_id.length() + 1;
            fmt::println("container_id: {}", container_id);

            // Extract player count and chungids
            uint8_t numclients = *buffer++;
            fmt::println("numclients: {}", numclients);

            std::unordered_set<std::string> expected_chungids;
            for (uint8_t i = 0; i < numclients; i++) {
                uint8_t chungid = *buffer++;
                fmt::println("chungID: {}", chungid);
                expected_chungids.insert(std::to_string(chungid));
            }

            // Initialize pending match in MatchHelper
            match_helper->initialize_pending_match(container_id, expected_chungids);
            break;
        }
        // CHUGNUS_PLAYERINFO packet
        case 2: {
            // Payload is hardcoded according to Chungusmod
            fmt::println("detected playerinfo");

            // Extract container ID
            std::string container_id = std::string(reinterpret_cast<char*>(buffer));
            buffer += container_id.length() + 1;
            fmt::println("container_id: {}", container_id);

            // Extract player data
            uint8_t chungid = *buffer++;
            fmt::println("chungid: {}", chungid);
            std::string test_string = std::string(reinterpret_cast<char*>(buffer));
            buffer += test_string.length() + 1;
            fmt::println("test_string: {}", test_string);
            uint8_t test_int = *buffer++;
            fmt::println("test_int: {}", test_int);

            // Create Stats object and append to MatchHelper
            chungusdb::Stats stats;
            stats.set_kills(test_int);  // Using test_int as kills for now
            match_helper->append_player_stats(container_id, std::to_string(chungid), stats);
            break;
        }   
    }
}

void init_grpc_service() {
    std::string server_address = "0.0.0.0:50051";

    ChungusService service;

    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    fmt::println("Server listening on {}", server_address);

    server->Wait();
}

void init_enet_host() {
    ENetAddress address;
    ENetHost *host;
    address.host = ENET_HOST_ANY;
    address.port = 30000;

    host = enet_host_create(&address, 32, 2, 0, 0);
    if (host == nullptr) {
        fmt::println("An error occurred while creating the ENet server host.");
        exit(1);
    }
    fmt::println("ENet server listening on port {}", address.port);
    fmt::println("Waiting for connections...");

    ENetEvent event;
    while(enet_host_service(host, &event, 1000) >= 0) {
        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT:
               fmt::println("Gameserver connected");
                break;
            case ENET_EVENT_TYPE_RECEIVE:
                fmt::println("Received packet");
                process_packet(event.channelID, event.packet);
                enet_packet_destroy (event.packet);
                break;
            case ENET_EVENT_TYPE_DISCONNECT:
                fmt::println("Client disconnected.");
                break;
            case ENET_EVENT_TYPE_NONE:
                break;
        }
    }

    enet_host_destroy(host);
    enet_deinitialize();
}

int main() {
    if (enet_initialize() != 0) {
        fmt::println("An error occured when initializing ENet.");
        return 1;
    }

    // Initialize ChungusDB gRPC client stub
    std::string chungusdb_address = "localhost:50052";  // TODO: Make configurable
    auto channel = grpc::CreateChannel(chungusdb_address, grpc::InsecureChannelCredentials());
    auto stub = chungusdb::ChungusDBService::NewStub(channel);

    // Initialize global MatchHelper (convert unique_ptr to shared_ptr)
    std::shared_ptr<chungusdb::ChungusDBService::Stub> shared_stub = std::move(stub);
    match_helper = std::make_unique<MatchHelper>(shared_stub);
    fmt::println("Initialized MatchHelper with ChungusDB at {}", chungusdb_address);

    std::thread grpc_thread(init_grpc_service);
    init_enet_host();

    grpc_thread.join();
    return 0;
}
