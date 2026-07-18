#include "match_helper.h"
#include "proto/chungusway_chungusdb.pb.h"
#include <fmt/base.h>
#include <grpcpp/client_context.h>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <utility>

void MatchHelper::initialize_pending_match(
  const std::string& container_id,
  const std::unordered_set<std::string>& expected_chungids
) {
  std::lock_guard<std::mutex> lock(mutex_);
  pending_matches[container_id] = PendingMatchStats{
    expected_chungids,
    {},
    std::chrono::steady_clock::now()
  };
}

void MatchHelper::append_player_stats(
  const std::string& container_id,
  const std::string& chungid,
  const chungusdb::Stats& player_stats
) {
  std::unordered_map<std::string, chungusdb::Stats> ready_stats;

  {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = pending_matches.find(container_id);
    if (it == pending_matches.end()) {
      fmt::print("Error: Bad container_id in append_player_stats, {}\n", container_id);
      return;
    }

    // Only accept players announced in the all-packet: rejects empty/unknown
    // chungids so garbage can't complete (or stall) the batch.
    if (it->second.expected_chungids.find(chungid) == it->second.expected_chungids.end()) {
      fmt::print("Dropping stats for unexpected chungid '{}' in match {}\n", chungid, container_id);
      return;
    }

    it->second.player_stats[chungid] = player_stats;
    if (!it->second.is_done()) {
      return;
    }

    // Complete: take ownership and erase before sending, so the detached
    // sender never touches the map and late packets are cleanly rejected.
    ready_stats = std::move(it->second.player_stats);
    pending_matches.erase(it);
  }

  std::thread([this, container_id, stats = std::move(ready_stats)]{
    send_match_stats(container_id, std::move(stats));
  }).detach();
}

void MatchHelper::sweep_stale() {
  std::vector<std::pair<std::string, std::unordered_map<std::string, chungusdb::Stats>>> stale;

  {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto now = std::chrono::steady_clock::now();
    for (auto it = pending_matches.begin(); it != pending_matches.end();) {
      if (now - it->second.created_at >= STALE_AFTER) {
        fmt::print(
          "Match {} timed out with {}/{} player stats; flushing\n",
          it->first, it->second.player_stats.size(), it->second.expected_chungids.size()
        );
        stale.emplace_back(it->first, std::move(it->second.player_stats));
        it = pending_matches.erase(it);
      } else {
        ++it;
      }
    }
  }

  for (auto& [container_id, stats] : stale) {
    if (stats.empty()) {
      fmt::print("Match {} timed out with no stats; dropping\n", container_id);
      continue;
    }
    std::thread([this, container_id, s = std::move(stats)]() mutable {
      send_match_stats(container_id, std::move(s));
    }).detach();
  }
}

void MatchHelper::send_match_stats(
  const std::string& container_id,
  std::unordered_map<std::string, chungusdb::Stats> all_player_stats
) {
  chungusdb::RecordMatchStatsRequest match_stats;
  auto* pb_stats = match_stats.mutable_player_stats();
  for (auto& [chungid, stats] : all_player_stats) {
    (*pb_stats)[chungid] = std::move(stats);
  }

  grpc::ClientContext context;
  chungusdb::RecordMatchStatsResponse response;

  const auto& status = stub_->RecordMatchStats(&context, match_stats, &response);

  if (status.ok()) {
    fmt::print("Match Stats for {} sent\n", container_id);
  } else {
    fmt::print(
      "Error: Failed to send match stats for {}: {} (code {})\n",
      container_id,
      status.error_message(),
      static_cast<int>(status.error_code())
    );
  }
}

bool MatchHelper::PendingMatchStats::is_done() const {
  // Appends are membership-checked, so player_stats keys are a subset of
  // expected_chungids; equal sizes therefore means exact set equality.
  return expected_chungids.size() == player_stats.size();
}
