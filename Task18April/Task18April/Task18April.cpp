#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <algorithm>
#include <chrono>
#include <memory>

using namespace std;

struct ListItem {
    string data;
    mutex mtx;

    ListItem(const string& str) : data(str) {}
};

class SharedList {
private:
    vector<shared_ptr<ListItem>> list;
    mutex list_mtx; // Мьютекс для операций с самим вектором

public:
    void addToFront(const string& item) {
        lock_guard<mutex> lock(list_mtx);

        // Если строка длиннее 90 символов, разрезаем на части
        if (item.length() > 90) {
            for (size_t i = item.length(); i > 0; ) {
                i -= min(i, (size_t)90);
                string part = item.substr(i, min((size_t)90, item.length() - i));
                list.insert(list.begin(), make_shared<ListItem>(part));
            }
        }
        else {
            list.insert(list.begin(), make_shared<ListItem>(item));
        }
    }

    void bubbleSort() {
        lock_guard<mutex> list_lock(list_mtx); // Блокируем список на время сортировки

        if (list.empty()) return;

        for (size_t i = 0; i < list.size(); ++i) {
            for (size_t j = 0; j < list.size() - i - 1; ++j) {
                // Блокируем три элемента: j, j+1 и j-1 (если существует)
                unique_lock<mutex> lock1(list[j]->mtx, defer_lock);
                unique_lock<mutex> lock2(list[j + 1]->mtx, defer_lock);

                // Всегда блокируем в порядке от начала к концу списка
                if (j > 0) {
                    unique_lock<mutex> lock0(list[j - 1]->mtx, defer_lock);
                    lock(lock0, lock1, lock2);
                }
                else {
                    lock(lock1, lock2);
                }

                if (list[j]->data > list[j + 1]->data) {
                    swap(list[j], list[j + 1]);
                }
            }
        }
    }

    void printList() {
        lock_guard<mutex> list_lock(list_mtx);
        cout << "Текущее состояние списка:" << endl;

        for (size_t i = 0; i < list.size(); ++i) {
            lock_guard<mutex> item_lock(list[i]->mtx);
            cout << list[i]->data << endl;
        }
        cout << "-------------------------" << endl;
    }

    size_t size() {
        lock_guard<mutex> lock(list_mtx);
        return list.size();
    }
};

void sortingThread(SharedList& sharedList, int interval, int id, bool& running) {
    while (running) {
        this_thread::sleep_for(chrono::seconds(interval));
        sharedList.bubbleSort();
    }
}

int main() {
    std::setlocale(LC_ALL, "ru");
    SharedList sharedList;
    bool running = true;

    // Запускаем несколько потоков сортировки с разными интервалами
    thread sorter1(sortingThread, ref(sharedList), 5, 1, ref(running));
    thread sorter2(sortingThread, ref(sharedList), 7, 2, ref(running));

    cout << "Вводите строки. Пустая строка - вывод списка. 'exit' - завершение программы." << endl;

    string input;
    while (true) {
        getline(cin, input);

        if (input == "exit") {
            running = false;
            break;
        }
        else if (input.empty()) {
            sharedList.printList();
        }
        else {
            sharedList.addToFront(input);
        }
    }

    sorter1.join();
    sorter2.join();
    return 0;
}