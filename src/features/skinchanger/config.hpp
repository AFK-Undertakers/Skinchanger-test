#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <mutex>
#include "../features/skinchanger/items.hpp"

namespace cs2 {

class ConfigManager {
public:
    static bool Load(const std::string& path = "");
    static bool Save(const std::string& path = "");
    static std::vector<SkinConfig>& GetConfigs();
    static void AddConfig(const SkinConfig& config);
    static void RemoveConfig(uint64_t item_id);
    static SkinConfig* FindConfig(uint64_t item_id);

private:
    static std::vector<SkinConfig> m_configs;
    static std::string m_config_path;
    static std::mutex m_mutex;
    static std::string GetDefaultPath();
};

}
