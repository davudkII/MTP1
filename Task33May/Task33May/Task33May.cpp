#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <unordered_map>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <regex>
#include <memory>
#include <queue>

#pragma comment(lib, "ws2_32.lib")

// Константы
constexpr int MAX_URL_LENGTH = 2048;
constexpr int BUFFER_SIZE = 16384;
constexpr int CACHE_MAX_SIZE = 100;
constexpr int DEFAULT_HTTP_PORT = 80;
constexpr int PROXY_PORT = 8080;
constexpr int CACHE_TTL_SEC = 300; // 5 минут

using namespace std;

// Структура для хранения кэшированных ответов
struct CachedResponse {
    string data;
    time_t timestamp;
    string etag;
    string last_modified;
};

// Класс для управления соединением
class Connection {
public:
    SOCKET socket;
    string buffer;
    SOCKET peer_socket = INVALID_SOCKET;
    string host;
    string path;
    int port = DEFAULT_HTTP_PORT;

    Connection(SOCKET s) : socket(s) {}
    ~Connection() {
        if (socket != INVALID_SOCKET) closesocket(socket);
        if (peer_socket != INVALID_SOCKET) closesocket(peer_socket);
    }
};

// Класс для управления кэшем
class CacheManager {
private:
    unordered_map<string, CachedResponse> cache;
    mutex mtx;

public:
    void put(const string& key, const string& data, const string& etag, const string& last_modified) {
        lock_guard<mutex> lock(mtx);
        cleanup();
        if (cache.size() >= CACHE_MAX_SIZE) {
            auto oldest = min_element(cache.begin(), cache.end(),
                [](const auto& a, const auto& b) { return a.second.timestamp < b.second.timestamp; });
            cache.erase(oldest);
        }
        cache[key] = { data, time(nullptr), etag, last_modified };
    }

    pair<bool, CachedResponse> get(const string& key) {
        lock_guard<mutex> lock(mtx);
        auto it = cache.find(key);
        if (it != cache.end() && (time(nullptr) - it->second.timestamp) <= CACHE_TTL_SEC) {
            return { true, it->second };
        }
        return { false, {} };
    }

    void cleanup() {
        time_t now = time(nullptr);
        for (auto it = cache.begin(); it != cache.end(); ) {
            if (now - it->second.timestamp > CACHE_TTL_SEC) {
                it = cache.erase(it);
            }
            else {
                ++it;
            }
        }
    }
};

// Класс для управления соединениями
class ConnectionManager {
private:
    vector<unique_ptr<Connection>> connections;
    mutex mtx;

public:
    void add(unique_ptr<Connection> conn) {
        lock_guard<mutex> lock(mtx);
        connections.push_back(move(conn));
    }

    void remove(SOCKET s) {
        lock_guard<mutex> lock(mtx);
        connections.erase(
            remove_if(connections.begin(), connections.end(),
                [s](const unique_ptr<Connection>& c) { return c->socket == s; }),
            connections.end());
    }

    vector<SOCKET> get_sockets() {
        lock_guard<mutex> lock(mtx);
        vector<SOCKET> sockets;
        for (const auto& conn : connections) {
            sockets.push_back(conn->socket);
            if (conn->peer_socket != INVALID_SOCKET) {
                sockets.push_back(conn->peer_socket);
            }
        }
        return sockets;
    }

    Connection* get_connection(SOCKET s) {
        lock_guard<mutex> lock(mtx);
        for (const auto& conn : connections) {
            if (conn->socket == s || conn->peer_socket == s) {
                return conn.get();
            }
        }
        return nullptr;
    }
};

// Функции для работы с HTTP
namespace HttpUtils {
    bool parse_url(const string& request, string& host, string& path, int& port) {
        smatch match;
        regex host_regex(R"(Host:\s*([^\s:\r\n]+)(?::(\d+))?)", regex_constants::icase);
        if (regex_search(request, match, host_regex) && match.size() >= 2) {
            host = match[1].str();
            port = (match.size() > 2 && !match[2].str().empty()) ? stoi(match[2].str()) : DEFAULT_HTTP_PORT;
        }
        else {
            return false;
        }

        regex get_regex(R"(GET\s+(https?://[^/\s]+)?(/[^\s]*)?\s+HTTP)", regex_constants::icase);
        if (regex_search(request, match, get_regex) && match.size() >= 3) {
            path = match[2].str().empty() ? "/" : match[2].str();
            return true;
        }
        return false;
    }

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

        // Устанавливаем таймауты
        DWORD timeout = 5000; // 5 секунд
        setsockopt(server_socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
        setsockopt(server_socket, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));

        if (connect(server_socket, res->ai_addr, (int)res->ai_addrlen) == SOCKET_ERROR) {
            closesocket(server_socket);
            server_socket = INVALID_SOCKET;
        }

        freeaddrinfo(res);
        return server_socket;
    }

    void send_error_response(SOCKET client_socket, int code, const string& message) {
        string response = "HTTP/1.1 " + to_string(code) + " " + message + "\r\n";
        response += "Content-Type: text/plain\r\n";
        response += "Connection: close\r\n";
        response += "\r\n";
        response += message;
        send(client_socket, response.c_str(), (int)response.size(), 0);
    }

    string get_header_value(const string& headers, const string& header_name) {
        regex header_regex(header_name + ":\\s*([^\\r\\n]+)", regex_constants::icase);
        smatch match;
        if (regex_search(headers, match, header_regex) && match.size() >= 2) {
            return match[1].str();
        }
        return "";
    }

    string remove_proxy_headers(const string& request) {
        vector<string> headers_to_remove = {
            "Proxy-Connection", "If-Modified-Since", "If-None-Match"
        };

        string result = request;
        for (const auto& header : headers_to_remove) {
            regex header_regex(header + ":.*\r\n", regex_constants::icase);
            result = regex_replace(result, header_regex, "");
        }
        return result;
    }
}

// Рабочий поток
void worker_thread(SOCKET proxy_socket, CacheManager& cache, ConnectionManager& conn_manager,
    atomic<bool>& stop_flag, int thread_id) {
    fd_set read_fds;
    timeval timeout{ 1, 0 };

    while (!stop_flag.load()) {
        FD_ZERO(&read_fds);
        FD_SET(proxy_socket, &read_fds);
        SOCKET max_fd = proxy_socket;

        // Получаем список сокетов для мониторинга
        auto sockets = conn_manager.get_sockets();
        for (SOCKET s : sockets) {
            FD_SET(s, &read_fds);
            if (s > max_fd) max_fd = s;
        }

        // Ждем активности на сокетах
        int activity = select((int)max_fd + 1, &read_fds, nullptr, nullptr, &timeout);
        if (activity == SOCKET_ERROR) {
            cerr << "[" << thread_id << "] select() error: " << WSAGetLastError() << endl;
            continue;
        }

        // Обрабатываем новые подключения
        if (FD_ISSET(proxy_socket, &read_fds)) {
            SOCKET client_socket = accept(proxy_socket, nullptr, nullptr);
            if (client_socket != INVALID_SOCKET) {
                cout << "[" << thread_id << "] New client connected" << endl;
                conn_manager.add(make_unique<Connection>(client_socket));
            }
        }

        // Обрабатываем активные сокеты
        for (SOCKET s : sockets) {
            if (FD_ISSET(s, &read_fds)) {
                Connection* conn = conn_manager.get_connection(s);
                if (!conn) continue;

                // Это клиентский сокет
                if (conn->socket == s) {
                    char buffer[BUFFER_SIZE];
                    int bytes_received = recv(s, buffer, BUFFER_SIZE - 1, 0);
                    if (bytes_received <= 0) {
                        conn_manager.remove(s);
                        continue;
                    }

                    buffer[bytes_received] = '\0';
                    conn->buffer.append(buffer, bytes_received);

                    // Если получили полные заголовки
                    if (conn->buffer.find("\r\n\r\n") != string::npos && conn->peer_socket == INVALID_SOCKET) {
                        if (!HttpUtils::parse_url(conn->buffer, conn->host, conn->path, conn->port)) {
                            HttpUtils::send_error_response(s, 400, "Bad Request: Invalid URL or Host header");
                            conn_manager.remove(s);
                            continue;
                        }

                        string cache_key = conn->host + ":" + to_string(conn->port) + conn->path;
                        auto cached = cache.get(cache_key);
                        if (cached.first) {
                            cout << "[" << thread_id << "] Serving from cache: " << cache_key << endl;
                            send(s, cached.second.data.c_str(), (int)cached.second.data.size(), 0);
                            conn_manager.remove(s);
                            continue;
                        }

                        // Подключаемся к целевому серверу
                        conn->peer_socket = HttpUtils::connect_to_server(conn->host, conn->port);
                        if (conn->peer_socket == INVALID_SOCKET) {
                            HttpUtils::send_error_response(s, 502, "Bad Gateway: Failed to connect to target server");
                            conn_manager.remove(s);
                            continue;
                        }

                        // Отправляем запрос на сервер
                        string modified_request = HttpUtils::remove_proxy_headers(conn->buffer);
                        if (send(conn->peer_socket, modified_request.c_str(), (int)modified_request.size(), 0) == SOCKET_ERROR) {
                            HttpUtils::send_error_response(s, 502, "Bad Gateway: Failed to send request to target server");
                            conn_manager.remove(s);
                            continue;
                        }

                        cout << "[" << thread_id << "] Forwarding request to: " << conn->host << conn->path << endl;
                    }
                }
                // Это серверный сокет
                else if (conn->peer_socket == s) {
                    char buffer[BUFFER_SIZE];
                    string response_data;
                    int bytes_received = recv(s, buffer, BUFFER_SIZE - 1, 0);
                    if (bytes_received <= 0) {
                        conn_manager.remove(conn->socket);
                        continue;
                    }

                    buffer[bytes_received] = '\0';
                    response_data.append(buffer, bytes_received);

                    // Отправляем ответ клиенту
                    send(conn->socket, response_data.c_str(), (int)response_data.size(), 0);

                    // Кэшируем успешные ответы
                    if (response_data.find("HTTP/1.1 200") != string::npos) {
                        string cache_key = conn->host + ":" + to_string(conn->port) + conn->path;
                        string etag = HttpUtils::get_header_value(response_data, "ETag");
                        string last_modified = HttpUtils::get_header_value(response_data, "Last-Modified");
                        cache.put(cache_key, response_data, etag, last_modified);
                    }

                    conn_manager.remove(conn->socket);
                }
            }
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        cerr << "Usage: proxy_server <thread_pool_size>" << endl;
        return 1;
    }

    int thread_pool_size = atoi(argv[1]);
    if (thread_pool_size <= 0) {
        cerr << "Thread pool size must be positive" << endl;
        return 1;
    }

    // Инициализация Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "Failed to initialize Winsock" << endl;
        return 1;
    }

    // Создание сокета прокси
    SOCKET proxy_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (proxy_socket == INVALID_SOCKET) {
        cerr << "Failed to create proxy socket" << endl;
        WSACleanup();
        return 1;
    }

    // Настройка сокета
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

    cout << "Proxy server running on port " << PROXY_PORT << " with " << thread_pool_size << " worker threads" << endl;

    // Инициализация менеджеров
    CacheManager cache;
    ConnectionManager conn_manager;
    atomic<bool> stop_flag(false);

    // Создание рабочих потоков
    vector<thread> threads;
    for (int i = 0; i < thread_pool_size; ++i) {
        threads.emplace_back(worker_thread, proxy_socket, ref(cache), ref(conn_manager), ref(stop_flag), i);
    }

    // Ожидание завершения
    cout << "Press Enter to stop the server..." << endl;
    cin.get();

    // Остановка сервера
    stop_flag.store(true);
    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }

    closesocket(proxy_socket);
    WSACleanup();
    return 0;
}