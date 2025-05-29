#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <chrono>

using namespace std;
using namespace std::chrono;

// Количество философов
const int PHILOSOPHERS = 5;

// Время, которое философ может есть за один раз (в секундах)
const int MAX_EAT_TIME_MS = 2000;

struct Philosopher {
    int id;
    mutex* left_fork;
    mutex* right_fork;
    condition_variable* cv;

    void dine() {
        while (true) {
            // Попытка взять обе вилки
            if (left_fork->try_lock()) {
                if (right_fork->try_lock()) {
                    auto start = high_resolution_clock::now();

                    cout << "Философ " << id << " начал есть." << endl;

                    // Ест не более MAX_EAT_TIME_MS миллисекунд
                    while ((high_resolution_clock::now() - start) < milliseconds(MAX_EAT_TIME_MS)) {
                        
                    }

                    cout << "Философ " << id << " закончил есть и освободил вилки." << endl;

                    // Освобождаем вилки
                    left_fork->unlock();
                    right_fork->unlock();

                    // Уведомляем других о возможном изменении состояния
                    {
                        unique_lock<mutex> lock(cv_mutex);
                        cv->notify_all();
                    }
                }
                else {
                    // Не смогли взять правую вилку — освобождаем левую
                    left_fork->unlock();
                    {
                        unique_lock<mutex> lock(cv_mutex);
                        cv->notify_all();
                    }
                }
            }
            else {
                // Не смогли взять левую вилку
                {
                    unique_lock<mutex> lock(cv_mutex);
                    cv->notify_all();
                }
            }


            this_thread::sleep_for(1ms);
        }
    }

private:
    mutex cv_mutex; // Для защиты условной переменной
};

int main() {
    setlocale(LC_ALL, "ru");

    vector<mutex> forks(PHILOSOPHERS);
    vector<condition_variable> cvs(PHILOSOPHERS);
    vector<Philosopher> philosophers(PHILOSOPHERS);

    // Инициализируем философов
    for (int i = 0; i < PHILOSOPHERS; ++i) {
        philosophers[i].id = i;
        philosophers[i].left_fork = &forks[i];
        philosophers[i].right_fork = &forks[(i + 1) % PHILOSOPHERS];
        philosophers[i].cv = &cvs[i];
    }

    // Создаем потоки
    vector<thread> threads;
    for (auto& p : philosophers) {
        threads.emplace_back(&Philosopher::dine, &p);
    }

   
    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }

    return 0;
}
