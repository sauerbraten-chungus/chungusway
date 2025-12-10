#include "match_helper.h"
#include "proto/chungusway_chungusdb.pb.h"
#include <fmt/base.h>
#include <grpcpp/client_context.h>
#include <string>
#include <thread>
#include <unordered_map>

void MatchHelper::initialize_pending_match(
  const std::string& container_id,
  const std::unordered_set<std::string>& expected_chungids
) {
  pending_matches[container_id] = PendingMatchStats{
    expected_chungids,
    {}
  };
}

void MatchHelper::append_player_stats(
  const std::string& container_id,
  const std::string& chungid,
  const chungusdb::Stats& player_stats
) {
  auto it = pending_matches.find(container_id);
  if (it == pending_matches.end()) {
    fmt::print("Error: Bad container_id in append_player_stats, {}\n", container_id);
    return;
  }

  it->second.player_stats[chungid] = player_stats;
  if (it->second.is_done()) {

    std::string container_id_copy = container_id;
    auto all_player_stats_copy = it->second.player_stats;
    
    std::thread([this, container_id_copy, all_player_stats_copy]{
      send_match_stats(container_id_copy, all_player_stats_copy);
    }).detach();
  }
}

void MatchHelper::send_match_stats(
  const std::string& container_id,
  const std::unordered_map<std::string, chungusdb::Stats>& all_player_stats
) {
  chungusdb::MatchStats match_stats;
  auto* pb_stats = match_stats.mutable_player_stats();
  for (const auto& [chungid, stats] : all_player_stats) {
    (*pb_stats)[chungid] = stats;
  }

  grpc::ClientContext context;
  chungusdb::MatchStatsResponse response;
  
  const auto& status = stub_->SendMatchStats(&context, match_stats, &response);

  if (status.ok()) {
    fmt::print("Match Stats for {} sent\n", container_id);
    pending_matches.erase(container_id);
  } else {
    fmt::print("Error: Failed to send match stats for {}\n", container_id);
  }
}

bool MatchHelper::PendingMatchStats::is_done() const {
  return expected_chungids.size() == player_stats.size();
}
