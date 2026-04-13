#pragma once

#include <Windows.h>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include <cstdint>
#include "items.hpp"
#include "config.hpp"
#include "../../sdk/inventory.hpp"

namespace cs2 {

enum class FrameStage : int {
    FRAME_START = 0,
    FRAME_NET_UPDATE_START = 1,
    FRAME_NET_UPDATE_POSTDATAUPDATE_START = 2,
    FRAME_NET_UPDATE_POSTDATAUPDATE_END = 3,
    FRAME_NET_UPDATE_END = 4,
    FRAME_RENDER_START = 5,
    FRAME_RENDER_END = 6
};

class SkinChanger {
public:
    static bool Initialize();
    static void Shutdown();

    static bool ApplySkin(const SkinConfig& config);
    static bool RemoveSkin(uint64_t item_id);
    static void RemoveAllSkins();
    static bool EquipItem(int loadout_slot, uint64_t item_id);

    static std::vector<DumpedItem> GetDumpedItems();
    static std::vector<DumpedItem> Search(std::string_view query);
    static std::vector<DumpedItem> Search(std::string_view query, ItemCategory category);

    static bool LoadConfig(const std::string& path = "");
    static bool SaveConfig(const std::string& path = "");

    static void OnFrameStageNotify(int stage);
    static void OnSetModel(void* entity, const char*& model_path);
    static void OnPreFireEvent(void* event);

    static void ForceViewModelUpdate();

    static bool IsInitialized() { return s_initialized; }
    static bool IsDumpComplete() { return s_dump_complete; }
    static std::string GetStatusMessage();
    static int GetActiveSkinCount();

private:
    static uint64_t GenerateItemID(int def_index);
    static void ApplyWeaponSkin(void* weapon_entity, const SkinConfig& config);
    static void ApplyGloveSkin(void* pawn_entity, const SkinConfig& config);
    static void InvalidateGloveMaterials(void* view_model);

    static bool s_initialized;
    static bool s_dump_complete;
    static std::string s_status_message;
    static std::shared_mutex s_mutex;
    static std::unordered_map<uint64_t, SkinConfig> s_active_skins;
    static std::vector<uint64_t> s_added_item_ids;

    static uintptr_t s_force_viewmodel_fn;

    static constexpr uint32_t GLOVE_MATERIAL_MAGIC = 0xF143B82A;
    static constexpr size_t VIEWMODEL_GLOVE_MATERIAL_OFFSET = 0xF80;

    struct MaterialRecord {
        uint32_t unknown;
        uint32_t identifier;
        uint32_t handle;
        uint32_t type_index;
    };

    struct MaterialInfo {
        MaterialRecord* records;
        uint32_t count;
    };
};

}
