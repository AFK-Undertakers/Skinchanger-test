#include "skinchanger.hpp"
#include "../../sdk/offsets.hpp"
#include "../../core/memory.hpp"
#include "../../sdk/schema.hpp"
#include <algorithm>
#include <chrono>

namespace cs2 {

bool SkinChanger::s_initialized = false;
bool SkinChanger::s_dump_complete = false;
std::string SkinChanger::s_status_message = "Not initialized";
std::shared_mutex SkinChanger::s_mutex;
std::unordered_map<uint64_t, SkinConfig> SkinChanger::s_active_skins;
std::vector<uint64_t> SkinChanger::s_added_item_ids;
uintptr_t SkinChanger::s_force_viewmodel_fn = 0;

bool SkinChanger::Initialize() {
    if (s_initialized) return true;

    s_status_message = "Initializing...";

    PatternScanner::GetModuleHandle(L"client.dll");
    PatternScanner::GetModuleHandle(L"engine2.dll");
    PatternScanner::GetModuleHandle(L"schemasystem.dll");
    PatternScanner::GetModuleHandle(L"filesystem_stdio.dll");

    SchemaParser::Initialize();
    ItemDatabase::Initialize();

    HMODULE hClient = PatternScanner::GetModuleHandle(L"client.dll");
    if (hClient) {
        uintptr_t addr = PatternScanner::FindPattern(hClient,
            "E8 ? ? ? ? 8B 45 ? 85 C0 74 ? 8B 08");
        if (addr) {
            s_force_viewmodel_fn = PatternScanner::ResolveRelativeAddress(addr, 1, 5);
        }
    }

    s_dump_complete = true;
    s_initialized = true;
    s_status_message = "Initialized - " + std::to_string(ItemDatabase::GetItems().size()) + " items loaded";

    ConfigManager::Load();
    return true;
}

void SkinChanger::Shutdown() {
    std::unique_lock lock(s_mutex);
    s_active_skins.clear();
    s_added_item_ids.clear();
    s_initialized = false;
    s_dump_complete = false;
    s_status_message = "Shutdown";
}

uint64_t SkinChanger::GenerateItemID(int def_index) {
    uint64_t id = static_cast<uint64_t>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());
    id &= 0x0000FFFFFFFFFFFF;
    id |= (static_cast<uint64_t>(def_index) << 48);
    return id;
}

bool SkinChanger::ApplySkin(const SkinConfig& config) {
    if (!s_initialized) return false;

    uint64_t item_id = GenerateItemID(config.definition_index);

    SkinConfig saved_config = config;
    saved_config.item_id = item_id;

    std::unique_lock lock(s_mutex);
    s_active_skins[item_id] = saved_config;
    s_added_item_ids.push_back(item_id);

    ConfigManager::AddConfig(saved_config);

    int slot = ItemDatabase::GetLoadoutSlotForDefIndex(config.definition_index);
    if (slot >= 0) {
        lock.unlock();
        CCSInventoryManager* mgr = CCSInventoryManager::GetInstance();
        if (mgr) {
            mgr->EquipItemInLoadout(2, slot, item_id);
        }
    }

    return true;
}

bool SkinChanger::RemoveSkin(uint64_t item_id) {
    std::unique_lock lock(s_mutex);
    auto it = s_active_skins.find(item_id);
    if (it == s_active_skins.end()) return false;

    s_active_skins.erase(it);
    s_added_item_ids.erase(
        std::remove(s_added_item_ids.begin(), s_added_item_ids.end(), item_id),
        s_added_item_ids.end());

    ConfigManager::RemoveConfig(item_id);
    return true;
}

void SkinChanger::RemoveAllSkins() {
    std::unique_lock lock(s_mutex);
    s_active_skins.clear();
    s_added_item_ids.clear();
}

bool SkinChanger::EquipItem(int loadout_slot, uint64_t item_id) {
    if (!s_initialized) return false;

    std::shared_lock lock(s_mutex);
    auto it = s_active_skins.find(item_id);
    if (it == s_active_skins.end()) return false;

    CCSInventoryManager* mgr = CCSInventoryManager::GetInstance();
    if (!mgr) return false;

    mgr->EquipItemInLoadout(2, loadout_slot, item_id);
    return true;
}

std::vector<DumpedItem> SkinChanger::GetDumpedItems() {
    return ItemDatabase::GetItems();
}

std::vector<DumpedItem> SkinChanger::Search(std::string_view query) {
    return ItemDatabase::Search(query);
}

std::vector<DumpedItem> SkinChanger::Search(std::string_view query, ItemCategory category) {
    return ItemDatabase::Search(query, category);
}

bool SkinChanger::LoadConfig(const std::string& path) {
    return ConfigManager::Load(path);
}

bool SkinChanger::SaveConfig(const std::string& path) {
    return ConfigManager::Save(path);
}

std::string SkinChanger::GetStatusMessage() {
    std::shared_lock lock(s_mutex);
    return s_status_message;
}

int SkinChanger::GetActiveSkinCount() {
    std::shared_lock lock(s_mutex);
    return static_cast<int>(s_active_skins.size());
}

void SkinChanger::OnFrameStageNotify(int stage) {
    if (stage != static_cast<int>(FrameStage::FRAME_RENDER_END)) return;
    if (!s_initialized) return;

    std::shared_lock lock(s_mutex);
    if (s_active_skins.empty()) return;

    CCSInventoryManager* inv_mgr = CCSInventoryManager::GetInstance();
    if (!inv_mgr) return;

    CCSPlayerInventory* inventory = inv_mgr->GetLocalInventory();
    if (!inventory) return;

    uint64_t steam_id = inventory->GetOwner().id;

    // TODO: RE REQUIRED - Full entity iteration via CGameEntitySystem
    // Pattern: 48 8B 1D ? ? ? ? 48 85 DB 74 ? 48 8B CB (EntityList)
    // For now, this is a stub that should be called from the base's entity loop

    // The base cheat should iterate entities 65 to highestEntityIndex
    // and call ApplyWeaponSkin for each weapon owned by the local player
}

void SkinChanger::ApplyWeaponSkin(void* weapon_entity, const SkinConfig& config) {
    if (!weapon_entity) return;

    uintptr_t base = reinterpret_cast<uintptr_t>(weapon_entity);

    *(int32_t*)(base + offsets::C_EconEntity::m_nFallbackPaintKit) = config.paint_kit_id;
    *(int32_t*)(base + offsets::C_EconEntity::m_nFallbackSeed) = config.seed;
    *(float*)(base + offsets::C_EconEntity::m_flFallbackWear) = config.wear;

    if (config.stattrak >= 0) {
        *(int32_t*)(base + offsets::C_EconEntity::m_nFallbackStatTrak) = config.stattrak;
    }
}

void SkinChanger::ApplyGloveSkin(void* pawn_entity, const SkinConfig& config) {
    if (!pawn_entity) return;

    uintptr_t base = reinterpret_cast<uintptr_t>(pawn_entity);
    uintptr_t gloves = base + offsets::C_CSPlayerPawn::m_EconGloves;

    *(int32_t*)(gloves + offsets::C_EconEntity::m_nFallbackPaintKit) = config.paint_kit_id;
    *(int32_t*)(gloves + offsets::C_EconEntity::m_nFallbackSeed) = config.seed;
    *(float*)(gloves + offsets::C_EconEntity::m_flFallbackWear) = config.wear;

    if (config.stattrak >= 0) {
        *(int32_t*)(gloves + offsets::C_EconEntity::m_nFallbackStatTrak) = config.stattrak;
    }

    *(bool*)(base + offsets::C_CSPlayerPawn::m_bNeedToReApplyGloves) = true;
}

void SkinChanger::InvalidateGloveMaterials(void* view_model) {
    if (!view_model) return;

    MaterialInfo* mat_info = reinterpret_cast<MaterialInfo*>(
        reinterpret_cast<uint8_t*>(view_model) + VIEWMODEL_GLOVE_MATERIAL_OFFSET);

    for (uint32_t i = 0; i < mat_info->count; ++i) {
        if (mat_info->records[i].identifier == GLOVE_MATERIAL_MAGIC) {
            mat_info->records[i].type_index = 0xFFFFFFFF;
            break;
        }
    }
}

void SkinChanger::OnSetModel(void* entity, const char*& model_path) {
    if (!entity || !model_path) return;

    std::shared_lock lock(s_mutex);
    if (s_active_skins.empty()) return;

    // TODO: RE REQUIRED - Match entity to active knife skin
    // If the entity is a viewmodel with a knife weapon, override the model
    // with the knife model from the active skin config
}

void SkinChanger::OnPreFireEvent(void* event) {
    if (!event) return;

    // TODO: RE REQUIRED - Kill event weapon name fix for knives
    // Hook IGameEventManager2::FireEvent and modify weapon name
    // when a knife kill occurs to show the correct knife name
}

void SkinChanger::ForceViewModelUpdate() {
    if (!s_force_viewmodel_fn) return;
    reinterpret_cast<void(*)()>(s_force_viewmodel_fn)();
}

}
