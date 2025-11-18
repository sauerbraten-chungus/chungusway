#include "enet_client.h"
#include <enet/enet.h>

void send_verifications_to_game_server(
    const char* game_server_address,
    int game_server_port,
    std::string buffer
) {
    ENetHost* host;

    host = enet_host_create(NULL, 1, 2, 0, 0);
    if (host == nullptr)
    {
        fmt::print("An error occured when initializing an ENet host.\n");
    }

    ENetAddress address;
    ENetEvent event;
    ENetPeer* peer;

    enet_address_set_host(&address, game_server_address);
    address.port = game_server_port;

    peer = enet_host_connect(host, &address, 3, 0);
    if (peer == nullptr)
    {
        fmt::print("No available poeers for initiating an ENet connection.\n");
    }

    if (enet_host_service(host, &event, 5000) > 0 &&
        event.type == ENET_EVENT_TYPE_CONNECT)
    {
        fmt::print("Connection to host succeeded.\n");
    }
    else
    {
        enet_peer_reset(peer);
        fmt::print("Connection to host has failed.\n");
        exit (EXIT_FAILURE);
    }
    enet_host_flush(host);

    ENetPacket* packet = enet_packet_create(buffer.data(), buffer.size(), ENET_PACKET_FLAG_RELIABLE);

    fmt::print("Sending test packet to server\n");
    int packet_sent = enet_peer_send(peer, 2, packet);
    if (packet_sent != 0)
    {
        fmt::print("Boy what the hell\n");
    }
    else
    {
        fmt::print("yay\n");
    }
    enet_host_flush(host);
    enet_host_destroy(host);
}
