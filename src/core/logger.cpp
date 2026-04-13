#include "logger.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <io.h>
#include <fcntl.h>

namespace cs2 {

bool Logger::s_initialized = false;
bool Logger::s_console_allocated = false;
std::string Logger::s_session_id;
std::string Logger::s_log_path;
std::ofstream Logger::s_log_file;
std::mutex Logger::s_mutex;

void Logger::Initialize(const std::string& session_id) {
    std::lock_guard lock(s_mutex);
    if (s_initialized) return;

    s_session_id = session_id;

    // Allocate console window
    if (AllocConsole()) {
        s_console_allocated = true;
        FILE* fp;
        freopen_s(&fp, "CONOUT$", "w", stdout);
        freopen_s(&fp, "CONOUT$", "w", stderr);
        freopen_s(&fp, "CONIN$", "r", stdin);

        SetConsoleTitleA("SkinChanger Debug Console");
        SetConsoleOutputCP(CP_UTF8);

        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut != INVALID_HANDLE_VALUE) {
            DWORD mode = 0;
            GetConsoleMode(hOut, &mode);
            mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, mode);
        }
    }

    // Create log file
    char exe_path[MAX_PATH];
    GetModuleFileNameA(nullptr, exe_path, MAX_PATH);
    std::string dir = exe_path;
    size_t last_sep = dir.find_last_of("\\/");
    if (last_sep != std::string::npos) dir = dir.substr(0, last_sep);

    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
    localtime_s(&tm_buf, &time_t);

    std::ostringstream oss;
    oss << dir << "\\skinchanger_session-"
        << std::put_time(&tm_buf, "%Y%m%d-%H%M%S") << ".log";
    s_log_path = oss.str();

    s_log_file.open(s_log_path, std::ios::out | std::ios::app);
    if (s_log_file.is_open()) {
        s_log_file << "========================================\n";
        s_log_file << " SkinChanger Debug Log\n";
        s_log_file << " Session: " << s_session_id << "\n";
        s_log_file << " Started: " << GetTimestamp() << "\n";
        s_log_file << "========================================\n\n";
        s_log_file.flush();
    }

    s_initialized = true;
    Info("Logger initialized. Session: " + s_session_id);
    Info("Log file: " + s_log_path);
}

void Logger::Shutdown() {
    std::lock_guard lock(s_mutex);
    if (!s_initialized) return;

    Info("Logger shutting down");
    if (s_log_file.is_open()) {
        s_log_file << "\n--- Session ended: " << GetTimestamp() << " ---\n";
        s_log_file.close();
    }
    if (s_console_allocated) {
        FreeConsole();
        s_console_allocated = false;
    }
    s_initialized = false;
}

std::string Logger::GetTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    std::tm tm_buf;
    localtime_s(&tm_buf, &time_t);

    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

std::string Logger::GetSessionId() {
    return s_session_id;
}

void Logger::WriteToConsole(const std::string& level, std::string_view msg) {
    if (!s_console_allocated) return;

    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;

    std::string line = "[" + GetTimestamp() + "] [" + level + "] " + std::string(msg) + "\n";

    if (level == "ERROR") {
        SetConsoleTextAttribute(hOut, FOREGROUND_RED | FOREGROUND_INTENSITY);
    } else if (level == "WARN") {
        SetConsoleTextAttribute(hOut, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    } else if (level == "DEBUG") {
        SetConsoleTextAttribute(hOut, FOREGROUND_GREEN | FOREGROUND_BLUE);
    } else {
        SetConsoleTextAttribute(hOut, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    }

    WriteConsoleA(hOut, line.c_str(), static_cast<DWORD>(line.size()), nullptr, nullptr);
    SetConsoleTextAttribute(hOut, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
}

void Logger::WriteToFile(const std::string& level, std::string_view msg) {
    if (!s_log_file.is_open()) return;
    s_log_file << "[" << GetTimestamp() << "] [" << level << "] " << msg << "\n";
    s_log_file.flush();
}

void Logger::Info(std::string_view msg) {
    std::lock_guard lock(s_mutex);
    WriteToConsole("INFO", msg);
    WriteToFile("INFO", msg);
}

void Logger::Warn(std::string_view msg) {
    std::lock_guard lock(s_mutex);
    WriteToConsole("WARN", msg);
    WriteToFile("WARN", msg);
}

void Logger::Error(std::string_view msg) {
    std::lock_guard lock(s_mutex);
    WriteToConsole("ERROR", msg);
    WriteToFile("ERROR", msg);
}

void Logger::Debug(std::string_view msg) {
    std::lock_guard lock(s_mutex);
    WriteToConsole("DEBUG", msg);
    WriteToFile("DEBUG", msg);
}

void Logger::LogRead(uintptr_t address, size_t size, const std::string& context) {
    std::lock_guard lock(s_mutex);
    std::ostringstream oss;
    oss << "READ  [0x" << std::hex << address << std::dec << "] (" << size << " bytes) - " << context;
    WriteToConsole("MEM", oss.str());
    WriteToFile("MEM", oss.str());
}

void Logger::LogWrite(uintptr_t address, size_t size, const std::string& context) {
    std::lock_guard lock(s_mutex);
    std::ostringstream oss;
    oss << "WRITE [0x" << std::hex << address << std::dec << "] (" << size << " bytes) - " << context;
    WriteToConsole("MEM", oss.str());
    WriteToFile("MEM", oss.str());
}

void Logger::LogUIAction(std::string_view action, std::string_view details) {
    std::lock_guard lock(s_mutex);
    std::ostringstream oss;
    oss << "UI: " << action << " - " << details;
    WriteToConsole("UI", oss.str());
    WriteToFile("UI", oss.str());
}

void Logger::LogConfigSave(const std::string& path) {
    std::lock_guard lock(s_mutex);
    std::string msg = "CONFIG SAVE: " + path;
    WriteToConsole("CFG", msg);
    WriteToFile("CFG", msg);
}

void Logger::LogConfigLoad(const std::string& path) {
    std::lock_guard lock(s_mutex);
    std::string msg = "CONFIG LOAD: " + path;
    WriteToConsole("CFG", msg);
    WriteToFile("CFG", msg);
}

void Logger::LogSkinApply(int def_index, int paint_kit, const std::string& item_name) {
    std::lock_guard lock(s_mutex);
    std::ostringstream oss;
    oss << "SKIN APPLY: " << item_name << " (def:" << def_index << ", kit:" << paint_kit << ")";
    WriteToConsole("SKIN", oss.str());
    WriteToFile("SKIN", oss.str());
}

void Logger::LogPatternScan(const std::string& module, const std::string& pattern, bool found) {
    std::lock_guard lock(s_mutex);
    std::ostringstream oss;
    oss << "PATTERN: " << module << " | " << pattern << " -> " << (found ? "FOUND" : "NOT FOUND");
    WriteToConsole("SCAN", oss.str());
    WriteToFile("SCAN", oss.str());
}

void Logger::LogHook(const std::string& hook_name, bool success) {
    std::lock_guard lock(s_mutex);
    std::string msg = "HOOK: " + hook_name + " -> " + (success ? "SUCCESS" : "FAILED");
    WriteToConsole("HOOK", msg);
    WriteToFile("HOOK", msg);
}

void Logger::LogFrameStage(int stage) {
    std::lock_guard lock(s_mutex);
    std::ostringstream oss;
    oss << "FRAME STAGE: " << stage;
    WriteToConsole("FRAME", oss.str());
    WriteToFile("FRAME", oss.str());
}

void Logger::Printf(const char* fmt, ...) {
    std::lock_guard lock(s_mutex);
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf_s(buf, sizeof(buf), fmt, args);
    va_end(args);

    WriteToConsole("LOG", buf);
    WriteToFile("LOG", buf);
}

}
