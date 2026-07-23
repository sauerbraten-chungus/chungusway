#include "chungus_service.h"
#include "match_helper.h"
#include <fmt/base.h>
#include <fmt/core.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include <enet/enet.h>
#include <thread>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

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
                std::string chungid = std::string(reinterpret_cast<char*>(buffer));
                buffer += chungid.length() + 1;
                fmt::println("chungID: {}", chungid);
                expected_chungids.insert(chungid);
            }

            // Start assembling this match's stats report in MatchHelper
            match_helper->initialize_pending_report(container_id, expected_chungids);
            break;
        }
        // CHUNGUS_PLAYERINFO packet
        case 2: {
            fmt::println("detected playerinfo");

            // Extract container ID
            std::string container_id = std::string(reinterpret_cast<char*>(buffer));
            buffer += container_id.length() + 1;
            fmt::println("container_id: {}", container_id);

            // Extract player data
            std::string chungid = std::string(reinterpret_cast<char*>(buffer));
            buffer += chungid.length() + 1;
            fmt::println("chungid: {}", chungid);

            std::string name = std::string(reinterpret_cast<char*>(buffer));
            buffer += name.length() + 1;
            fmt::println("name: {}", name);

            uint8_t health = *buffer++; // delete l8r
            uint8_t frags = *buffer++;
            uint8_t deaths = *buffer++;

            float accuracy;
            std::memcpy(&accuracy, buffer, sizeof(accuracy));
            buffer += sizeof(accuracy);

            uint8_t elo = *buffer++;

            fmt::println("health: {} frags: {} deaths: {} accuracy: {:.2f} elo: {}", health, frags, deaths, accuracy, elo);

            // Create Stats object and append to MatchHelper
            chungusdb::Stats stats;
            stats.set_name(name);
            stats.set_frags(frags);
            stats.set_deaths(deaths);
            stats.set_accuracy(accuracy);
            stats.set_elo(elo);

            match_helper->append_player_stats(container_id, chungid, stats);
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
        match_helper->sweep_stale();
    }

    enet_host_destroy(host);
    enet_deinitialize();
}

int main() {
    if (enet_initialize() != 0) {
        fmt::println("An error occured when initializing ENet.");
        return 1;
    }

    // ChungusDB gRPC client stub; address from CHUNGUSDB_URL, local default
    const char* chungusdb_env = std::getenv("CHUNGUSDB_URL");
    std::string chungusdb_address = chungusdb_env ? chungusdb_env : "localhost:50052";
    auto channel = grpc::CreateChannel(chungusdb_address, grpc::InsecureChannelCredentials());
    auto stub = chungusdb::ChungusDB::NewStub(channel);

    // Initialize global MatchHelper (convert unique_ptr to shared_ptr)
    std::shared_ptr<chungusdb::ChungusDB::Stub> shared_stub = std::move(stub);
    match_helper = std::make_unique<MatchHelper>(shared_stub);
    fmt::println("Initialized MatchHelper with ChungusDB at {}", chungusdb_address);

    std::thread grpc_thread(init_grpc_service);
    init_enet_host();

    grpc_thread.join();
    return 0;
}
