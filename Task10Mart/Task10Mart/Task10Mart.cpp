#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <chrono>
#include <random>

const int NUM_PHILOSOPHERS = 5;
std::mutex forks[NUM_PHILOSOPHERS];
std::mutex cout_mutex; // Для безопасного вывода

void philosopher(int id) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> think_dist(100, 500);
    std::uniform_int_distribution<> eat_dist(100, 300);

    while (true) {
        // Фаза размышления
        {
            std::lock_guard<std::mutex> lock(cout_mutex);
            std::cout << "Философ " << id << " размышляет\n";
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(think_dist(gen)));

        // Фаза еды (с предотвращением deadlock)
        int left = id;
        int right = (id + 1) % NUM_PHILOSOPHERS;

        // Чтобы избежать deadlock, один философ берет вилки в обратном порядке
        if (id == NUM_PHILOSOPHERS - 1) {
            std::swap(left, right);
        }

        std::unique_lock<std::mutex> first_fork(forks[left], std::defer_lock);
        std::unique_lock<std::mutex> second_fork(forks[right], std::defer_lock);

        std::lock(first_fork, second_fork); // Атомарно берем обе вилки

        {
            std::lock_guard<std::mutex> lock(cout_mutex);
            std::cout << "Философ " << id << " ест\n";
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(eat_dist(gen)));

        // Вилки автоматически освобождаются при выходе из области видимости
    }
}

int main() {
    std::setlocale(LC_ALL, "ru");
    std::vector<std::thread> philosophers;

    for (int i = 0; i < NUM_PHILOSOPHERS; ++i) {
        philosophers.emplace_back(philosopher, i);
    }

    for (auto& ph : philosophers) {
        ph.join();
    }

    return 0;
}