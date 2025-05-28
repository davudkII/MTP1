#include <iostream>
#include <list>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <memory>
#include <chrono>

#include <io.h>
#include <fcntl.h>
#include <windows.h> 

using namespace std;
using namespace std::chrono_literals;

// Узел списка с защитой мьютексом
struct ListNode {
    string data;
    mutex mtx;

    explicit ListNode(string str) : data(move(str)) {}

    // Запрещаем копирование
    ListNode(const ListNode&) = delete;
    ListNode& operator=(const ListNode&) = delete;

    // Разрешаем перемещение
    ListNode(ListNode&&) noexcept = default;
    ListNode& operator=(ListNode&&) noexcept = default;
};

class ThreadSafeList {
    list<unique_ptr<ListNode>> data_list;
    mutex global_mtx; // Защищает структуру списка
    atomic<bool> running{ true };

public:
    // Добавление строки в список
    void add_string(const string& str) {
        lock_guard<mutex> lock(global_mtx);
        if (str.size() > 80) {
            size_t pos = 0;
            while (pos < str.size()) {
                string part = str.substr(pos, 80);
                pos += part.size();
                data_list.push_front(make_unique<ListNode>(part));
            }
        }
        else {
            data_list.push_front(make_unique<ListNode>(str));
        }
    }

    // Вывод текущего состояния списка
    void print() {
        lock_guard<mutex> lock(global_mtx);

        cout << "\nТекущее состояние списка:" << endl;
        int index = 1;
        for (auto& node : data_list) {
            lock_guard<mutex> node_lock(node->mtx);
            cout << index++ << ". " << node->data << endl;
        }
    }

    // Шаг пузырьковой сортировки
    bool bubble_sort_step() {
        lock_guard<mutex> lock(global_mtx);

        bool swapped = false;
        auto it = data_list.begin();

        while (it != data_list.end()) {
            auto next_it = next(it);
            if (next_it == data_list.end())
                break;

            // Избегаем дедлоки: захватываем мьютексы в порядке адресов
            if (&(*it)->mtx < &(*next_it)->mtx) {
                lock_guard<mutex> lock1((*it)->mtx);
                lock_guard<mutex> lock2((*next_it)->mtx);
                if ((*it)->data > (*next_it)->data) {
                    swap((*it)->data, (*next_it)->data);
                    swapped = true;
                }
            }
            else {
                lock_guard<mutex> lock2((*next_it)->mtx);
                lock_guard<mutex> lock1((*it)->mtx);
                if ((*it)->data > (*next_it)->data) {
                    swap((*it)->data, (*next_it)->data);
                    swapped = true;
                }
            }

            ++it;
        }

        return swapped;
    }

    // Основной метод сортировки
    void sort() {
        while (running) {
            {
                lock_guard<mutex> lock(global_mtx);
                if (data_list.size() < 2) {
                    this_thread::sleep_for(100ms);
                    continue;
                }
            }

            bool swapped;
            do {
                swapped = bubble_sort_step();
            } while (swapped && running);

            this_thread::sleep_for(500ms); // Лёгкая пауза между полными проходами
        }
    }

    // Остановка потока сортировки
    void stop() {
        running = false;
    }
};

int main() {
    // Переключаем консоль на UTF-8
    system("chcp 65001 > nul"); // Смена кодовой страницы на UTF-8
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // Устанавливаем глобальную локаль UTF-8 (может потребоваться установка языковых пакетов)
    setlocale(LC_ALL, "ru"); 

    ThreadSafeList safe_list;
    thread sort_thread(&ThreadSafeList::sort, ref(safe_list));

    cout << "Вводите строки (пустая строка - вывод списка, 'exit' - выход):" << endl;

    string input;
    while (true) {
        getline(cin, input);

        if (input.empty()) {
            safe_list.print(); // Вывести текущее состояние списка
            continue;
        }

        if (input == "exit") {
            safe_list.stop(); // Остановить фоновый поток
            break;
        }

        safe_list.add_string(input); // Добавить строку в список
    }

    if (sort_thread.joinable()) {
        sort_thread.join(); // Дождаться завершения потока
    }

    return 0;
}