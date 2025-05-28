#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <vector>
#include <string>
#include <io.h>      // Для _fileno
#include <fcntl.h>   // Для _setmode
#include <conio.h>   // Для _kbhit(), _getch()

#pragma comment(lib, "ws2_32.lib")

#define MAX_URL_LENGTH 1024
#define BUFFER_SIZE 4096
#define SCREEN_LINES 25

// Отключаем предупреждения о устаревших функциях
#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#pragma warning(disable : 4996)

// Функция для установки неblocking режима сокета
void set_nonblocking(SOCKET fd) {
    u_long mode = 1;
    ioctlsocket(fd, FIONBIO, &mode);
}

// Парсинг URL
bool parse_url(const char* url, char* host, int* port, char* path) {
    const char* protocol_end = strstr(url, "://");
    if (!protocol_end) return false;

    // Получаем домен и путь
    const char* domain_start = protocol_end + 3;
    const char* path_start = strchr(domain_start, '/');
    if (!path_start) path_start = "/";

    // Копируем домен
    size_t host_len = path_start - domain_start;
    strncpy_s(host, MAX_URL_LENGTH, domain_start, host_len);
    host[host_len] = '\0';

    // Устанавливаем порт по умолчанию (80)
    *port = 80;

    // Копируем путь
    strcpy_s(path, MAX_URL_LENGTH, path_start);

    return true;
}

int main(int argc, char* argv[]) {
    // Инициализация Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "Failed to initialize Winsock" << std::endl;
        return 1;
    }

    if (argc != 2) {
        std::cerr << "Usage: http_client <URL>" << std::endl;
        WSACleanup();
        return 1;
    }

    char host[MAX_URL_LENGTH];
    int port = 80;
    char path[MAX_URL_LENGTH];

    if (!parse_url(argv[1], host, &port, path)) {
        std::cerr << "Invalid URL format." << std::endl;
        WSACleanup();
        return 1;
    }

    // Создание сокета
    SOCKET sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd == INVALID_SOCKET) {
        std::cerr << "Failed to create socket" << std::endl;
        WSACleanup();
        return 1;
    }

    // Разрешение имени хоста через getaddrinfo
    ADDRINFO hints = { 0 }, * res = nullptr;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    std::string port_str = std::to_string(port);
    if (getaddrinfo(host, port_str.c_str(), &hints, &res) != 0) {
        std::cerr << "Failed to resolve hostname: " << host << std::endl;
        closesocket(sockfd);
        WSACleanup();
        return 1;
    }

    // Подключение к серверу
    if (connect(sockfd, res->ai_addr, (int)res->ai_addrlen) == SOCKET_ERROR) {
        std::cerr << "Failed to connect to server" << std::endl;
        freeaddrinfo(res);
        closesocket(sockfd);
        WSACleanup();
        return 1;
    }

    freeaddrinfo(res);

    // Отправка HTTP-запроса
    std::string request = "GET " + std::string(path) + " HTTP/1.1\r\n";
    request += "Host: " + std::string(host) + "\r\n";
    request += "Connection: close\r\n\r\n";

    send(sockfd, request.c_str(), static_cast<int>(request.length()), 0);

    // Настройка неblocking режима для stdin и sockfd
    set_nonblocking(sockfd);

    // Буфер для чтения данных
    char buffer[BUFFER_SIZE];
    int total_lines = 0;

    while (true) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(sockfd, &read_fds);

        // Добавляем stdin
        FD_SET((SOCKET)_fileno(stdin), &read_fds);

        int max_fd = static_cast<int>(sockfd) + 1;

        select(max_fd, &read_fds, nullptr, nullptr, nullptr);

        // Проверяем, есть ли данные от сервера
        if (FD_ISSET(sockfd, &read_fds)) {
            int bytes_read = recv(sockfd, buffer, BUFFER_SIZE - 1, 0);
            if (bytes_read <= 0) break;

            buffer[bytes_read] = '\0';
            std::cout.write(buffer, bytes_read);
            total_lines += bytes_read / 80; // Примерное количество строк

            if (total_lines > SCREEN_LINES) {
                std::cout << "\nPress space to scroll down..." << std::endl;
                while (true) {
                    if (_kbhit() && _getch() == ' ') break;
                    Sleep(100);
                }
                total_lines = 0;
            }
        }

        // Проверяем, если пользователь нажал пробел
        if (FD_ISSET((SOCKET)_fileno(stdin), &read_fds)) {
            char key;
            int bytes = read(_fileno(stdin), &key, 1);
            if (bytes > 0 && key == ' ') {
                total_lines = 0;
            }
        }
    }

    closesocket(sockfd);
    WSACleanup();

    return 0;
}