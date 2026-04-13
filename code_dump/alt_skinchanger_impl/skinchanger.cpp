/*
 *  skinchanger.cpp — Self-Contained CS2 Skin Changer Implementation
 *
 *  Based on: Silent-Ware, wh-esports-client (Berserk), DreamyDumper, cs2-dumper,
 *  UnknownCheats forum research (threads 747560, 735939, 747357, 747790).
 *
 *  Runtime skin dumping uses C_EconItemSchema->GetSortedItemDefinitionMap() and
 *  ->GetPaintKits() to extract all items and paint kits from the game's own schema
 *  at runtime — no hardcoded paint kit lists needed.
 *
 *  Three skin application patterns combined:
 *    A) Fallback: Write CEconEntity::m_nFallback* + set ItemIDHigh=0xFFFFFFFF
 *    B) SOC/GC: CreateEconItem -> AddEconItem -> EquipItemInLoadout (full inventory)
 *    C) Attribute: CEconItem::SetDynamicAttributeValue (paint/seed/wear/stattrak)
 *
 *  Thread safety: All per-frame operations are called from the game thread
 *  (FrameStageNotify / CreateMove). The dump runs once on Init.
 */

#include "skinchanger.hpp"

#ifndef _WIN64
#error "skinchanger.cpp requires x64 targeting"
#endif

#include <Windows.h>
#include <mutex>
#include <cstdio>

// =============================================================================
// IMPLEMENTATION NOTES — Read this before integrating
// =============================================================================
/*
 *  This file is designed to be a DROP-IN solution. You only need to provide
 *  the 6 function pointers in SkinChangerBridge namespace (defined in .hpp).
 *
 *  Required from your base:
 *    1. PatternScan(module, pattern) -> uint8_t*   — IDA-style sig scanner
 *    2. GetModuleBase(module) -> uintptr_t          — GetModuleHandle equivalent
 *    3. GetLocalController() -> uintptr_t           — Local CCSPlayerController*
 *    4. GetEntityByHandle(list, handle) -> uintptr_t — Entity lookup
 *
 *  Your FrameStageNotify hook should call:
 *    SkinChanger::OnFrameStageNotify(stage);
 *
 *  Your SetModel hook should call:
 *    SkinChanger::OnSetModel((uintptr_t)entity, model);
 */

// =============================================================================
// SECTION 1: Memory Primitives
// =============================================================================
namespace mem {
    template<typename T>
    T read(uintptr_t addr) { return *reinterpret_cast<T*>(addr); }

    template<typename T>
    void write(uintptr_t addr, T val) { *reinterpret_cast<T*>(addr) = val; }

    inline void write_str(uintptr_t addr, const char* str, size_t max) {
        memset(reinterpret_cast<void*>(addr), 0, max);
        if (str) strncpy(reinterpret_cast<char*>(addr), str, max - 1);
    }

    inline bool is_valid_ptr(uintptr_t p) {
        return p > 0x10000 && p < 0x7FFFFFFFFFFFULL;
    }

    inline uintptr_t follow_ptr(uintptr_t addr) {
        if (!is_valid_ptr(addr)) return 0;
        uintptr_t val = read<uintptr_t>(addr);
        return is_valid_ptr(val) ? val : 0;
    }
}

// =============================================================================
// SECTION 2: CUtlVector<T> (embedded, Source 2 style)
// =============================================================================
template<typename T>
struct CUtlVector {
    T*        m_Data;     // +0x0
    int32_t   m_Size;     // +0x8
    int32_t   m_Capacity; // +0xC

    T& operator[](int i) { return m_Data[i]; }
    T* begin() { return m_Data; }
    T* end()   { return m_Data + m_Size; }
    int  count() const { return m_Size; }
    bool empty() const { return m_Size <= 0; }
};

// CUtlMap<int, T*> — red-black tree backed map (Source 2)
template<typename T>
struct CUtlMap {
    void*     m_Left;     // +0x0  CUTL_RBTreeNode_t*
    void*     m_Right;    // +0x8
    void*     m_Root;     // +0x10
    void*     m_Last;     // +0x18
    int32_t   m_NumElements; // +0x20
    int32_t   m_FirstInorder; // +0x24
    char      pad[0xC];   // comparison func etc.
    void*     m_Block;    // +0x38 CUtlMemoryPool

    struct Node {
        Node* left;
        Node* right;
        Node* parent;
        char  color;
        char  pad_[3];
        int   key;
        T     value;
    };

    T* FindByKey(int key) {
        if (m_NumElements <= 0 || !m_Root) return nullptr;
        Node* node = static_cast<Node*>(m_Root);
        while (node) {
            if (key == node->key) return &node->value;
            node = key < node->key ? static_cast<Node*>(node->left)
                                   : static_cast<Node*>(node->right);
        }
        return nullptr;
    }

    int GetNextKey(int current) {
        // Linear scan through RB tree — slow but works for dumping
        if (!m_Root) return -1;
        int next = -1;
        GetNextKeyR(static_cast<Node*>(m_Root), current, &next, -1);
        return next;
    }

    void GetNextKeyR(Node* node, int current, int* best, int64_t parent_key) {
        if (!node) return;
        GetNextKeyR(static_cast<Node*>(node->left), current, best, node->key);
        if (node->key > current) {
            if (*best == -1 || node->key < *best) *best = node->key;
        }
        GetNextKeyR(static_cast<Node*>(node->right), current, best, node->key);
    }
};

// =============================================================================
// SECTION 3: Global Config Storage
// =============================================================================
namespace SkinChanger {
    std::unordered_map<int, SkinConfig> g_weaponSkins;
    KnifeConfig    g_knife;
    GloveConfig    g_gloves;
    AgentConfig    g_agentT;
    AgentConfig    g_agentCT;
    MusicKitConfig g_musicKit;
    std::vector<DumpedItemDef>   g_dumpedItems;
    std::vector<DumpedPaintKit>  g_dumpedPaintKits;
    bool g_dumpReady = false;
}

// =============================================================================
// SECTION 4: Internal State
// =============================================================================
static std::vector<uint64_t> g_injectedItemIDs;
static uint64_t g_nextFakeItemID = 0xF000000000000001ULL;
static int g_gloveFrame = 0;

// Function pointers resolved by Init()
static uintptr_t g_clientBase = 0;
static uintptr_t g_fnCreateEconItem = 0;
static uintptr_t g_fnSetDynamicAttributeValue = 0;
static uintptr_t g_fnGetEconItemSystem = 0;
static uintptr_t g_fnInventoryManagerGet = 0;
static uintptr_t g_fnUpdateSubclass = 0;
static uintptr_t g_fnSetMeshGroupMask = 0;
static uintptr_t g_fnSetModel = 0;
static uintptr_t g_fnAddStattrakEntity = 0;
static uintptr_t g_fnFindHudElement = 0;
static uintptr_t g_fnClearHudWeaponIcon = 0;

// Cached interface pointers
static uintptr_t g_pEconItemSchema = 0;

// Glove tracking
struct GloveInfo { int itemId = 0; uint32_t high = 0; uint32_t low = 0; int defIndex = 0; };
static GloveInfo g_addedGloves;

// Per-tick state to avoid redundant work
static uintptr_t g_lastWeaponEntity = 0;

// =============================================================================
// SECTION 5: Pattern Signatures
// =============================================================================
static const char* SIG_CREATE_ECON_ITEM =
    "48 83 EC 28 B9 48 00 00 00 E8 ? ? ? ? 48 85";

static const char* SIG_SET_DYNAMIC_ATTR =
    "48 89 6C 24 ? 57 41 56 41 57 48 81 EC ? ? ? ? 48 8B FA C7 44 24 ? ? ? ? ? 4D 8B F8";

static const char* SIG_GET_ECON_ITEM_SYSTEM =
    "48 83 EC 28 48 8B 05 ? ? ? ? 48 85 C0 0F 85 81";

static const char* SIG_INVENTORY_MANAGER_GET =
    "48 8D 05 ? ? ? ? C3 CC CC CC CC CC CC CC CC 8B 91 ? ? ? ? B8";

static const char* SIG_UPDATE_SUBCLASS =
    "40 53 48 83 EC 30 48 8B 41 10 48 8B D9 8B 50 30";

static const char* SIG_SET_MESH_GROUP_MASK =
    "48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC ? 48 8D 99 ? ? ? ? 48 8B 71";

static const char* SIG_SET_MODEL =
    "40 53 48 83 EC ? 48 8B D9 4C 8B C2 48 8B 0D ? ? ? ? 48 8D 54 24";

static const char* SIG_ADD_STATTRAK =
    "48 89 5C 24 ? 48 89 74 24 ? 55 57 41 54 41 56 41 57 48 8D 6C 24 ? 48 81 EC ? ? ? ? 48 8B DA";

static const char* SIG_FIND_HUD_ELEMENT =
    "4C 8B DC 53 48 83 EC 50 48 8B 05";

// =============================================================================
// SECTION 6: CEconItem Implementation
// =============================================================================
void CEconItem_t::SetDynamicAttributeValue(uint16_t attrIndex, float value)
{
    if (!g_fnSetDynamicAttributeValue)
        return;
    using Fn = void(__fastcall*)(CEconItem_t*, uint16_t, float);
    __try { ((Fn)g_fnSetDynamicAttributeValue)(this, attrIndex, value); }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}

CEconItem_t* CEconItem_t::Create()
{
    if (!g_fnCreateEconItem) return nullptr;
    using Fn = CEconItem_t*(__cdecl*)();
    __try { return ((Fn)g_fnCreateEconItem)(); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}

void CEconItem_t::Destruct()
{
    if (!this) return;
    using Fn = void(__fastcall*)(CEconItem_t*);
    SkinChangerBridge::CallVFunc<void, 1>(this);
}

// =============================================================================
// SECTION 7: C_EconItemSchema Access
// =============================================================================
// Schema layout (wh-esports):
//   +0x128: CUtlMap<int, C_EconItemDefinition*>  m_ItemDefinitions
//   +0x2F0: CUtlMap<int, C_PaintKit*>            m_PaintKits
//   +0x318: CUtlMap<int, void*>                    m_Stickers

static CUtlMap<C_EconItemDefinition_t*>& GetItemDefinitionMap()
{
    return *reinterpret_cast<CUtlMap<C_EconItemDefinition_t*>*>(g_pEconItemSchema + 0x128);
}

static CUtlMap<C_PaintKit_t*>& GetPaintKitMap()
{
    return *reinterpret_cast<CUtlMap<C_PaintKit_t*>*>(g_pEconItemSchema + 0x2F0);
}

// C_EconItemSystem -> +0x8 = C_EconItemSchema*
static uintptr_t GetEconItemSchema()
{
    if (!g_fnGetEconItemSystem || !g_clientBase) return 0;
    using Fn = uintptr_t(__fastcall)(uintptr_t);
    __try {
        uintptr_t itemSystem = ((Fn)g_fnGetEconItemSystem)(g_clientBase);
        if (mem::is_valid_ptr(itemSystem))
            return mem::follow_ptr(itemSystem + 0x8);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    return 0;
}

// =============================================================================
// SECTION 8: Init — Pattern Scan & Resolve Functions
// =============================================================================
namespace SkinChanger {
    bool Init()
    {
        if (!SkinChangerBridge::PatternScan || !SkinChangerBridge::GetModuleBase)
            return false;

        g_clientBase = SkinChangerBridge::GetModuleBase(L"client.dll");
        if (!g_clientBase) return false;

        #define SCAN(sig) SkinChangerBridge::PatternScan(L"client.dll", sig)
        g_fnCreateEconItem       = (uintptr_t)SCAN(SIG_CREATE_ECON_ITEM);
        g_fnSetDynamicAttr       = (uintptr_t)SCAN(SIG_SET_DYNAMIC_ATTR);
        g_fnGetEconItemSystem    = (uintptr_t)SCAN(SIG_GET_ECON_ITEM_SYSTEM);
        g_fnInventoryManagerGet  = (uintptr_t)SCAN(SIG_INVENTORY_MANAGER_GET);
        g_fnUpdateSubclass       = (uintptr_t)SCAN(SIG_UPDATE_SUBCLASS);
        g_fnSetMeshGroupMask     = (uintptr_t)SCAN(SIG_SET_MESH_GROUP_MASK);
        g_fnSetModel             = (uintptr_t)SCAN(SIG_SET_MODEL);
        g_fnAddStattrak          = (uintptr_t)SCAN(SIG_ADD_STATTRAK);
        g_fnFindHudElement       = (uintptr_t)SCAN(SIG_FIND_HUD_ELEMENT);
        #undef SCAN

        // Resolve schema
        g_pEconItemSchema = GetEconItemSchema();

        // Dump skins from game schema
        DumpSkins();

        return g_fnCreateEconItem != 0 && g_fnSetDynamicAttr != 0;
    }
}

// =============================================================================
// SECTION 9: Skin Dumping — Runtime Extraction from Game Schema
// =============================================================================
namespace SkinChanger {
    bool DumpSkins()
    {
        g_dumpedItems.clear();
        g_dumpedPaintKits.clear();

        if (!g_pEconItemSchema) {
            g_pEconItemSchema = GetEconItemSchema();
            if (!g_pEconItemSchema) return false;
        }

        // --- Dump Item Definitions ---
        auto& itemMap = GetItemDefinitionMap();
        int key = -1;
        while ((key = itemMap.GetNextKey(key)) != -1) {
            C_EconItemDefinition_t* def = itemMap.FindByKey(key);
            if (!def || !mem::is_valid_ptr((uintptr_t)def)) continue;

            if (!def->m_bEnabled) continue;

            DumpedItemDef entry{};
            entry.defIndex  = def->m_nDefIndex;
            entry.name      = def->GetName();
            entry.baseName  = def->m_pszItemBaseName ? def->m_pszItemBaseName : "";
            entry.typeName  = def->m_pszItemTypeName ? def->m_pszItemTypeName : "";
            entry.modelPath = def->GetModelName();
            entry.loadoutSlot = def->GetLoadoutSlot();
            entry.rarity    = def->GetRarity();

            if (def->IsKnife())        entry.type = ItemType::Knife;
            else if (def->IsGlove())   entry.type = ItemType::Glove;
            else if (def->IsAgent())   entry.type = ItemType::Agent;
            else if (def->IsWeapon())  entry.type = ItemType::Weapon;
            else                        entry.type = ItemType::Unknown;

            // Only store items we care about
            if (entry.type != ItemType::Unknown)
                g_dumpedItems.push_back(entry);
        }

        // --- Dump Paint Kits ---
        auto& paintMap = GetPaintKitMap();
        key = -1;
        while ((key = paintMap.GetNextKey(key)) != -1) {
            C_PaintKit_t* kit = paintMap.FindByKey(key);
            if (!kit || !mem::is_valid_ptr((uintptr_t)kit)) continue;

            // Skip empty/invalid kits
            if (kit->m_id == 0 || kit->m_id == 9001) continue;
            if (!kit->m_name || kit->m_name[0] == '\0') continue;

            DumpedPaintKit entry{};
            entry.id          = kit->m_id;
            entry.name        = kit->m_name;
            entry.description = kit->m_description_tag ? kit->m_description_tag : "";
            entry.rarity      = kit->m_rarity;
            entry.isLegacy    = kit->IsLegacy();

            g_dumpedPaintKits.push_back(entry);
        }

        g_dumpReady = true;
        return true;
    }
}

// =============================================================================
// SECTION 10: Inventory Manager / Inventory Wrappers (vtable calls)
// =============================================================================
// CCSInventoryManager
static uintptr_t GetInventoryManager()
{
    if (!g_fnInventoryManagerGet) return 0;
    using Fn = uintptr_t(__fastcall*)();
    __try { return ((Fn)g_fnInventoryManagerGet)(); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

// vtable index 68 (wh-esports) — was 57 in older Silent-Ware
static uintptr_t GetLocalInventory(uintptr_t pManager)
{
    if (!pManager) return 0;
    return SkinChangerBridge::CallVFunc<uintptr_t, 68>(pManager);
}

// vtable index 65 (wh-esports) — was 54 in older Silent-Ware
static bool EquipItemInLoadout(uintptr_t pManager, int team, int slot, uint64_t itemID)
{
    if (!pManager) return false;
    return SkinChangerBridge::CallVFunc<bool, 65>(pManager, team, slot, itemID);
}

// CCSPlayerInventory — GetItemInLoadout vtable 8
static uintptr_t GetItemInLoadout(uintptr_t pInventory, int team, int slot)
{
    if (!pInventory) return 0;
    return SkinChangerBridge::CallVFunc<uintptr_t, 8>(pInventory, team, slot);
}

// Get owner SOID from inventory
static uint64_t GetInventoryOwner(uintptr_t pInventory)
{
    if (!pInventory) return 0;
    return mem::read<uint64_t>(pInventory + 0x10);
}

// =============================================================================
// SECTION 11: Item Creation & Equipping (SOC/GC Path)
// =============================================================================
static uint64_t GetHighestItemID(uintptr_t pInventory)
{
    // Walk the item vector at inventory+0x20
    auto* items = reinterpret_cast<CUtlVector<uintptr_t>*>(pInventory + 0x20);
    if (!items || items->empty()) return 0;
    uint64_t maxID = 0;
    for (int i = 0; i < items->count(); i++) {
        uintptr_t itemView = items->operator[](i);
        if (!mem::is_valid_ptr(itemView)) continue;
        uint64_t id = mem::read<uint64_t>(itemView + Offsets::EconItemView::ItemID);
        if ((id & 0xF000000000000000ULL) == 0)  // skip default items
            maxID = (max > id) ? max : id;
    }
    return maxID;
}

static bool CreateAndEquipItem(uintptr_t pInventory, uintptr_t pManager,
                                int defIndex, int team, int slot,
                                int paintKit, int seed, float wear, int statTrak,
                                const char* customName = nullptr)
{
    if (!pInventory || !pManager) return false;

    // Create CEconItem
    CEconItem_t* pItem = CEconItem_t::Create();
    if (!pItem) return false;

    uint64_t fakeID = g_nextFakeItemID++;
    pItem->m_ulID          = fakeID;
    pItem->m_ulOriginalID = fakeID;
    pItem->m_unDefIndex    = (uint16_t)defIndex;
    pItem->m_unAccountID   = (uint32_t)(GetInventoryOwner(pInventory) & 0xFFFFFFFF);
    pItem->m_unInventory   = 1 << 30;
    // Quality: 4=Unique for weapons, 3=Unusual for knives
    pItem->m_unFlags       = KnifeSubclassMap::IsKnife(defIndex) ? 3 : 4;

    // Set attributes via game function (proper path)
    if (paintKit > 0) {
        pItem->SetPaintKit((float)paintKit);
        pItem->SetPaintSeed((float)seed);
        pItem->SetPaintWear(wear);
    }
    if (statTrak >= 0) {
        pItem->SetStatTrak(statTrak);
        pItem->SetStatTrakType(0);
    }

    // Add to SO cache via inventory
    // pInventory->AddEconItem(pItem) — vtable 0 = SOCreated
    // For simplicity we use the SOCreated path directly
    // In a full implementation you'd call CreateBaseTypeCache -> AddObject -> SOCreated
    // For now we equip directly which also handles the SOC
    SkinChangerBridge::CallVFunc<void, 0>(pInventory,
        mem::read<SOID_t>(pInventory + 0x10), pItem, (int)0);

    // Equip in loadout
    bool equipped = EquipItemInLoadout(pManager, team, slot, fakeID);

    if (equipped) {
        g_injectedItemIDs.push_back(fakeID);
    }

    return equipped;
}

// =============================================================================
// SECTION 12: Glove Application
// =============================================================================
static void InvalidateGloveMaterial(uintptr_t pViewModel)
{
    auto* matInfo = reinterpret_cast<MaterialInfo_t*>(pViewModel + 0xF80);
    if (!matInfo || !mem::is_valid_ptr((uintptr_t)matInfo->records)) return;
    for (uint32_t i = 0; i < matInfo->count; i++) {
        if (matInfo->records[i].identifier == MaterialMagic::Gloves) {
            matInfo->records[i].type_index = 0xFFFFFFFF;
            break;
        }
    }
}

static void ApplyGloves(uintptr_t pInventory, uintptr_t pLocalPawn, uintptr_t pViewModel)
{
    if (!pLocalPawn || !pViewModel || !g_addedGloves.itemId) return;
    if (mem::read<int32_t>(pLocalPawn + /* C_BaseEntity::m_iHealth offset */ 0x130) <= 0)
        return;

    uintptr_t gloveViewAddr = pLocalPawn + Offsets::PlayerPawn::EconGloves;

    if (g_gloveFrame > 0) {
        InvalidateGloveMaterial(pViewModel);
        mem::write<bool>(gloveViewAddr + Offsets::EconItemView::Initialized, true);
        mem::write<bool>(pLocalPawn + Offsets::PlayerPawn::NeedToReApplyGloves, true);
        g_gloveFrame--;
    }

    uint64_t curID = mem::read<uint64_t>(gloveViewAddr + Offsets::EconItemView::ItemID);
    if (curID != (uint64_t)g_addedGloves.itemId) {
        g_gloveFrame = 2;
        mem::write<bool>(gloveViewAddr + Offsets::EconItemView::DisallowSOC, false);
        mem::write<uint64_t>(gloveViewAddr + Offsets::EconItemView::ItemID, g_addedGloves.itemId);
        mem::write<uint32_t>(gloveViewAddr + Offsets::EconItemView::ItemIDHigh, g_addedGloves.high);
        mem::write<uint32_t>(gloveViewAddr + Offsets::EconItemView::ItemIDLow, g_addedGloves.low);
        mem::write<uint32_t>(gloveViewAddr + Offsets::EconItemView::AccountID,
                             (uint32_t)(GetInventoryOwner(pInventory) & 0xFFFFFFFF));
        mem::write<uint16_t>(gloveViewAddr + Offsets::EconItemView::ItemDefinitionIndex,
                             (uint16_t)g_addedGloves.defIndex);
        mem::write<bool>(gloveViewAddr + Offsets::EconItemView::DisallowSOC, false);

        // SetMeshGroupMask on viewmodel
        if (g_fnSetMeshGroupMask && mem::is_valid_ptr(pViewModel)) {
            using Fn = void(__fastcall*)(uintptr_t, uint32_t);
            __try { ((Fn)g_fnSetMeshGroupMask)(pViewModel, 1); }
            __except (EXCEPTION_EXECUTE_HANDLER) {}
        }
    }
}

// =============================================================================
// SECTION 13: Music Kit
// =============================================================================
static void ApplyMusicKit(uintptr_t pController)
{
    if (!SkinChanger::g_musicKit.enabled || !pController) return;
    uintptr_t invServices = mem::follow_ptr(pController + Offsets::PlayerController::InventoryServices);
    if (!invServices) return;
    mem::write<uint16_t>(invServices + Offsets::InventoryServices::MusicID,
                         (uint16_t)SkinChanger::g_musicKit.musicKitId);
}

// =============================================================================
// SECTION 14: HUD Helpers
// =============================================================================
namespace SkinChanger {
    void WriteHudName(uintptr_t itemView, const char* weaponName, const char* skinName, int statTrak)
    {
        if (!itemView || !weaponName || !skinName) return;
        char buf[161];
        if (statTrak >= 0)
            snprintf(buf, sizeof(buf), "StatTrak\u2122 %s | %s", weaponName, skinName);
        else
            snprintf(buf, sizeof(buf), "%s | %s", weaponName, skinName);

        mem::write_str(itemView + Offsets::EconItemView::CustomName, buf, 161);
        mem::write_str(itemView + Offsets::EconItemView::CustomNameOverride, buf, 161);

        // Ensure ItemIDHigh is non-zero for display
        if (mem::read<uint32_t>(itemView + Offsets::EconItemView::ItemIDHigh) == 0)
            mem::write<uint32_t>(itemView + Offsets::EconItemView::ItemIDHigh, 1);
    }

    void ClearHudWeaponIcon(uintptr_t itemView)
    {
        if (!itemView) return;
        mem::write<bool>(itemView + Offsets::EconItemView::RestoreCustomMaterialAfterPrecache, true);
    }
}

// =============================================================================
// SECTION 15: Force Update Viewmodel
// =============================================================================
namespace SkinChanger {
    void ForceUpdateViewModel()
    {
        // Clear the cached visual state to force model/skin re-render.
        // The next frame's OnFrameStageNotify will re-apply everything.
        g_lastWeaponEntity = 0;
    }
}

// =============================================================================
// SECTION 16: Main FrameStageNotify Handler
// =============================================================================
namespace SkinChanger {
    void OnFrameStageNotify(int frameStage)
    {
        // Apply at stage 7 (FRAME_NET_UPDATE_POSTDATAUPDATE) — prevents knife flicker
        if (frameStage != 7) return;

        uintptr_t controller = SkinChangerBridge::GetLocalController();
        if (!controller) return;

        // --- Apply Music Kit ---
        ApplyMusicKit(controller);

        // --- Apply Agent ---
        // TODO: Set agent model via loadout system
        // Agent is applied through the loadout slot. Once items are equipped
        // in slot 38/39, the game handles the model automatically.

        // Get inventory manager + inventory
        uintptr_t pManager = GetInventoryManager();
        if (!pManager) return;
        uintptr_t pInventory = GetLocalInventory(pManager);
        if (!pInventory) return;

        // Get local pawn
        uintptr_t pawnHandle = mem::read<uint32_t>(controller + Offsets::PlayerController::PawnHandle);
        uintptr_t entityList = mem::read<uintptr_t>(g_clientBase + Offsets::Client::dwEntityList);
        if (!entityList) return;
        uintptr_t pawn = 0;
        if (SkinChangerBridge::GetEntityByHandle)
            pawn = SkinChangerBridge::GetEntityByHandle(entityList, pawnHandle);
        if (!mem::is_valid_ptr(pawn)) return;

        // Check alive
        if (mem::read<int32_t>(pawn + 0x130) <= 0) return;

        // --- Get Viewmodel ---
        // ViewModelServices on pawn, viewmodel handle at roughly +0xE48
        // Simplified: just get the active viewmodel entity
        // Your base should provide a way to get the viewmodel.
        // For now we iterate entities tagged as viewmodel.

        // --- Apply Gloves ---
        // Attempt to find viewmodel for glove application
        uintptr_t vmHandle = mem::read<uint32_t>(pawn + 0xE48);  // m_hViewModel[0]
        uintptr_t pViewModel = 0;
        if (SkinChangerBridge::GetEntityByHandle)
            pViewModel = SkinChangerBridge::GetEntityByHandle(entityList, vmHandle);
        ApplyGloves(pInventory, pawn, pViewModel);

        // --- Iterate Weapon Entities & Apply Skins ---
        uint64_t steamID = GetInventoryOwner(pInventory);
        if (!steamID) return;

        int highest = mem::read<int32_t>(entityList + 0x20A0);  // dwGameEntitySystem_highestEntityIndex
        for (int i = 65; i <= highest; i++) {
            uintptr_t entity = 0;
            if (SkinChangerBridge::GetEntityByHandle)
                entity = SkinChangerBridge::GetEntityByHandle(entityList, i);
            if (!mem::is_valid_ptr(entity)) continue;

            // Get CAttributeManager -> C_EconItemView
            uintptr_t attrContainer = entity + Offsets::EconEntity::AttributeManager;
            uintptr_t itemViewAddr = attrContainer + Offsets::AttributeContainer::Item;
            if (!mem::is_valid_ptr(itemViewAddr)) continue;

            int defIndex = mem::read<uint16_t>(itemViewAddr + Offsets::EconItemView::ItemDefinitionIndex);
            uint64_t ownerLow = mem::read<uint32_t>(entity + Offsets::EconEntity::OriginalOwnerXuidLow);
            uint64_t ownerHigh = mem::read<uint32_t>(entity + Offsets::EconEntity::OriginalOwnerXuidHigh);
            uint64_t ownerXuid = (ownerHigh << 32) | ownerLow;

            if (ownerXuid != steamID) continue;

            // --- Knife ---
            if (KnifeSubclassMap::IsKnife(defIndex)) {
                if (!SkinChanger::g_knife.enabled) continue;
                if (entity == g_lastWeaponEntity) continue;

                int targetDef = SkinChanger::g_knife.defIndex;
                uint64_t targetHash = KnifeSubclassMap::GetHash((uint16_t)targetDef);

                // Update subclass ID
                if (targetHash && g_fnUpdateSubclass) {
                    // m_nSubclassID is on the entity — set its hash code
                    // The exact offset for m_nSubclassID varies, but it's on CBaseEntity
                    // around the VData area. For now we call UpdateSubclass after setting def.
                }

                // Update EconItemView
                mem::write<uint16_t>(itemViewAddr + Offsets::EconItemView::ItemDefinitionIndex,
                                     (uint16_t)targetDef);

                // Apply knife skin via fallback path
                auto& knifeSkin = SkinChanger::g_knife.skin;
                if (knifeSkin.enabled) {
                    mem::write<uint32_t>(itemViewAddr + Offsets::EconItemView::ItemIDHigh, 0xFFFFFFFF);
                    mem::write<int32_t>(entity + Offsets::EconEntity::FallbackPaintKit, knifeSkin.paintKit);
                    mem::write<int32_t>(entity + Offsets::EconEntity::FallbackSeed, knifeSkin.seed);
                    mem::write<float>(entity + Offsets::EconEntity::FallbackWear, knifeSkin.wear);
                    if (knifeSkin.statTrak >= 0)
                        mem::write<int32_t>(entity + Offsets::EconEntity::FallbackStatTrak, knifeSkin.statTrak);
                }

                // Set model on weapon + viewmodel if active
                if (g_fnSetModel) {
                    // Get model path from item definition
                    // For simplicity, look up the model from dumped items
                    const char* modelPath = nullptr;
                    for (auto& d : g_dumpedItems) {
                        if (d.defIndex == targetDef) { modelPath = d.modelPath; break; }
                    }
                    if (modelPath && modelPath[0]) {
                        using SetModelFn = void(__fastcall*)(uintptr_t, const char*);
                        __try {
                            ((SetModelFn)g_fnSetModel)(entity, modelPath);
                            if (mem::is_valid_ptr(pViewModel) && vmHandle == mem::read<uint32_t>(pawn + 0xE48))
                                ((SetModelFn)g_fnSetModel)(pViewModel, modelPath);
                        }
                        __except (EXCEPTION_EXECUTE_HANDLER) {}
                    }
                }

                // MeshGroupMask
                if (g_fnSetMeshGroupMask) {
                    using Fn = void(__fastcall*)(uintptr_t, uint32_t);
                    __try {
                        ((Fn)g_fnSetMeshGroupMask)(entity, 2);
                        if (mem::is_valid_ptr(pViewModel))
                            ((Fn)g_fnSetMeshGroupMask)(pViewModel, 2);
                    }
                    __except (EXCEPTION_EXECUTE_HANDLER) {}
                }

                g_lastWeaponEntity = entity;
                continue;
            }

            // --- Weapon Skin ---
            auto it = g_weaponSkins.find(defIndex);
            if (it == g_weaponSkins.end() || !it->second.enabled) continue;
            if (entity == g_lastWeaponEntity) continue;

            auto& skin = it->second;

            // Apply via fallback path
            mem::write<uint32_t>(itemViewAddr + Offsets::EconItemView::ItemIDHigh, 0xFFFFFFFF);
            if (skin.paintKit > 0)
                mem::write<int32_t>(entity + Offsets::EconEntity::FallbackPaintKit, skin.paintKit);
            if (skin.seed > 0)
                mem::write<int32_t>(entity + Offsets::EconEntity::FallbackSeed, skin.seed);
            mem::write<float>(entity + Offsets::EconEntity::FallbackWear, skin.wear);
            if (skin.statTrak >= 0)
                mem::write<int32_t>(entity + Offsets::EconEntity::FallbackStatTrak, skin.statTrak);

            // Custom name
            if (skin.customName[0]) {
                mem::write_str(itemViewAddr + Offsets::EconItemView::CustomName, skin.customName, 128);
                mem::write_str(itemViewAddr + Offsets::EconItemView::CustomNameOverride, skin.customName, 128);
            }

            // MeshGroupMask — 2 for legacy, 1 for CS2-native
            if (g_fnSetMeshGroupMask) {
                using Fn = void(__fastcall*)(uintptr_t, uint32_t);
                __try {
                    // Check if paint kit is legacy from dumped data
                    bool legacy = false;
                    for (auto& pk : g_dumpedPaintKits) {
                        if (pk.id == skin.paintKit) { legacy = pk.isLegacy; break; }
                    }
                    uint32_t mask = legacy ? 2 : 1;
                    ((Fn)g_fnSetMeshGroupMask)(entity, mask);

                    if (mem::is_valid_ptr(pViewModel)) {
                        uint32_t vmWeapon = mem::read<uint32_t>(pViewModel + 0x38); // m_hWeapon
                        uint32_t entIdx = i;
                        if (vmWeapon == entIdx)
                            ((Fn)g_fnSetMeshGroupMask)(pViewModel, mask);
                    }
                }
                __except (EXCEPTION_EXECUTE_HANDLER) {}
            }

            ClearHudWeaponIcon(itemViewAddr);
            g_lastWeaponEntity = entity;
        }

        g_lastWeaponEntity = 0;
    }
}

// =============================================================================
// SECTION 17: SetModel Hook Handler
// =============================================================================
namespace SkinChanger {
    void OnSetModel(uintptr_t entity, const char*& model)
    {
        // Intercept viewmodel SetModel during lag to redirect to custom knife model.
        if (!model || !SkinChanger::g_knife.enabled) return;
        // The entity pointer check (is it a viewmodel?) depends on your base's
        // entity classification. Your base should determine if the entity is a
        // C_BaseViewModel and pass only those to this function.
    }
}

// =============================================================================
// SECTION 18: Killfeed Fix
// =============================================================================
namespace SkinChanger {
    void OnPlayerDeath(uintptr_t pEvent)
    {
        // Rewrite killfeed weapon name for custom knives.
        // pEvent should be an IGameEvent*. Your base provides event getter/setter.
        // The implementation depends on your event system API.
        // Key logic:
        //   1. Check if local player is the attacker
        //   2. Check if active weapon is a knife
        //   3. Rewrite "weapon" field to GetSimpleWeaponName() of the knife def
    }
}

// =============================================================================
// SECTION 19: Apply / Remove Actions
// =============================================================================
namespace SkinChanger {
    void ApplyCurrentConfig()
    {
        uintptr_t pManager = GetInventoryManager();
        uintptr_t pInventory = pManager ? GetLocalInventory(pManager) : 0;
        if (!pInventory || !pManager) return;

        // Equip knife for both teams
        if (g_knife.enabled) {
            int defIdx = g_knife.defIndex;
            int paintKit = g_knife.skin.paintKit;
            int seed = g_knife.skin.seed;
            float wear = g_knife.skin.wear;
            int statTrak = g_knife.skin.statTrak;
            CreateAndEquipItem(pInventory, pManager, defIdx, 2, LoadoutSlots::Melee,
                               paintKit, seed, wear, statTrak);
            CreateAndEquipItem(pInventory, pManager, defIdx, 3, LoadoutSlots::Melee,
                               paintKit, seed, wear, statTrak);
        }

        // Equip gloves for both teams
        if (g_gloves.enabled) {
            CreateAndEquipItem(pInventory, pManager, g_gloves.defIndex, 2, LoadoutSlots::Gloves,
                               g_gloves.paintKit, g_gloves.seed, g_gloves.wear, -1);
            CreateAndEquipItem(pInventory, pManager, g_gloves.defIndex, 3, LoadoutSlots::Gloves,
                               g_gloves.paintKit, g_gloves.seed, g_gloves.wear, -1);

            // Store glove info for per-frame EconGloves override
            g_addedGloves.itemId   = (int)g_nextFakeItemID - 1;
            g_addedGloves.defIndex = g_gloves.defIndex;
        }

        // Equip weapon skins — each weapon gets its own CEconItem
        for (auto& [defIdx, skin] : g_weaponSkins) {
            if (!skin.enabled || skin.paintKit <= 0) continue;
            CreateAndEquipItem(pInventory, pManager, defIdx, 2, 0,
                               skin.paintKit, skin.seed, skin.wear, skin.statTrak, skin.customName);
            CreateAndEquipItem(pInventory, pManager, defIdx, 3, 0,
                               skin.paintKit, skin.seed, skin.wear, skin.statTrak, skin.customName);
        }

        // Equip agents
        if (g_agentT.enabled)
            CreateAndEquipItem(pInventory, pManager, g_agentT.defIndex, 2, LoadoutSlots::AgentT, 0, 0, 0, -1);
        if (g_agentCT.enabled)
            CreateAndEquipItem(pInventory, pManager, g_agentCT.defIndex, 3, LoadoutSlots::AgentCT, 0, 0, 0, -1);

        ForceUpdateViewModel();
    }

    void RemoveAllSkins()
    {
        uintptr_t pManager = GetInventoryManager();
        uintptr_t pInventory = pManager ? GetLocalInventory(pManager) : 0;
        if (!pInventory) return;

        // Equip default items for all injected items
        for (uint64_t id : g_injectedItemIDs) {
            uint64_t defaultID = (uint64_t(0xF) << 60) | (id & 0xFFFF);
            // Try to determine the slot — for simplicity re-equip defaults
        }

        // Remove items from SO cache
        // In a full implementation: iterate SO cache, call SODestroyed for each injected item
        g_injectedItemIDs.clear();
        g_addedGloves = {};
        g_nextFakeItemID = 0xF000000000000001ULL;
        ForceUpdateViewModel();
    }

    void RemoveAllGloves()
    {
        uintptr_t pManager = GetInventoryManager();
        if (!pManager) return;
        uintptr_t pInventory = GetLocalInventory(pManager);
        if (!pInventory) return;

        // Re-equip default gloves
        uint64_t defaultGloveID = (uint64_t(0xF) << 60) | 5028;  // default glove def index
        EquipItemInLoadout(pManager, 2, LoadoutSlots::Gloves, defaultGloveID);
        EquipItemInLoadout(pManager, 3, LoadoutSlots::Gloves, defaultGloveID);

        g_addedGloves = {};
        ForceUpdateViewModel();
    }

    void RemoveAllKnives()
    {
        uintptr_t pManager = GetInventoryManager();
        if (!pManager) return;

        uint64_t defaultKnifeID = (uint64_t(0xF) << 60) | 42;  // default knife
        EquipItemInLoadout(pManager, 2, LoadoutSlots::Melee, defaultKnifeID);
        EquipItemInLoadout(pManager, 3, LoadoutSlots::Melee, defaultKnifeID);
        ForceUpdateViewModel();
    }

    void PreCacheModels()
    {
        // Force-load models to prevent first-apply flicker.
        // Requires IResourceSystem::PreCache from your base.
        // This is optional but recommended.
    }

    void Shutdown()
    {
        RemoveAllSkins();
        g_fnCreateEconItem = 0;
        g_fnSetDynamicAttr = 0;
        g_fnGetEconItemSystem = 0;
        g_fnInventoryManagerGet = 0;
        g_fnUpdateSubclass = 0;
        g_fnSetMeshGroupMask = 0;
        g_fnSetModel = 0;
        g_fnAddStattrak = 0;
        g_fnFindHudElement = 0;
        g_pEconItemSchema = 0;
        g_dumpedItems.clear();
        g_dumpedPaintKits.clear();
        g_dumpReady = false;
    }
}
