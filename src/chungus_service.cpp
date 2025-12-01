#include "chungus_service.h"
#include "enet_client.h"
#include <chrono>
#include <fmt/base.h>
#include <thread>
#include <atomic>

// Global queue and synchronization primitives
std::queue<chungustrator_enet::ChunguswayMessage> outgoing_queue;
std::mutex queue_mutex;
std::condition_variable queue_cv;

// Helper function for ENet thread to push shutdown notifications
void push_shutdown_notification(const std::string& server_container_id, const std::string& reason) {
    chungustrator_enet::ChunguswayMessage msg;
    auto* shutdown = msg.mutable_shutdown();
    shutdown->set_server_container_id(server_container_id);
    shutdown->set_timestamp(std::chrono::system_clock::now().time_since_epoch().count());
    shutdown->set_reason(reason);

    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        outgoing_queue.push(msg);
    }
    queue_cv.notify_one();
}

grpc::Status ChungusService::SendVerificationCodes(
    grpc::ServerContext* context,
    const chungustrator_enet::VerificationCodeRequest* request,
    chungustrator_enet::VerificationCodeResponse* response
) {
    const auto& codes = request->codes();
    std::string buffer;
    for (const auto& [id, code] : codes) {
        buffer += id + ":" + code + ",";
    }

    std::thread([buffer]{
            std::this_thread::sleep_for(std::chrono::seconds(10));
            send_verifications_to_game_server("127.0.0.1", 28785, buffer);
        }).detach();

    response->set_msg("Received");
    return grpc::Status::OK;
}

grpc::Status ChungusService::StreamEvents(
    grpc::ServerContext* context,
    grpc::ServerReaderWriter<chungustrator_enet::ChunguswayMessage,
                              chungustrator_enet::ChungustratorMessage>* stream
) {
    std::atomic<bool> stream_active{true};

    // Writer thread: reads from queue and sends to client
    std::thread writer([stream, &stream_active]() {
        while (stream_active.load()) {
            chungustrator_enet::ChunguswayMessage msg;

            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                // Wait for either a message in queue or timeout
                if (!queue_cv.wait_for(lock, std::chrono::milliseconds(100),
                    []{ return !outgoing_queue.empty(); })) {
                    continue;  // Timeout, check if stream still active
                }

                msg = outgoing_queue.front();
                outgoing_queue.pop();
            }

            // Write to client (outside the lock)
            if (!stream->Write(msg)) {
                // Write failed, stream probably closed
                break;
            }
        }
    });

    // Reader thread: handle incoming messages from client (runs in this thread)
    chungustrator_enet::ChungustratorMessage incoming;
    while (stream->Read(&incoming)) {
        chungustrator_enet::ChunguswayMessage outgoing;
        if (incoming.has_verification_code_req()) {
            // Handle verification codes
            const auto& req = incoming.verification_code_req();
            const auto& codes = req.codes();
            std::string buffer;
            for (const auto& [id, code] : codes) {
                buffer += id + ":" + code + ",";
            }

            std::thread([buffer]{
                    std::this_thread::sleep_for(std::chrono::seconds(10));
                    send_verifications_to_game_server("127.0.0.1", 28785, buffer);
                }).detach();

            auto* response = outgoing.mutable_verification_code_res();
            fmt::println("Received verification codes");
            response->set_msg("Received verification codes");
            stream->Write(outgoing);
        } else if (incoming.has_ping()) {
            // Handle ping
            auto* pong = outgoing.mutable_pong();
            pong->set_timestamp(incoming.ping().timestamp());
            stream->Write(outgoing);
        }
    }

    // Stream ended, signal writer thread to stop
    stream_active.store(false);
    queue_cv.notify_one();  // Wake up writer if it's waiting
    writer.join();

    return grpc::Status::OK;
}
