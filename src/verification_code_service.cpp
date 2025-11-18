#include "verification_code_service.h"
#include "enet_client.h"
#include <chrono>
#include <fmt/base.h>
#include <thread>

grpc::Status VerificationCodeService::SendVerificationCodes(
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
