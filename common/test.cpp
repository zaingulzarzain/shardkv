#include <iostream>

#include "common/hash_ring.h"

int main()
{
    std::vector<std::string> shards =
    {
        "shard1",
        "shard2",
        "shard3"
    };

    ConsistentHashRing ring(shards);

    std::cout << "10 -> "
              << ring.get_shard("10")
              << std::endl;

    std::cout << "51 -> "
              << ring.get_shard("51")
              << std::endl;

    std::cout << "90 -> "
              << ring.get_shard("90")
              << std::endl;

    return 0;
}