#ifndef CHUNGUS_SERVICE_H
#define CHUNGUS_SERVICE_H

#include "proto/chungustrator_enet_streaming.grpc.pb.h"
#include "proto/chungustrator_enet_streaming.pb.h"
#include <grpcpp/server_context.h>
#include <queue>
#include <mutex>
#include <condition_variable>

// Global queue for sending messages from ENet thread to gRPC clients
extern std::queue<chungustrator_enet::ChunguswayMessage> outgoing_queue;
extern std::mutex queue_mutex;
extern std::condition_variable queue_cv;

// Helper function to push shutdown messages from ENet thread
void push_shutdown_notification(const std::string& server_container_id, const std::string& reason);

class ChungusService final : public chungustrator_enet::ChungusService::Service {
public:
    grpc::Status StreamEvents(
        grpc::ServerContext* context,
        grpc::ServerReaderWriter<chungustrator_enet::ChunguswayMessage,
                                  chungustrator_enet::ChungustratorMessage>* stream
    ) override;
};

#endif
