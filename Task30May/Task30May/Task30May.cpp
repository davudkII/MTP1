#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <mutex>
#include <string>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")

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

// Чтение данных из сокета
// Функция для чтения данных из сокета
void read_from_socket(SOCKET sockfd, string& buffer, mutex& mtx, bool* running) {
    char recv_buffer[BUFFER_SIZE];
    while (*running) {
        int bytes_received = recv(sockfd, recv_buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received <= 0) {
            cout << "[DEBUG] Сервер закрыл соединение" << endl;
            *running = false;
            break;
        }

        recv_buffer[bytes_received] = '\0';
        {
            lock_guard<mutex> lock(mtx);
            buffer += recv_buffer;
        }
    }
}

// Взаимодействие с пользователем через Windows API
void user_interaction(string& buffer, mutex& mtx, bool* running) {
    cout << "[DEBUG] Нажмите Enter для вывода, Esc — для выхода..." << endl;

    while (*running) {
        HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
        if (hStdin == INVALID_HANDLE_VALUE) break;

        INPUT_RECORD inputRecord;
        DWORD eventsRead;

        if (ReadConsoleInput(hStdin, &inputRecord, 1, &eventsRead)) {
            if (inputRecord.EventType == KEY_EVENT &&
                inputRecord.Event.KeyEvent.bKeyDown) {

                // Выход по Esc
                if (inputRecord.Event.KeyEvent.wVirtualKeyCode == VK_ESCAPE) {
                    cout << "[DEBUG] Получен сигнал выхода от пользователя" << endl;
                    *running = false;
                }

                // Вывод данных по Enter
                if (inputRecord.Event.KeyEvent.wVirtualKeyCode == VK_RETURN) {
                    lock_guard<mutex> lock(mtx);
                    cout << "[DEBUG] Полученные данные:\n" << buffer << "\n--- Конец данных ---\n";
                    buffer.clear();
                }

                FlushConsoleInputBuffer(hStdin);
            }
        }

        Sleep(100); // Защита от высокой нагрузки
    }
}

int main(int argc, char* argv[]) {
    setlocale(LC_ALL, "ru");
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

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "Failed to initialize Winsock" << endl;
        return 1;
    }

    SOCKET sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd == INVALID_SOCKET) {
        cerr << "Failed to create socket" << endl;
        WSACleanup();
        return 1;
    }

    ADDRINFO hints = { 0 }, * res = nullptr;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    string port_str = std::to_string(port);
    if (getaddrinfo(host, port_str.c_str(), &hints, &res) != 0) {
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

    string request = "GET " + string(path) + " HTTP/1.1\r\n";
    request += "Host: " + string(host) + "\r\n";
    request += "Connection: close\r\n\r\n";

    send(sockfd, request.c_str(), static_cast<int>(request.length()), 0);
    cout << "[DEBUG] Запрос отправлен:\n" << request << endl;

    string buffer;
    mutex mtx;
    bool running = true;

    thread reader(read_from_socket, sockfd, ref(buffer), ref(mtx), &running);
    thread user(user_interaction, ref(buffer), ref(mtx), &running);

    while (running) {
        Sleep(100);
    }

    reader.join();
    user.join();

    closesocket(sockfd);
    WSACleanup();

    cout << "[DEBUG] Программа завершена" << endl;

    return 0;
}