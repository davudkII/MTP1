// parent_win.cpp
#include <windows.h>
#include <iostream>
#include <clocale>

const int NUM_LINES = 10;
const wchar_t* PARENT_SEM_NAME = L"ParentSemaphore";
const wchar_t* CHILD_SEM_NAME = L"ChildSemaphore";

int main() {
    std::setlocale(LC_ALL, "Russian");

    // Создаем именованные семафоры
    HANDLE hParentSem = CreateSemaphoreW(nullptr, 1, 1, PARENT_SEM_NAME);
    HANDLE hChildSem = CreateSemaphoreW(nullptr, 0, 1, CHILD_SEM_NAME);

    if (hParentSem == nullptr || hChildSem == nullptr) {
        std::cerr << "Ошибка создания семафоров" << std::endl;
        return 1;
    }

    for (int i = 0; i < NUM_LINES; i++) {
        WaitForSingleObject(hParentSem, INFINITE);

        std::cout << "Родительский процесс: строка " << i + 1 << std::endl;

        ReleaseSemaphore(hChildSem, 1, nullptr);
    }

    CloseHandle(hParentSem);
    CloseHandle(hChildSem);

    return 0;
}