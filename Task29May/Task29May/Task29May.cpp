#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "kernel32.lib")

#define MAX_URL_LENGTH 1024
#define BUFFER_SIZE 4096

using namespace std;

// Парсинг URL
bool parse_url(const char* url, char* host, int* port, char* path) {
    const char* protocol_end = strstr(url, "://");
    if (!protocol_end) return false;

    const char* domain_start = protocol_end + 3;
    const char* path_start = strchr(domain_start, '/');
    if (!path_start) path_start = "/";

    size_t host_len = path_start - domain_start;
    strncpy_s(host, MAX_URL_LENGTH, domain_start, host_len);
    host[host_len] = '\0';

    *port = 80; // По умолчанию HTTP
    strcpy_s(path, MAX_URL_LENGTH, path_start);

    return true;
}

// Callback для WriteFileEx
VOID NTAPI WriteCompletion(DWORD dwErrorCode, DWORD dwBytesWritten, LPOVERLAPPED lpOverlapped) {
    cout << "[DEBUG] Запрос отправлен (" << dwBytesWritten << " байт)" << endl;
}

// Callback для ReadFileEx
VOID NTAPI ReadCompletion(DWORD dwErrorCode, DWORD dwBytesRead, LPOVERLAPPED lpOverlapped) {
    static char buffer[BUFFER_SIZE];
    if (dwErrorCode == ERROR_SUCCESS && dwBytesRead > 0) {
        buffer[dwBytesRead] = '\0';
        cout << "[DEBUG] Получено " << dwBytesRead << " байт от сервера:\n" << buffer << endl;
    }
    else {
        cerr << "[ERROR] Ошибка чтения данных. Код ошибки: " << dwErrorCode << endl;
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        cerr << "Usage: http_client <URL>" << endl;
        return 1;
    }

    char host[MAX_URL_LENGTH];
    int port = 80;
    char path[MAX_URL_LENGTH];

    if (!parse_url(argv[1], host, &port, path)) {
        cerr << "Invalid URL format." << endl;
        return 1;
    }

    // Инициализация Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "Failed to initialize Winsock" << endl;
        return 1;
    }

    // Создание сокета
    SOCKET sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd == INVALID_SOCKET) {
        cerr << "Failed to create socket" << std::endl;
        WSACleanup();
        return 1;
    }

    // Подключение к серверу
    ADDRINFO hints = { 0 }, * res = nullptr;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    char port_str[16];
    sprintf_s(port_str, sizeof(port_str), "%d", port); // Преобразуем порт в строку

    if (getaddrinfo(host, port_str, &hints, &res) != 0) {
        cerr << "Failed to resolve hostname: " << host << endl;
        closesocket(sockfd);
        WSACleanup();
        return 1;
    }

    if (connect(sockfd, res->ai_addr, (int)res->ai_addrlen) == SOCKET_ERROR) {
        cerr << "Failed to connect to server" << endl;
        freeaddrinfo(res);
        closesocket(sockfd);
        WSACleanup();
        return 1;
    }
    freeaddrinfo(res);

    // Формирование HTTP-запроса
    string request = "GET " + string(path) + " HTTP/1.1\r\n";
    request += "Host: " + string(host) + "\r\n";
    request += "Connection: close\r\n\r\n";

    // Преобразуем сокет в дескриптор файла
    HANDLE hSocket = (HANDLE)sockfd;

    // Асинхронная отправка запроса
    OVERLAPPED overlappedWrite = { 0 };
    overlappedWrite.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

    if (overlappedWrite.hEvent == nullptr) {
        cerr << "[ERROR] Не удалось создать событие для записи" << endl;
        closesocket(sockfd);
        WSACleanup();
        return 1;
    }

    DWORD bytesSent = 0;
    BOOL success = WriteFileEx(hSocket, request.c_str(), static_cast<DWORD>(request.length()), &overlappedWrite, WriteCompletion);
    if (!success) {
        cerr << "[ERROR] Не удалось запланировать асинхронную запись" << endl;
        CloseHandle(overlappedWrite.hEvent);
        closesocket(sockfd);
        WSACleanup();
        return 1;
    }

    // Ожидание завершения записи с таймаутом
    DWORD timeout = 5000; // 5 секунд
    DWORD result = WaitForSingleObject(overlappedWrite.hEvent, timeout);

    if (result == WAIT_TIMEOUT) {
        std::cerr << "[ERROR] Таймаут при отправке запроса" << std::endl;
    }
    else if (result != WAIT_OBJECT_0) {
        std::cerr << "[ERROR] Ошибка при ожидании завершения записи" << std::endl;
    }

    // Асинхронное чтение ответа
    OVERLAPPED overlappedRead = { 0 };
    overlappedRead.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

    if (overlappedRead.hEvent == nullptr) {
        cerr << "[ERROR] Не удалось создать событие для чтения" << endl;
        CloseHandle(overlappedWrite.hEvent);
        closesocket(sockfd);
        WSACleanup();
        return 1;
    }

    char buffer[BUFFER_SIZE];
    DWORD bytesRead = 0;

    success = ReadFileEx(hSocket, buffer, BUFFER_SIZE - 1, &overlappedRead, ReadCompletion);
    if (!success) {
        cerr << "[ERROR] Не удалось запланировать асинхронное чтение" << endl;
        CloseHandle(overlappedWrite.hEvent);
        CloseHandle(overlappedRead.hEvent);
        closesocket(sockfd);
        WSACleanup();
        return 1;
    }

    // Ожидание завершения чтения с таймаутом
    result = WaitForSingleObject(overlappedRead.hEvent, timeout);

    if (result == WAIT_TIMEOUT) {
        std::cerr << "[ERROR] Таймаут при чтении данных" << std::endl;
    }
    else if (result != WAIT_OBJECT_0) {
        std::cerr << "[ERROR] Ошибка при ожидании завершения чтения" << std::endl;
    }

    buffer[BUFFER_SIZE - 1] = '\0'; // Защита от переполнения
    cout << "[DEBUG] Ответ сервера:\n" << buffer << endl;

    // Закрытие ресурсов
    CloseHandle(overlappedWrite.hEvent);
    CloseHandle(overlappedRead.hEvent);
    closesocket(sockfd);
    WSACleanup();

    return 0;
}