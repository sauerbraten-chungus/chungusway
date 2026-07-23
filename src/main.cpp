#include "chungus_service.h"
#include "match_helper.h"
#include "logging.h"
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
    uint8_t* buffer = packet->data;
    const uint8_t flag = *buffer++;

    switch (flag) {
        case 0: {
            const std::string container_id(reinterpret_cast<char*>(buffer));
            chunguslog::info(
                "event=game_server_shutdown container_id={} channel={}",
                container_id, channel_id);
            push_shutdown_notification(container_id, "Game server disconnected");
            break;
        }
        case 1: {
            const std::string container_id(reinterpret_cast<char*>(buffer));
            buffer += container_id.length() + 1;

            const uint8_t numclients = *buffer++;
            std::unordered_set<std::string> expected_chungids;
            for (uint8_t i = 0; i < numclients; i++) {
                std::string chungid(reinterpret_cast<char*>(buffer));
                buffer += chungid.length() + 1;
                chunguslog::debug(
                    "event=stats_report_player_expected container_id={} chungid={}",
                    container_id, chungid);
                expected_chungids.insert(std::move(chungid));
            }

            chunguslog::info(
                "event=stats_report_started container_id={} expected_players={}",
                container_id, expected_chungids.size());
            match_helper->initialize_pending_report(container_id, expected_chungids);
            break;
        }
        case 2: {
            const std::string container_id(reinterpret_cast<char*>(buffer));
            buffer += container_id.length() + 1;

            const std::string chungid(reinterpret_cast<char*>(buffer));
            buffer += chungid.length() + 1;

            const std::string name(reinterpret_cast<char*>(buffer));
            buffer += name.length() + 1;

            buffer++; // health is transmitted but not persisted
            const uint8_t frags = *buffer++;
            const uint8_t deaths = *buffer++;

            float accuracy;
            std::memcpy(&accuracy, buffer, sizeof(accuracy));
            buffer += sizeof(accuracy);

            const uint8_t elo = *buffer++;

            chunguslog::info(
                "event=player_stats_received container_id={} chungid={} frags={} deaths={} accuracy={:.2f} elo={}",
                container_id, chungid, static_cast<int>(frags), static_cast<int>(deaths),
                accuracy, static_cast<int>(elo));

            chungusdb::Stats stats;
            stats.set_name(name);
            stats.set_frags(frags);
            stats.set_deaths(deaths);
            stats.set_accuracy(accuracy);
            stats.set_elo(elo);

            match_helper->append_player_stats(container_id, chungid, stats);
            break;
        }
        default:
            chunguslog::warn(
                "event=packet_dropped reason=unknown_flag flag={} channel={}",
                static_cast<int>(flag), channel_id);
            break;
    }
}

void init_grpc_service() {
    const std::string server_address = "0.0.0.0:50051";

    ChungusService service;
    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    if (!server) {
        chunguslog::error(
            "event=grpc_server_start_failed listen_address={}",
            server_address);
        return;
    }

    chunguslog::info("event=grpc_server_started listen_address={}", server_address);
    server->Wait();
}

void init_enet_host() {
    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = 30000;

    ENetHost* host = enet_host_create(&address, 32, 2, 0, 0);
    if (host == nullptr) {
        chunguslog::error(
            "event=enet_server_start_failed listen_port={}",
            address.port);
        exit(1);
    }
    chunguslog::info("event=enet_server_started listen_port={}", address.port);

    ENetEvent event;
    while (enet_host_service(host, &event, 1000) >= 0) {
        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT:
                chunguslog::info("event=game_server_connected");
                break;
            case ENET_EVENT_TYPE_RECEIVE:
                chunguslog::debug(
                    "event=packet_received channel={} bytes={}",
                    event.channelID, event.packet->dataLength);
                process_packet(event.channelID, event.packet);
                enet_packet_destroy(event.packet);
                break;
            case ENET_EVENT_TYPE_DISCONNECT:
                chunguslog::info("event=game_server_disconnected");
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
        chunguslog::error("event=enet_initialization_failed");
        return 1;
    }

    const char* chungusdb_env = std::getenv("CHUNGUSDB_URL");
    const std::string chungusdb_address =
        chungusdb_env ? chungusdb_env : "localhost:50052";
    auto channel =
        grpc::CreateChannel(chungusdb_address, grpc::InsecureChannelCredentials());
    auto stub = chungusdb::ChungusDB::NewStub(channel);

    std::shared_ptr<chungusdb::ChungusDB::Stub> shared_stub = std::move(stub);
    match_helper = std::make_unique<MatchHelper>(shared_stub);
    chunguslog::info(
        "event=chungusdb_client_initialized address={}",
        chungusdb_address);

    std::thread grpc_thread(init_grpc_service);
    init_enet_host();

    grpc_thread.join();
    return 0;
}
