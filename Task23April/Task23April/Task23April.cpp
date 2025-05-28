#include <iostream>   
#include <fstream>    
#include <string>     
#include <vector>     
#include <thread>
#include <mutex>
#include <list>
#include <chrono>

using namespace std;
using namespace std::chrono_literals;

// Класс потокобезопасного списка
class ThreadSafeList {
    list<string> data_list;
    mutex mtx;

public:
    void add_string(const string& str) {
        lock_guard<mutex> lock(mtx);
        data_list.push_back(str);
    }

    void print() {
        lock_guard<mutex> lock(mtx);
        for (const auto& str : data_list) {
            cout << str << endl;
        }
    }
};

// Функция, которую запускает каждый поток
void sort_thread(string str, ThreadSafeList& list) {
    this_thread::sleep_for(1s * static_cast<int>(str.size()));
    list.add_string(str);
}

int main() {
    setlocale(LC_ALL, "ru");

    ThreadSafeList safe_list;
    vector<thread> threads;

    cout << "Введите строки (пустая строка - выход):" << endl;
    string input;

    while (true) {
        // Используем std::getline явно
        if (!getline(cin, input)) break;

        if (input.empty()) break;

        // Запускаем поток
        threads.emplace_back(sort_thread, input, ref(safe_list));
    }

    // Ожидаем завершения всех потоков
    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }

    // Выводим результат
    cout << "\nОтсортированный список:" << endl;
    safe_list.print();

    return 0;
}