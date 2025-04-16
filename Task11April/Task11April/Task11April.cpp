#include <stdio.h>
#include <windows.h>
#include <locale.h>

#define NUM_LINES 10

HANDLE hMutex;
HANDLE hConditionEvent;
int turn = 0;

DWORD WINAPI child_thread(LPVOID arg) {
    for (int i = 0; i < NUM_LINES; i++) {
        WaitForSingleObject(hMutex, INFINITE);

        while (turn != 1) {
            ReleaseMutex(hMutex);
            WaitForSingleObject(hConditionEvent, INFINITE);
            WaitForSingleObject(hMutex, INFINITE);
        }

        printf("Дочерний поток: строка %d\n", i + 1);

        turn = 0;
        SetEvent(hConditionEvent);
        ReleaseMutex(hMutex);
    }
    return 0;
}

int main() {
    // Установка локали для русского языка
    setlocale(LC_ALL, "ru");

    HANDLE hThread;
    hMutex = CreateMutex(NULL, FALSE, NULL);
    hConditionEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

    hThread = CreateThread(NULL, 0, child_thread, NULL, 0, NULL);
    if (hThread == NULL) {
        fprintf(stderr, "Ошибка создания потока\n");
        return 1;
    }

    for (int i = 0; i < NUM_LINES; i++) {
        WaitForSingleObject(hMutex, INFINITE);

        while (turn != 0) {
            ReleaseMutex(hMutex);
            WaitForSingleObject(hConditionEvent, INFINITE);
            WaitForSingleObject(hMutex, INFINITE);
        }

        printf("Родительский поток: строка %d\n", i + 1);

        turn = 1;
        SetEvent(hConditionEvent);
        ReleaseMutex(hMutex);
    }

    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hMutex);
    CloseHandle(hConditionEvent);
    CloseHandle(hThread);

    return 0;
}