#include "skinchanger.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>

// ============================================================================
// EXTERNAL DEPENDENCY STUBS
// TODO: Replace with actual base includes when integrating
// ============================================================================

// Stub for nlohmann::json - replace with #include "nlohmann/json.hpp" in production
namespace json_stub {
    struct json {
        json() = default;
        template<typename T>
        json(T) {}
        template<typename T>
        T get() const { return T{}; }
        template<typename T>
        T value(const std::string&, T def) const { return def; }
        bool contains(const std::string&) const { return false; }
        json& operator[](const std::string&) { return *this; }
        const json& operator[](const std::string&) const { return *this; }
        template<typename T>
        void push_back(T) {}
        static json parse(const std::string&) { return {}; }
        std::string dump(int = 2) const { return "{}"; }
    };
    inline json parse(std::ifstream&) { return {}; }
}

// Stub for memory utilities - replace with actual base M:: namespace
namespace M {
    inline uintptr_t ResolveRelativeAddress(uintptr_t address, size_t offset, size_t instr_length) {
        int32_t rel = *(int32_t*)(address + offset);
        return address + offset + instr_length + rel;
    }
}

// ============================================================================
// SCHEMA PARSER IMPLEMENTATION
// ============================================================================

void* SchemaParser::m_schema_system = nullptr;
std::unordered_map<std::string, SchemaClass_t> SchemaParser::m_classes;
std::shared_mutex SchemaParser::m_mutex;

bool SchemaParser::Initialize() {
    std::unique_lock lock(m_mutex);

    // TODO: RE REQUIRED - SchemaSystem_001 interface resolution
    // Pattern: 48 8D 05 ? ? ? ? C6 05 ? ? ? ? ? E8 (schemasystem.dll)
    // Or use interface name "SchemaSystem_001" from schemasystem.dll
    HMODULE hSchema = GetModuleHandleA("schemasystem.dll");
    if (!hSchema) {
        hSchema = LoadLibraryA("schemasystem.dll");
    }
    if (!hSchema) return false;

    // TODO: RE REQUIRED - GetSchemaSystemSingleton export or pattern
    using CreateInterfaceFn = void* (*)(const char*, int*);
    auto CreateInterface = (CreateInterfaceFn)GetProcAddress(hSchema, "CreateInterface");
    if (!CreateInterface) return false;

    m_schema_system = CreateInterface("SchemaSystem_001", nullptr);
    if (!m_schema_system) return false;

    // TODO: RE REQUIRED - Schema system internal traversal
    // The schema system stores class info in a linked list or hash table
    // Structure: SchemaSystem -> SchemaClasses (CUtlVector<SchemaClassInfo_t>)
    // Each SchemaClassInfo_t has: name, fields, static_fields, base_classes
    // This requires reversing the schema system internals

    return m_schema_system != nullptr;
}

std::vector<SchemaClass_t> SchemaParser::GetClasses() {
    std::shared_lock lock(m_mutex);
    std::vector<SchemaClass_t> result;
    result.reserve(m_classes.size());
    for (auto& [name, cls] : m_classes) {
        result.push_back(cls);
    }
    return result;
}

std::optional<SchemaClass_t> SchemaParser::FindClass(std::string_view name) {
    std::shared_lock lock(m_mutex);
    auto it = m_classes.find(std::string(name));
    if (it != m_classes.end()) {
        return it->second;
    }
    return std::nullopt;
}

uint32_t SchemaParser::GetOffset(std::string_view class_name, std::string_view field_name) {
    std::shared_lock lock(m_mutex);
    auto it = m_classes.find(std::string(class_name));
    if (it == m_classes.end()) return 0;

    for (auto& field : it->second.fields) {
        if (field.name == field_name) {
            return field.offset;
        }
    }
    return 0;
}

// ============================================================================
// PATTERN SCANNER IMPLEMENTATION
// ============================================================================

std::unordered_map<std::string, HMODULE> PatternScanner::m_modules;
std::unordered_map<std::string, uintptr_t> PatternScanner::m_cache;
std::mutex PatternScanner::m_mutex;

void PatternScanner::CacheModule(HMODULE module, const std::string& name) {
    std::lock_guard lock(m_mutex);
    m_modules[name] = module;
}

uintptr_t PatternScanner::GetModuleBase(const std::string& name) {
    std::lock_guard lock(m_mutex);
    auto it = m_modules.find(name);
    if (it != m_modules.end()) {
        return (uintptr_t)it->second;
    }
    HMODULE hMod = GetModuleHandleA(name.c_str());
    if (hMod) {
        m_modules[name] = hMod;
        return (uintptr_t)hMod;
    }
    return 0;
}

static bool PatternMatch(const uint8_t* data, const uint8_t* pattern, const char* mask) {
    for (; *mask; ++pattern, ++mask, ++data) {
        if (*mask == 'x' && *data != *pattern) {
            return false;
        }
    }
    return !*mask;
}

uintptr_t PatternScanner::FindPattern(HMODULE module, const char* pattern) {
    if (!module) return 0;

    std::string mask_str;
    std::vector<uint8_t> pattern_bytes;
    const char* p = pattern;

    while (*p) {
        if (*p == ' ') {
            ++p;
            continue;
        }
        if (*p == '?') {
            pattern_bytes.push_back(0);
            mask_str += '?';
            ++p;
        } else {
            uint8_t byte = 0;
            for (int i = 0; i < 2; ++i) {
                byte <<= 4;
                char c = *p++;
                if (c >= '0' && c <= '9') byte |= (c - '0');
                else if (c >= 'A' && c <= 'F') byte |= (c - 'A' + 10);
                else if (c >= 'a' && c <= 'f') byte |= (c - 'a' + 10);
            }
            pattern_bytes.push_back(byte);
            mask_str += 'x';
        }
    }

    MODULEINFO modInfo;
    if (!GetModuleInformation(GetCurrentProcess(), module, &modInfo, sizeof(modInfo))) {
        return 0;
    }

    uintptr_t start = (uintptr_t)modInfo.lpBaseOfDll;
    size_t size = modInfo.SizeOfImage;

    for (size_t i = 0; i < size - pattern_bytes.size(); ++i) {
        if (PatternMatch((const uint8_t*)(start + i), pattern_bytes.data(), mask_str.c_str())) {
            return start + i;
        }
    }
    return 0;
}

uintptr_t PatternScanner::FindPatternInRange(uintptr_t start, size_t length, const char* pattern) {
    std::string mask_str;
    std::vector<uint8_t> pattern_bytes;
    const char* p = pattern;

    while (*p) {
        if (*p == ' ') { ++p; continue; }
        if (*p == '?') {
            pattern_bytes.push_back(0);
            mask_str += '?';
            ++p;
        } else {
            uint8_t byte = 0;
            for (int i = 0; i < 2; ++i) {
                byte <<= 4;
                char c = *p++;
                if (c >= '0' && c <= '9') byte |= (c - '0');
                else if (c >= 'A' && c <= 'F') byte |= (c - 'A' + 10);
                else if (c >= 'a' && c <= 'f') byte |= (c - 'a' + 10);
            }
            pattern_bytes.push_back(byte);
            mask_str += 'x';
        }
    }

    for (size_t i = 0; i < length - pattern_bytes.size(); ++i) {
        if (PatternMatch((const uint8_t*)(start + i), pattern_bytes.data(), mask_str.c_str())) {
            return start + i;
        }
    }
    return 0;
}

uintptr_t PatternScanner::ResolveRelativeAddress(uintptr_t address, size_t offset, size_t instr_length) {
    return M::ResolveRelativeAddress(address, offset, instr_length);
}

// ============================================================================
// CEconItemView STATIC METHODS
// ============================================================================

CEconItemView* CEconItemView::CreateInstance() {
    // Pattern: 48 83 EC 28 B9 48 00 00 00 E8 ? ? ? ? 48 85
    static uintptr_t fn = 0;
    if (!fn) {
        HMODULE hClient = GetModuleHandleA("client.dll");
        if (!hClient) return nullptr;

        uintptr_t addr = PatternScanner::FindPattern(hClient,
            "48 83 EC 28 B9 48 00 00 00 E8 ? ? ? ? 48 85");
        if (!addr) return nullptr;

        // The E8 is the call instruction, resolve it
        fn = PatternScanner::ResolveRelativeAddress(addr + 8, 1, 5);
    }

    if (!fn) return nullptr;
    return ((CEconItemView * (*)()) fn)();
}

bool CEconItemView::SetAttribute(CEconItemView* item, SkinAttrib attrib, float value) {
    // Pattern: 48 89 6C 24 ? 57 41 56 41 57 48 81 EC ? ? ? ? 48 8B FA C7 44 24
    static uintptr_t fn = 0;
    if (!fn) {
        HMODULE hClient = GetModuleHandleA("client.dll");
        if (!hClient) return false;

        fn = PatternScanner::FindPattern(hClient,
            "48 89 6C 24 ? 57 41 56 41 57 48 81 EC ? ? ? ? 48 8B FA C7 44 24");
        if (!fn) return false;
    }

    using SetAttribFn = bool(__fastcall*)(CEconItemView*, uint32_t, float);
    return ((SetAttribFn)fn)(item, (uint32_t)attrib, value);
}

// ============================================================================
// SKINCHANGER STATIC INITIALIZATION
// ============================================================================

std::vector<GloveMaterialInfo> SkinChanger::s_glove_materials;
std::shared_mutex SkinChanger::s_mutex;
std::shared_mutex SkinChanger::s_status_mutex;
std::unordered_map<uint64_t, SkinConfig> SkinChanger::s_active_skins;
std::vector<DumpedItem> SkinChanger::s_dumped_items;
std::vector<SkinConfig> SkinChanger::s_configs;

bool SkinChanger::Initialize() {
    if (s_initialized) return true;

    {
        std::lock_guard lock(s_status_mutex);
        s_status_message = "Initializing...";
    }

    // Cache modules
    PatternScanner::CacheModule(GetModuleHandleA("client.dll"), "client.dll");
    PatternScanner::CacheModule(GetModuleHandleA("engine2.dll"), "engine2.dll");
    PatternScanner::CacheModule(GetModuleHandleA("schemasystem.dll"), "schemasystem.dll");
    PatternScanner::CacheModule(GetModuleHandleA("filesystem_stdio.dll"), "filesystem_stdio.dll");

    // Initialize schema parser
    SchemaParser::Initialize();

    // Resolve CCSInventoryManager
    // Pattern: 48 8D 05 ? ? ? ? C3 CC CC CC CC CC CC CC CC 8B 91
    {
        HMODULE hClient = GetModuleHandleA("client.dll");
        if (hClient) {
            uintptr_t addr = PatternScanner::FindPattern(hClient,
                "48 8D 05 ? ? ? ? C3 CC CC CC CC CC CC CC CC 8B 91");
            if (addr) {
                s_inventory_manager = (CCSInventoryManager*)PatternScanner::ResolveRelativeAddress(addr, 3, 7);
            }
        }
    }

    // Resolve CGCClientSystem
    // Pattern: E8 ? ? ? ? 48 8B 4F 10 8B 1D
    {
        HMODULE hClient = GetModuleHandleA("client.dll");
        if (hClient) {
            uintptr_t addr = PatternScanner::FindPattern(hClient,
                "E8 ? ? ? ? 48 8B 4F 10 8B 1D");
            if (addr) {
                s_gc_client_system = (CGCClientSystem*)PatternScanner::ResolveRelativeAddress(addr, 1, 5);
            }
        }
    }

    // Resolve CEconItem creation function
    // Pattern: 48 83 EC 28 B9 48 00 00 00 E8 ? ? ? ? 48 85
    {
        HMODULE hClient = GetModuleHandleA("client.dll");
        if (hClient) {
            uintptr_t addr = PatternScanner::FindPattern(hClient,
                "48 83 EC 28 B9 48 00 00 00 E8 ? ? ? ? 48 85");
            if (addr) {
                s_create_econ_item_fn = PatternScanner::ResolveRelativeAddress(addr + 8, 1, 5);
            }
        }
    }

    // Resolve SetAttribute function
    // Pattern: 48 89 6C 24 ? 57 41 56 41 57 48 81 EC ? ? ? ? 48 8B FA C7 44 24
    {
        HMODULE hClient = GetModuleHandleA("client.dll");
        if (hClient) {
            s_set_attribute_fn = PatternScanner::FindPattern(hClient,
                "48 89 6C 24 ? 57 41 56 41 57 48 81 EC ? ? ? ? 48 8B FA C7 44 24");
        }
    }

    // Resolve ForceViewModelUpdate pattern
    // Pattern: E8 ? ? ? ? 8B 45 ? 85 C0 74 ? 8B 08
    {
        HMODULE hClient = GetModuleHandleA("client.dll");
        if (hClient) {
            uintptr_t addr = PatternScanner::FindPattern(hClient,
                "E8 ? ? ? ? 8B 45 ? 85 C0 74 ? 8B 08");
            if (addr) {
                s_force_viewmodel_fn = PatternScanner::ResolveRelativeAddress(addr, 1, 5);
            }
        }
    }

    // Start async schema dump
    std::thread([this]() {
        DumpItemSchema();
    }).detach();

    s_initialized = true;

    {
        std::lock_guard lock(s_status_mutex);
        s_status_message = "Initialized, dumping schema...";
    }

    return true;
}

void SkinChanger::Shutdown() {
    std::unique_lock lock(s_mutex);
    s_active_skins.clear();
    s_configs.clear();
    s_dumped_items.clear();
    s_glove_materials.clear();
    s_initialized = false;
    s_dump_complete = false;

    {
        std::lock_guard lock2(s_status_mutex);
        s_status_message = "Shutdown";
    }
}

// ============================================================================
// RUNTIME SCHEMA DUMPING
// ============================================================================

bool SkinChanger::DumpItemSchema() {
    if (!s_initialized) return false;

    {
        std::lock_guard lock(s_status_mutex);
        s_status_message = "Dumping item schema...";
    }

    // TODO: RE REQUIRED - Item schema traversal at runtime
    // CS2 stores item definitions in a global schema that can be accessed via:
    // 1. CCStrike15ItemSchema singleton (client.dll)
    // 2. SchemaSystem_001 interface (schemasystem.dll)
    // 3. items_game.txt parsed at runtime (VFileSystem017)
    //
    // The most reliable method is to parse items_game.txt via VFileSystem
    // Pattern for VFileSystem: interface name "VFileSystem017" from filesystem_stdio.dll
    //
    // Alternative: Use the schema system to enumerate all weapon definitions
    // Schema classes: CWeaponCSBase, CKnife, CEconEntity, etc.

    HMODULE hClient = GetModuleHandleA("client.dll");
    if (!hClient) {
        std::lock_guard lock(s_status_mutex);
        s_status_message = "Schema dump failed: client.dll not found";
        return false;
    }

    // Attempt schema-based extraction
    auto classes = SchemaParser::GetClasses();

    // Fallback: Use known definition indices and populate from memory
    // This is a minimal working set - the full list should come from items_game.txt

    std::unique_lock lock(s_mutex);

    // Weapons (definition indices 1-64 range)
    const struct { int def; const char* name; const char* model; } weapon_data[] = {
        {1, "Desert Eagle", "models/weapons/w_pist_deagle.mdl"},
        {2, "Dual Berettas", "models/weapons/w_pist_elite.mdl"},
        {3, "Five-SeveN", "models/weapons/w_pist_fiveseven.mdl"},
        {4, "Glock-18", "models/weapons/w_pist_glock18.mdl"},
        {7, "AK-47", "models/weapons/w_rif_ak47.mdl"},
        {8, "AUG", "models/weapons/w_rif_aug.mdl"},
        {9, "AWP", "models/weapons/w_snip_awp.mdl"},
        {10, "FAMAS", "models/weapons/w_rif_famas.mdl"},
        {11, "G3SG1", "models/weapons/w_snip_g3sg1.mdl"},
        {13, "Galil AR", "models/weapons/w_rif_galilar.mdl"},
        {14, "M249", "models/weapons/w_mach_m249para.mdl"},
        {16, "M4A4", "models/weapons/w_rif_m4a1.mdl"},
        {17, "MAC-10", "models/weapons/w_smg_mac10.mdl"},
        {19, "P90", "models/weapons/w_smg_p90.mdl"},
        {23, "MP5-SD", "models/weapons/w_smg_mp5sd.mdl"},
        {24, "UMP-45", "models/weapons/w_smg_ump45.mdl"},
        {25, "XM1014", "models/weapons/w_shot_xm1014.mdl"},
        {26, "PP-Bizon", "models/weapons/w_smg_bizon.mdl"},
        {27, "MAG-7", "models/weapons/w_shot_mag7.mdl"},
        {28, "Negev", "models/weapons/w_mach_negev.mdl"},
        {29, "Sawed-Off", "models/weapons/w_shot_sawedoff.mdl"},
        {30, "Tec-9", "models/weapons/w_pist_tec9.mdl"},
        {31, "Zeus x27", "models/weapons/w_eq_taser.mdl"},
        {32, "P2000", "models/weapons/w_pist_hkp2000.mdl"},
        {33, "MP7", "models/weapons/w_smg_mp7.mdl"},
        {34, "MP9", "models/weapons/w_smg_mp9.mdl"},
        {35, "Nova", "models/weapons/w_shot_nova.mdl"},
        {36, "P250", "models/weapons/w_pist_p250.mdl"},
        {38, "Scar-20", "models/weapons/w_snip_scar20.mdl"},
        {39, "SG 553", "models/weapons/w_rif_sg556.mdl"},
        {40, "SSG 08", "models/weapons/w_snip_ssg08.mdl"},
        {60, "M4A1-S", "models/weapons/w_rif_m4a1_s.mdl"},
        {61, "USP-S", "models/weapons/w_pist_usp_silencer.mdl"},
        {63, "CZ75-Auto", "models/weapons/w_pist_cz_75.mdl"},
        {64, "R8 Revolver", "models/weapons/w_pist_revolver.mdl"}
    };

    s_dumped_items.clear();

    for (auto& w : weapon_data) {
        DumpedItem item;
        item.definition_index = w.def;
        item.name = w.name;
        item.model_path = w.model;
        item.category = ItemCategory::Weapon;
        s_dumped_items.push_back(item);
    }

    // Knives
    for (auto& [def, model] : s_knife_models) {
        DumpedItem item;
        item.definition_index = def;
        item.model_path = model;

        // Extract name from model path
        std::string name = model;
        size_t start = name.find_last_of('/');
        if (start != std::string::npos) {
            name = name.substr(start + 1);
            size_t dot = name.find(".mdl");
            if (dot != std::string::npos) name = name.substr(0, dot);
            // Convert v_knife_xxx to readable name
            if (name.find("v_knife_") == 0) {
                name = name.substr(8);
                for (auto& c : name) if (c == '_') c = ' ';
                name[0] = std::toupper(name[0]);
            }
        }
        item.name = name;
        item.category = ItemCategory::Knife;
        s_dumped_items.push_back(item);
    }

    // Gloves
    for (int def : s_glove_def_indices) {
        DumpedItem item;
        item.definition_index = def;
        item.name = "Sport Gloves";
        item.category = ItemCategory::Gloves;
        s_dumped_items.push_back(item);
    }

    // TODO: RE REQUIRED - Runtime paint kit extraction
    // Paint kits are stored in the schema under CPaintKit class
    // Can be extracted from:
    // 1. items_game.txt paint_kits section
    // 2. SchemaSystem CPaintKit class enumeration
    // 3. C_EconItemView attribute definitions
    //
    // For now, populate with common paint kits
    const struct { int id; const char* name; } paint_data[] = {
        {0, "Stock"},
        {2, "Candy Apple"},
        {3, "Blue Steel"},
        {5, "Well-Worn"},
        {8, "Anodized Navy"},
        {28, "Night"},
        {30, "Ultraviolet"},
        {32, "Fade"},
        {37, "Safari Mesh"},
        {38, "Boreal Forest"},
        {39, "Forest DDPAT"},
        {40, "Urban Masked"},
        {42, "Sand Dune"},
        {43, "Tiger Tooth"},
        {44, "Damascus Steel"},
        {46, "Rust Coat"},
        {47, "Doppler"},
        {48, "Marble Fade"},
        {59, "Crimson Web"},
        {60, "Slaughter"},
        {61, "Case Hardened"},
        {67, "Stained"},
        {70, "Scorched"},
        {143, "Hypnotic"},
        {180, "Water Elemental"},
        {222, "Serum"},
        {223, "Bloodsport"},
        {226, "Asiimov"},
        {235, "Hyper Beast"},
        {246, "Neon Rider"},
        {300, "Printstream"},
        {340, "Neo-Noir"},
        {380, "The Prince"},
        {416, "Phantom Disruptor"},
        {456, "Welcome to the Jungle"},
        {515, "Kill Confirmed"},
        {600, "In Living Color"},
        {694, "Wasteland Princess"},
        {707, "Gold Arabesque"},
        {800, "X-Ray"},
        {900, "Cyrex"},
        {1000, "Vulcan"}
    };

    // Assign paint kits to all dumped items
    for (auto& item : s_dumped_items) {
        for (auto& p : paint_data) {
            item.paint_kits.push_back({p.id, p.name, ""});
        }
    }

    s_dump_complete = true;

    {
        std::lock_guard lock(s_status_mutex);
        s_status_message = "Schema dump complete: " + std::to_string(s_dumped_items.size()) + " items";
    }

    return true;
}

std::vector<DumpedItem> SkinChanger::GetDumpedItems() {
    std::shared_lock lock(s_mutex);
    return s_dumped_items;
}

std::vector<DumpedItem> SkinChanger::Search(std::string_view query) {
    std::shared_lock lock(s_mutex);

    if (query.empty()) {
        return s_dumped_items;
    }

    std::string lower_query;
    lower_query.reserve(query.size());
    for (char c : query) {
        lower_query += std::tolower(static_cast<unsigned char>(c));
    }

    std::vector<DumpedItem> results;
    for (const auto& item : s_dumped_items) {
        std::string lower_name = item.name;
        std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(),
            [](unsigned char c) { return std::tolower(c); });

        if (lower_name.find(lower_query) != std::string::npos) {
            results.push_back(item);
        }
    }

    return results;
}

// ============================================================================
// SKIN APPLICATION
// ============================================================================

uint64_t SkinChanger::CreateEconItem(int def_index, int paint_kit, int seed, float wear, int stattrak, std::string_view custom_name) {
    CEconItemView* item = CEconItemView::CreateInstance();
    if (!item) return 0;

    // Generate unique item ID
    uint64_t item_id = static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    item_id |= (static_cast<uint64_t>(def_index) << 48);

    item->GetDefinitionIndex() = static_cast<uint16_t>(def_index);
    item->GetItemID() = item_id;
    item->GetItemIDHigh() = static_cast<uint64_t>(item_id >> 32);
    item->GetItemIDLow() = static_cast<uint64_t>(item_id & 0xFFFFFFFF);
    item->GetEntityQuality() = 4; // Custom quality
    item->GetInitialized() = 1;

    // Set paint kit attribute
    CEconItemView::SetAttribute(item, SkinAttrib::PaintKit, static_cast<float>(paint_kit));

    // Set seed attribute
    CEconItemView::SetAttribute(item, SkinAttrib::Seed, static_cast<float>(seed));

    // Set wear attribute
    CEconItemView::SetAttribute(item, SkinAttrib::Wear, wear);

    // Set StatTrak if enabled
    if (stattrak >= 0) {
        CEconItemView::SetAttribute(item, SkinAttrib::StatTrak, static_cast<float>(stattrak));
        CEconItemView::SetAttribute(item, SkinAttrib::StatTrakType, 1.0f); // Kill counter type
    }

    // TODO: RE REQUIRED - Custom name attribute
    // Custom name requires setting a string attribute (attrib 111)
    // This may need a different SetAttribute overload that accepts const char*

    return item_id;
}

bool SkinChanger::ApplySkin(const SkinConfig& config) {
    if (!s_initialized) return false;
    if (!s_inventory_manager) return false;

    uint64_t item_id = CreateEconItem(
        config.definition_index,
        config.paint_kit_id,
        config.seed,
        config.wear,
        config.stattrak,
        config.custom_name
    );

    if (item_id == 0) return false;

    std::unique_lock lock(s_mutex);
    s_active_skins[item_id] = config;

    // Equip to loadout
    int slot = GetLoadoutSlotForDefIndex(config.definition_index);
    if (slot >= 0) {
        lock.unlock();
        s_inventory_manager->EquipItemInLoadout(2, slot, item_id); // Team 2 = T, will be overridden
    }

    return true;
}

bool SkinChanger::RemoveSkin(uint64_t item_id) {
    std::unique_lock lock(s_mutex);
    auto it = s_active_skins.find(item_id);
    if (it == s_active_skins.end()) return false;

    s_active_skins.erase(it);

    // TODO: RE REQUIRED - Proper inventory item removal
    // This requires calling into the GC to destroy the item
    // Pattern: look for SODestroyed or inventory removal functions
    // For now, just remove from active skins map

    return true;
}

void SkinChanger::RemoveAllSkins() {
    std::unique_lock lock(s_mutex);
    s_active_skins.clear();
}

bool SkinChanger::EquipItem(int loadout_slot, uint64_t item_id) {
    if (!s_inventory_manager) return false;

    std::shared_lock lock(s_mutex);
    auto it = s_active_skins.find(item_id);
    if (it == s_active_skins.end()) return false;

    s_inventory_manager->EquipItemInLoadout(2, loadout_slot, item_id);
    return true;
}

// ============================================================================
// FRAME STAGE HOOKS
// ============================================================================

void SkinChanger::OnFrameStageNotify(int stage) {
    // stage 6 = FRAME_RENDER_START (just before rendering)
    if (stage != 6) return;
    if (!s_initialized) return;

    std::shared_lock lock(s_mutex);
    if (s_active_skins.empty()) return;

    // TODO: RE REQUIRED - Entity iteration
    // Use SDK::EntityList or game entity system to iterate all entities
    // Pattern: 48 8B 1D ? ? ? ? 48 85 DB 74 ? 48 8B CB (EntityList)
    // Highest entity index offset: +0x20A0 from GameEntitySystem

    // For each active skin, find matching weapon entity and apply visual override
    for (auto& [item_id, config] : s_active_skins) {
        if (!config.enabled) continue;

        // TODO: Implement weapon skin application via entity traversal
        // 1. Find weapon entity owned by local player
        // 2. Set m_nFallbackPaintKit, m_flFallbackWear, m_nFallbackSeed, m_nFallbackStatTrak
        // 3. Force model update if knife/glove

        // Schema offsets for C_EconEntity:
        // m_nFallbackPaintKit = 0x1850
        // m_nFallbackSeed = 0x1854
        // m_flFallbackWear = 0x1858
        // m_nFallbackStatTrak = 0x185C
    }
}

void SkinChanger::OnSetModel(void* entity, const char* model_path) {
    if (!entity || !model_path) return;

    std::shared_lock lock(s_mutex);

    // Check if this is a knife model that needs override
    for (auto& [def, model] : s_knife_models) {
        // TODO: RE REQUIRED - Match entity to active skin config
        // Compare current model path with knife models and override if needed
        // This prevents knife flicker on spawn/pickup
    }
}

void SkinChanger::OnPreFireEvent(void* event, const char* name) {
    if (!event || !name) return;

    // TODO: RE REQUIRED - Kill event weapon name fix
    // When a knife kill occurs, the game may report wrong weapon name in kill feed
    // Hook the fire event and replace weapon name with correct knife name
    // Pattern: look for CGameEvent::Fire or IGameEventManager2::FireEvent
}

void SkinChanger::ApplyWeaponSkin(void* weapon_entity, const SkinConfig& config) {
    if (!weapon_entity) return;

    // Apply fallback values directly to entity
    // These are read by the game when rendering the weapon

    // m_nFallbackPaintKit
    *(int*)((uintptr_t)weapon_entity + OFFSET_ECON_ENTITY_FALLBACK_PAINT_KIT) = config.paint_kit_id;

    // m_nFallbackSeed
    *(int*)((uintptr_t)weapon_entity + OFFSET_ECON_ENTITY_FALLBACK_SEED) = config.seed;

    // m_flFallbackWear
    *(float*)((uintptr_t)weapon_entity + OFFSET_ECON_ENTITY_FALLBACK_WEAR) = config.wear;

    // m_nFallbackStatTrak
    if (config.stattrak >= 0) {
        *(int*)((uintptr_t)weapon_entity + OFFSET_ECON_ENTITY_FALLBACK_STATTRAK) = config.stattrak;
    }

    // Force the game to re-evaluate the weapon's appearance
    // TODO: RE REQUIRED - Force update function
    // Pattern: look for UpdateFunction or OnDataChanged on weapon entities
}

void SkinChanger::ApplyGloveSkin(void* pawn_entity, const SkinConfig& config) {
    if (!pawn_entity) return;

    // Get gloves econ entity from pawn
    void* gloves = (void*)((uintptr_t)pawn_entity + OFFSET_PAWN_ECON_GLOVES);
    if (!gloves) return;

    // Apply glove skin
    *(int*)((uintptr_t)gloves + OFFSET_ECON_ENTITY_FALLBACK_PAINT_KIT) = config.paint_kit_id;
    *(int*)((uintptr_t)gloves + OFFSET_ECON_ENTITY_FALLBACK_SEED) = config.seed;
    *(float*)((uintptr_t)gloves + OFFSET_ECON_ENTITY_FALLBACK_WEAR) = config.wear;

    if (config.stattrak >= 0) {
        *(int*)((uintptr_t)gloves + OFFSET_ECON_ENTITY_FALLBACK_STATTRAK) = config.stattrak;
    }

    // Flag for reapplication
    *(bool*)((uintptr_t)pawn_entity + OFFSET_PAWN_NEED_REAPPLY_GLOVES) = true;
}

void SkinChanger::ApplyAgentModel(void* pawn_entity, const std::string& model_path) {
    if (!pawn_entity || model_path.empty()) return;

    // TODO: RE REQUIRED - Agent model changing
    // Requires hooking OnFrameStageNotify at stage FRAME_START (before model setup)
    // Then calling SetModel on the pawn entity with the agent model path
    // Agent models are stored in: models/player/custom_player/legacy/<agent>.mdl
}

void SkinChanger::InvalidateGloveMaterials(void* view_model) {
    if (!view_model) return;

    // Get glove material handle from viewmodel
    uint32_t& material_handle = *(uint32_t*)((uintptr_t)view_model + VIEWMODEL_GLOVE_MATERIAL_OFFSET);

    // Invalidate by setting to known magic values
    // This forces the game to reload glove materials with new paint kit
    const uint32_t magic_values[] = {
        GLOVE_MATERIAL_MAGIC_1,
        GLOVE_MATERIAL_MAGIC_2,
        GLOVE_MATERIAL_MAGIC_3,
        GLOVE_MATERIAL_MAGIC_4,
        GLOVE_MATERIAL_MAGIC_5
    };

    for (uint32_t magic : magic_values) {
        if (material_handle == magic) {
            material_handle = 0; // Invalidate
            break;
        }
    }
}

void SkinChanger::CacheGloveMaterials(void* view_model) {
    if (!view_model) return;

    s_glove_materials.clear();

    GloveMaterialInfo info;
    info.material_handle = *(uint32_t*)((uintptr_t)view_model + VIEWMODEL_GLOVE_MATERIAL_OFFSET);
    s_glove_materials.push_back(info);
}

void SkinChanger::FixKnifeKillEvent(void* event, const char* original_name, int def_index) {
    if (!event || !original_name) return;

    // TODO: RE REQUIRED - Kill event modification
    // When a knife kill happens, intercept the kill event and modify:
    // 1. Weapon name to match the actual knife being used
    // 2. Item definition index for proper kill icon
    // This requires hooking IGameEventManager2::FireEvent or CGameEvent::Fire
}

// ============================================================================
// VIEWMODEL UPDATE
// ============================================================================

void SkinChanger::ForceViewModelUpdate() {
    if (!s_force_viewmodel_fn) return;

    // Pattern: E8 ? ? ? ? 8B 45 ? 85 C0 74 ? 8B 08
    // This function forces the viewmodel to refresh, applying skin changes immediately
    ((void(*)()) s_force_viewmodel_fn)();
}

// ============================================================================
// CONFIG MANAGEMENT
// ============================================================================

bool SkinChanger::LoadConfig(const std::string& path) {
    std::string config_path = path;
    if (config_path.empty()) {
        // Default path: executable directory + /skinchanger_config.json
        char exe_path[MAX_PATH];
        GetModuleFileNameA(nullptr, exe_path, MAX_PATH);
        std::string dir = exe_path;
        size_t last_sep = dir.find_last_of("\\/");
        if (last_sep != std::string::npos) {
            dir = dir.substr(0, last_sep);
        }
        config_path = dir + "\\skinchanger_config.json";
    }

    std::ifstream file(config_path);
    if (!file.is_open()) {
        return false;
    }

    try {
        // TODO: Replace json_stub with actual nlohmann::json
        // auto j = nlohmann::json::parse(file);
        json_stub::json j = json_stub::parse(file);

        std::unique_lock lock(s_mutex);
        s_configs.clear();

        if (j.contains("skins") && j["skins"].is_array()) {
            for (auto& skin_entry : j["skins"]) {
                SkinConfig config;
                config.definition_index = skin_entry.value("definition_index", 0);
                config.paint_kit_id = skin_entry.value("paint_kit_id", 0);
                config.seed = skin_entry.value("seed", 0);
                config.wear = skin_entry.value("wear", 0.0f);
                config.stattrak = skin_entry.value("stattrak", -1);
                config.custom_name = skin_entry.value("custom_name", "");
                config.model_override = skin_entry.value("model_override", "");
                config.enabled = skin_entry.value("enabled", true);
                s_configs.push_back(config);
            }
        }
    } catch (...) {
        return false;
    }

    return true;
}

bool SkinChanger::SaveConfig(const std::string& path) {
    std::string config_path = path;
    if (config_path.empty()) {
        char exe_path[MAX_PATH];
        GetModuleFileNameA(nullptr, exe_path, MAX_PATH);
        std::string dir = exe_path;
        size_t last_sep = dir.find_last_of("\\/");
        if (last_sep != std::string::npos) {
            dir = dir.substr(0, last_sep);
        }
        config_path = dir + "\\skinchanger_config.json";
    }

    try {
        // TODO: Replace json_stub with actual nlohmann::json
        json_stub::json j;
        json_stub::json skins_array;

        std::shared_lock lock(s_mutex);
        for (const auto& config : s_configs) {
            json_stub::json skin_entry;
            skin_entry["definition_index"] = config.definition_index;
            skin_entry["paint_kit_id"] = config.paint_kit_id;
            skin_entry["seed"] = config.seed;
            skin_entry["wear"] = config.wear;
            skin_entry["stattrak"] = config.stattrak;
            skin_entry["custom_name"] = config.custom_name;
            skin_entry["model_override"] = config.model_override;
            skin_entry["enabled"] = config.enabled;
            skins_array.push_back(skin_entry);
        }

        j["skins"] = skins_array;

        std::ofstream file(config_path);
        if (!file.is_open()) return false;
        file << j.dump(2);
    } catch (...) {
        return false;
    }

    return true;
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

SkinConfig* SkinChanger::GetConfigForItem(uint64_t item_id) {
    std::shared_lock lock(s_mutex);
    auto it = s_active_skins.find(item_id);
    if (it != s_active_skins.end()) {
        return &it->second;
    }
    return nullptr;
}

int SkinChanger::GetLoadoutSlotForDefIndex(int def_index) {
    // Knives
    if (s_knife_models.count(def_index)) {
        return static_cast<int>(LoadoutSlot::LoadoutSlot_Weapon_Knife);
    }

    // Check if it's a primary weapon
    const int primary_weapons[] = {7, 8, 9, 10, 11, 13, 14, 16, 19, 23, 24, 25, 26, 28, 29, 33, 34, 35, 38, 39, 40, 60};
    for (int pw : primary_weapons) {
        if (def_index == pw) return static_cast<int>(LoadoutSlot::LoadoutSlot_Weapon_Primary);
    }

    // Check if it's a secondary weapon
    const int secondary_weapons[] = {1, 2, 3, 4, 30, 32, 36, 61, 63, 64};
    for (int sw : secondary_weapons) {
        if (def_index == sw) return static_cast<int>(LoadoutSlot::LoadoutSlot_Weapon_Secondary);
    }

    // Gloves
    for (int g : s_glove_def_indices) {
        if (def_index == g) return static_cast<int>(LoadoutSlot::LoadoutSlot_Clothing_Hands);
    }

    return -1;
}

ItemCategory SkinChanger::CategorizeDefIndex(int def_index) {
    if (s_knife_models.count(def_index)) return ItemCategory::Knife;

    for (int g : s_glove_def_indices) {
        if (def_index == g) return ItemCategory::Gloves;
    }

    // Agent definition indices are typically in the 5000+ range
    if (def_index >= 5000) return ItemCategory::Agent;

    return ItemCategory::Weapon;
}
