#include <iostream>
#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <cstring>
#include <algorithm>
#include <vector>
#include <random>

// Структура очереди
struct Queue {
    std::queue<std::string> messages;
    std::mutex mtx;
    std::condition_variable cv_put;
    std::condition_variable cv_get;
    int max_size = 10;
    bool is_dropped = false;
};

// Инициализация очереди
void mymsginit(Queue* queue) {
    queue->max_size = 10;
    queue->is_dropped = false;
    std::cout << "Queue initialized." << std::endl;
}

// Добавление сообщения в очередь
int mymsgput(Queue* queue, char* msg) {
    const size_t MAX_MSG_LEN = 80;

    std::unique_lock<std::mutex> lock(queue->mtx);

    // Ждём, пока будет место или очередь не закроют
    queue->cv_put.wait(lock, [queue] {
        return queue->messages.size() < queue->max_size || queue->is_dropped;
        });

    if (queue->is_dropped) {
        std::cerr << "mymsgput: Queue is dropped. Cannot put message." << std::endl;
        return 0;
    }

    // Обрезаем строку до 80 символов
    size_t len = strlen(msg);
    size_t copy_len = std::min(len, MAX_MSG_LEN);

    std::string buffer(msg, copy_len); // Копируем только нужное количество символов

    queue->messages.push(buffer);
    std::cout << "Produced (" << copy_len << " chars): " << buffer << std::endl;

    queue->cv_get.notify_one();

    return static_cast<int>(copy_len); // Возвращаем количество переданных символов
}

// Извлечение сообщения из очереди
int mymsgget(Queue* queue, char* buf, size_t bufsize) {
    std::unique_lock<std::mutex> lock(queue->mtx);

    // Ждём, пока появится сообщение
    queue->cv_get.wait(lock, [queue] { return !queue->messages.empty(); });

    std::string message = queue->messages.front();
    queue->messages.pop();

    // Обрезаем под размер буфера
    size_t copy_len = std::min(message.length(), bufsize - 1);

    // Копируем в буфер
    memcpy(buf, message.c_str(), copy_len);
    buf[copy_len] = '\0'; // Завершаем строку

    std::cout << "Consumed (" << copy_len << " chars): " << buf << std::endl;

    queue->cv_put.notify_one();

    return static_cast<int>(copy_len); // Возвращаем количество скопированных символов
}

// Закрытие очереди для новых сообщений
void mymsgdrop(Queue* queue) {
    std::lock_guard<std::mutex> lock(queue->mtx);
    queue->is_dropped = true;

    // Разблокируем ожидающие потоки
    queue->cv_put.notify_all();
    queue->cv_get.notify_all();

    std::cout << "Queue dropped. No new messages allowed." << std::endl;
}

// Уничтожение очереди
void mymsgdestroy(Queue* queue) {
    std::lock_guard<std::mutex> lock(queue->mtx);
    while (!queue->messages.empty()) {
        queue->messages.pop();
    }
    queue->is_dropped = true;

    std::cout << "Queue destroyed." << std::endl;
}



std::string generate_random_message() {
    static std::mt19937 gen(std::random_device{}());
    static std::uniform_int_distribution<> char_dist(32, 126); 
    static std::uniform_int_distribution<> len_dist(1, 128);

    size_t length = len_dist(gen);
    std::string message;

    for (size_t i = 0; i < length; ++i) {
        message += static_cast<char>(char_dist(gen));
    }

    return message;
}

// Производитель
void producer(Queue* queue, int id) {
    int message_count = 0;
    const int total_messages = 5;

    while (message_count < total_messages) {
        std::string msg = generate_random_message();
        int sent = mymsgput(queue, const_cast<char*>(msg.c_str()));

        if (sent > 0) {
            std::cout << "Producer " << id << " sent " << sent << " chars" << std::endl;
            ++message_count;
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        else {
            std::cerr << "Producer " << id << " stopped: cannot send more messages." << std::endl;
            break;
        }
    }

    std::cout << "Producer " << id << " finished sending all messages." << std::endl;
}

// Потребитель
void consumer(Queue* queue, int id) {
    char buffer[128];

    while (true) {
        int received = mymsgget(queue, buffer, sizeof(buffer));

        if (received > 0) {
            std::cout << "Consumer " << id << " received " << received << " chars: " << buffer << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }
        else {
            std::cout << "Consumer " << id << " exiting: no more messages." << std::endl;
            break;
        }
    }
}

// Точка входа
int main() {
    Queue queue;
    mymsginit(&queue);

    std::thread p1(producer, &queue, 1);
    std::thread p2(producer, &queue, 2);
    std::thread c1(consumer, &queue, 1);
    std::thread c2(consumer, &queue, 2);

    p1.join();
    p2.join();
    c1.join();
    c2.join();

    mymsgdrop(&queue);     // Закрываем очередь
    mymsgdestroy(&queue);  // Освобождаем ресурсы

    return 0;
}