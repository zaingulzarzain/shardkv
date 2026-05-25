#include "hash_ring.h"
#include <algorithm>
#include <iostream>

ConsistentHashRing::ConsistentHashRing(
    std::vector<std::string> shards,
    int v_nodes
) : virtual_nodes(v_nodes)
{
    for (auto& shard : shards)
    {
        add_shard(shard);
    }
    
    // Sort after all virtual nodes added
    std::sort(ring.begin(), ring.end());
}

unsigned int ConsistentHashRing::fnv1a_hash(
    const std::string& key
)
{
    unsigned int hash = 2166136261u;
    for (char c : key)
    {
        hash ^= static_cast<unsigned char>(c);
        hash *= 16777619u;
    }
    return hash;
}

void ConsistentHashRing::add_shard(
    const std::string& shard_name
)
{
    for (int i = 0; i < virtual_nodes; i++)
    {
        // Use different delimiter to ensure unique keys
        std::string virtual_key = shard_name + "#" + std::to_string(i);
        unsigned int hash_value = fnv1a_hash(virtual_key);
        ring.push_back(hash_value);
        shard_map[hash_value] = shard_name;
    }
}

std::string ConsistentHashRing::get_shard(
    const std::string& key
)
{
    if (ring.empty())
    {
        return "";
    }

    unsigned int hash_value = fnv1a_hash(key);
    
    // Binary search for first position >= hash_value
    auto it = std::lower_bound(ring.begin(), ring.end(), hash_value);
    
    // If at end, wrap around to beginning
    if (it == ring.end())
    {
        it = ring.begin();
    }
    
    return shard_map[*it];
}