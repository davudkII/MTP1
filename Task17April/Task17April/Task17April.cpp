#include <iostream>
#include <list>
#include <string>
#include <thread>
#include <mutex>
#include <chrono>
#include <algorithm>

using namespace std;

// Глобальные переменные
list<string> shared_list;
mutex list_mutex;
bool program_running = true;

// Функция для разделения длинных строк
void split_and_add(const string& input) {
    size_t pos = 0;
    while (pos < input.size()) {
        string part = input.substr(pos, 60);
        pos += part.size();

        lock_guard<mutex> lock(list_mutex);
        shared_list.push_front(part);
    }
}

// Функция дочернего потока (сортировка каждые 5 секунд)
void sort_thread_func() {
    while (program_running) {
        this_thread::sleep_for(chrono::seconds(5));

        lock_guard<mutex> lock(list_mutex);
        if (!shared_list.empty()) {
            // Пузырьковая сортировка
            bool swapped;
            do {
                swapped = false;
                auto it = shared_list.begin();
                auto next_it = it;
                ++next_it;

                while (next_it != shared_list.end()) {
                    if (*it > *next_it) {
                        iter_swap(it, next_it);
                        swapped = true;
                    }
                    ++it;
                    ++next_it;
                }
            } while (swapped);

            
        }
    }
}

int main() {
    setlocale(LC_ALL, "ru");

    // Запускаем дочерний поток
    thread sort_thread(sort_thread_func);

    cout << "Вводите строки (пустая строка - вывод списка, Ctrl+C - выход):" << endl;

    string input;
    while (true) {
        getline(cin, input);

        if (input.empty()) {
            // Вывод текущего состояния списка
            lock_guard<mutex> lock(list_mutex);
            cout << "Текущее состояние списка:" << endl;
            for (const auto& str : shared_list) {
                cout << str << endl;
            }
            continue;
        }

        // Добавление строки в список
        if (input.size() > 60) {
            split_and_add(input);
        }
        else {
            lock_guard<mutex> lock(list_mutex);
            shared_list.push_front(input);
        }
    }

    // Завершение работы 
    program_running = false;
    sort_thread.join();
    return 0;
}
