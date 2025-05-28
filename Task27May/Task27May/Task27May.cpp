#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <vector>
#include <string>



#pragma comment(lib, "ws2_32.lib")

#define MAX_CONNECTIONS 510
#define BUFFER_SIZE 4096

// Функция для установки неblocking режима сокета
void set_nonblocking(SOCKET fd) {
    u_long mode = 1;
    ioctlsocket(fd, FIONBIO, &mode);
}

// Функция для установки соединения с удалённым сервером
SOCKET connect_to_remote(const char* host, int port) {
    struct hostent* server = gethostbyname(host);
    if (!server) {
        std::cerr << "Failed to resolve hostname: " << host << std::endl;
        return INVALID_SOCKET;
    }

    SOCKADDR_IN server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr_list[0], server->h_length);
    server_addr.sin_port = htons(port);

    SOCKET sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd == INVALID_SOCKET) {
        std::cerr << "Failed to create socket" << std::endl;
        return INVALID_SOCKET;
    }

    if (connect(sockfd, (SOCKADDR*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        std::cerr << "Failed to connect to remote server" << std::endl;
        closesocket(sockfd);
        return INVALID_SOCKET;
    }

    return sockfd;
}

int main(int argc, char* argv[]) {
    // Инициализация Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "Failed to initialize Winsock" << std::endl;
        return 1;
    }

    if (argc != 4) {
        std::cerr << "Usage: server <listen_port> <remote_host> <remote_port>" << std::endl;
        WSACleanup();
        return 1;
    }

    int listen_port = std::stoi(argv[1]);
    const char* remote_host = argv[2];
    int remote_port = std::stoi(argv[3]);

    // Создание слушающего сокета
    SOCKET listen_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_fd == INVALID_SOCKET) {
        std::cerr << "Failed to create listening socket" << std::endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(listen_port);

    if (bind(listen_fd, (SOCKADDR*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        std::cerr << "Failed to bind socket" << std::endl;
        closesocket(listen_fd);
        WSACleanup();
        return 1;
    }

    if (listen(listen_fd, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Failed to listen on socket" << std::endl;
        closesocket(listen_fd);
        WSACleanup();
        return 1;
    }

    std::cout << "Server is listening on port " << listen_port << std::endl;

    // Массив для хранения всех дескрипторов
    std::vector<SOCKET> client_socks;
    std::vector<SOCKET> remote_socks;
    std::vector<char*> buffers;
    std::vector<int> buffer_sizes;

    while (true) {
        // Ожидание нового соединения
        sockaddr_in client_addr;
        int client_len = sizeof(client_addr);
        SOCKET client_sock = accept(listen_fd, (SOCKADDR*)&client_addr, &client_len);
        if (client_sock == INVALID_SOCKET) {
            std::cerr << "Failed to accept connection" << std::endl;
            continue;
        }

        // Если достигнут лимит соединений
        if (client_socks.size() >= MAX_CONNECTIONS) {
            std::cerr << "Connection limit reached. Closing new connection." << std::endl;
            closesocket(client_sock);
            continue;
        }

        std::cout << "Accepted connection from " << inet_ntoa(client_addr.sin_addr) << ":" << ntohs(client_addr.sin_port) << std::endl;

        // Установка соединения с удалённым сервером
        SOCKET remote_sock = connect_to_remote(remote_host, remote_port);
        if (remote_sock == INVALID_SOCKET) {
            std::cerr << "Failed to connect to remote server." << std::endl;
            closesocket(client_sock);
            continue;
        }

        // Добавление дескрипторов в списки
        client_socks.push_back(client_sock);
        remote_socks.push_back(remote_sock);
        buffers.push_back(new char[BUFFER_SIZE]);
        buffer_sizes.push_back(0);

        // Настраиваем сокеты на non-blocking режим
        set_nonblocking(client_sock);
        set_nonblocking(remote_sock);
    }

    // Очистка ресурсов
    for (size_t i = 0; i < client_socks.size(); ++i) {
        delete[] buffers[i];
        closesocket(client_socks[i]);
        closesocket(remote_socks[i]);
    }

    closesocket(listen_fd);
    WSACleanup();

    return 0;
}