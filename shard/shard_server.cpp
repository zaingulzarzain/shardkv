#include <iostream>
#include <string>
#include <unordered_map>
#include <sstream>
#include <map>
#include <vector>
#include <tuple>

#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

// Main data store
unordered_map<string, string> data_store;

// Staging area: tx_id -> list of (op_type, key, value)
map<int, vector<tuple<string, string, string>>> staging_area;


string handle_put(string key, string value)
{
    data_store[key] = value;
    return "OK";
}


string handle_get(string key)
{
    if (data_store.find(key) != data_store.end())
    {
        return data_store[key];
    }
    return "NOT_FOUND";
}


string handle_delete(string key)
{
    if (data_store.find(key) != data_store.end())
    {
        data_store.erase(key);
        return "OK";
    }
    return "NOT_FOUND";
}


string handle_stage(int tx_id, string op_type, string key, string value)
{
    staging_area[tx_id].push_back(make_tuple(op_type, key, value));
    return "STAGED";
}


string handle_prepare(int tx_id)
{
    if (staging_area.find(tx_id) == staging_area.end())
    {
        return "NO|No operations staged";
    }
    return "YES";
}


string handle_commit(int tx_id)
{
    if (staging_area.find(tx_id) == staging_area.end())
    {
        return "NO|No transaction";
    }
    
    // Apply all staged operations
    for (auto& op : staging_area[tx_id])
    {
        string op_type = get<0>(op);
        string key = get<1>(op);
        string value = get<2>(op);
        
        if (op_type == "PUT")
        {
            data_store[key] = value;
        }
        else if (op_type == "DELETE")
        {
            data_store.erase(key);
        }
    }
    
    staging_area.erase(tx_id);
    return "COMMITTED";
}


string handle_abort(int tx_id)
{
    staging_area.erase(tx_id);
    return "ABORTED";
}


void handle_client(SOCKET client_socket, string shard_name)
{
    char buffer[4096];

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

        // ===== SINGLE-KEY OPERATIONS =====
        if (command == "PUT")
        {
            string key, value;
            ss >> key >> value;
            response = handle_put(key, value);
        }
        else if (command == "GET")
        {
            string key;
            ss >> key;
            response = handle_get(key);
        }
        else if (command == "DELETE")
        {
            string key;
            ss >> key;
            response = handle_delete(key);
        }
        
        // ===== 2PC TRANSACTION OPERATIONS =====
        else if (command == "STAGE")
        {
            int tx_id;
            string op_type, key, value;
            ss >> tx_id >> op_type >> key >> value;
            response = handle_stage(tx_id, op_type, key, value);
        }
        else if (command == "PREPARE")
        {
            int tx_id;
            ss >> tx_id;
            response = handle_prepare(tx_id);
        }
        else if (command == "COMMIT_TXN")
        {
            int tx_id;
            ss >> tx_id;
            response = handle_commit(tx_id);
        }
        else if (command == "ABORT_TXN")
        {
            int tx_id;
            ss >> tx_id;
            response = handle_abort(tx_id);
        }
        else if (command == "KEYS")
        {
            response = "Keys in this shard:\n";
            for (auto& kv : data_store)
            {
                response += kv.first + ": " + kv.second + "\n";
            }
        }
        else
        {
            response = "INVALID";
        }

        send(client_socket, response.c_str(), response.size(), 0);
    }

    closesocket(client_socket);
}


int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        cout << "Usage: shard_server.exe shard1 7001\n";
        return 1;
    }

    string shard_name = argv[1];
    int port = stoi(argv[2]);

    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    SOCKET server_socket = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    bind(server_socket, (sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_socket, 5);

    cout << shard_name << " running on port " << port << endl;

    while (true)
    {
        sockaddr_in client_addr;
        int client_len = sizeof(client_addr);
        SOCKET client_socket = accept(server_socket, (sockaddr*)&client_addr, &client_len);
        cout << shard_name << " connected" << endl;
        handle_client(client_socket, shard_name);
    }

    closesocket(server_socket);
    WSACleanup();
    return 0;
}