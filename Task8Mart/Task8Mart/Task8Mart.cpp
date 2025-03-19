#include <iostream>
#include <vector>
#include <thread>
#include <mutex>

const int NUM_STEPS = 200000000; // Количество итераций
std::mutex mtx; // Мьютекс для синхронизации доступа к общей переменной

double pi = 0.0; // Глобальная переменная для хранения результата

// Функция для вычисления частичной суммы ряда Лейбница
void computePiPartialSum(int start, int end) {
    double partialSum = 0.0;
    for (int i = start; i < end; i++) {
        partialSum += 1.0 / (i * 4.0 + 1.0);
        partialSum -= 1.0 / (i * 4.0 + 3.0);
    }

    // Синхронизация доступа к глобальной переменной pi
    std::lock_guard<std::mutex> lock(mtx);
    pi += partialSum;
}

int main(int argc, char** argv) {
    std::setlocale(LC_ALL, "ru");
    if (argc != 2) {
        std::cerr << "Использование: " << argv[0] << " <количество потоков>" << std::endl;
        return EXIT_FAILURE;
    }

    int numThreads = std::atoi(argv[1]);
    if (numThreads <= 0) {
        std::cerr << "Количество потоков должно быть положительным числом." << std::endl;
        return EXIT_FAILURE;
    }

    std::vector<std::thread> threads;
    int stepsPerThread = NUM_STEPS / numThreads;

    // Создание потоков
    for (int i = 0; i < numThreads; i++) {
        int start = i * stepsPerThread;
        int end = (i == numThreads - 1) ? NUM_STEPS : (i + 1) * stepsPerThread;
        threads.emplace_back(computePiPartialSum, start, end);
    }

    // Ожидание завершения всех потоков
    for (auto& thread : threads) {
        thread.join();
    }

    // Умножение на 4 для получения значения Пи
    pi *= 4.0;
    std::cout.precision(15);
    std::cout << "pi done - " << pi << std::endl;

    return EXIT_SUCCESS;
}