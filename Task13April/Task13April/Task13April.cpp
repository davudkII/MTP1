#include <windows.h>
#include <iostream>
#include <clocale>

const int NUM_LINES = 10;

// Глобальные переменные синхронизации
HANDLE hMutex;
HANDLE hConditionEvent;
bool parent_turn = true; // Флаг очереди вывода

DWORD WINAPI child_thread(LPVOID arg) {
    for (int i = 0; i < NUM_LINES; i++) {
        WaitForSingleObject(hMutex, INFINITE);

        // Ждем, пока не наступит очередь дочернего потока
        while (parent_turn) {
            ReleaseMutex(hMutex);
            WaitForSingleObject(hConditionEvent, INFINITE);
            WaitForSingleObject(hMutex, INFINITE);
        }

        std::cout << "Дочерний поток: строка " << i + 1 << std::endl;

        // Передаем очередь родительскому потоку
        parent_turn = true;
        SetEvent(hConditionEvent);
        ReleaseMutex(hMutex);
    }
    return 0;
}

int main() {
    setlocale(LC_ALL, "Russian");

    // Инициализация объектов синхронизации
    hMutex = CreateMutex(nullptr, FALSE, nullptr);
    hConditionEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    HANDLE hThread = CreateThread(nullptr, 0, child_thread, nullptr, 0, nullptr);
    if (hThread == nullptr) {
        std::cerr << "Ошибка создания потока" << std::endl;
        return 1;
    }

    // Родительский поток
    for (int i = 0; i < NUM_LINES; i++) {
        WaitForSingleObject(hMutex, INFINITE);

        // Ждем, пока не наступит очередь родительского потока
        while (!parent_turn) {
            ReleaseMutex(hMutex);
            WaitForSingleObject(hConditionEvent, INFINITE);
            WaitForSingleObject(hMutex, INFINITE);
        }

        std::cout << "Родительский поток: строка " << i + 1 << std::endl;

        // Передаем очередь дочернему потоку
        parent_turn = false;
        SetEvent(hConditionEvent);
        ReleaseMutex(hMutex);
    }

    // Ожидаем завершения дочернего потока
    WaitForSingleObject(hThread, INFINITE);

    // Освобождаем ресурсы
    CloseHandle(hMutex);
    CloseHandle(hConditionEvent);
    CloseHandle(hThread);

    return 0;
}