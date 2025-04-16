#include <stdio.h>
#include <windows.h>
#include <locale.h>

#define NUM_LINES 10
#define LANG_RU 0
#define LANG_EN 1

// Структура для локализованных строк
typedef struct {
    const char* parent_line;
    const char* child_line;
    const char* thread_error;
} LocalizedStrings;

LocalizedStrings strings[] = {
    // Русский
    {
        "Родительский поток: строка %d\n",
        "Дочерний поток: строка %d\n",
        "Ошибка создания потока\n"
    },
    // Английский
    {
        "Parent thread: line %d\n",
        "Child thread: line %d\n",
        "Thread creation error\n"
    }
};

HANDLE hMutex;
HANDLE hConditionEvent;
int turn = 0; // 0 - родительский поток, 1 - дочерний
int language = LANG_RU; // Текущий язык (можно менять)

DWORD WINAPI child_thread(LPVOID arg) {
    for (int i = 0; i < NUM_LINES; i++) {
        WaitForSingleObject(hMutex, INFINITE);

        // Ждем своей очереди
        while (turn != 1) {
            ReleaseMutex(hMutex);
            WaitForSingleObject(hConditionEvent, INFINITE);
            WaitForSingleObject(hMutex, INFINITE);
        }

        printf(strings[language].child_line, i + 1);

        // Передаем очередь родительскому потоку
        turn = 0;
        SetEvent(hConditionEvent);
        ReleaseMutex(hMutex);
    }
    return 0;
}

int main() {
    // Установка локали для корректного отображения русских символов
    setlocale(LC_ALL, "");

    HANDLE hThread;

    // Инициализация мьютекса и события
    hMutex = CreateMutex(NULL, FALSE, NULL);
    hConditionEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

    // Создание дочернего потока
    hThread = CreateThread(NULL, 0, child_thread, NULL, 0, NULL);
    if (hThread == NULL) {
        fprintf(stderr, strings[language].thread_error);
        return 1;
    }

    // Родительский поток
    for (int i = 0; i < NUM_LINES; i++) {
        WaitForSingleObject(hMutex, INFINITE);

        // Ждем своей очереди
        while (turn != 0) {
            ReleaseMutex(hMutex);
            WaitForSingleObject(hConditionEvent, INFINITE);
            WaitForSingleObject(hMutex, INFINITE);
        }

        printf(strings[language].parent_line, i + 1);

        // Передаем очередь дочернему потоку
        turn = 1;
        SetEvent(hConditionEvent);
        ReleaseMutex(hMutex);
    }

    // Ожидание завершения дочернего потока
    WaitForSingleObject(hThread, INFINITE);

    // Освобождение ресурсов
    CloseHandle(hMutex);
    CloseHandle(hConditionEvent);
    CloseHandle(hThread);

    return 0;
}