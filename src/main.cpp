#include "verification_code_service.h"
#include <fmt/base.h>
#include <fmt/core.h>
#include <grpcpp/server_builder.h>
#include <enet/enet.h>
#include <thread>

void process_packet(uint8_t channel_id, ENetPacket* packet) {
    uint8_t *buffer = packet->data;

    uint8_t flag = *buffer++;
    std::string test = std::string(reinterpret_cast<char*>(buffer));

    fmt::println("flag: {} string: {} channel: {}", flag, test, channel_id);
}

void init_grpc_service() {
    std::string server_address = "0.0.0.0:50051";

    VerificationCodeService service;

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
               fmt::println("awesome");
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

    std::thread grpc_thread(init_grpc_service);
    init_enet_host();

    grpc_thread.join();
    return 0;
}
