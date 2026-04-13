#include "items.hpp"
#include <algorithm>
#include <cctype>

namespace cs2 {

std::vector<DumpedItem> ItemDatabase::m_items;
std::vector<PaintKitInfo> ItemDatabase::m_paint_kits;
std::unordered_map<int, const char*> ItemDatabase::m_knife_models;
std::vector<int> ItemDatabase::m_glove_def_indices;

void ItemDatabase::Initialize() {
    m_knife_models = {
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

    m_glove_def_indices = {
        4725, 4726, 4727, 4728, 4729, 4722, 4723, 4724,
        5027, 5028, 5029, 5030, 5031, 5032, 5033, 5034, 5035
    };

    m_paint_kits = {
        {0, "Stock", ""},
        {2, "Candy Apple", ""},
        {3, "Blue Steel", ""},
        {5, "Well-Worn", ""},
        {8, "Anodized Navy", ""},
        {28, "Night", ""},
        {30, "Ultraviolet", ""},
        {32, "Fade", ""},
        {37, "Safari Mesh", ""},
        {38, "Boreal Forest", ""},
        {39, "Forest DDPAT", ""},
        {40, "Urban Masked", ""},
        {42, "Sand Dune", ""},
        {43, "Tiger Tooth", ""},
        {44, "Damascus Steel", ""},
        {46, "Rust Coat", ""},
        {47, "Doppler", ""},
        {48, "Marble Fade", ""},
        {59, "Crimson Web", ""},
        {60, "Slaughter", ""},
        {61, "Case Hardened", ""},
        {67, "Stained", ""},
        {70, "Scorched", ""},
        {143, "Hypnotic", ""},
        {180, "Water Elemental", ""},
        {222, "Serum", ""},
        {223, "Bloodsport", ""},
        {226, "Asiimov", ""},
        {235, "Hyper Beast", ""},
        {246, "Neon Rider", ""},
        {300, "Printstream", ""},
        {340, "Neo-Noir", ""},
        {380, "The Prince", ""},
        {416, "Phantom Disruptor", ""},
        {456, "Welcome to the Jungle", ""},
        {515, "Kill Confirmed", ""},
        {600, "In Living Color", ""},
        {694, "Wasteland Princess", ""},
        {707, "Gold Arabesque", ""},
        {800, "X-Ray", ""},
        {900, "Cyrex", ""},
        {1000, "Vulcan", ""}
    };

    struct WeaponDef { int def; const char* name; const char* model; };
    static const WeaponDef weapons[] = {
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

    m_items.clear();

    for (const auto& w : weapons) {
        DumpedItem item;
        item.definition_index = w.def;
        item.name = w.name;
        item.model_path = w.model;
        item.category = ItemCategory::Weapon;
        item.paint_kits = m_paint_kits;
        m_items.push_back(std::move(item));
    }

    for (const auto& [def, model] : m_knife_models) {
        DumpedItem item;
        item.definition_index = def;
        item.model_path = model;
        item.category = ItemCategory::Knife;

        std::string name = model;
        size_t start = name.find_last_of('/');
        if (start != std::string::npos) {
            name = name.substr(start + 1);
            size_t dot = name.find(".mdl");
            if (dot != std::string::npos) name = name.substr(0, dot);
            if (name.find("v_knife_") == 0) {
                name = name.substr(8);
                for (auto& c : name) if (c == '_') c = ' ';
                if (!name.empty()) name[0] = std::toupper(name[0]);
            }
        }
        item.name = name;
        item.paint_kits = m_paint_kits;
        m_items.push_back(std::move(item));
    }

    const char* glove_names[] = {
        "Sport Gloves | Pandora's Box",
        "Sport Gloves | Hedge Maze",
        "Sport Gloves | Arbitration",
        "Sport Gloves | Slingshot",
        "Sport Gloves | Big Game",
        "Sport Gloves | Scarlet Shamagh",
        "Sport Gloves | Vice",
        "Sport Gloves | Wasteland Rebel",
        "Moto Gloves | Spearmint",
        "Moto Gloves | Boom!",
        "Moto Gloves | 3rd Commando Company",
        "Moto Gloves | Turtleneck",
        "Moto Gloves | Transport",
        "Moto Gloves | Polygon",
        "Specialist Gloves | Crimson Kimono",
        "Specialist Gloves | Emerald Web",
        "Specialist Gloves | Foundation"
    };

    for (size_t i = 0; i < m_glove_def_indices.size() && i < 17; ++i) {
        DumpedItem item;
        item.definition_index = m_glove_def_indices[i];
        item.name = glove_names[i];
        item.category = ItemCategory::Gloves;
        item.paint_kits = m_paint_kits;
        m_items.push_back(std::move(item));
    }
}

const std::vector<DumpedItem>& ItemDatabase::GetItems() {
    return m_items;
}

const std::vector<DumpedItem>& ItemDatabase::GetItems(ItemCategory category) {
    static std::vector<DumpedItem> filtered;
    filtered.clear();
    for (const auto& item : m_items) {
        if (item.category == category) filtered.push_back(item);
    }
    return filtered;
}

std::vector<DumpedItem> ItemDatabase::Search(std::string_view query) {
    std::vector<DumpedItem> results;
    if (query.empty()) return m_items;

    std::string lower_query;
    lower_query.reserve(query.size());
    for (char c : query) lower_query += std::tolower(static_cast<unsigned char>(c));

    for (const auto& item : m_items) {
        std::string lower_name = item.name;
        std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(),
            [](unsigned char c) { return std::tolower(c); });
        if (lower_name.find(lower_query) != std::string::npos)
            results.push_back(item);
    }
    return results;
}

std::vector<DumpedItem> ItemDatabase::Search(std::string_view query, ItemCategory category) {
    std::vector<DumpedItem> results;
    if (query.empty()) return GetItems(category);

    std::string lower_query;
    lower_query.reserve(query.size());
    for (char c : query) lower_query += std::tolower(static_cast<unsigned char>(c));

    for (const auto& item : m_items) {
        if (item.category != category) continue;
        std::string lower_name = item.name;
        std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(),
            [](unsigned char c) { return std::tolower(c); });
        if (lower_name.find(lower_query) != std::string::npos)
            results.push_back(item);
    }
    return results;
}

const DumpedItem* ItemDatabase::FindByDefIndex(int def_index) {
    for (const auto& item : m_items) {
        if (item.definition_index == def_index) return &item;
    }
    return nullptr;
}

const char* ItemDatabase::GetKnifeModel(int def_index) {
    auto it = m_knife_models.find(def_index);
    return (it != m_knife_models.end()) ? it->second : nullptr;
}

bool ItemDatabase::IsKnife(int def_index) {
    return m_knife_models.count(def_index) > 0;
}

bool ItemDatabase::IsGlove(int def_index) {
    for (int g : m_glove_def_indices)
        if (g == def_index) return true;
    return false;
}

bool ItemDatabase::IsWeapon(int def_index) {
    return !IsKnife(def_index) && !IsGlove(def_index) && def_index > 0 && def_index < 500;
}

ItemCategory ItemDatabase::CategorizeDefIndex(int def_index) {
    if (IsKnife(def_index)) return ItemCategory::Knife;
    if (IsGlove(def_index)) return ItemCategory::Gloves;
    if (def_index >= 5000) return ItemCategory::Agent;
    if (def_index > 0 && def_index < 500) return ItemCategory::Weapon;
    return ItemCategory::Unknown;
}

int ItemDatabase::GetLoadoutSlotForDefIndex(int def_index) {
    if (IsKnife(def_index)) return static_cast<int>(LoadoutSlot::Knife);

    static const int primary[] = {7,8,9,10,11,13,14,16,17,19,23,24,25,26,27,28,29,33,34,35,38,39,40,60};
    for (int p : primary) if (def_index == p) return static_cast<int>(LoadoutSlot::Primary);

    static const int secondary[] = {1,2,3,4,30,32,36,61,63,64};
    for (int s : secondary) if (def_index == s) return static_cast<int>(LoadoutSlot::Secondary);

    if (IsGlove(def_index)) return static_cast<int>(LoadoutSlot::Hands);
    return -1;
}

const std::vector<PaintKitInfo>& ItemDatabase::GetPaintKits() {
    return m_paint_kits;
}

}
