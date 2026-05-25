#include <iostream>
#include <string>
#include <unordered_map>
#include <sstream>

#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

using namespace std;


unordered_map<string, string> data_store;


void handle_client(
    SOCKET client_socket,
    string shard_name
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

        // PUT
        if (command == "PUT")
        {
            string key;
            string value;

            ss >> key;
            ss >> value;

            data_store[key] = value;

            response =
                "Stored in " + shard_name;
        }

        // GET
        else if (command == "GET")
        {
            string key;

            ss >> key;

            if (data_store.find(key)
                != data_store.end())
            {
                response = data_store[key];
            }
            else
            {
                response = "Key not found";
            }
        }

        // DELETE
        else if (command == "DELETE")
        {
            string key;

            ss >> key;

            if (data_store.find(key)
                != data_store.end())
            {
                data_store.erase(key);

                response =
                    "Deleted successfully";
            }
            else
            {
                response = "Key not found";
            }
        }

        else
        {
            response = "Invalid command";
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


int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        cout << "Usage:\n";

        cout << "./shard_server.exe "
             << "shard1 7001\n";

        return 1;
    }

    string shard_name = argv[1];

    int port = stoi(argv[2]);

    // initialize winsock
    WSADATA wsa;

    WSAStartup(
        MAKEWORD(2, 2),
        &wsa
    );

    // create socket
    SOCKET server_socket =
        socket(
            AF_INET,
            SOCK_STREAM,
            0
        );

    sockaddr_in server_addr;

    server_addr.sin_family = AF_INET;

    server_addr.sin_port = htons(port);

    server_addr.sin_addr.s_addr =
        INADDR_ANY;

    // bind
    bind(
        server_socket,
        (sockaddr*)&server_addr,
        sizeof(server_addr)
    );

    // listen
    listen(server_socket, 5);

    cout << shard_name
         << " running on port "
         << port
         << endl;

    while (true)
    {
        sockaddr_in client_addr;

        int client_len =
            sizeof(client_addr);

        SOCKET client_socket =
            accept(
                server_socket,
                (sockaddr*)&client_addr,
                &client_len
            );

        cout << shard_name
             << " connected"
             << endl;

        handle_client(
            client_socket,
            shard_name
        );
    }

    closesocket(server_socket);

    WSACleanup();

    return 0;
}