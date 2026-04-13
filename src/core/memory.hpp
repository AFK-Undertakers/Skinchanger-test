#pragma once

#include <Windows.h>
#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <mutex>

namespace cs2 {

class PatternScanner {
public:
    static uintptr_t FindPattern(HMODULE module, const char* pattern);
    static uintptr_t FindPattern(const wchar_t* module_name, const char* pattern);
    static uintptr_t FindPatternInRange(uintptr_t start, size_t length, const char* pattern);
    static std::vector<uintptr_t> FindPatternAllOccurrences(HMODULE module, const char* pattern);
    static uintptr_t ResolveRelativeAddress(uintptr_t address, size_t offset = 3, size_t instruction_length = 7);
    static uintptr_t GetAbsoluteAddress(uintptr_t address, int pre_offset = 0, int post_offset = 0);
    static HMODULE GetModuleHandle(const char* name);
    static HMODULE GetModuleHandle(const wchar_t* name);
    static void CacheModule(HMODULE module, const std::string& name);
    static uintptr_t GetModuleBase(const std::string& name);
    static size_t GetModuleSize(HMODULE module);

private:
    static bool PatternMatch(const uint8_t* data, const uint8_t* pattern, const char* mask);
    static void ParsePattern(const char* pattern, std::vector<uint8_t>& bytes, std::string& mask);

    static std::unordered_map<std::string, HMODULE> m_modules;
    static std::unordered_map<std::string, uintptr_t> m_module_bases;
    static std::unordered_map<std::string, size_t> m_module_sizes;
    static std::mutex m_mutex;
};

}
