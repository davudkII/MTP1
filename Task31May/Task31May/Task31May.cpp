#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <unordered_map>
#include <string>
#include <vector>
#include <algorithm>
#include <regex>

#pragma comment(lib, "ws2_32.lib")

#define MAX_URL_LENGTH 2048
#define BUFFER_SIZE 16384
#define CACHE_MAX_SIZE 100
#define DEFAULT_HTTP_PORT 80
#define PROXY_PORT 8080

using namespace std;

struct ClientContext {
    string request_data;
    SOCKET server_socket = INVALID_SOCKET;
};

unordered_map<SOCKET, ClientContext> clients;
unordered_map<string, pair<string, time_t>> cache; // URL -> (response, timestamp)

// Парсинг URL из Host и GET
bool parse_url(const string& request, string& host, string& path, int& port) {
    smatch match;
    regex host_regex(R"(Host:\s*([^\s\r\n]+)(?::(\d+))?)", regex_constants::icase);
    if (regex_search(request, match, host_regex) && match.size() >= 2) {
        host = match[1].str();
        port = (match.size() > 2 && !match[2].str().empty()) ? stoi(match[2].str()) : DEFAULT_HTTP_PORT;
    }
    else {
        return false;
    }

    regex get_regex(R"(GET\s+(https?://[^/\s]+)?(/[^\s]*)?\s+HTTP)", regex_constants::icase);
    if (regex_search(request, match, get_regex) && match.size() >= 3) {
        path = match[2].matched ? match[2].str() : "/";
        return true;
    }

    return false;
}

// Подключение к целевому серверу
SOCKET connect_to_server(const string& host, int port) {
    addrinfo hints = { 0 }, * res = nullptr;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    string port_str = to_string(port);
    if (getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res) != 0) {
        cerr << "[ERROR] Failed to resolve host: " << host << endl;
        return INVALID_SOCKET;
    }

    SOCKET server_socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (server_socket == INVALID_SOCKET) {
        freeaddrinfo(res);
        return INVALID_SOCKET;
    }

    DWORD timeout = 5000; // 5 секунд
    setsockopt(server_socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
    setsockopt(server_socket, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));

    if (connect(server_socket, res->ai_addr, (int)res->ai_addrlen) == SOCKET_ERROR) {
        int error_code = WSAGetLastError();
        cerr << "[ERROR] Connection failed. Error code: " << error_code << endl;
        closesocket(server_socket);
        server_socket = INVALID_SOCKET;
    }

    freeaddrinfo(res);
    return server_socket;
}

// Отправка ошибок клиенту
void send_error_response(SOCKET client_socket, int code, const string& message) {
    string response = "HTTP/1.1 " + to_string(code) + " " + message + "\r\n";
    response += "Content-Type: text/plain\r\n";
    response += "Connection: close\r\n";
    response += "Cache-Control: no-cache\r\n";
    response += "\r\n";
    response += message;
    send(client_socket, response.c_str(), (int)response.size(), 0);
}

// Очистка кэша
void cleanup_cache() {
    const time_t now = time(nullptr);
    const time_t cache_ttl = 300; // 5 минут TTL

    for (auto it = cache.begin(); it != cache.end();) {
        if (now - it->second.second > cache_ttl) {
            it = cache.erase(it);
        }
        else {
            ++it;
        }
    }

    // Ограничение размера кэша
    if (cache.size() > CACHE_MAX_SIZE) {
        auto oldest = min_element(cache.begin(), cache.end(),
            [](const auto& a, const auto& b) { return a.second.second < b.second.second; });
        if (oldest != cache.end()) {
            cache.erase(oldest);
        }
    }
}

// Обработка клиентского запроса
void process_client_request(SOCKET client_socket, ClientContext& context) {
    string host, path;
    int port = DEFAULT_HTTP_PORT;

    if (!parse_url(context.request_data, host, path, port)) {
        send_error_response(client_socket, 400, "Bad Request");
        return;
    }

    string cache_key = host + ":" + to_string(port) + path;
    cleanup_cache();

    auto cache_it = cache.find(cache_key);
    if (cache_it != cache.end()) {
        cout << "[CACHE] Serving from cache: " << cache_key << endl;
        send(client_socket, cache_it->second.first.c_str(), (int)cache_it->second.first.size(), 0);
        return;
    }

    context.server_socket = connect_to_server(host, port);
    if (context.server_socket == INVALID_SOCKET) {
        send_error_response(client_socket, 502, "Bad Gateway: Can't connect to target server");
        return;
    }

    // Убираем ненужные заголовки
    vector<string> headers_to_remove = { "Proxy-Connection", "If-Modified-Since", "If-None-Match" };
    string modified_request = context.request_data;
    for (const auto& header : headers_to_remove) {
        regex header_regex(header + ":.*\r\n", regex_constants::icase);
        modified_request = regex_replace(modified_request, header_regex, "");
    }

    if (send(context.server_socket, modified_request.c_str(), (int)modified_request.size(), 0) == SOCKET_ERROR) {
        send_error_response(client_socket, 502, "Bad Gateway: Failed to forward request");
        closesocket(context.server_socket);
        context.server_socket = INVALID_SOCKET;
        return;
    }

    cout << "[PROXY] Forwarding request to: " << host << path << endl;
}

// Обработка ответа сервера
void process_server_response(SOCKET client_socket, ClientContext& context) {
    char buffer[BUFFER_SIZE];
    string response_data;

    while (true) {
        int bytes_received = recv(context.server_socket, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received <= 0) break;
        buffer[bytes_received] = '\0';
        response_data.append(buffer, bytes_received);
    }

    if (!response_data.empty()) {
        if (response_data.find("HTTP/1.1 200") != string::npos) {
            string host, path;
            int port;
            if (parse_url(context.request_data, host, path, port)) {
                string cache_key = host + ":" + to_string(port) + path;
                cache[cache_key] = make_pair(response_data, time(nullptr));
            }
        }
        send(client_socket, response_data.c_str(), (int)response_data.size(), 0);
    }

    closesocket(context.server_socket);
    context.server_socket = INVALID_SOCKET;
}

// Поддержка HTTPS через CONNECT
void handle_connect_method(SOCKET client_socket, const string& host, int port) {
    string response = "HTTP/1.1 200 Connection established\r\n";
    response += "Proxy-Agent: MyProxy/1.0\r\n\r\n";
    send(client_socket, response.c_str(), (int)response.size(), 0);

    // Создаем туннель между клиентом и сервером
    string remote_host = host;
    SOCKET server_socket = connect_to_server(remote_host, port);
    if (server_socket == INVALID_SOCKET) {
        send_error_response(client_socket, 502, "Bad Gateway: Can't connect to HTTPS server");
        return;
    }

    cout << "[HTTPS] Tunnel established with: " << remote_host << ":" << port << endl;

    char buffer[BUFFER_SIZE];
    fd_set fds;
    timeval tv{ 1, 0 };

    while (true) {
        FD_ZERO(&fds);
        FD_SET(client_socket, &fds);
        FD_SET(server_socket, &fds);

        SOCKET max_fd = max(client_socket, server_socket);
        int activity = select((int)max_fd + 1, &fds, nullptr, nullptr, &tv);
        if (activity <= 0) continue;

        if (FD_ISSET(client_socket, &fds)) {
            int len = recv(client_socket, buffer, BUFFER_SIZE, 0);
            if (len <= 0) break;
            send(server_socket, buffer, len, 0);
        }

        if (FD_ISSET(server_socket, &fds)) {
            int len = recv(server_socket, buffer, BUFFER_SIZE, 0);
            if (len <= 0) break;
            send(client_socket, buffer, len, 0);
        }
    }

    closesocket(server_socket);
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "Failed to initialize Winsock" << endl;
        return 1;
    }

    SOCKET proxy_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (proxy_socket == INVALID_SOCKET) {
        cerr << "Failed to create proxy socket" << endl;
        WSACleanup();
        return 1;
    }

    int reuse = 1;
    setsockopt(proxy_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));

    sockaddr_in proxy_addr{};
    proxy_addr.sin_family = AF_INET;
    proxy_addr.sin_addr.s_addr = INADDR_ANY;
    proxy_addr.sin_port = htons(PROXY_PORT);

    if (bind(proxy_socket, (SOCKADDR*)&proxy_addr, sizeof(proxy_addr)) == SOCKET_ERROR) {
        cerr << "Failed to bind proxy socket" << endl;
        closesocket(proxy_socket);
        WSACleanup();
        return 1;
    }

    if (listen(proxy_socket, SOMAXCONN) == SOCKET_ERROR) {
        cerr << "Failed to listen on proxy socket" << endl;
        closesocket(proxy_socket);
        WSACleanup();
        return 1;
    }

    cout << "Proxy server is running on port " << PROXY_PORT << endl;

    fd_set read_fds;
    timeval timeout{ 1, 0 };

    while (true) {
        FD_ZERO(&read_fds);
        FD_SET(proxy_socket, &read_fds);
        SOCKET max_fd = proxy_socket;

        vector<SOCKET> to_remove;

        // Добавляем клиентские сокеты
        for (auto& client : clients) {
            FD_SET(client.first, &read_fds);
            max_fd = max(max_fd, client.first);

            if (client.second.server_socket != INVALID_SOCKET) {
                FD_SET(client.second.server_socket, &read_fds);
                max_fd = max(max_fd, client.second.server_socket);
            }
        }

        int activity = select((int)max_fd + 1, &read_fds, nullptr, nullptr, &timeout);
        if (activity == SOCKET_ERROR) {
            cerr << "select() error: " << WSAGetLastError() << endl;
            continue;
        }

        // Новое подключение
        if (FD_ISSET(proxy_socket, &read_fds)) {
            SOCKET client_socket = accept(proxy_socket, nullptr, nullptr);
            if (client_socket != INVALID_SOCKET) {
                cout << "New client connected" << endl;
                clients[client_socket] = ClientContext();
            }
        }

        // Чтение от клиентов
        for (auto it = clients.begin(); it != clients.end(); ++it) {
            SOCKET client_socket = it->first;
            ClientContext& context = it->second;

            if (FD_ISSET(client_socket, &read_fds)) {
                char buffer[BUFFER_SIZE];
                int bytes = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
                if (bytes <= 0) {
                    to_remove.push_back(client_socket);
                    continue;
                }
                buffer[bytes] = '\0';
                context.request_data.append(buffer, bytes);

                // Проверяем, есть ли CONNECT
                if (context.request_data.find("CONNECT") != string::npos) {
                    smatch match;
                    regex connect_regex(R"(CONNECT ([^:]+):(\d+))", regex_constants::icase);
                    if (regex_search(context.request_data, match, connect_regex) && match.size() >= 3) {
                        string host = match[1].str();
                        int port = stoi(match[2].str());
                        cout << "[HTTPS] Establishing tunnel to: " << host << ":" << port << endl;
                        FD_CLR(client_socket, &read_fds); // Удаляем из набора
                        handle_connect_method(client_socket, host, port);
                        to_remove.push_back(client_socket);
                    }
                }

                // Проверяем завершение заголовков
                if (context.request_data.find("\r\n\r\n") != string::npos) {
                    process_client_request(client_socket, context);
                }
            }

            // Чтение от сервера
            if (context.server_socket != INVALID_SOCKET &&
                FD_ISSET(context.server_socket, &read_fds)) {
                process_server_response(client_socket, context);
                to_remove.push_back(client_socket);
            }
        }

        // Удаление отключенных клиентов
        for (SOCKET s : to_remove) {
            if (clients[s].server_socket != INVALID_SOCKET)
                closesocket(clients[s].server_socket);
            closesocket(s);
            clients.erase(s);
        }
    }

    // Очистка ресурсов 
    for (auto& client : clients) {
        if (client.second.server_socket != INVALID_SOCKET)
            closesocket(client.second.server_socket);
        closesocket(client.first);
    }

    closesocket(proxy_socket);
    WSACleanup();
    return 0;
}
