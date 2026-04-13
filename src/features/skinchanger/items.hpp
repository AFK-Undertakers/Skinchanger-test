#pragma once

#include <Windows.h>
#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <shared_mutex>

namespace cs2 {

enum class ItemType : uint8_t {
    Unknown = 0,
    Weapon,
    Knife,
    Glove,
    Agent,
    MusicKit,
    Sticker,
    Graffiti,
    Tool
};

enum class LoadoutSlot : uint8_t {
    Melee = 0,
    C4 = 1,
    Secondary0 = 2,
    SMG0 = 8,
    Rifle0 = 14,
    Heavy0 = 20,
    Grenade0 = 26,
    AgentT = 38,
    AgentCT = 39,
    Gloves = 41,
    MusicKit = 54
};

enum class AttrDef : uint16_t {
    PaintKit = 6,
    Seed = 7,
    Wear = 8,
    StatTrak = 80,
    StatTrakType = 81
};

#pragma pack(push, 1)
struct CEconItemAttribute_t {
    void*    vtable[6];
    uint16_t m_iAttributeDefinitionIndex;
    char     pad_[2];
    float    m_flValue;
    float    m_flInitialValue;
    int32_t  m_nRefundableCurrency;
    bool     m_bSetBonus;
    char     pad2_[7];
};
#pragma pack(pop)

struct C_PaintKit_t {
    uint32_t    m_id;
    char        pad0_[4];
    const char* m_name;
    const char* m_description_string;
    const char* m_description_tag;
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
    bool        m_use_legacy_model;

    bool IsLegacy() const { return m_use_legacy_model; }
};

struct C_EconItemDefinition_t {
    void*       vtable;
    void*       m_pKVItem;
    uint16_t    m_nDefIndex;
    void*       m_pAssociatedItems;
    bool        m_bEnabled;
    const char* m_szPrefab;
    char        pad1_[0x28];
    uint8_t     m_nQuality;
    uint8_t     m_nRarity;
    const char* m_pszItemBaseName;
    const char* m_pszItemTypeName;
    uint32_t    m_unItemTypeID;
    const char* m_pszItemDesc;

    template<typename T>
    T CallVMethod(uintptr_t idx) {
        using Fn = T(__thiscall*)(void*);
        return reinterpret_cast<Fn>((*reinterpret_cast<void***>(this))[idx])(this);
    }

    const char* GetName()              { return CallVMethod<const char*>(17); }
    const char* GetItemTypeName()      { return CallVMethod<const char*>(20); }
    const char* GetModelName()         { return CallVMethod<const char*>(53); }
    const char* GetSimpleWeaponName()  { return CallVMethod<const char*>(97); }
    int         GetLoadoutSlot()       { return CallVMethod<int>(206); }
    int         GetStickersSupported() { return CallVMethod<int>(89); }
    uint8_t     GetRarity()            { return m_nRarity; }

    bool IsWeapon()   { return GetStickersSupported() >= 4; }
    bool IsKnife()    { return m_pszItemTypeName && std::string_view(m_pszItemTypeName).find("#CSGO_Type_Knife") != std::string_view::npos; }
    bool IsGlove()    { return m_pszItemTypeName && std::string_view(m_pszItemTypeName).find("#Type_Hands") != std::string_view::npos; }
    bool IsAgent()    { return m_pszItemTypeName && std::string_view(m_pszItemTypeName).find("#Type_CustomPlayer") != std::string_view::npos; }
};

template<typename T>
struct CUtlMap {
    void*     m_Left;
    void*     m_Right;
    void*     m_Root;
    void*     m_Last;
    int32_t   m_NumElements;
    int32_t   m_FirstInorder;
    char      pad[0xC];
    void*     m_Block;

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
        if (m_NumElements <= 0 || !m_Root) return -1;
        int next = -1;
        GetNextKeyR(static_cast<Node*>(m_Root), current, &next);
        return next;
    }

    void GetNextKeyR(Node* node, int current, int* best) {
        if (!node) return;
        GetNextKeyR(static_cast<Node*>(node->left), current, best);
        if (node->key > current) {
            if (*best == -1 || node->key < *best) *best = node->key;
        }
        GetNextKeyR(static_cast<Node*>(node->right), current, best);
    }
};

template<typename T>
struct CUtlVector {
    T*      m_Data;
    int32_t m_Size;
    int32_t m_Capacity;
    T& operator[](int i) { return m_Data[i]; }
    int  count() const { return m_Size; }
    bool empty() const { return m_Size <= 0; }
};

struct DumpedItemDef {
    int         def_index = 0;
    std::string name;
    std::string base_name;
    std::string type_name;
    std::string model_path;
    int         loadout_slot = 0;
    uint8_t     rarity = 0;
    ItemType    type = ItemType::Unknown;
};

struct DumpedPaintKit {
    int         id = 0;
    std::string name;
    std::string description;
    uint32_t    rarity = 0;
    bool        is_legacy = false;
};

struct SkinConfig {
    uint64_t item_id = 0;
    int      definition_index = 0;
    int      paint_kit_id = 0;
    int      seed = 0;
    float    wear = 0.0001f;
    int      stattrak = -1;
    std::string custom_name;
    std::string model_override;
    bool     enabled = true;
};

class ItemDatabase {
public:
    static bool DumpFromSchema(uintptr_t econ_item_schema);
    static bool DumpFromSchema();

    static const std::vector<DumpedItemDef>& GetItems();
    static const std::vector<DumpedItemDef>& GetItems(ItemType type);
    static std::vector<DumpedItemDef> Search(std::string_view query);
    static std::vector<DumpedItemDef> Search(std::string_view query, ItemType type);
    static const DumpedItemDef* FindByDefIndex(int def_index);
    static const DumpedPaintKit* FindPaintKit(int id);

    static const std::vector<DumpedPaintKit>& GetPaintKits();
    static bool IsDumpReady() { return s_dump_ready; }
    static int GetItemCount() { return static_cast<int>(s_items.size()); }
    static int GetPaintKitCount() { return static_cast<int>(s_paint_kits.size()); }

    static bool IsKnife(int def_index);
    static bool IsGlove(int def_index);
    static bool IsWeapon(int def_index);
    static ItemType CategorizeDefIndex(int def_index);
    static int GetLoadoutSlotForDefIndex(int def_index);

private:
    static std::vector<DumpedItemDef> s_items;
    static std::vector<DumpedPaintKit> s_paint_kits;
    static bool s_dump_ready;
    static std::shared_mutex s_mutex;
};

}
