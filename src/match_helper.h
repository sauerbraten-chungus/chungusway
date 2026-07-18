#include "proto/chungusway_chungusdb.grpc.pb.h"
#include "proto/chungusway_chungusdb.pb.h"
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

class MatchHelper {
public:
  explicit MatchHelper(std::shared_ptr<chungusdb::ChungusDB::Stub> stub) : stub_(stub) {};

  void initialize_pending_match(const std::string& container_id, const std::unordered_set<std::string>& chungids);
  void append_player_stats(const std::string& container_id, const std::string& chungid, const chungusdb::Stats& stats);

  // Flush pending matches older than STALE_AFTER: send whatever stats arrived
  // (some data beats none) and drop the entry either way. Called from the
  // ENet loop tick so a lost packet can't park a match forever.
  void sweep_stale();

private:
  static constexpr std::chrono::seconds STALE_AFTER{60};

  struct PendingMatchStats {
    std::unordered_set<std::string> expected_chungids;              // from all-packet
    std::unordered_map<std::string, chungusdb::Stats> player_stats; // from individual packets
    std::chrono::steady_clock::time_point created_at;

    bool is_done() const;
  };

  std::mutex mutex_;  // guards pending_matches (ENet thread + detached senders)
  std::unordered_map<std::string, PendingMatchStats> pending_matches;
  const std::shared_ptr<chungusdb::ChungusDB::Stub> stub_;

  // Takes ownership of the stats (entry already erased); safe to run detached.
  void send_match_stats(const std::string& container_id, std::unordered_map<std::string, chungusdb::Stats> player_stats);
};
