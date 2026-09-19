#include "imux_launcher_functions.h"

#include <windows.h>
#include <vector>

std::filesystem::path imux_launcher_app_data_root() {
    wchar_t buffer[32768]{};
    const DWORD capacity = static_cast<DWORD>(sizeof(buffer) / sizeof(buffer[0]));
    const DWORD length = GetEnvironmentVariableW(L"APPDATA", buffer, capacity);
    if (length == 0 || length >= capacity) return {};
    return std::filesystem::path(buffer) / L"Imux";
}

std::filesystem::path imux_launcher_find_game_executable() {
    const auto appData = imux_launcher_app_data_root();
    const std::filesystem::path current = std::filesystem::current_path();

    const std::vector<std::filesystem::path> candidates = {
        current / L"ImuxGame.exe",
        appData / L"game" / L"ImuxGame.exe",
        appData / L"instances" / L"default" / L"ImuxGame.exe",
        appData / L"instances" / L"default" / L"game" / L"ImuxGame.exe"
    };

    for (const auto& candidate : candidates) {
        std::error_code ec;
        if (std::filesystem::is_regular_file(candidate, ec)) return candidate;
    }
    return {};
}

int imux_launcher_try_launch_game() {
    const auto executable = imux_launcher_find_game_executable();
    if (executable.empty()) return 0;

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION processInfo{};
    std::wstring commandLine = L"\"" + executable.wstring() + L"\"";
    std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
    mutableCommand.push_back(L'\0');

    const std::wstring workingDirectory = executable.parent_path().wstring();
    const BOOL started = CreateProcessW(
        executable.c_str(),
        mutableCommand.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NEW_PROCESS_GROUP,
        nullptr,
        workingDirectory.empty() ? nullptr : workingDirectory.c_str(),
        &startup,
        &processInfo
    );

    if (!started) return -1;

    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    return 1;
}
