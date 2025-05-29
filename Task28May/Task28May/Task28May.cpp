#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>

#pragma comment(lib, "ws2_32.lib")

#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#pragma warning(disable : 4996)

#define MAX_URL_LENGTH 1024
#define BUFFER_SIZE 4096

// Парсинг URL
bool parse_url(const char* url, char* host, int* port, char* path) {
    std::cout << "[DEBUG] Вход в parse_url(), URL = " << url << std::endl;

    const char* protocol_end = strstr(url, "://");
    if (!protocol_end) {
        std::cerr << "[ERROR] URL не содержит '://'" << std::endl;
        return false;
    }

    const char* domain_start = protocol_end + 3;
    const char* path_start = strchr(domain_start, '/');
    if (!path_start) {
        std::cerr << "[WARNING] Путь не найден, используется '/' по умолчанию" << std::endl;
        path_start = "/";
    }

    size_t host_len = path_start - domain_start;
    if (host_len >= MAX_URL_LENGTH) host_len = MAX_URL_LENGTH - 1;

    strncpy_s(host, MAX_URL_LENGTH, domain_start, host_len);
    host[host_len] = '\0';

    *port = 80; // По умолчанию HTTP
    strcpy_s(path, MAX_URL_LENGTH, path_start);

    std::cout << "[DEBUG] URL разобран: хост=" << host << ", порт=" << *port << ", путь=" << path << std::endl;
    return true;
}

int main(int argc, char* argv[]) {
    setlocale(LC_ALL, "ru");
    if (argc != 2) {
        std::cerr << "Usage: http_client <URL>" << std::endl;
        return 1;
    }

    const char* test_url = argv[1]; // Получаем URL из аргумента
    std::cout << "[DEBUG] Программа запущена. Тестовый URL: " << test_url << std::endl;

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "[ERROR] Не удалось инициализировать Winsock" << std::endl;
        return 1;
    }
    std::cout << "[DEBUG] Winsock успешно инициализирован" << std::endl;

    char host[MAX_URL_LENGTH];
    int port = 80;
    char path[MAX_URL_LENGTH];

    if (!parse_url(test_url, host, &port, path)) {
        std::cerr << "[ERROR] Ошибка при разборе URL" << std::endl;
        WSACleanup();
        return 1;
    }

    SOCKET sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd == INVALID_SOCKET) {
        std::cerr << "[ERROR] Не удалось создать сокет" << std::endl;
        WSACleanup();
        return 1;
    }
    std::cout << "[DEBUG] Сокет создан" << std::endl;

    ADDRINFO hints = { 0 }, * res = nullptr;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    std::string port_str = std::to_string(port);
    if (getaddrinfo(host, port_str.c_str(), &hints, &res) != 0) {
        std::cerr << "[ERROR] Не удалось разрешить имя хоста: " << host << std::endl;
        closesocket(sockfd);
        WSACleanup();
        return 1;
    }
    std::cout << "[DEBUG] Адрес хоста разрешён" << std::endl;

    std::cout << "[DEBUG] Попытка подключения к серверу..." << std::endl;

    if (connect(sockfd, res->ai_addr, (int)res->ai_addrlen) == SOCKET_ERROR) {
        std::cerr << "[ERROR] Ошибка подключения к серверу" << std::endl;
        freeaddrinfo(res);
        closesocket(sockfd);
        WSACleanup();
        return 1;
    }

    freeaddrinfo(res);
    std::cout << "[DEBUG] Подключение установлено" << std::endl;

    // === Отправляем HTTP-запрос ===
    std::string request = "GET " + std::string(path) + " HTTP/1.1\r\n";
    request += "Host: " + std::string(host) + "\r\n";
    request += "Connection: close\r\n\r\n";

    send(sockfd, request.c_str(), static_cast<int>(request.length()), 0);
    std::cout << "[DEBUG] Запрос отправлен:\n" << request << std::endl;

    // === Ждём данные от сервера с помощью select() ===
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(sockfd, &read_fds);

    timeval timeout;
    timeout.tv_sec = 5;  // Ждём до 5 секунд
    timeout.tv_usec = 0;

    int activity = select(0, &read_fds, nullptr, nullptr, &timeout);
    if (activity == SOCKET_ERROR) {
        std::cerr << "[ERROR] Ошибка select(): " << WSAGetLastError() << std::endl;
    }
    else if (activity == 0) {
        std::cerr << "[ERROR] Таймаут: данные от сервера не получены" << std::endl;
    }
    else {
        // === Данные доступны — читаем ===
        char buffer[BUFFER_SIZE];
        int bytes_read = recv(sockfd, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            std::cout << "[DEBUG] Прямой вывод ответа сервера:\n" << buffer << std::endl;
        }
        else {
            std::cerr << "[ERROR] recv() вернул " << bytes_read << ", соединение закрыто сервером" << std::endl;
        }
    }

    closesocket(sockfd);
    WSACleanup();
    std::cout << "[DEBUG] Программа завершена" << std::endl;

    std::cin.get(); // Чтобы окно не закрывалось
    return 0;
}