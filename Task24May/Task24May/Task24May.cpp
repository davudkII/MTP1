#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <chrono>

using namespace std;
using namespace std::chrono;

// Счетный семафор (реализация через mutex и condition_variable)
class Semaphore {
private:
    std::mutex mtx;
    std::condition_variable cv;
    int count;

public:
    Semaphore(int initial = 0) : count(initial) {}

    void notify() {
        std::unique_lock<std::mutex> lock(mtx);
        ++count;
        cv.notify_one();
    }

    void wait() {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this]() { return count > 0; });
        --count;
    }
};

// Семафоры для готовых деталей и модулей
Semaphore semA(0), semB(0), semC(0);
Semaphore semModule(0); // Готовые модули (A+B)

void produceA() {
    while (true) {
        this_thread::sleep_for(1s);
        cout << "Деталь A произведена\n";
        semA.notify();
    }
}

void produceB() {
    while (true) {
        this_thread::sleep_for(2s);
        cout << "Деталь B произведена\n";
        semB.notify();
    }
}

void produceC() {
    while (true) {
        this_thread::sleep_for(3s);
        cout << "Деталь C произведена\n";
        semC.notify();
    }
}

void assembleModule() {
    while (true) {
        semA.wait(); // Ждём A
        semB.wait(); // Ждём B
        this_thread::sleep_for(500ms); // Имитация времени сборки
        cout << "Модуль (A+B) собран\n";
        semModule.notify();
    }
}

void assembleWidget() {
    while (true) {
        semModule.wait(); // Ждём модуль
        semC.wait();      // Ждём C
        this_thread::sleep_for(500ms); // Имитация времени сборки
        cout << "Винтик (модуль+C) собран\n";
    }
}

int main() {
    std::setlocale(LC_ALL, "ru");
    thread tA(produceA);
    thread tB(produceB);
    thread tC(produceC);
    thread tMod(assembleModule);
    thread tWid(assembleWidget);

 
    tA.join();
    tB.join();
    tC.join();
    tMod.join();
    tWid.join();

    return 0;
}