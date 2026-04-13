#include "memory.hpp"
#include <Psapi.h>
#include <cstring>

namespace cs2 {

std::unordered_map<std::string, HMODULE> PatternScanner::m_modules;
std::unordered_map<std::string, uintptr_t> PatternScanner::m_module_bases;
std::unordered_map<std::string, size_t> PatternScanner::m_module_sizes;
std::mutex PatternScanner::m_mutex;

void PatternScanner::CacheModule(HMODULE module, const std::string& name) {
    std::lock_guard lock(m_mutex);
    m_modules[name] = module;
    m_module_bases[name] = reinterpret_cast<uintptr_t>(module);

    MODULEINFO info;
    if (GetModuleInformation(GetCurrentProcess(), module, &info, sizeof(info))) {
        m_module_sizes[name] = info.SizeOfImage;
    }
}

HMODULE PatternScanner::GetModuleHandle(const char* name) {
    std::wstring wname(name, name + strlen(name));
    return GetModuleHandle(wname.c_str());
}

HMODULE PatternScanner::GetModuleHandle(const wchar_t* name) {
    HMODULE hMod = ::GetModuleHandleW(name);
    if (hMod) {
        char buf[256];
        WideCharToMultiByte(CP_UTF8, 0, name, -1, buf, sizeof(buf), nullptr, nullptr);
        CacheModule(hMod, buf);
    }
    return hMod;
}

uintptr_t PatternScanner::GetModuleBase(const std::string& name) {
    std::lock_guard lock(m_mutex);
    auto it = m_module_bases.find(name);
    if (it != m_module_bases.end()) return it->second;

    HMODULE hMod = GetModuleHandle(name.c_str());
    if (hMod) return reinterpret_cast<uintptr_t>(hMod);
    return 0;
}

size_t PatternScanner::GetModuleSize(HMODULE module) {
    MODULEINFO info;
    if (GetModuleInformation(GetCurrentProcess(), module, &info, sizeof(info)))
        return info.SizeOfImage;
    return 0;
}

bool PatternScanner::PatternMatch(const uint8_t* data, const uint8_t* pattern, const char* mask) {
    for (; *mask; ++pattern, ++mask, ++data) {
        if (*mask == 'x' && *data != *pattern)
            return false;
    }
    return !*mask;
}

void PatternScanner::ParsePattern(const char* pattern, std::vector<uint8_t>& bytes, std::string& mask) {
    const char* p = pattern;
    while (*p) {
        if (*p == ' ') { ++p; continue; }
        if (*p == '?') {
            bytes.push_back(0);
            mask += '?';
            ++p;
            if (*p == '?') ++p;
        } else {
            uint8_t byte = 0;
            for (int i = 0; i < 2; ++i) {
                byte <<= 4;
                char c = *p++;
                if (c >= '0' && c <= '9') byte |= (c - '0');
                else if (c >= 'A' && c <= 'F') byte |= (c - 'A' + 10);
                else if (c >= 'a' && c <= 'f') byte |= (c - 'a' + 10);
            }
            bytes.push_back(byte);
            mask += 'x';
        }
    }
}

uintptr_t PatternScanner::FindPattern(HMODULE module, const char* pattern) {
    if (!module) return 0;

    std::vector<uint8_t> bytes;
    std::string mask;
    ParsePattern(pattern, bytes, mask);

    MODULEINFO info;
    if (!GetModuleInformation(GetCurrentProcess(), module, &info, sizeof(info)))
        return 0;

    uintptr_t start = reinterpret_cast<uintptr_t>(info.lpBaseOfDll);
    size_t size = info.SizeOfImage;

    for (size_t i = 0; i < size - bytes.size(); ++i) {
        if (PatternMatch(reinterpret_cast<const uint8_t*>(start + i), bytes.data(), mask.c_str()))
            return start + i;
    }
    return 0;
}

uintptr_t PatternScanner::FindPattern(const wchar_t* module_name, const char* pattern) {
    HMODULE hMod = GetModuleHandle(module_name);
    return FindPattern(hMod, pattern);
}

uintptr_t PatternScanner::FindPatternInRange(uintptr_t start, size_t length, const char* pattern) {
    std::vector<uint8_t> bytes;
    std::string mask;
    ParsePattern(pattern, bytes, mask);

    for (size_t i = 0; i < length - bytes.size(); ++i) {
        if (PatternMatch(reinterpret_cast<const uint8_t*>(start + i), bytes.data(), mask.c_str()))
            return start + i;
    }
    return 0;
}

std::vector<uintptr_t> PatternScanner::FindPatternAllOccurrences(HMODULE module, const char* pattern) {
    std::vector<uintptr_t> results;
    if (!module) return results;

    std::vector<uint8_t> bytes;
    std::string mask;
    ParsePattern(pattern, bytes, mask);

    MODULEINFO info;
    if (!GetModuleInformation(GetCurrentProcess(), module, &info, sizeof(info)))
        return results;

    uintptr_t start = reinterpret_cast<uintptr_t>(info.lpBaseOfDll);
    size_t size = info.SizeOfImage;

    for (size_t i = 0; i < size - bytes.size(); ++i) {
        if (PatternMatch(reinterpret_cast<const uint8_t*>(start + i), bytes.data(), mask.c_str()))
            results.push_back(start + i);
    }
    return results;
}

uintptr_t PatternScanner::ResolveRelativeAddress(uintptr_t address, size_t offset, size_t instruction_length) {
    int32_t rel = *reinterpret_cast<int32_t*>(address + offset);
    return address + offset + instruction_length + rel;
}

uintptr_t PatternScanner::GetAbsoluteAddress(uintptr_t address, int pre_offset, int post_offset) {
    uintptr_t ptr = address + pre_offset;
    ptr += sizeof(int32_t) + *reinterpret_cast<int32_t*>(ptr);
    ptr += post_offset;
    return ptr;
}

}
