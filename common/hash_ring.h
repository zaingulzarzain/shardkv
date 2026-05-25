#ifndef HASH_RING_H
#define HASH_RING_H

#include <string>
#include <vector>
#include <map>

class ConsistentHashRing
{
private:

    int virtual_nodes;

    std::vector<unsigned int> ring;

    std::map<unsigned int, std::string> shard_map;

public:

    ConsistentHashRing(
        std::vector<std::string> shards,
        int v_nodes = 150
    );

    unsigned int fnv1a_hash(
        const std::string& key
    );

    void add_shard(
        const std::string& shard_name
    );

    std::string get_shard(
        const std::string& key
    );
};

#endif