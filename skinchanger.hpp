#pragma once

#include <Windows.h>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include <mutex>
#include <optional>
#include <functional>
#include <cstdint>

// ============================================================================
// ENUMERATIONS - No magic numbers
// ============================================================================

enum class ItemCategory : uint8_t {
    Weapon = 0,
    Knife = 1,
    Gloves = 2,
    Agent = 3,
    MusicKit = 4,
    Sticker = 5,
    Patch = 6,
    Graffiti = 7,
    Tool = 8,
    Unknown = 255
};

enum class LoadoutSlot : uint8_t {
    LoadoutSlot_Weapon_Primary = 0,
    LoadoutSlot_Weapon_Secondary = 1,
    LoadoutSlot_Weapon_Knife = 2,
    LoadoutSlot_Weapon_C4 = 3,
    LoadoutSlot_Clothing_Face = 4,
    LoadoutSlot_Clothing_Head = 5,
    LoadoutSlot_Clothing_Ears = 6,
    LoadoutSlot_Clothing_Hands = 7,
    LoadoutSlot_Clothing_Body = 8,
    LoadoutSlot_Clothing_Legs = 9,
    LoadoutSlot_Clothing_Feet = 10,
    LoadoutSlot_Music = 11,
    LoadoutSlot_Max = 56
};

enum class WearValue : uint8_t {
    FactoryNew = 0,
    MinimalWear = 1,
    FieldTested = 2,
    WellWorn = 3,
    BattleScarred = 4
};

enum class SkinAttrib : uint32_t {
    PaintKit = 6,
    Seed = 7,
    Wear = 8,
    StatTrak = 80,
    StatTrakType = 81,
    CustomName = 111
};

// ============================================================================
// DATA STRUCTURES
// ============================================================================

struct PaintKitInfo {
    int id = 0;
    std::string name;
    std::string description;
};

struct DumpedItem {
    int definition_index = 0;
    std::string name;
    std::string model_path;
    ItemCategory category = ItemCategory::Unknown;
    std::vector<PaintKitInfo> paint_kits;
};

struct SkinConfig {
    uint64_t item_id = 0;
    int definition_index = 0;
    int paint_kit_id = 0;
    int seed = 0;
    float wear = 0.0f;
    int stattrak = -1;
    std::string custom_name;
    std::string model_override;
    bool enabled = true;
};

struct GloveMaterialInfo {
    uint32_t material_handle = 0;
    uint32_t fallback_paint_kit = 0;
    float fallback_wear = 0.0f;
    int fallback_seed = 0;
};

// ============================================================================
// INVENTORY & GC INTERFACES (Runtime Resolved)
// ============================================================================

struct SOID_t {
    uint64_t id = 0;
    uint32_t type = 0;
    uint32_t pad = 0;
};

struct CGCClientSharedObjectTypeCache {
    int GetCount() { return *(int*)((uintptr_t)this + 0x8); }
    uint64_t GetID(int index) { return *(uint64_t*)((uintptr_t)this + 0x10 + (index * 8)); }
};

struct CCSPlayerInventory {
    SOID_t& GetOwner() { return *(SOID_t*)((uintptr_t)this + 0x10); }
    CGCClientSharedObjectTypeCache* GetTypeCache() { return *(CGCClientSharedObjectTypeCache**)((uintptr_t)this + 0x20); }
    void SOCreated(void* obj) { ((void(__thiscall*)(void*, void*))((uintptr_t)this + 0x0))(this, obj); }
    void SOUpdated(void* obj) { ((void(__thiscall*)(void*, void*))((uintptr_t)this + 0x8))(this, obj); }
    void SODestroyed(void* obj) { ((void(__thiscall*)(void*, void*))((uintptr_t)this + 0x10))(this, obj); }
    uint64_t GetItemInLoadout(int loadout_slot) {
        return ((uint64_t(__thiscall*)(void*, int))((uintptr_t)this + 0x40))(this, loadout_slot);
    }
};

struct CCSInventoryManager {
    CCSPlayerInventory* GetLocalInventory() {
        return ((CCSPlayerInventory * (__thiscall*)(void*))((uintptr_t)this + 0x228))(this);
    }
    void EquipItemInLoadout(int team, int slot, uint64_t item_id) {
        ((void(__thiscall*)(void*, int, int, uint64_t))((uintptr_t)this + 0x1A8))(this, team, slot, item_id);
    }
    void SetItemForLoadoutSlot(int team, int slot, uint64_t item_id) {
        ((void(__thiscall*)(void*, int, int, uint64_t))((uintptr_t)this + 0x1B0))(this, team, slot, item_id);
    }
};

struct CGCClientSystem {
    void* GetSOCache() { return (void*)((uintptr_t)this + 0x8); }
};

// ============================================================================
// ECON ITEM VIEW (Schema-based offsets)
// ============================================================================

struct CEconItemView {
    static CEconItemView* CreateInstance();
    static bool SetAttribute(CEconItemView* item, SkinAttrib attrib, float value);

    uint16_t& GetDefinitionIndex() { return *(uint16_t*)((uintptr_t)this + 0x1BA); }
    uint32_t& GetAccountID() { return *(uint32_t*)((uintptr_t)this + 0x1D8); }
    uint64_t& GetItemID() { return *(uint64_t*)((uintptr_t)this + 0x1C8); }
    uint64_t& GetItemIDHigh() { return *(uint64_t*)((uintptr_t)this + 0x1D0); }
    uint64_t& GetItemIDLow() { return *(uint64_t*)((uintptr_t)this + 0x1D4); }
    uint32_t& GetEntityQuality() { return *(uint32_t*)((uintptr_t)this + 0x1BC); }
    uint8_t& GetInitialized() { return *(uint8_t*)((uintptr_t)this + 0x1BD); }

    void InitItem(uint16_t def_index, uint64_t item_id, bool unknown) {
        using InitFn = void(__thiscall*)(CEconItemView*, uint16_t, uint64_t, bool);
        // TODO: RE REQUIRED - pattern scan for CEconItemView::InitItem
        // Pattern: 48 89 5C 24 ? 57 48 83 EC ? 48 8B F9 0F B7 DA 48 8B F2 E8
        static InitFn fn = nullptr;
        if (fn) fn(this, def_index, item_id, unknown);
    }
};

// ============================================================================
// SCHEMA PARSER (Runtime Item Extraction)
// ============================================================================

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
    static std::optional<SchemaClass_t> FindClass(std::string_view name);
    static uint32_t GetOffset(std::string_view class_name, std::string_view field_name);

private:
    static void* m_schema_system;
    static std::unordered_map<std::string, SchemaClass_t> m_classes;
    static std::shared_mutex m_mutex;
};

// ============================================================================
// PATTERN SCANNER
// ============================================================================

class PatternScanner {
public:
    static uintptr_t FindPattern(HMODULE module, const char* pattern);
    static uintptr_t FindPatternInRange(uintptr_t start, size_t length, const char* pattern);
    static uintptr_t ResolveRelativeAddress(uintptr_t address, size_t offset, size_t instr_length);
    static void CacheModule(HMODULE module, const std::string& name);
    static uintptr_t GetModuleBase(const std::string& name);

private:
    static std::unordered_map<std::string, HMODULE> m_modules;
    static std::unordered_map<std::string, uintptr_t> m_cache;
    static std::mutex m_mutex;
};

// ============================================================================
// MAIN SKINCHANGER CLASS
// ============================================================================

class SkinChanger {
public:
    // Lifecycle
    static bool Initialize();
    static void Shutdown();

    // Core Operations
    static bool ApplySkin(const SkinConfig& config);
    static bool RemoveSkin(uint64_t item_id);
    static void RemoveAllSkins();
    static bool EquipItem(int loadout_slot, uint64_t item_id);

    // Runtime Dumping
    static bool DumpItemSchema();
    static std::vector<DumpedItem> GetDumpedItems();
    static std::vector<DumpedItem> Search(std::string_view query);

    // Config
    static bool LoadConfig(const std::string& path = "");
    static bool SaveConfig(const std::string& path = "");

    // Frame Stage Hooks (call from hooks.cpp)
    static void OnFrameStageNotify(int stage);
    static void OnSetModel(void* entity, const char* model_path);
    static void OnPreFireEvent(void* event, const char* name);

    // ViewModel
    static void ForceViewModelUpdate();

    // State Query
    static bool IsInitialized() { return s_initialized; }
    static bool IsDumpComplete() { return s_dump_complete; }
    static std::string GetStatusMessage() { std::shared_lock lock(s_status_mutex); return s_status_message; }
    static int GetActiveSkinCount() { std::shared_lock lock(s_mutex); return (int)s_active_skins.size(); }

private:
    // Internal Helpers
    static uint64_t CreateEconItem(int def_index, int paint_kit, int seed, float wear, int stattrak, std::string_view custom_name);
    static void ApplyWeaponSkin(void* weapon_entity, const SkinConfig& config);
    static void ApplyGloveSkin(void* pawn_entity, const SkinConfig& config);
    static void ApplyAgentModel(void* pawn_entity, const std::string& model_path);
    static void InvalidateGloveMaterials(void* view_model);
    static void FixKnifeKillEvent(void* event, const char* original_name, int def_index);

    static SkinConfig* GetConfigForItem(uint64_t item_id);
    static int GetLoadoutSlotForDefIndex(int def_index);
    static ItemCategory CategorizeDefIndex(int def_index);

    // Glove material tracking
    static std::vector<GloveMaterialInfo> s_glove_materials;
    static void CacheGloveMaterials(void* view_model);

    // State
    static inline bool s_initialized = false;
    static inline bool s_dump_complete = false;
    static inline std::string s_status_message = "Not initialized";

    static inline std::shared_mutex s_mutex;
    static inline std::unordered_map<uint64_t, SkinConfig> s_active_skins;
    static inline std::vector<DumpedItem> s_dumped_items;
    static inline std::vector<SkinConfig> s_configs;

    static inline std::shared_mutex s_status_mutex;

    // Interface Pointers (Runtime Resolved)
    static inline CCSInventoryManager* s_inventory_manager = nullptr;
    static inline CGCClientSystem* s_gc_client_system = nullptr;
    static inline void* s_game_rules = nullptr;

    // Pattern Resolved Functions
    static inline uintptr_t s_create_econ_item_fn = 0;
    static inline uintptr_t s_set_attribute_fn = 0;
    static inline uintptr_t s_force_viewmodel_fn = 0;

    // Knife model mapping (definition_index -> model path)
    static inline std::unordered_map<int, const char*> s_knife_models = {
        {500, "models/weapons/v_knife_bayonet.mdl"},
        {503, "models/weapons/v_knife_flip.mdl"},
        {505, "models/weapons/v_knife_gut.mdl"},
        {506, "models/weapons/v_knife_karam.mdl"},
        {507, "models/weapons/v_knife_m9_bay.mdl"},
        {508, "models/weapons/v_knife_tactical.mdl"},
        {509, "models/weapons/v_knife_falchion_advanced.mdl"},
        {512, "models/weapons/v_knife_survival_bowie.mdl"},
        {514, "models/weapons/v_knife_butterfly.mdl"},
        {515, "models/weapons/v_knife_push.mdl"},
        {516, "models/weapons/v_knife_cord.mdl"},
        {517, "models/weapons/v_knife_canis.mdl"},
        {518, "models/weapons/v_knife_ursus.mdl"},
        {519, "models/weapons/v_knife_gypsy_jackknife.mdl"},
        {520, "models/weapons/v_knife_outdoor.mdl"},
        {521, "models/weapons/v_knife_stiletto.mdl"},
        {522, "models/weapons/v_knife_widowmaker.mdl"},
        {523, "models/weapons/v_knife_skeleton.mdl"},
        {525, "models/weapons/v_knife_kukri.mdl"},
        {526, "models/weapons/v_knife_css.mdl"},
        {527, "models/weapons/v_knife_ks.mdl"}
    };

    // Glove definition indices
    static inline std::vector<int> s_glove_def_indices = {
        4725, 4726, 4727, 4728, 4729, 4722, 4723, 4724,
        5027, 5028, 5029, 5030, 5031, 5032, 5033, 5034, 5035
    };

    // Agent model paths (populated at runtime via schema dump)
    static inline std::unordered_map<int, std::string> s_agent_models;

    // Music kit IDs (populated at runtime via schema dump)
    static inline std::vector<int> s_music_kit_ids;

    // Glove material magic identifiers for invalidation
    static constexpr uint32_t GLOVE_MATERIAL_MAGIC_1 = 0xF143B82A;
    static constexpr uint32_t GLOVE_MATERIAL_MAGIC_2 = 0x1B52829C;
    static constexpr uint32_t GLOVE_MATERIAL_MAGIC_3 = 0xA6EBE9B9;
    static constexpr uint32_t GLOVE_MATERIAL_MAGIC_4 = 0x423B2ED4;
    static constexpr uint32_t GLOVE_MATERIAL_MAGIC_5 = 0xC8D7255E;

    // ViewModel glove material offset
    static constexpr size_t VIEWMODEL_GLOVE_MATERIAL_OFFSET = 0xF80;

    // C_EconEntity schema offsets (from schema system or fallback)
    static constexpr size_t OFFSET_ECON_ENTITY_ATTRIBUTE_MANAGER = 0x1378;
    static constexpr size_t OFFSET_ECON_ENTITY_FALLBACK_PAINT_KIT = 0x1850;
    static constexpr size_t OFFSET_ECON_ENTITY_FALLBACK_SEED = 0x1854;
    static constexpr size_t OFFSET_ECON_ENTITY_FALLBACK_WEAR = 0x1858;
    static constexpr size_t OFFSET_ECON_ENTITY_FALLBACK_STATTRAK = 0x185C;
    static constexpr size_t OFFSET_ECON_ENTITY_ORIGINAL_TEAM = 0x19F8;
    static constexpr size_t OFFSET_ECON_ENTITY_PREV_OWNER = 0x1AB4;

    // C_CSPlayerPawn offsets
    static constexpr size_t OFFSET_PAWN_ECON_GLOVES = 0x1890;
    static constexpr size_t OFFSET_PAWN_NEED_REAPPLY_GLOVES = 0x188D;
    static constexpr size_t OFFSET_PAWN_LAST_SPAWN_TIME = 0x15D4;
};
