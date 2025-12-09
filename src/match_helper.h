#include "proto/chungusway_chungusdb.grpc.pb.h"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
class MatchHelper {
public:
  explicit MatchHelper(chungusdb::ChungusDBSerivce::Stub &stub);

  void initialize_pending_match(std::string &container_id);
  void append_player_stats(std::string &container_id);

private:
  struct PlayerStats {
    uint64_t kills;
  };

  struct PendingMatchStats {
    std::unordered_set<std::string> expected_chungids;// hashset<ChungIDs> from all-packet
    std::unordered_map<std::string, PlayerStats> player_stats; // hashmap<ChungIDs, stats> from individual packet

    bool is_done();
  };

  std::unordered_map<std::string, PendingMatchStats> pending_matches;
  // insert gRPC stub
};
