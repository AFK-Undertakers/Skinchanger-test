#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace cs2 {

enum class ItemCategory : uint8_t {
    Weapon = 0,
    Knife = 1,
    Gloves = 2,
    Agent = 3,
    MusicKit = 4,
    Sticker = 5,
    Graffiti = 6,
    Tool = 7,
    Unknown = 255
};

enum class LoadoutSlot : uint8_t {
    Primary = 0,
    Secondary = 1,
    Knife = 2,
    C4 = 3,
    Face = 4,
    Head = 5,
    Ears = 6,
    Hands = 7,
    Body = 8,
    Legs = 9,
    Feet = 10,
    Music = 11,
    Max = 56
};

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

class ItemDatabase {
public:
    static void Initialize();
    static const std::vector<DumpedItem>& GetItems();
    static const std::vector<DumpedItem>& GetItems(ItemCategory category);
    static std::vector<DumpedItem> Search(std::string_view query);
    static std::vector<DumpedItem> Search(std::string_view query, ItemCategory category);
    static const DumpedItem* FindByDefIndex(int def_index);
    static const char* GetKnifeModel(int def_index);
    static bool IsKnife(int def_index);
    static bool IsGlove(int def_index);
    static bool IsWeapon(int def_index);
    static ItemCategory CategorizeDefIndex(int def_index);
    static int GetLoadoutSlotForDefIndex(int def_index);
    static const std::vector<PaintKitInfo>& GetPaintKits();

private:
    static std::vector<DumpedItem> m_items;
    static std::vector<PaintKitInfo> m_paint_kits;
    static std::unordered_map<int, const char*> m_knife_models;
    static std::vector<int> m_glove_def_indices;
};

}
