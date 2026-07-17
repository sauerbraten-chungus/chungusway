#include "enet_client.h"
#include <enet/enet.h>

// Per-attempt ENet connect window; total budget is
// CONNECT_ATTEMPTS * CONNECT_TIMEOUT_MS (~60s) to cover container boot time.
static constexpr int CONNECT_ATTEMPTS = 12;
static constexpr int CONNECT_TIMEOUT_MS = 5000;

void send_verifications_to_game_server(
    const char* game_server_address,
    int game_server_port,
    std::string buffer
) {
    for (int attempt = 1; attempt <= CONNECT_ATTEMPTS; attempt++) {
        ENetHost* host = enet_host_create(NULL, 1, 2, 0, 0);
        if (host == nullptr) {
            fmt::print("ENet host creation failed; cannot send codes to {}:{}\n",
                       game_server_address, game_server_port);
            return;
        }

        ENetAddress address;
        enet_address_set_host(&address, game_server_address);
        address.port = game_server_port;

        ENetEvent event;
        ENetPeer* peer = enet_host_connect(host, &address, 3, 0);

        if (peer != nullptr &&
            enet_host_service(host, &event, CONNECT_TIMEOUT_MS) > 0 &&
            event.type == ENET_EVENT_TYPE_CONNECT)
        {
            ENetPacket* packet = enet_packet_create(buffer.data(), buffer.size(), ENET_PACKET_FLAG_RELIABLE);
            if (enet_peer_send(peer, 2, packet) != 0) {
                fmt::print("Failed to queue verification codes packet for {}:{}\n",
                           game_server_address, game_server_port);
                enet_packet_destroy(packet);
            } else {
                fmt::print("Verification codes sent to {}:{} (attempt {})\n",
                           game_server_address, game_server_port, attempt);
            }
            enet_host_flush(host);
            enet_peer_disconnect(peer, 0);
            enet_host_service(host, &event, 500);  // let the disconnect flush
            enet_host_destroy(host);
            return;
        }

        if (peer != nullptr) {
            enet_peer_reset(peer);
        }
        enet_host_destroy(host);
        fmt::print("Game server {}:{} not ready (attempt {}/{}), retrying...\n",
                   game_server_address, game_server_port, attempt, CONNECT_ATTEMPTS);
    }

    fmt::print("Giving up on sending verification codes to {}:{} after {} attempts\n",
               game_server_address, game_server_port, CONNECT_ATTEMPTS);
}
