#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <windows.h> // Для Sleep()

// Функция, которая будет выполняться в каждом потоке
void printString(const std::string& str) {
    // Задержка пропорциональна длине строки
    Sleep(str.length() * 100); // Коэффициент 100 мс на символ
    // Вывод строки
    std::cout << str << std::endl;
}

int main() {
    std::vector<std::string> strings;
    std::string input;

    // Чтение строк со стандартного ввода
    while (std::getline(std::cin, input)) {
        strings.push_back(input);
    }

    // Создание потоков для каждой строки
    std::vector<std::thread> threads;
    for (const auto& str : strings) {
        threads.emplace_back(printString, str);
    }

    // Ожидание завершения всех потоков
    for (auto& thread : threads) {
        thread.join();
    }

    return 0;
}