#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

// Forward declarations (provided by your base SDK)
class C_CSPlayerPawn;
class C_CSWeaponBase;
class C_BaseViewModel;
class C_EconItemView;
class CEconItem;
class CEconItemDefinition;
struct C_PaintKit;

namespace SkinChanger
{
    // ------------------------------------------------------------
    // Configuration structures
    // ------------------------------------------------------------
    struct SkinConfig
    {
        uint64_t item_id = 0;           // Assigned after creation
        int      definition_index = 0;
        int      paint_kit = 0;
        int      seed = 0;
        float    wear = 0.0001f;
        int      stat_trak = -1;
        std::string custom_name;
        bool     legacy = false;
    };

    // Dumped item info (for UI)
    struct DumpedSkin
    {
        std::string name;
        int         id = 0;
        int         rarity = 0;
        bool        legacy = false;
        std::string token;
    };

    struct DumpedItem
    {
        std::string name;
        uint16_t    def_index = 0;
        int         rarity = 0;
        std::string simple_name;
        bool        is_weapon = false;
        bool        is_knife = false;
        bool        is_glove = false;
        bool        is_agent = false;
        std::vector<DumpedSkin> skins;
    };

    // ------------------------------------------------------------
    // Public API
    // ------------------------------------------------------------
    bool Initialize();                               // Call once after interfaces ready
    void Shutdown();                                 // Call on cheat unload

    void OnFrameStageNotify(int stage);              // Hook FrameStageNotify (stage==6)
    void OnSetModel(C_BaseViewModel* vm, const char*& model); // Hook SetModel (knife flicker fix)
    void OnEquipItemInLoadout(int team, int slot, uint64_t item_id); // Hook EquipItemInLoadout

    bool DumpItemSchema();                           // Extract all items/paintkits from game memory
    const std::vector<DumpedItem>& GetDumpedItems(); // Get cached dump for UI
    bool IsDumped() const;

    uint64_t ApplySkin(const SkinConfig& cfg);       // Create & equip skin, returns item ID
    bool RemoveSkin(uint64_t item_id);               // Delete skin from inventory
    void RemoveAllSkins();                           // Delete all added skins
    bool EquipItem(int team, int slot, uint64_t item_id); // Force equip an item

    const SkinConfig* GetConfigForDefinition(int def_index) const; // Get current skin for weapon

    bool LoadConfig(const std::string& path);        // JSON config load/save
    bool SaveConfig(const std::string& path);

    void ForceViewModelUpdate(C_BaseViewModel* vm = nullptr); // Force material refresh

    inline bool enabled = true;
}