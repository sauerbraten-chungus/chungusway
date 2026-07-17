#include "proto/chungusway_chungusdb.grpc.pb.h"
#include "proto/chungusway_chungusdb.pb.h"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
class MatchHelper {
public:
  explicit MatchHelper(std::shared_ptr<chungusdb::ChungusDB::Stub> stub) : stub_(stub) {};

  void initialize_pending_match(const std::string& container_id, const std::unordered_set<std::string>& chungids);
  void append_player_stats(const std::string& container_id, const std::string& chungid, const chungusdb::Stats& stats);

private:
  struct PendingMatchStats {
    std::unordered_set<std::string> expected_chungids;// hashset<ChungIDs> from all-packet 
    std::unordered_map<std::string, chungusdb::Stats> player_stats; // The end product, hashmap<ChungIDs, stats> from individual packets.

    bool is_done() const;
  };

  std::unordered_map<std::string, PendingMatchStats> pending_matches;
  const std::shared_ptr<chungusdb::ChungusDB::Stub> stub_;

  void send_match_stats(const std::string& container_id, const std::unordered_map<std::string, chungusdb::Stats>& player_stats);
};
