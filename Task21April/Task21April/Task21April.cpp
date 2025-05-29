#include <iostream>
#include <list>
#include <string>
#include <thread>
#include <shared_mutex>
#include <atomic>
#include <memory>

using namespace std;

// Узел списка с защитой блокировкой чтения-записи
struct ListNode {
    string data;
    shared_mutex mtx;

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
    shared_mutex global_mtx; // Защита всей структуры списка
    atomic<bool> running{ true };

public:
    // Добавление строки в начало списка
    void add_string(const string& str) {
        unique_lock<shared_mutex> lock(global_mtx); 

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
        shared_lock<shared_mutex> lock(global_mtx); // Разделяемая блокировка для чтения

        cout << "\nТекущее состояние списка:" << endl;
        int index = 1;
        for (auto& node : data_list) {
            shared_lock<shared_mutex> node_lock(node->mtx); // Чтение данных узла
            cout << index++ << ". " << node->data << endl;
        }
    }

    // Шаг пузырьковой сортировки
    bool bubble_sort_step() {
        unique_lock<shared_mutex> lock(global_mtx); 

        bool swapped = false;
        auto it = data_list.begin();

        while (it != data_list.end()) {
            auto next_it = next(it);
            if (next_it == data_list.end())
                break;

            shared_lock<shared_mutex> lock1((*it)->mtx);
            shared_lock<shared_mutex> lock2((*next_it)->mtx);

            if ((*it)->data > (*next_it)->data) {
                swap((*it)->data, (*next_it)->data);
                swapped = true;
            }

            ++it;
        }

        return swapped;
    }

    // Основной метод сортировки
    void sort() {
        while (running) {
            bool swapped;
            do {
                swapped = bubble_sort_step();
            } while (swapped && running); // Повторяем пока есть перестановки
        }
    }

    // Остановка потока сортировки
    void stop() {
        running = false;
    }
};

int main() {
    setlocale(LC_ALL, "ru");

    ThreadSafeList safe_list;
    thread sort_thread(&ThreadSafeList::sort, ref(safe_list));

    cout << "Вводите строки (пустая строка - вывод списка, 'exit' - выход):" << endl;

    string input;
    while (true) {
        getline(cin, input);

        if (input.empty()) {
            safe_list.print(); // Вывод текущего состояния списка
            continue;
        }

        if (input == "exit") {
            safe_list.stop(); // Остановить сортировку
            break;
        }

        safe_list.add_string(input); // Добавление строки
    }

    if (sort_thread.joinable()) {
        sort_thread.join(); // Дождаться завершения потока
    }

    return 0;
}
