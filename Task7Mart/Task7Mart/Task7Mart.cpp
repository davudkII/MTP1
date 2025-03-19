#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <windows.h>

// Функция для копирования файла
void copyFile(const std::wstring& src, const std::wstring& dst) {
    if (!CopyFile(src.c_str(), dst.c_str(), FALSE)) {
        std::wcerr << L"Ошибка копирования файла: " << src << L" (" << GetLastError() << L")" << std::endl;
    }
}

// Функция для рекурсивного копирования каталога
void copyDirectory(const std::wstring& src, const std::wstring& dst) {
    WIN32_FIND_DATA findFileData;
    HANDLE hFind = FindFirstFile((src + L"\\*").c_str(), &findFileData);

    if (hFind == INVALID_HANDLE_VALUE) {
        std::wcerr << "Ошибка открытия каталога: " << src << " (" << GetLastError() << ")" << std::endl;
        return;
    }

    if (!CreateDirectory(dst.c_str(), NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
        std::wcerr << "Ошибка создания каталога: " << dst << " (" << GetLastError() << ")" << std::endl;
        FindClose(hFind);
        return;
    }

    std::vector<std::thread> threads;
    do {
        std::wstring name = findFileData.cFileName;
        if (name == L"." || name == L"..") continue;

        std::wstring srcPath = src + L"\\" + name;
        std::wstring dstPath = dst + L"\\" + name;

        if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            // Рекурсивно копируем подкаталог в новом потоке
            threads.emplace_back(copyDirectory, srcPath, dstPath);
        }
        else {
            // Копируем файл в новом потоке
            threads.emplace_back(copyFile, srcPath, dstPath);
        }
    } while (FindNextFile(hFind, &findFileData) != 0);

    FindClose(hFind);

    // Ожидаем завершения всех потоков
    for (auto& thread : threads) {
        thread.join();
    }
}

int main(int argc, char* argv[]) {
    std::setlocale(LC_ALL, "ru");
    if (argc != 3) {
        std::cerr << "Использование: " << argv[0] << " <исходный каталог> <целевой каталог>" << std::endl;
        return 1;
    }

    // Преобразуем аргументы в wide-строки
    std::wstring srcDir(argv[1], argv[1] + strlen(argv[1]));
    std::wstring dstDir(argv[2], argv[2] + strlen(argv[2]));

    copyDirectory(srcDir, dstDir);

    return 0;
}