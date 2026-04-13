#pragma once
/*
 *  skinchanger.hpp — Self-Contained CS2 Skin Changer
 *
 *  Zero external dependencies. Only requires your project's pattern scanner
 *  and memory access primitives (configured via the SKINCHANGER_BRIDGE section).
 *
 *  Features:
 *    - Runtime skin dumping (paint kits, item definitions, stickers from game schema)
 *    - Weapon skin apply/remove with paint kit, seed, wear, StatTrak, nametag
 *    - Knife model swap with subclass hash map (20 knives)
 *    - Glove injection with material invalidation
 *    - Agent model override per team
 *    - Music kit override
 *    - Force viewmodel update
 *    - Anti-flickering (FrameStage 7, MeshGroupMask, SEH guards)
 *    - Killfeed knife name fix
 *    - HUD weapon icon refresh
 *
 *  Usage:
 *    1. Fill in SKINCHANGER_BRIDGE with your project's memory/pattern primitives
 *    2. Call SkinChanger::Init() after modules are loaded
 *    3. Call SkinChanger::OnFrameStageNotify(stage) from your FrameStageNotify hook
 *    4. Call SkinChanger::OnSetModel(entity, model) from your SetModel hook
 *    5. Call SkinChanger::OnPlayerDeath(event) from your game event listener
 *    6. Call SkinChanger::Shutdown() on unload
 */

#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <vector>
#include <map>
#include <string>
#include <array>
#include <functional>
#include <optional>
#include <algorithm>

// =============================================================================
// SKINCHANGER_BRIDGE — Fill in these 6 functions with your project's primitives.
// =============================================================================
// This is the ONLY integration point. Everything else is self-contained.
namespace SkinChangerBridge
{
    // Pattern scan in a module by name (e.g. L"client.dll").
    // Returns pointer to match, or nullptr.
    using PatternScanFn = std::add_pointer_t<uint8_t*(const wchar_t* module, const char* pattern)>;
    inline PatternScanFn PatternScan = nullptr;

    // Call a virtual function by index.
    // Example: return (*reinterpret_cast<T* const*>(thisptr))[index](thisptr, args...);
    // For __fastcall on x64, 'this' is rcx.
    template<typename T, size_t Index, typename C, typename... A>
    static inline T CallVFunc(C* thisptr, A... args)
    {
        using Fn = T(__fastcall*)(C*, A...);
        return (*reinterpret_cast<Fn* const*>(reinterpret_cast<uintptr_t>(thisptr)))[Index](thisptr, args...);
    }

    // Set a virtual function pointer (for hooking).
    template<typename T>
    static inline void SetVFunc(void* thisptr, size_t index, T fn)
    {
        (*reinterpret_cast<void***>(thisptr))[index] = reinterpret_cast<void*>(fn);
    }

    // Get module base address by name.
    using GetModuleBaseFn = std::add_pointer_t<uintptr_t(const wchar_t* module)>;
    inline GetModuleBaseFn GetModuleBase = nullptr;

    // Get local player controller address (raw pointer).
    // Should return the address of the CCSPlayerController entity.
    using GetLocalControllerFn = std::add_pointer_t<uintptr_t()>;
    inline GetLocalControllerFn GetLocalController = nullptr;

    // Get entity from entity system by handle.
    // handle = entity index (& 0x7FFF). Returns entity address or 0.
    using GetEntityByHandleFn = std::add_pointer_t<uintptr_t(uintptr_t entityList, uint32_t handle)>;
    inline GetEntityByHandleFn GetEntityByHandle = nullptr;
}

// =============================================================================
// CLIENT-SIDE OFFSETS — Source 2 Schema (cs2-dumper, build 14141)
// =============================================================================
namespace Offsets
{
    // C_EconItemView (client) — embedded in CAttributeContainer at +0x50
    namespace EconItemView {
        constexpr std::ptrdiff_t RestoreCustomMaterialAfterPrecache = 0x1B8;
        constexpr std::ptrdiff_t ItemDefinitionIndex    = 0x1BA;  // uint16
        constexpr std::ptrdiff_t EntityQuality           = 0x1BC;  // int32
        constexpr std::ptrdiff_t EntityLevel             = 0x1C0;  // uint32
        constexpr std::ptrdiff_t ItemID                  = 0x1C8;  // uint64
        constexpr std::ptrdiff_t ItemIDHigh              = 0x1D0;  // uint32 (0xFFFFFFFF = force fallback)
        constexpr std::ptrdiff_t ItemIDLow               = 0x1D4;  // uint32
        constexpr std::ptrdiff_t AccountID               = 0x1D8;  // uint32
        constexpr std::ptrdiff_t InventoryPosition       = 0x1DC;  // uint32
        constexpr std::ptrdiff_t Initialized             = 0x1E8;  // bool — write LAST
        constexpr std::ptrdiff_t DisallowSOC             = 0x1E9;  // bool
        constexpr std::ptrdiff_t IsStoreItem             = 0x1EA;  // bool
        constexpr std::ptrdiff_t IsTradeItem             = 0x1EB;  // bool
        constexpr std::ptrdiff_t EntityQuantity          = 0x1EC;  // int32
        constexpr std::ptrdiff_t RarityOverride          = 0x1F0;  // int32
        constexpr std::ptrdiff_t QualityOverride         = 0x1F4;  // int32
        constexpr std::ptrdiff_t OriginOverride          = 0x1F8;  // int32
        constexpr std::ptrdiff_t AttributeList           = 0x208;  // CAttributeList
        constexpr std::ptrdiff_t NetworkedDynamicAttributes = 0x280;
        constexpr std::ptrdiff_t CustomName              = 0x2F8;  // char[161]
        constexpr std::ptrdiff_t CustomNameOverride      = 0x399;  // char[161]
        constexpr std::ptrdiff_t InitializedTags         = 0x468;  // bool
    }

    // C_EconEntity (client)
    namespace EconEntity {
        constexpr std::ptrdiff_t AttributesInitialized   = 0x1370;
        constexpr std::ptrdiff_t AttributeManager         = 0x1378;
        constexpr std::ptrdiff_t OriginalOwnerXuidLow     = 0x1848;
        constexpr std::ptrdiff_t OriginalOwnerXuidHigh    = 0x184C;
        constexpr std::ptrdiff_t FallbackPaintKit         = 0x1850;  // int32
        constexpr std::ptrdiff_t FallbackSeed             = 0x1854;  // int32
        constexpr std::ptrdiff_t FallbackWear             = 0x1858;  // float
        constexpr std::ptrdiff_t FallbackStatTrak         = 0x185C;  // int32
        constexpr std::ptrdiff_t Clientside               = 0x1860;
        constexpr std::ptrdiff_t ViewmodelAttachment      = 0x1880;
    }

    // C_AttributeContainer
    namespace AttributeContainer {
        constexpr std::ptrdiff_t Item = 0x50;  // C_EconItemView embedded
    }

    // CAttributeList
    namespace AttributeList {
        constexpr std::ptrdiff_t Attributes = 0x08;  // CUtlVector<CEconItemAttribute>
        constexpr std::ptrdiff_t Manager    = 0x70;  // CAttributeManager*
    }

    // C_CSPlayerPawn
    namespace PlayerPawn {
        constexpr std::ptrdiff_t NeedToReApplyGloves = 0x188D;  // bool
        constexpr std::ptrdiff_t EconGloves           = 0x1890;  // C_EconItemView
    }

    // CCSPlayerController
    namespace PlayerController {
        constexpr std::ptrdiff_t InventoryServices = 0x810;
        constexpr std::ptrdiff_t PawnHandle        = 0x790;  // m_hPlayerPawn (CBaseHandle, 4 bytes)
    }

    // CCSPlayerController_InventoryServices
    namespace InventoryServices {
        constexpr std::ptrdiff_t NetworkableLoadout = 0x40;  // CUtlVector<NetworkedLoadoutSlot_t>
        constexpr std::ptrdiff_t MusicID            = 0x58;  // uint16
    }

    // NetworkedLoadoutSlot_t
    namespace LoadoutSlot {
        constexpr std::ptrdiff_t Item = 0x0;   // C_EconItemView* (but embedded, so offset within slot)
        constexpr std::ptrdiff_t Team = 0x8;   // uint16
        constexpr std::ptrdiff_t Slot = 0xA;   // uint16
    }

    // C_EconWearable
    namespace EconWearable {
        constexpr std::ptrdiff_t ForceSkin   = 0x18C0;
        constexpr std::ptrdiff_t AlwaysAllow = 0x18C4;
    }

    // Global offsets (cs2-dumper build 14141)
    namespace Client {
        constexpr std::ptrdiff_t dwEntityList                 = 0x24B3268;
        constexpr std::ptrdiff_t dwLocalPlayerController      = 0x22F8028;
        constexpr std::ptrdiff_t dwLocalPlayerPawn            = 0x206D9E0;
    }

    // CAttributeManager sizeof = 0x50 (virtual destructor only)
    constexpr std::ptrdiff_t AttributeManagerSize = 0x50;
}

// =============================================================================
// CEconItemAttribute (0x48 bytes)
// =============================================================================
#pragma pack(push, 1)
struct CEconItemAttribute_t {
    void*    vtable[6];                    // 0x00
    uint16_t m_iAttributeDefinitionIndex;  // 0x30
    char     pad_[2];                       // 0x32
    float    m_flValue;                     // 0x34
    float    m_flInitialValue;              // 0x38
    int32_t  m_nRefundableCurrency;         // 0x3C
    bool     m_bSetBonus;                   // 0x40
    char     pad2_[7];                      // 0x41
};  // 0x48
#pragma pack(pop)

// =============================================================================
// CEconItem — Shared Object Cache item (reverse from wh-esports)
// =============================================================================
#pragma pack(push, 1)
struct CEconItem_t {
    void*    vtable[2];                // 0x00
    uint64_t m_ulID;                    // 0x10
    uint64_t m_ulOriginalID;            // 0x18
    void*    m_pCustomDataOptimized;    // 0x20
    uint32_t m_unAccountID;             // 0x28
    uint32_t m_unInventory;             // 0x2C
    uint16_t m_unDefIndex;              // 0x30
    // Bitfield at 0x32: origin(5), quality(4), level(2), rarity(4), dirtybit(1)
    uint16_t m_unFlags;                 // 0x32
    int16_t  m_iItemSet;                // 0x34
    int32_t  m_bSOUpdateFrame;          // 0x38
    uint8_t  m_unExtraFlags;            // 0x3C

    // Dynamic attribute setting via game function
    void SetDynamicAttributeValue(uint16_t attrIndex, float value);

    void SetPaintKit(float kit)   { SetDynamicAttributeValue(6, kit); }
    void SetPaintSeed(float seed) { SetDynamicAttributeValue(7, seed); }
    void SetPaintWear(float wear) { SetDynamicAttributeValue(8, wear); }
    void SetStatTrak(int count)   { SetDynamicAttributeValue(80, (float)count); }
    void SetStatTrakType(int type){ SetDynamicAttributeValue(81, (float)type); }

    static CEconItem_t* Create();
    void Destruct();
};
#pragma pack(pop)

// =============================================================================
// C_EconItemDefinition (reverse from wh-esports)
// =============================================================================
struct C_EconItemDefinition_t {
    void*       vtable;
    void*       m_pKVItem;
    uint16_t    m_nDefIndex;
    void*       m_pAssociatedItems;   // 0x10 CUtlVector<uint16_t>
    bool        m_bEnabled;            // 0x18
    const char* m_szPrefab;            // 0x20
    char        pad1_[0x28];
    uint8_t     m_nQuality;            // 0x4A
    uint8_t     m_nRarity;             // 0x4B
    const char* m_pszItemBaseName;     // 0x50
    const char* m_pszItemTypeName;     // 0x58  "#CSGO_Type_Knife", "#Type_Hands", "#Type_CustomPlayer"
    uint32_t    m_unItemTypeID;        // 0x60
    const char* m_pszItemDesc;         // 0x68

    // Virtual methods (hardcoded offsets from wh-esports):
    const char* GetName()              { return CallVMethod<const char*>(0x70); }
    const char* GetItemTypeName()      { return CallVMethod<const char*>(0x80); }
    const char* GetModelName()         { return CallVMethod<const char*>(0x148); }
    const char* GetSimpleWeaponName()  { return CallVMethod<const char*>(0x248); }
    const char* GetWeaponSimpleName()  { return CallVMethod<const char*>(0x260); }
    int         GetLoadoutSlot()       { return CallVMethod<int>(0x338); }
    int         GetStickersSupported() { return CallVMethod<int>(0x168); }
    uint8_t     GetRarity()            { return CallVMethod<uint8_t>(0x42); }

    bool IsWeapon()   { return GetStickersSupported() >= 4; }
    bool IsKnife()    { return m_pszItemTypeName && strcmp(m_pszItemTypeName, "#CSGO_Type_Knife") == 0; }
    bool IsGlove()    { return m_pszItemTypeName && strcmp(m_pszItemTypeName, "#Type_Hands") == 0; }
    bool IsAgent()    { return m_pszItemTypeName && strcmp(m_pszItemTypeName, "#Type_CustomPlayer") == 0; }

private:
    template<typename T>
    T CallVMethod(uintptr_t offset) {
        return SkinChangerBridge::CallVFunc<T, 0>(reinterpret_cast<char*>(this) + offset);
    }
};

// =============================================================================
// C_PaintKit (reverse from wh-esports)
// =============================================================================
struct C_PaintKit_t {
    uint32_t    m_id;
    char        pad0_[4];
    const char* m_name;               // token e.g. "cu_ak47_asimov"
    const char* m_description_string;
    const char* m_description_tag;    // e.g. "#PaintKit_AK47_Asiimov"
    char        pad1_[8];
    const char* m_pattern;
    const char* m_normal;
    const char* m_logo_material;
    char        pad2_[4];
    uint32_t    m_rarity;
    uint32_t    m_style;
    float       m_rgba_color[4][4];
    float       m_rgba_logo_color[4][4];
    float       m_wear_default;
    float       m_wear_remap_min;
    float       m_wear_remap_max;
    char        pad3_[0x30];
    bool        m_use_legacy_model;   // at 0xAE

    bool IsLegacy() const { return m_use_legacy_model; }
};

// =============================================================================
// Knife Subclass Hash Map
// =============================================================================
namespace KnifeSubclassMap {
    static const std::map<uint16_t, uint64_t> kMap = {
        { 500, 3933374535ULL },  // Bayonet
        { 503, 3787235507ULL },  // Flip Knife
        { 505, 4046390180ULL },  // Gut Knife
        { 506, 2047704618ULL },  // Karambit
        { 507, 1731408398ULL },  // M9 Bayonet
        { 508, 1638561588ULL },  // Falchion Knife
        { 509, 2282479884ULL },  // Bowie Knife
        { 512, 3412259219ULL },  // Butterfly Knife
        { 514, 2511498851ULL },  // Shadow Daggers
        { 515, 1353709123ULL },  // Huntsman Knife
        { 516, 4269888884ULL },  // Talon Knife
        { 517, 1105782941ULL },  // Skeleton Knife
        { 518, 275962944ULL  },  // Kukri Knife
        { 519, 1338637359ULL },  // Nomad Knife
        { 520, 3230445913ULL },  // Paracord Knife
        { 521, 3206681373ULL },  // Survival Knife
        { 522, 2595277776ULL },  // Classic Knife
        { 523, 4029975521ULL },  // Ivory Knife
        { 524, 2463111489ULL },  // Ursus Knife
        { 525, 365028728ULL  },  // Navaja Knife
        { 526, 3845286452ULL },  // Stiletto Knife
    };
    static constexpr std::array<int, 20> kKnifeDefIndexes = {
        500, 503, 505, 506, 507, 508, 509, 512, 514, 515,
        516, 517, 518, 519, 520, 521, 522, 523, 525, 526
    };
    inline bool IsKnife(int def) {
        if (def >= 500 && def <= 526) return true;
        return false;
    }
    inline uint64_t GetHash(uint16_t def) {
        auto it = kMap.find(def);
        return it != kMap.end() ? it->second : 0;
    }
}

// =============================================================================
// Material System (Glove Rendering)
// =============================================================================
namespace MaterialMagic { enum : uint32_t { Gloves = 0xF143B82A }; }

struct MaterialRecord_t {
    uint32_t ui32_static;
    uint32_t identifier;
    uint32_t handle;
    uint32_t type_index;
};

struct MaterialInfo_t {
    MaterialRecord_t* records;
    uint32_t count;
};

// =============================================================================
// SOID_t (Steam Object ID)
// =============================================================================
struct SOID_t {
    uint64_t m_id;
};

// =============================================================================
// Attribute Definition Indices
// =============================================================================
namespace AttrDef {
    constexpr uint16_t PaintKit  = 6;
    constexpr uint16_t Seed      = 7;
    constexpr uint16_t Wear      = 8;
    constexpr uint16_t StatTrak  = 80;
    constexpr uint16_t StatTrakType = 81;
}

// =============================================================================
// Loadout Slots
// =============================================================================
namespace LoadoutSlots {
    constexpr int Melee         = 0;
    constexpr int C4            = 1;
    constexpr int Secondary0    = 2;
    constexpr int SMG0          = 8;
    constexpr int Rifle0        = 14;
    constexpr int Heavy0        = 20;
    constexpr int Grenade0      = 26;
    constexpr int AgentT        = 38;  // CustomPlayer T
    constexpr int AgentCT       = 39;  // CustomPlayer CT
    constexpr int Gloves        = 41;  // Hands
    constexpr int MusicKit      = 54;
}

// =============================================================================
// Item Type Enum
// =============================================================================
enum class ItemType : uint8_t {
    Unknown = 0,
    Weapon,
    Knife,
    Glove,
    Agent,
    MusicKit,
};

// =============================================================================
// Configuration Structs
// =============================================================================
struct SkinConfig {
    bool     enabled     = false;
    int      paintKit    = 0;
    int      seed        = 0;
    float    wear        = 0.0001f;
    int      statTrak    = -1;
    char     customName[128] = {};
};

struct KnifeConfig {
    bool     enabled     = false;
    int      defIndex    = 507;  // M9 Bayonet
    SkinConfig skin;
};

struct GloveConfig {
    bool     enabled     = false;
    int      defIndex    = 0;
    int      paintKit    = 0;
    float    wear        = 0.0001f;
    int      seed        = 0;
};

struct AgentConfig {
    bool     enabled     = false;
    int      defIndex    = 0;
};

struct MusicKitConfig {
    bool     enabled     = false;
    int      musicKitId  = 0;
};

// =============================================================================
// Dumped Skin Data (runtime extraction)
// =============================================================================
struct DumpedItemDef {
    int         defIndex;
    const char* name;       // display name
    const char* baseName;   // e.g. "weapon_ak47"
    const char* typeName;   // "#CSGO_Type_Knife" etc.
    const char* modelPath;
    int         loadoutSlot;
    uint8_t     rarity;
    ItemType    type;
};

struct DumpedPaintKit {
    int         id;
    const char* name;       // token
    const char* description;
    uint32_t    rarity;
    bool        isLegacy;
};

// =============================================================================
// SkinChanger — Public API
// =============================================================================
namespace SkinChanger
{
    // --- Global Config ---
    extern std::unordered_map<int, SkinConfig> g_weaponSkins;
    extern KnifeConfig    g_knife;
    extern GloveConfig    g_gloves;
    extern AgentConfig    g_agentT;
    extern AgentConfig    g_agentCT;
    extern MusicKitConfig g_musicKit;

    // --- Dumped Data (filled by DumpSkins()) ---
    extern std::vector<DumpedItemDef>   g_dumpedItems;
    extern std::vector<DumpedPaintKit>  g_dumpedPaintKits;
    extern bool g_dumpReady;

    // --- Initialization ---
    bool Init();           // Scan patterns, resolve function pointers. Call once.
    bool DumpSkins();      // Extract all paint kits and item defs from game schema.
    void PreCacheModels(); // Force-load knife/weapon models to prevent flicker.
    void Shutdown();       // Remove all injected items, clean up.

    // --- Frame Hooks (call these from your hooks) ---
    void OnFrameStageNotify(int frameStage);
    void OnSetModel(uintptr_t entity, const char*& model);
    void OnPlayerDeath(uintptr_t pEvent);

    // --- Actions ---
    void ForceUpdateViewModel();
    void ApplyCurrentConfig();
    void RemoveAllSkins();
    void RemoveAllGloves();
    void RemoveAllKnives();

    // --- HUD ---
    void WriteHudName(uintptr_t itemView, const char* weaponName, const char* skinName, int statTrak);
    void ClearHudWeaponIcon(uintptr_t itemView);
}
