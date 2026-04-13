#pragma once

#include <Windows.h>
#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <shared_mutex>
#include <vector>

namespace cs2 {

struct SchemaField_t {
    const char* name;
    uint32_t offset;
    uint32_t size;
};

struct SchemaClass_t {
    const char* name;
    std::vector<SchemaField_t> fields;
};

class SchemaParser {
public:
    static bool Initialize();
    static std::vector<SchemaClass_t> GetClasses();
    static const SchemaClass_t* FindClass(std::string_view name);
    static uint32_t GetOffset(std::string_view class_name, std::string_view field_name);
    static void* GetSchemaSystem() { return m_schema_system; }

private:
    static void* m_schema_system;
    static std::unordered_map<std::string, SchemaClass_t> m_classes;
    static std::shared_mutex m_mutex;
    static std::string m_last_error;
};

}
