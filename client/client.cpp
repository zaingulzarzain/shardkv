#include <iostream>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

int main()
{
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    SOCKET client_socket = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8000);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(client_socket, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR)
    {
        cout << "Connection failed\n";
        return 1;
    }

    cout << "Connected to coordinator\n\n";
    cout << "Commands:\n";
    cout << "  PUT key value\n";
    cout << "  GET key\n";
    cout << "  DELETE key\n";
    cout << "  BEGIN              - start transaction\n";
    cout << "  COMMIT             - commit transaction (2PC)\n";
    cout << "  ABORT              - abort transaction\n";
    cout << "  \\keys              - show all keys\n";
    cout << "  \\stats             - show statistics\n";
    cout << "  EXIT\n\n";

    while (true)
    {
        cout << "> ";
        string message;
        getline(cin, message);

        if (message == "EXIT" || message == "exit") break;

        send(client_socket, message.c_str(), message.size(), 0);

        char buffer[8192];
        memset(buffer, 0, sizeof(buffer));
        int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);

        if (bytes_received <= 0)
        {
            cout << "Disconnected\n";
            break;
        }

        cout << buffer << endl << endl;
    }

    closesocket(client_socket);
    WSACleanup();
    return 0;
}