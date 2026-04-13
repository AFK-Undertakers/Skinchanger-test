#include "config.hpp"
#include <fstream>
#include <sstream>
#include <Windows.h>

namespace cs2 {

std::vector<SkinConfig> ConfigManager::m_configs;
std::string ConfigManager::m_config_path;
std::mutex ConfigManager::m_mutex;

std::string ConfigManager::GetDefaultPath() {
    if (!m_config_path.empty()) return m_config_path;

    char exe_path[MAX_PATH];
    GetModuleFileNameA(nullptr, exe_path, MAX_PATH);
    std::string dir = exe_path;
    size_t last_sep = dir.find_last_of("\\/");
    if (last_sep != std::string::npos) dir = dir.substr(0, last_sep);
    m_config_path = dir + "\\skinchanger_config.json";
    return m_config_path;
}

bool ConfigManager::Load(const std::string& path) {
    std::string config_path = path.empty() ? GetDefaultPath() : path;

    std::ifstream file(config_path);
    if (!file.is_open()) return false;

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string json_str = buffer.str();

    std::lock_guard lock(m_mutex);
    m_configs.clear();

    // TODO: Replace with nlohmann::json when integrating
    // For now, parse a minimal JSON subset manually
    // Expected format: {"skins": [{"def_index": 1, "paint_kit": 0, "seed": 0, "wear": 0.0, "stattrak": -1, "custom_name": "", "enabled": true}]}

    // Minimal JSON parser for skin configs
    size_t pos = json_str.find("\"skins\"");
    if (pos == std::string::npos) return true;

    pos = json_str.find('[', pos);
    if (pos == std::string::npos) return true;

    size_t end = json_str.find(']', pos);
    if (end == std::string::npos) return true;

    std::string array_str = json_str.substr(pos + 1, end - pos - 1);

    size_t obj_start = 0;
    while ((obj_start = array_str.find('{', obj_start)) != std::string::npos) {
        size_t obj_end = array_str.find('}', obj_start);
        if (obj_end == std::string::npos) break;

        std::string obj_str = array_str.substr(obj_start, obj_end - obj_start + 1);

        SkinConfig config;

        auto extract_int = [&](const char* key, int default_val) -> int {
            std::string search = std::string("\"") + key + "\"";
            size_t kpos = obj_str.find(search);
            if (kpos == std::string::npos) return default_val;
            size_t cpos = obj_str.find(':', kpos);
            if (cpos == std::string::npos) return default_val;
            return std::stoi(obj_str.substr(cpos + 1));
        };

        auto extract_float = [&](const char* key, float default_val) -> float {
            std::string search = std::string("\"") + key + "\"";
            size_t kpos = obj_str.find(search);
            if (kpos == std::string::npos) return default_val;
            size_t cpos = obj_str.find(':', kpos);
            if (cpos == std::string::npos) return default_val;
            return std::stof(obj_str.substr(cpos + 1));
        };

        auto extract_string = [&](const char* key) -> std::string {
            std::string search = std::string("\"") + key + "\"";
            size_t kpos = obj_str.find(search);
            if (kpos == std::string::npos) return "";
            size_t cpos = obj_str.find(':', kpos);
            if (cpos == std::string::npos) return "";
            size_t q1 = obj_str.find('"', cpos + 1);
            if (q1 == std::string::npos) return "";
            size_t q2 = obj_str.find('"', q1 + 1);
            if (q2 == std::string::npos) return "";
            return obj_str.substr(q1 + 1, q2 - q1 - 1);
        };

        auto extract_bool = [&](const char* key, bool default_val) -> bool {
            std::string search = std::string("\"") + key + "\"";
            size_t kpos = obj_str.find(search);
            if (kpos == std::string::npos) return default_val;
            size_t cpos = obj_str.find(':', kpos);
            if (cpos == std::string::npos) return default_val;
            return (obj_str.find("true", cpos) < obj_str.find(',', cpos));
        };

        config.definition_index = extract_int("definition_index", 0);
        config.paint_kit_id = extract_int("paint_kit_id", 0);
        config.seed = extract_int("seed", 0);
        config.wear = extract_float("wear", 0.0f);
        config.stattrak = extract_int("stattrak", -1);
        config.custom_name = extract_string("custom_name");
        config.model_override = extract_string("model_override");
        config.enabled = extract_bool("enabled", true);

        m_configs.push_back(config);
        obj_start = obj_end + 1;
    }

    return true;
}

bool ConfigManager::Save(const std::string& path) {
    std::string config_path = path.empty() ? GetDefaultPath() : path;

    std::lock_guard lock(m_mutex);

    std::ofstream file(config_path);
    if (!file.is_open()) return false;

    file << "{\n  \"skins\": [\n";

    for (size_t i = 0; i < m_configs.size(); ++i) {
        const auto& c = m_configs[i];
        file << "    {\n";
        file << "      \"definition_index\": " << c.definition_index << ",\n";
        file << "      \"paint_kit_id\": " << c.paint_kit_id << ",\n";
        file << "      \"seed\": " << c.seed << ",\n";
        file << "      \"wear\": " << c.wear << ",\n";
        file << "      \"stattrak\": " << c.stattrak << ",\n";
        file << "      \"custom_name\": \"" << c.custom_name << "\",\n";
        file << "      \"model_override\": \"" << c.model_override << "\",\n";
        file << "      \"enabled\": " << (c.enabled ? "true" : "false") << "\n";
        file << "    }";
        if (i + 1 < m_configs.size()) file << ",";
        file << "\n";
    }

    file << "  ]\n}\n";
    return true;
}

std::vector<SkinConfig>& ConfigManager::GetConfigs() {
    return m_configs;
}

void ConfigManager::AddConfig(const SkinConfig& config) {
    std::lock_guard lock(m_mutex);
    m_configs.push_back(config);
}

void ConfigManager::RemoveConfig(uint64_t item_id) {
    std::lock_guard lock(m_mutex);
    m_configs.erase(
        std::remove_if(m_configs.begin(), m_configs.end(),
            [item_id](const SkinConfig& c) { return c.item_id == item_id; }),
        m_configs.end());
}

SkinConfig* ConfigManager::FindConfig(uint64_t item_id) {
    std::lock_guard lock(m_mutex);
    for (auto& c : m_configs) {
        if (c.item_id == item_id) return &c;
    }
    return nullptr;
}

}
