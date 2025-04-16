#include <windows.h>
#include <iostream>
#include <clocale>

const int NUM_LINES = 10;

// Семафоры для синхронизации
HANDLE hParentSem;
HANDLE hChildSem;

DWORD WINAPI child_thread(LPVOID arg) {
    for (int i = 0; i < NUM_LINES; i++) {
        WaitForSingleObject(hChildSem, INFINITE); // Ожидаем разрешения

        std::cout << "Дочерний поток: строка " << i + 1 << std::endl;

        ReleaseSemaphore(hParentSem, 1, nullptr); // Разрешаем родительскому потоку
    }
    return 0;
}

int main() {
    setlocale(LC_ALL, "Russian");

    // Создаем семафоры
    hParentSem = CreateSemaphore(nullptr, 1, 1, nullptr);  // Родительский начинает первым
    hChildSem = CreateSemaphore(nullptr, 0, 1, nullptr);   // Дочерний ждет разрешения

    HANDLE hThread = CreateThread(nullptr, 0, child_thread, nullptr, 0, nullptr);
    if (hThread == nullptr) {
        std::cerr << "Ошибка создания потока" << std::endl;
        return 1;
    }

    // Родительский поток
    for (int i = 0; i < NUM_LINES; i++) {
        WaitForSingleObject(hParentSem, INFINITE); // Ожидаем разрешения

        std::cout << "Родительский поток: строка " << i + 1 << std::endl;

        ReleaseSemaphore(hChildSem, 1, nullptr); // Разрешаем дочернему потоку
    }

    // Ожидаем завершения дочернего потока
    WaitForSingleObject(hThread, INFINITE);

    // Освобождаем ресурсы
    CloseHandle(hParentSem);
    CloseHandle(hChildSem);
    CloseHandle(hThread);

    return 0;
}