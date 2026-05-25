#include <iostream>
#include <string>
#include <sstream>
#include <map>
#include <vector>
#include <set>

#include <winsock2.h>
#include <ws2tcpip.h>

#include "../common/hash_ring.h"

#pragma comment(lib, "ws2_32.lib")

using namespace std;

map<string, pair<string, int>> shards;
map<string, string> key_map;

// Transaction structure
struct Transaction
{
    int tx_id;
    vector<tuple<string, string, string>> ops;  // (op, key, value)
    set<string> involved_shards;
};

map<int, Transaction> active_txns;
int next_tx_id = 1;


string send_to_shard(string shard_name, string message)
{
    SOCKET shard_socket = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in shard_addr;
    shard_addr.sin_family = AF_INET;
    shard_addr.sin_port = htons(shards[shard_name].second);
    shard_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(shard_socket, (sockaddr*)&shard_addr, sizeof(shard_addr)) == SOCKET_ERROR)
    {
        closesocket(shard_socket);
        return "ERROR|Connection failed";
    }

    send(shard_socket, message.c_str(), message.size(), 0);

    char buffer[4096];
    memset(buffer, 0, sizeof(buffer));
    int bytes_received = recv(shard_socket, buffer, sizeof(buffer), 0);

    closesocket(shard_socket);

    if (bytes_received <= 0)
    {
        return "ERROR|No response";
    }

    return string(buffer);
}


bool run_two_phase_commit(Transaction& tx)
{
    cout << "\n=== 2PC START: tx_id=" << tx.tx_id << " ===" << endl;
    cout << "Involved shards: ";
    for (string s : tx.involved_shards) cout << s << " ";
    cout << endl;

    // PHASE 1: PREPARE
    cout << "\n[PHASE 1] Sending PREPARE..." << endl;
    bool all_yes = true;

    for (string shard : tx.involved_shards)
    {
        string response = send_to_shard(shard, "PREPARE " + to_string(tx.tx_id));
        cout << "  " << shard << " -> " << response << endl;
        if (response != "YES") all_yes = false;
    }

    // PHASE 2: DECISION
    string decision;
    if (all_yes)
    {
        decision = "COMMIT";
        cout << "\n[PHASE 2] Decision: COMMIT" << endl;
    }
    else
    {
        decision = "ABORT";
        cout << "\n[PHASE 2] Decision: ABORT" << endl;
    }

    // PHASE 3: COMMIT or ABORT
    cout << "\n[PHASE 3] Sending " << decision << "..." << endl;
    for (string shard : tx.involved_shards)
    {
        string cmd = (decision == "COMMIT") ? "COMMIT_TXN" : "ABORT_TXN";
        string response = send_to_shard(shard, cmd + " " + to_string(tx.tx_id));
        cout << "  " << shard << " -> " << response << endl;
    }

    cout << "=== 2PC END: " << decision << " ===" << endl;
    return all_yes;
}


void handle_client(SOCKET client_socket, ConsistentHashRing& ring)
{
    char buffer[4096];
    int current_tx = -1;

    while (true)
    {
        memset(buffer, 0, sizeof(buffer));
        int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
        if (bytes_received <= 0) break;

        string message(buffer);
        stringstream ss(message);
        string command;
        ss >> command;

        string response;

        // ===== METADATA =====
        if (command == "\\keys")
        {
            if (key_map.empty()) response = "No keys stored";
            else for (auto& entry : key_map) response += entry.first + " -> " + entry.second + "\n";
            send(client_socket, response.c_str(), response.size(), 0);
            continue;
        }

        if (command == "\\stats")
        {
            int c1=0, c2=0, c3=0;
            for (auto& entry : key_map)
            {
                if (entry.second == "shard1") c1++;
                else if (entry.second == "shard2") c2++;
                else if (entry.second == "shard3") c3++;
            }
            response = "Total keys: " + to_string(key_map.size()) + "\n" +
                       "shard1: " + to_string(c1) + "\n" +
                       "shard2: " + to_string(c2) + "\n" +
                       "shard3: " + to_string(c3) + "\n" +
                       "Active transactions: " + to_string(active_txns.size());
            send(client_socket, response.c_str(), response.size(), 0);
            continue;
        }

        // ===== BEGIN =====
        if (command == "BEGIN")
        {
            current_tx = next_tx_id++;
            active_txns[current_tx] = {current_tx};
            response = "Transaction started. ID = " + to_string(current_tx);
            send(client_socket, response.c_str(), response.size(), 0);
            continue;
        }

        // ===== COMMIT =====
        if (command == "COMMIT")
        {
            if (current_tx == -1 || active_txns.find(current_tx) == active_txns.end())
            {
                response = "ERROR: No active transaction";
                send(client_socket, response.c_str(), response.size(), 0);
                continue;
            }

            Transaction& tx = active_txns[current_tx];
            
            // Single shard optimization
            if (tx.involved_shards.size() == 1)
            {
                cout << "Single shard - skipping 2PC" << endl;
                string shard = *tx.involved_shards.begin();
                for (auto& op : tx.ops)
                {
                    string op_type = get<0>(op), key = get<1>(op), value = get<2>(op);
                    string cmd = op_type + " " + key + (op_type == "PUT" ? " " + value : "");
                    send_to_shard(shard, cmd);
                    if (op_type == "PUT") key_map[key] = shard;
                    else if (op_type == "DELETE") key_map.erase(key);
                }
                response = "COMMITTED (single shard)";
            }
            else
            {
                // Multi-shard: Run 2PC
                bool success = run_two_phase_commit(tx);
                if (success)
                {
                    for (auto& op : tx.ops)
                    {
                        string op_type = get<0>(op), key = get<1>(op);
                        string shard = ring.get_shard(key);
                        if (op_type == "PUT") key_map[key] = shard;
                        else if (op_type == "DELETE") key_map.erase(key);
                    }
                    response = "COMMITTED (2PC)";
                }
                else
                {
                    response = "ABORTED (2PC failed)";
                }
            }
            
            active_txns.erase(current_tx);
            current_tx = -1;
            send(client_socket, response.c_str(), response.size(), 0);
            continue;
        }

        // ===== ABORT =====
        if (command == "ABORT")
        {
            if (current_tx != -1 && active_txns.find(current_tx) != active_txns.end())
            {
                Transaction& tx = active_txns[current_tx];
                for (string shard : tx.involved_shards)
                {
                    send_to_shard(shard, "ABORT_TXN " + to_string(tx.tx_id));
                }
                active_txns.erase(current_tx);
                response = "ABORTED";
            }
            else
            {
                response = "No active transaction";
            }
            current_tx = -1;
            send(client_socket, response.c_str(), response.size(), 0);
            continue;
        }

        // ===== SINGLE-KEY (non-transactional) =====
        string key;
        ss >> key;
        if (key.empty())
        {
            response = "ERROR: Key missing";
            send(client_socket, response.c_str(), response.size(), 0);
            continue;
        }

        string shard_name = ring.get_shard(key);
        cout << "Routed " << command << " " << key << " -> " << shard_name << endl;

        // If inside transaction for writes
        if (current_tx != -1 && (command == "PUT" || command == "DELETE"))
        {
            string value;
            if (command == "PUT") ss >> value;
            
            string stage_msg = "STAGE " + to_string(current_tx) + " " + command + " " + key + " " + value;
            response = send_to_shard(shard_name, stage_msg);
            
            if (response == "STAGED")
            {
                Transaction& tx = active_txns[current_tx];
                tx.ops.push_back(make_tuple(command, key, value));
                tx.involved_shards.insert(shard_name);
                response = "STAGED on " + shard_name;
            }
        }
        else
        {
            response = send_to_shard(shard_name, message);
            if (command == "PUT") key_map[key] = shard_name;
            else if (command == "DELETE") key_map.erase(key);
        }

        send(client_socket, response.c_str(), response.size(), 0);
    }

    if (current_tx != -1) active_txns.erase(current_tx);
    closesocket(client_socket);
}


int main()
{
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    vector<string> shard_names = {"shard1", "shard2", "shard3"};
    ConsistentHashRing ring(shard_names);

    shards["shard1"] = {"127.0.0.1", 7001};
    shards["shard2"] = {"127.0.0.1", 7002};
    shards["shard3"] = {"127.0.0.1", 7003};

    SOCKET server_socket = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8000);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    bind(server_socket, (sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_socket, 5);

    cout << "Coordinator running on port 8000" << endl;
    cout << "Ring built: 3 shards, 150 virtual nodes each" << endl;

    while (true)
    {
        sockaddr_in client_addr;
        int client_len = sizeof(client_addr);
        SOCKET client_socket = accept(server_socket, (sockaddr*)&client_addr, &client_len);
        cout << "Client connected" << endl;
        handle_client(client_socket, ring);
    }

    closesocket(server_socket);
    WSACleanup();
    return 0;
}