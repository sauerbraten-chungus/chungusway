#include "match_helper.h"
#include "logging.h"
#include "proto/chungusway_chungusdb.pb.h"
#include <grpcpp/client_context.h>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <utility>

void MatchHelper::initialize_pending_report(
  const std::string& container_id,
  const std::unordered_set<std::string>& expected_chungids
) {
  std::lock_guard<std::mutex> lock(mutex_);
  pending_reports[container_id] = PendingStatsReport{
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

    auto it = pending_reports.find(container_id);
    if (it == pending_reports.end()) {
      chunguslog::warn(
        "event=player_stats_dropped reason=unknown_report container_id={} chungid={}",
        container_id, chungid);
      return;
    }

    // Only accept players announced in the all-packet: rejects empty/unknown
    // chungids so garbage can't complete (or stall) the batch.
    if (it->second.expected_chungids.find(chungid) == it->second.expected_chungids.end()) {
      chunguslog::warn(
        "event=player_stats_dropped reason=unexpected_chungid container_id={} chungid={}",
        container_id, chungid);
      return;
    }

    it->second.player_stats[chungid] = player_stats;
    if (!it->second.is_done()) {
      return;
    }

    // Complete: take ownership and erase before sending, so the detached
    // sender never touches the map and late packets are cleanly rejected.
    ready_stats = std::move(it->second.player_stats);
    pending_reports.erase(it);
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
    for (auto it = pending_reports.begin(); it != pending_reports.end();) {
      if (now - it->second.created_at >= STALE_AFTER) {
        chunguslog::warn(
          "event=stats_report_timed_out container_id={} received_players={} expected_players={}",
          it->first, it->second.player_stats.size(), it->second.expected_chungids.size()
        );
        stale.emplace_back(it->first, std::move(it->second.player_stats));
        it = pending_reports.erase(it);
      } else {
        ++it;
      }
    }
  }

  for (auto& [container_id, stats] : stale) {
    if (stats.empty()) {
      chunguslog::warn(
        "event=stats_report_dropped reason=no_player_stats container_id={}",
        container_id);
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
    chunguslog::info(
      "event=match_stats_forwarded container_id={} players={}",
      container_id, pb_stats->size());
  } else {
    chunguslog::error(
      "event=match_stats_forward_failed container_id={} players={} grpc_code={} error={:?}",
      container_id,
      pb_stats->size(),
      static_cast<int>(status.error_code()),
      status.error_message()
    );
  }
}

bool MatchHelper::PendingStatsReport::is_done() const {
  // Appends are membership-checked, so player_stats keys are a subset of
  // expected_chungids; equal sizes therefore means exact set equality.
  return expected_chungids.size() == player_stats.size();
}
