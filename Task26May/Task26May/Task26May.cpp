#include <iostream>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <cstring>

// Структура очереди
struct Queue {
    std::queue<std::string> messages;
    std::mutex mtx;
    std::condition_variable cv_put; // Для производителей
    std::condition_variable cv_get; // Для потребителей
    int max_size = 10;             // Максимальный размер очереди
};

// Инициализация очереди
void mymsginit(Queue* queue) {
    queue->max_size = 10;
    std::cout << "Queue initialized." << std::endl;
}

// Добавление сообщения в очередь
int mymsgput(Queue* queue, const char* msg) {
    std::unique_lock<std::mutex> lock(queue->mtx);

    // Ждём, пока очередь не освободится
    queue->cv_put.wait(lock, [queue] { return queue->messages.size() < queue->max_size; });

    // Добавляем сообщение
    queue->messages.push(std::string(msg));
    std::cout << "Produced: " << msg << std::endl;

    // Уведомляем потребителей
    queue->cv_get.notify_one();

    return 1; // Успех
}

// Извлечение сообщения из очереди
int mymsgget(Queue* queue, char* buf, size_t bufsize) {
    std::unique_lock<std::mutex> lock(queue->mtx);

    // Ждём, пока появится сообщение
    queue->cv_get.wait(lock, [queue] { return !queue->messages.empty(); });

    std::string message = queue->messages.front();
    queue->messages.pop();

    // Копируем сообщение в буфер
    size_t copy_len = (message.length() < bufsize - 1) ? message.length() : bufsize - 1;
    strncpy(buf, message.c_str(), copy_len);
    buf[copy_len] = '\0';

    std::cout << "Consumed: " << buf << std::endl;

    // Уведомляем производителей
    queue->cv_put.notify_one();

    return static_cast<int>(copy_len);
}

// Уничтожение очереди
void mymsgdestroy(Queue* queue) {
    while (!queue->messages.empty()) {
        queue->messages.pop();
    }
    std::cout << "Queue destroyed." << std::endl;
}

// Производитель
void producer(Queue* queue, int id) {
    for (int i = 0; i < 5; ++i) {
        char msg[80];
        snprintf(msg, sizeof(msg), "Producer %d: Message #%d", id, i + 1);
        mymsgput(queue, msg);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

// Потребитель
void consumer(Queue* queue, int id) {
    char buffer[128];
    for (int i = 0; i < 10; ++i) {
        int received = mymsgget(queue, buffer, sizeof(buffer));
        if (received > 0) {
            std::cout << "Consumer " << id << " received " << received << " chars: " << buffer << std::endl;
        }
        else {
            std::cout << "Consumer " << id << " exiting: no more messages." << std::endl;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
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

    mymsgdestroy(&queue);

    return 0;
}