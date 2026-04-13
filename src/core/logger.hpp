#pragma once

#include <Windows.h>
#include <string>
#include <string_view>
#include <mutex>
#include <fstream>
#include <chrono>
#include <cstdio>
#include <cstdarg>

namespace cs2 {

class Logger {
public:
    static void Initialize(const std::string& session_id);
    static void Shutdown();

    static void Info(std::string_view msg);
    static void Warn(std::string_view msg);
    static void Error(std::string_view msg);
    static void Debug(std::string_view msg);

    static void LogRead(uintptr_t address, size_t size, const std::string& context);
    static void LogWrite(uintptr_t address, size_t size, const std::string& context);
    static void LogUIAction(std::string_view action, std::string_view details);
    static void LogConfigSave(const std::string& path);
    static void LogConfigLoad(const std::string& path);
    static void LogSkinApply(int def_index, int paint_kit, const std::string& item_name);
    static void LogPatternScan(const std::string& module, const std::string& pattern, bool found);
    static void LogHook(const std::string& hook_name, bool success);
    static void LogFrameStage(int stage);

    static void Printf(const char* fmt, ...);

    static bool IsInitialized() { return s_initialized; }

private:
    static void WriteToConsole(const std::string& level, std::string_view msg);
    static void WriteToFile(const std::string& level, std::string_view msg);
    static std::string GetTimestamp();
    static std::string GetSessionId();

    static bool s_initialized;
    static bool s_console_allocated;
    static std::string s_session_id;
    static std::string s_log_path;
    static std::ofstream s_log_file;
    static std::mutex s_mutex;
};

}

#ifdef SKINCHANGER_DEBUG
    #define SC_LOG_INFO(msg)    cs2::Logger::Info(msg)
    #define SC_LOG_WARN(msg)    cs2::Logger::Warn(msg)
    #define SC_LOG_ERROR(msg)   cs2::Logger::Error(msg)
    #define SC_LOG_DEBUG(msg)   cs2::Logger::Debug(msg)
    #define SC_LOG_READ(addr, sz, ctx)    cs2::Logger::LogRead(addr, sz, ctx)
    #define SC_LOG_WRITE(addr, sz, ctx)   cs2::Logger::LogWrite(addr, sz, ctx)
    #define SC_LOG_UI(action, details)    cs2::Logger::LogUIAction(action, details)
    #define SC_LOG_CFG_SAVE(path)         cs2::Logger::LogConfigSave(path)
    #define SC_LOG_CFG_LOAD(path)         cs2::Logger::LogConfigLoad(path)
    #define SC_LOG_SKIN(def, kit, name)   cs2::Logger::LogSkinApply(def, kit, name)
    #define SC_LOG_PATTERN(mod, pat, ok)  cs2::Logger::LogPatternScan(mod, pat, ok)
    #define SC_LOG_HOOK(name, ok)         cs2::Logger::LogHook(name, ok)
    #define SC_LOG_FRAME(stage)           cs2::Logger::LogFrameStage(stage)
    #define SC_LOG_PRINTF(fmt, ...)       cs2::Logger::Printf(fmt, __VA_ARGS__)
#else
    #define SC_LOG_INFO(msg)
    #define SC_LOG_WARN(msg)
    #define SC_LOG_ERROR(msg)
    #define SC_LOG_DEBUG(msg)
    #define SC_LOG_READ(addr, sz, ctx)
    #define SC_LOG_WRITE(addr, sz, ctx)
    #define SC_LOG_UI(action, details)
    #define SC_LOG_CFG_SAVE(path)
    #define SC_LOG_CFG_LOAD(path)
    #define SC_LOG_SKIN(def, kit, name)
    #define SC_LOG_PATTERN(mod, pat, ok)
    #define SC_LOG_HOOK(name, ok)
    #define SC_LOG_FRAME(stage)
    #define SC_LOG_PRINTF(fmt, ...)
#endif
