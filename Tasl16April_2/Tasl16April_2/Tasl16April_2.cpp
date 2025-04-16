// child_win.cpp
#include <windows.h>
#include <iostream>
#include <clocale>

const int NUM_LINES = 10;
const wchar_t* PARENT_SEM_NAME = L"ParentSemaphore";
const wchar_t* CHILD_SEM_NAME = L"ChildSemaphore";

int main() {
    std::setlocale(LC_ALL, "Russian");

    // Открываем существующие семафоры
    HANDLE hParentSem = OpenSemaphoreW(SEMAPHORE_ALL_ACCESS, FALSE, PARENT_SEM_NAME);
    HANDLE hChildSem = OpenSemaphoreW(SEMAPHORE_ALL_ACCESS, FALSE, CHILD_SEM_NAME);

    if (hParentSem == nullptr || hChildSem == nullptr) {
        std::cerr << "Ошибка открытия семафоров" << std::endl;
        return 1;
    }

    for (int i = 0; i < NUM_LINES; i++) {
        WaitForSingleObject(hChildSem, INFINITE);

        std::cout << "Дочерний процесс: строка " << i + 1 << std::endl;

        ReleaseSemaphore(hParentSem, 1, nullptr);
    }

    CloseHandle(hParentSem);
    CloseHandle(hChildSem);
    std::cout << "Нажмите Enter для выхода...";
    std::cin.ignore();
    return 0;
}