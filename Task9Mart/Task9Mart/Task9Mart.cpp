#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <windows.h>

std::atomic<bool> stopFlag(false); // Атомарный флаг для остановки потоков

// Функция для вычисления частичной суммы ряда Лейбница
void computePiPartialSum(int threadId, int numThreads, double& partialSum) {
    partialSum = 0.0;
    int i = threadId; // Каждый поток начинает с индекса, равного его ID
    while (!stopFlag.load()) { // Бесконечный цикл, пока не установлен флаг stopFlag
        partialSum += 1.0 / (i * 4.0 + 1.0);
        partialSum -= 1.0 / (i * 4.0 + 3.0);
        i += numThreads; // Переход к следующему элементу для этого потока
    }
    std::cout << "Поток " << threadId << " завершил работу после " << i / numThreads << " итераций." << std::endl;
}

// Обработчик сигнала Ctrl+C
BOOL WINAPI consoleHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT) {
        stopFlag.store(true); // Устанавливаем флаг для остановки потоков
        std::cout << "\nПолучен сигнал Ctrl+C. Завершение работы..." << std::endl;
    }
    return TRUE;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Использование: " << argv[0] << " <количество потоков>" << std::endl;
        return EXIT_FAILURE;
    }

    int numThreads = std::atoi(argv[1]);
    if (numThreads <= 0) {
        std::cerr << "Количество потоков должно быть положительным числом." << std::endl;
        return EXIT_FAILURE;
    }

    // Установка обработчика сигнала Ctrl+C
    if (!SetConsoleCtrlHandler(consoleHandler, TRUE)) {
        std::cerr << "Ошибка установки обработчика сигнала." << std::endl;
        return EXIT_FAILURE;
    }

    std::vector<std::thread> threads;
    std::vector<double> partialSums(numThreads, 0.0);

    // Создание потоков
    for (int i = 0; i < numThreads; i++) {
        threads.emplace_back(computePiPartialSum, i, numThreads, std::ref(partialSums[i]));
    }

    // Ожидание завершения всех потоков
    for (auto& thread : threads) {
        thread.join();
    }

    // Сбор частичных сумм и вычисление Пи
    double pi = 0.0;
    for (double sum : partialSums) {
        pi += sum;
    }
    pi *= 4.0; // Умножаем на 4 только один раз в конце

    std::cout.precision(15);
    std::cout << "Приближенное значение Пи: " << pi << std::endl;

    return EXIT_SUCCESS;
}