#include <iostream>
#include <map>
#include "../common/hash_ring.h"

int main()
{
    std::vector<std::string> shards = {"shard1", "shard2", "shard3"};
    ConsistentHashRing ring(shards, 150);
    
    int counts[3] = {0};  // shard1, shard2, shard3
    
    std::cout << "Testing hash ring distribution with 1000 random keys..." << std::endl;
    
    for (int i = 0; i < 1000; i++)
    {
        std::string key = "test_key_" + std::to_string(i);
        std::string shard = ring.get_shard(key);
        
        if (shard == "shard1") counts[0]++;
        else if (shard == "shard2") counts[1]++;
        else if (shard == "shard3") counts[2]++;
    }
    
    std::cout << "\nDistribution Results:" << std::endl;
    std::cout << "shard1: " << counts[0] << " keys (" << (counts[0] * 100 / 1000) << "%)" << std::endl;
    std::cout << "shard2: " << counts[1] << " keys (" << (counts[1] * 100 / 1000) << "%)" << std::endl;
    std::cout << "shard3: " << counts[2] << " keys (" << (counts[2] * 100 / 1000) << "%)" << std::endl;
    
    // Check if distribution is reasonable (between 20% and 45% each)
    bool balanced = true;
    for (int i = 0; i < 3; i++)
    {
        int percent = counts[i] * 100 / 1000;
        if (percent < 20 || percent > 45)
        {
            balanced = false;
        }
    }
    
    if (balanced)
    {
        std::cout << "\n✓ Distribution is balanced." << std::endl;
    }
    else
    {
        std::cout << "\n✗ Distribution is unbalanced." << std::endl;
    }
    
    return 0;
}