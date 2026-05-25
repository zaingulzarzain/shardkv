#include <iostream>
#include <string>
#include <sstream>
#include <map>
#include <vector>

#include <winsock2.h>
#include <ws2tcpip.h>

#include "../common/hash_ring.h"

#pragma comment(lib, "ws2_32.lib")

using namespace std;


map<string, pair<string, int>> shards;
map<string, string> key_map;


string send_to_shard(
    string shard_name,
    string message
)
{
    string shard_host = shards[shard_name].first;
    int shard_port = shards[shard_name].second;

    SOCKET shard_socket =
        socket(AF_INET, SOCK_STREAM, 0);

    if (shard_socket == INVALID_SOCKET)
    {
        return "ERROR: Could not create socket for " + shard_name;
    }

    sockaddr_in shard_addr;
    shard_addr.sin_family = AF_INET;
    shard_addr.sin_port = htons(shard_port);
    // FIXED: Use inet_addr correctly
    shard_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    int connect_result =
        connect(
            shard_socket,
            (sockaddr*)&shard_addr,
            sizeof(shard_addr)
        );

    if (connect_result == SOCKET_ERROR)
    {
        closesocket(shard_socket);
        return "ERROR: Could not connect to " + shard_name;
    }

    send(
        shard_socket,
        message.c_str(),
        message.size(),
        0
    );

    char buffer[4096];
    memset(buffer, 0, sizeof(buffer));

    int bytes_received =
        recv(
            shard_socket,
            buffer,
            sizeof(buffer),
            0
        );

    closesocket(shard_socket);

    if (bytes_received <= 0)
    {
        return "ERROR: No response from " + shard_name;
    }

    return string(buffer);
}


void handle_client(
    SOCKET client_socket,
    ConsistentHashRing& ring
)
{
    char buffer[4096];

    while (true)
    {
        memset(buffer, 0, sizeof(buffer));

        int bytes_received =
            recv(
                client_socket,
                buffer,
                sizeof(buffer),
                0
            );

        if (bytes_received <= 0)
        {
            break;
        }

        string message(buffer);

        stringstream ss(message);

        string command;
        ss >> command;

        string response;

        if (command == "\\keys")
        {
            if (key_map.empty())
            {
                response = "No keys stored";
            }
            else
            {
                for (auto& entry : key_map)
                {
                    response += entry.first;
                    response += " -> ";
                    response += entry.second;
                    response += "\n";
                }
            }

            send(
                client_socket,
                response.c_str(),
                response.size(),
                0
            );

            continue;
        }

        if (command == "\\stats")
        {
            int shard1_count = 0;
            int shard2_count = 0;
            int shard3_count = 0;

            for (auto& entry : key_map)
            {
                if (entry.second == "shard1")
                {
                    shard1_count++;
                }
                else if (entry.second == "shard2")
                {
                    shard2_count++;
                }
                else if (entry.second == "shard3")
                {
                    shard3_count++;
                }
            }

            response =
                "coordinator: 1\n"
                "shards: 3\n"
                "keys total: " + to_string(key_map.size()) + "\n"
                "shard1 keys: " + to_string(shard1_count) + "\n"
                "shard2 keys: " + to_string(shard2_count) + "\n"
                "shard3 keys: " + to_string(shard3_count);

            send(
                client_socket,
                response.c_str(),
                response.size(),
                0
            );

            continue;
        }

        string key;
        ss >> key;

        if (key.empty())
        {
            response = "ERROR: Key missing";

            send(
                client_socket,
                response.c_str(),
                response.size(),
                0
            );

            continue;
        }

        string shard_name =
            ring.get_shard(key);

        cout << "Coordinator routed "
            << key
            << " -> "
            << shard_name
            << endl;

        response =
            send_to_shard(
                shard_name,
                message
            );

        if (command == "PUT")
        {
            key_map[key] = shard_name;
        }
        else if (command == "DELETE")
        {
            key_map.erase(key);
        }

        send(
            client_socket,
            response.c_str(),
            response.size(),
            0
        );
    }

    closesocket(client_socket);
}


int main()
{
    WSADATA wsa;

    WSAStartup(
        MAKEWORD(2, 2),
        &wsa
    );

    vector<string> shard_names =
    {
        "shard1",
        "shard2",
        "shard3"
    };

    ConsistentHashRing ring(shard_names);

    shards["shard1"] = { "127.0.0.1", 7001 };
    shards["shard2"] = { "127.0.0.1", 7002 };
    shards["shard3"] = { "127.0.0.1", 7003 };

    SOCKET server_socket =
        socket(
            AF_INET,
            SOCK_STREAM,
            0
        );

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8000);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    bind(
        server_socket,
        (sockaddr*)&server_addr,
        sizeof(server_addr)
    );

    listen(server_socket, 5);

    cout << "Coordinator running on port 8000" << endl;
    cout << "Ring built: 3 shards, 150 virtual nodes each" << endl;

    while (true)
    {
        sockaddr_in client_addr;
        int client_len = sizeof(client_addr);

        SOCKET client_socket =
            accept(
                server_socket,
                (sockaddr*)&client_addr,
                &client_len
            );

        cout << "Client connected to coordinator" << endl;

        handle_client(
            client_socket,
            ring
        );
    }

    closesocket(server_socket);

    WSACleanup();

    return 0;
}