#include "skinchanger.hpp"
#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>   // Single external dependency (header-only)
#include "memory.h"             // M::FindPattern, M::CallVFunc, M::GetAbsoluteAddress
#include "interfaces.h"         // I::client, I::engine, I::localize, I::file_system
#include "entity.h"             // C_CSPlayerPawn, C_CSWeaponBase, C_BaseViewModel, etc.
#include "sdk/CEconItem.h"      // CEconItem, CEconItemDefinition, C_PaintKit (provided by base)

using json = nlohmann::json;
using namespace SkinChanger;

// ------------------------------------------------------------
// Internal state
// ------------------------------------------------------------
static std::vector<SkinConfig>        g_custom_skins;
static std::unordered_map<int, SkinConfig> g_equipped_skins; // def_index -> config
static std::vector<DumpedItem>        g_dumped_items;
static bool                            g_dumped = false;
static std::mutex                      g_mutex;
static int                             g_glove_update_frames = 0;
static float                           g_last_spawn_time = 0.0f;

// Knife model mapping (hardcoded, can be extended via dump)
static const std::unordered_map<int, const char*> knife_models = {
    {500, "models/weapons/v_knife_karam.mdl"},
    {505, "models/weapons/v_knife_flip.mdl"},
    {506, "models/weapons/v_knife_gut.mdl"},
    {507, "models/weapons/v_knife_karambit.mdl"},
    {508, "models/weapons/v_knife_m9_bay.mdl"},
    {509, "models/weapons/v_knife_tactical.mdl"},
    {512, "models/weapons/v_knife_falchion_advanced.mdl"},
    {514, "models/weapons/v_knife_survival_bowie.mdl"},
    {515, "models/weapons/v_knife_butterfly.mdl"},
    {516, "models/weapons/v_knife_push.mdl"},
    {517, "models/weapons/v_knife_cord.mdl"},
    {518, "models/weapons/v_knife_canis.mdl"},
    {519, "models/weapons/v_knife_ursus.mdl"},
    {520, "models/weapons/v_knife_gypsy_jackknife.mdl"},
    {521, "models/weapons/v_knife_outdoor.mdl"},
    {522, "models/weapons/v_knife_stiletto.mdl"},
    {523, "models/weapons/v_knife_widowmaker.mdl"},
    {525, "models/weapons/v_knife_skeleton.mdl"},
    {526, "models/weapons/v_knife_kukri.mdl"},
    {527, "models/weapons/v_knife_zeus.mdl"}
};

// ------------------------------------------------------------
// Inventory & Schema Helpers (inline to avoid extra files)
// ------------------------------------------------------------
class CCSInventoryManager {
public:
    static CCSInventoryManager* GetInstance() {
        static auto fn = reinterpret_cast<CCSInventoryManager*(__fastcall*)()>(
            M::FindPattern(L"client.dll", "48 8D 05 ? ? ? ? C3 CC CC CC CC CC CC CC CC 8B 91")
        );
        return fn ? fn() : nullptr;
    }
    bool EquipItemInLoadout(int team, int slot, uint64_t id) {
        return M::CallVFunc<bool, 54>(this, team, slot, id);
    }
    CCSPlayerInventory* GetLocalInventory() {
        return M::CallVFunc<CCSPlayerInventory*, 57>(this);
    }
};

class CCSPlayerInventory {
public:
    static CCSPlayerInventory* GetInstance() {
        auto mgr = CCSInventoryManager::GetInstance();
        return mgr ? mgr->GetLocalInventory() : nullptr;
    }
    SOID_t GetOwner() { return *reinterpret_cast<SOID_t*>(uintptr_t(this) + 0x10); }
    C_EconItemView* GetItemInLoadout(int team, int slot) {
        return M::CallVFunc<C_EconItemView*, 8>(this, team, slot);
    }
    bool AddEconItem(CEconItem* item);
    void RemoveEconItem(CEconItem* item);
    CEconItem* GetSOCDataForItem(uint64_t id);
    std::pair<uint64_t, uint32_t> GetHighestIDs();
};

// Implementation of inventory methods (as in provided code)
static CGCClientSystem* GetCGCClientSystem() {
    static auto fn = reinterpret_cast<CGCClientSystem*(__fastcall*)()>(
        M::ResolveRelativeAddress(M::FindPattern(L"client.dll", "E8 ? ? ? ? 48 8B 4F 10 8B 1D"), 1, 0)
    );
    return fn ? fn() : nullptr;
}

static CGCClientSharedObjectTypeCache* CreateBaseTypeCache(CCSPlayerInventory* inv) {
    auto sys = GetCGCClientSystem();
    if (!sys) return nullptr;
    auto gc = sys->GetCGCClient();
    if (!gc) return nullptr;
    auto cache = gc->FindSOCache(inv->GetOwner());
    return cache ? cache->CreateBaseTypeCache(1) : nullptr;
}

bool CCSPlayerInventory::AddEconItem(CEconItem* item) {
    if (!item) return false;
    auto typeCache = CreateBaseTypeCache(this);
    if (!typeCache || !typeCache->AddObject((CSharedObject*)item)) return false;
    M::CallVFunc<void, 0>(this, GetOwner(), (CSharedObject*)item, eSOCacheEvent_Incremental);
    return true;
}

void CCSPlayerInventory::RemoveEconItem(CEconItem* item) {
    if (!item) return;
    auto typeCache = CreateBaseTypeCache(this);
    if (!typeCache) return;
    const auto& vec = typeCache->GetVecObjects<CEconItem*>();
    if (!vec.Exists(item)) return;
    M::CallVFunc<void, 2>(this, GetOwner(), (CSharedObject*)item, eSOCacheEvent_Incremental);
    typeCache->RemoveObject((CSharedObject*)item);
    item->Destruct();
}

CEconItem* CCSPlayerInventory::GetSOCDataForItem(uint64_t id) {
    auto typeCache = CreateBaseTypeCache(this);
    if (!typeCache) return nullptr;
    for (auto it : typeCache->GetVecObjects<CEconItem*>())
        if (it && it->m_ulID == id) return it;
    return nullptr;
}

std::pair<uint64_t, uint32_t> CCSPlayerInventory::GetHighestIDs() {
    uint64_t maxItem = 0; uint32_t maxInv = 0;
    auto typeCache = CreateBaseTypeCache(this);
    if (typeCache) {
        for (auto it : typeCache->GetVecObjects<CEconItem*>()) {
            if (!it || (it->m_ulID & 0xF000000000000000)) continue;
            maxItem = std::max(maxItem, it->m_ulID);
            maxInv  = std::max(maxInv, it->m_unInventory);
        }
    }
    return {maxItem, maxInv};
}

// ------------------------------------------------------------
// Schema dumping (runtime extraction)
// ------------------------------------------------------------
static C_EconItemSchema* GetItemSchema() {
    auto itemSystem = I::iclient->get_econ_item_system();
    return itemSystem ? itemSystem->get_econ_item_schema() : nullptr;
}

static bool IsPaintKitForWeapon(C_PaintKit* kit, CEconItemDefinition* weapon) {
    std::string path = "panorama/images/econ/default_generated/";
    path += weapon->get_simple();
    path += "_";
    path += kit->m_name;
    path += "_light_png.vtex_c";
    return I::file_system->exists(path.c_str(), "GAME");
}

bool SkinChanger::DumpItemSchema() {
    std::lock_guard lock(g_mutex);
    auto schema = GetItemSchema();
    if (!schema) return false;

    g_dumped_items.clear();
    const auto& items = schema->GetSortedItemDefinitionMap();
    const auto& paintKits = schema->GetPaintKits();

    for (const auto& node : items) {
        auto* item = node.m_value;
        if (!item) continue;
        const char* baseName = item->m_pszItemBaseName;
        if (!baseName || !*baseName) continue;

        bool isWeapon = item->is_weapon();
        bool isKnife  = item->is_knife(true);
        bool isGlove  = item->is_glove(true);
        bool isAgent  = item->is_agent();
        if (!(isWeapon || isKnife || isGlove || isAgent)) continue;

        std::string locName = I::localize->find_key(baseName);
        if (locName.empty() || locName[0] == '#') continue;

        DumpedItem d;
        d.name = locName;
        d.def_index = item->m_nDefIndex;
        d.rarity = item->GetRarity();
        d.simple_name = item->get_simple() ? item->get_simple() : "";
        d.is_weapon = isWeapon;
        d.is_knife = isKnife;
        d.is_glove = isGlove;
        d.is_agent = isAgent;

        if (isAgent) {
            d.skins.push_back({"Vanilla", 0, (int)item->GetRarity(), false, ""});
        } else {
            for (const auto& kitNode : paintKits) {
                auto* kit = kitNode.m_value;
                if (!kit || kit->m_id == 0 || kit->m_id == 9001) continue;
                if (!IsPaintKitForWeapon(kit, item)) continue;
                std::string kitName = I::localize->find_key(kit->m_description_tag);
                if (kitName.empty() || kitName[0] == '#') continue;
                int rarity = (kit->m_rarity == 7) ? 7 :
                    std::clamp((int)item->GetRarity() + (int)kit->m_rarity - 1, 0, 6);
                d.skins.push_back({kitName, (int)kit->m_id, rarity, kit->legacy(), kit->m_name ? kit->m_name : ""});
            }
            // sort by rarity
            std::sort(d.skins.begin(), d.skins.end(),
                [](const DumpedSkin& a, const DumpedSkin& b) { return a.rarity > b.rarity; });
        }
        g_dumped_items.push_back(d);
    }
    g_dumped = true;
    return true;
}

const std::vector<DumpedItem>& SkinChanger::GetDumpedItems() { return g_dumped_items; }
bool SkinChanger::IsDumped() const { return g_dumped; }

// ------------------------------------------------------------
// CEconItem creation & attribute setting
// ------------------------------------------------------------
static CEconItem* CreateEconItem(const SkinConfig& cfg, CCSPlayerInventory* inv) {
    static auto fnCreate = reinterpret_cast<CEconItem*(__cdecl*)()>(
        M::FindPattern(L"client.dll", "48 83 EC 28 B9 48 00 00 00 E8 ? ? ? ? 48 85")
    );
    if (!fnCreate) return nullptr;
    auto* item = fnCreate();
    if (!item) return nullptr;

    auto [maxId, maxInv] = inv->GetHighestIDs();
    item->m_ulID = maxId + 1;
    item->m_unInventory = maxInv + 1;
    item->m_unAccountID = (uint32_t)inv->GetOwner().m_id;
    item->m_unDefIndex = (uint16_t)cfg.definition_index;

    // Set dynamic attributes (paintkit, seed, wear, stattrak)
    auto schema = GetItemSchema();
    auto setAttr = [&](int idx, void* val) {
        auto attrDef = schema->GetAttributeDefinitionInterface(idx);
        if (!attrDef) return;
        using fn_t = __int64(__fastcall*)(void*, void*, void*);
        static auto fnSet = reinterpret_cast<fn_t>(M::FindPattern(L"client.dll",
            "48 89 6C 24 ? 57 41 56 41 57 48 81 EC ? ? ? ? 48 8B FA C7 44 24"));
        if (fnSet) fnSet(item, attrDef, val);
    };
    float fPaint = (float)cfg.paint_kit, fSeed = (float)cfg.seed, fWear = cfg.wear;
    int iStatTrak = cfg.stat_trak;
    if (cfg.paint_kit > 0) {
        setAttr(6, &fPaint);
        setAttr(7, &fSeed);
        setAttr(8, &fWear);
    }
    if (cfg.stat_trak >= 0) {
        setAttr(80, &iStatTrak);
        int type = 0; setAttr(81, &type);
        if (item->m_nQuality != 9) item->m_nQuality = 9;
    }
    return item;
}

// ------------------------------------------------------------
// Apply / Remove API
// ------------------------------------------------------------
uint64_t SkinChanger::ApplySkin(const SkinConfig& cfg) {
    auto inv = CCSPlayerInventory::GetInstance();
    if (!inv) return 0;

    auto* item = CreateEconItem(cfg, inv);
    if (!item) return 0;

    if (!inv->AddEconItem(item)) {
        item->Destruct();
        return 0;
    }

    uint64_t id = item->m_ulID;
    {
        std::lock_guard lock(g_mutex);
        SkinConfig stored = cfg;
        stored.item_id = id;
        g_custom_skins.push_back(stored);
        g_equipped_skins[cfg.definition_index] = stored;
    }

    // Auto-equip if possible
    if (auto* local = C_CSPlayerPawn::GetLocalPawn()) {
        auto schema = GetItemSchema();
        if (schema) {
            auto* def = schema->GetItemDefinition(cfg.definition_index);
            if (def) EquipItem(local->team(), def->GetLoadoutSlot(), id);
        }
    }
    SaveConfig("skins.json");
    return id;
}

bool SkinChanger::RemoveSkin(uint64_t item_id) {
    auto inv = CCSPlayerInventory::GetInstance();
    if (!inv) return false;
    auto* item = inv->GetSOCDataForItem(item_id);
    if (!item) return false;
    inv->RemoveEconItem(item);

    std::lock_guard lock(g_mutex);
    auto it = std::find_if(g_custom_skins.begin(), g_custom_skins.end(),
        [=](const SkinConfig& c) { return c.item_id == item_id; });
    if (it != g_custom_skins.end()) {
        g_equipped_skins.erase(it->definition_index);
        g_custom_skins.erase(it);
    }
    SaveConfig("skins.json");
    return true;
}

void SkinChanger::RemoveAllSkins() {
    auto inv = CCSPlayerInventory::GetInstance();
    if (!inv) return;
    std::lock_guard lock(g_mutex);
    for (auto& cfg : g_custom_skins) {
        if (auto* item = inv->GetSOCDataForItem(cfg.item_id))
            inv->RemoveEconItem(item);
    }
    g_custom_skins.clear();
    g_equipped_skins.clear();
    SaveConfig("skins.json");
}

bool SkinChanger::EquipItem(int team, int slot, uint64_t item_id) {
    auto mgr = CCSInventoryManager::GetInstance();
    return mgr && mgr->EquipItemInLoadout(team, slot, item_id);
}

const SkinConfig* SkinChanger::GetConfigForDefinition(int def_index) const {
    auto it = g_equipped_skins.find(def_index);
    return it != g_equipped_skins.end() ? &it->second : nullptr;
}

// ------------------------------------------------------------
// Hooks: FrameStageNotify, SetModel, EquipItemInLoadout
// ------------------------------------------------------------
static void ApplyVisuals(C_CSWeaponBase* weapon, const SkinConfig& cfg) {
    auto& view = weapon->m_AttributeManager().m_Item();
    auto* def = view.get_static_data();
    if (!def) return;

    view.m_iItemDefinitionIndex() = cfg.definition_index;
    view.m_iItemID() = cfg.item_id;
    view.m_bDisallowSOCm() = false;

    if (auto* scene = weapon->GetGameSceneNode())
        scene->set_mesh_group_mask(1 + (cfg.legacy ? 1 : 0));

    if (def->is_knife(false)) {
        auto it = knife_models.find(cfg.definition_index);
        if (it != knife_models.end()) {
            weapon->set_model(it->second);
            if (auto* vm = C_CSPlayerPawn::GetLocalPawn()->GetViewModel())
                vm->set_model(it->second);
        }
    }
}

static void InvalidateGloveMaterial(C_BaseViewModel* vm) {
    struct MatRecord { uint32_t unk; uint32_t ident; uint32_t handle; uint32_t type; };
    struct MatInfo { MatRecord* rec; uint32_t cnt; };
    auto* info = reinterpret_cast<MatInfo*>(reinterpret_cast<uint8_t*>(vm) + 0xF80);
    for (uint32_t i = 0; i < info->cnt; ++i)
        if (info->rec[i].ident == 0xF143B82A) { info->rec[i].type = 0xFFFFFFFF; break; }
}

void SkinChanger::OnFrameStageNotify(int stage) {
    if (!enabled || stage != 6) return;
    auto* local = C_CSPlayerPawn::GetLocalPawn();
    if (!local || !local->IsAlive()) return;
    auto* inv = CCSPlayerInventory::GetInstance();
    if (!inv) return;

    uint64_t steam = inv->GetOwner().m_id;

    // Gloves
    auto* gloves = &local->m_EconGloves();
    auto* glovesLoadout = inv->GetItemInLoadout(local->team(), 41);
    if (gloves && glovesLoadout) {
        if (gloves->m_iItemID() != glovesLoadout->m_iItemID() || local->m_flLastSpawnTimeIndex() != g_last_spawn_time) {
            g_glove_update_frames = 3;
            gloves->m_iItemDefinitionIndex() = glovesLoadout->m_iItemDefinitionIndex();
            gloves->m_iItemID() = glovesLoadout->m_iItemID();
            gloves->m_iItemIDHigh() = glovesLoadout->m_iItemIDHigh();
            gloves->m_iItemIDLow() = glovesLoadout->m_iItemIDLow();
            gloves->m_iAccountID() = glovesLoadout->m_iAccountID();
            g_last_spawn_time = local->m_flLastSpawnTimeIndex();
        }
        if (g_glove_update_frames) {
            gloves->m_bInitialized() = true;
            local->m_bNeedToReApplyGloves() = true;
            if (auto* vm = local->GetViewModel()) InvalidateGloveMaterial(vm);
            --g_glove_update_frames;
        }
    }

    // Agent
    static bool agentDone = false;
    if (!agentDone) {
        if (auto* agentView = inv->GetItemInLoadout(local->team(), 38)) {
            if (auto* def = agentView->get_static_data())
                local->set_model(def->get_model_name());
        }
        agentDone = true;
    }

    // Weapons
    if (auto* weapon = local->GetActiveWeaponPlayer()) {
        if (weapon->GetOriginalOwnerXuid() == steam) {
            int defIdx = weapon->m_AttributeManager().m_Item().get_static_data()->m_nDefIndex;
            auto it = g_equipped_skins.find(defIdx);
            if (it != g_equipped_skins.end())
                ApplyVisuals(weapon, it->second);
        }
    }

    if (auto* vm = local->GetViewModel())
        ForceViewModelUpdate(vm);
}

void SkinChanger::OnSetModel(C_BaseViewModel* vm, const char*& model) {
    if (!vm) return;
    auto* weapon = I::GameResourceService->pGameEntitySystem->Get<C_CSWeaponBase>(vm->m_hWeapon());
    if (!weapon) return;
    auto* def = weapon->m_AttributeManager().m_Item().get_static_data();
    if (!def || !def->is_knife(false)) return;

    auto it = g_equipped_skins.find(def->m_nDefIndex);
    if (it == g_equipped_skins.end()) return;

    auto mit = knife_models.find(it->second.definition_index);
    if (mit != knife_models.end()) model = mit->second;
}

void SkinChanger::OnEquipItemInLoadout(int team, int slot, uint64_t id) {
    // Optional: save equipped state to JSON (can be extended)
}

void SkinChanger::OnRoundStart() {
    agentDone = false;
    g_glove_update_frames = 0;
}

void SkinChanger::ForceViewModelUpdate(C_BaseViewModel* vm) {
    if (!vm) {
        if (auto* local = C_CSPlayerPawn::GetLocalPawn())
            vm = local->GetViewModel();
    }
    if (vm) {
        static auto fnForce = reinterpret_cast<void(__thiscall*)(void*)>(
            M::FindPattern(L"client.dll", "E8 ? ? ? ? 8B 45 ? 85 C0 74 ? 8B 08")
        );
        if (fnForce) fnForce(vm);
        if (auto seq = vm->GetSequence(); seq != -1) vm->SetSequence(seq);
    }
}

// ------------------------------------------------------------
// Config load/save (JSON)
// ------------------------------------------------------------
bool SkinChanger::LoadConfig(const std::string& path) {
    std::ifstream f(path);
    if (!f) return false;
    try {
        json j; f >> j;
        std::lock_guard lock(g_mutex);
        g_custom_skins.clear();
        for (auto& item : j) {
            SkinConfig cfg;
            cfg.item_id = item.value("item_id", 0ULL);
            cfg.definition_index = item.value("def_index", 0);
            cfg.paint_kit = item.value("paint_kit", 0);
            cfg.seed = item.value("seed", 0);
            cfg.wear = item.value("wear", 0.0001f);
            cfg.stat_trak = item.value("stat_trak", -1);
            cfg.custom_name = item.value("name", "");
            cfg.legacy = item.value("legacy", false);
            g_custom_skins.push_back(cfg);
            g_equipped_skins[cfg.definition_index] = cfg;
        }
    } catch (...) { return false; }
    return true;
}

bool SkinChanger::SaveConfig(const std::string& path) {
    std::ofstream f(path);
    if (!f) return false;
    json arr = json::array();
    {
        std::lock_guard lock(g_mutex);
        for (auto& cfg : g_custom_skins) {
            arr.push_back({
                {"item_id", cfg.item_id},
                {"def_index", cfg.definition_index},
                {"paint_kit", cfg.paint_kit},
                {"seed", cfg.seed},
                {"wear", cfg.wear},
                {"stat_trak", cfg.stat_trak},
                {"name", cfg.custom_name},
                {"legacy", cfg.legacy}
            });
        }
    }
    f << arr.dump(4);
    return true;
}

bool SkinChanger::Initialize() {
    LoadConfig("skins.json");
    // Recreate items on startup (optional, depends on base lifecycle)
    return true;
}

void SkinChanger::Shutdown() {
    RemoveAllSkins();
}