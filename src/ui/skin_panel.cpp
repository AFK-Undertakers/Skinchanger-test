#include "skin_panel.hpp"
#include "../features/skinchanger/skinchanger.hpp"
#include "../core/logger.hpp"
#include <imgui.h>
#include <algorithm>
#include <cstring>

namespace cs2 {

char SkinPanel::s_search_buffer[256] = {0};
SkinConfig SkinPanel::s_current_config;
const DumpedItemDef* SkinPanel::s_selected_item = nullptr;
bool SkinPanel::s_show_editor = false;
std::string SkinPanel::s_status_log = "Ready";
int SkinPanel::s_selected_paint_kit = 0;
float SkinPanel::s_selected_wear = 0.0f;
int SkinPanel::s_selected_seed = 0;
int SkinPanel::s_selected_stattrak = 0;
char SkinPanel::s_custom_name_buffer[128] = {0};
bool SkinPanel::s_stattrak_enabled = false;
bool SkinPanel::s_enabled = true;

void SkinPanel::Initialize() {
    std::memset(s_search_buffer, 0, sizeof(s_search_buffer));
    std::memset(s_custom_name_buffer, 0, sizeof(s_custom_name_buffer));
    s_current_config = {};
    s_selected_item = nullptr;
    s_show_editor = false;
    s_status_log = "Ready";
    s_selected_paint_kit = 0;
    s_selected_wear = 0.0f;
    s_selected_seed = 0;
    s_selected_stattrak = 0;
    s_stattrak_enabled = false;
    s_enabled = true;
}

void SkinPanel::Render() {
    if (!SkinChanger::IsInitialized()) {
        ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "SkinChanger: Not initialized");
        return;
    }

    ImGui::BeginChild("SkinChangerMain", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() - 10), true);

    // Search bar
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##search", "Search items...", s_search_buffer, sizeof(s_search_buffer));

    ImGui::Separator();

    // Tabs
    if (ImGui::BeginTabBar("SkinTabs")) {
        if (ImGui::BeginTabItem("Weapons")) {
            RenderWeaponsTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Knives")) {
            RenderKnivesTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Gloves")) {
            RenderGlovesTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Agents")) {
            RenderAgentsTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Music Kits")) {
            RenderMusicKitsTab();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::EndChild();

    // Status panel
    RenderStatusPanel();

    // Skin editor modal
    if (s_show_editor && s_selected_item) {
        ImGui::OpenPopup("Edit Skin");
        s_show_editor = false;
    }

    if (ImGui::BeginPopupModal("Edit Skin", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        RenderSkinEditor(*s_selected_item);
        ImGui::EndPopup();
    }
}

void SkinPanel::RenderWeaponsTab() {
    auto items = ItemDatabase::Search(s_search_buffer, ItemType::Weapon);

    if (ImGui::BeginTable("WeaponsTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, 300))) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("DefIndex", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 120);
        ImGui::TableHeadersRow();

        for (const auto& item : items) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", item.name.c_str());

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%d", item.def_index);

            ImGui::TableSetColumnIndex(2);
            if (ImGui::SmallButton(("Edit##" + std::to_string(item.def_index)).c_str())) {
                s_selected_item = &item;
                s_current_config.definition_index = item.def_index;
                s_current_config.paint_kit_id = 0;
                s_current_config.seed = 0;
                s_current_config.wear = 0.0001f;
                s_current_config.stattrak = -1;
                s_current_config.custom_name.clear();
                s_current_config.enabled = true;
                s_selected_paint_kit = 0;
                s_selected_wear = 0.0001f;
                s_selected_seed = 0;
                s_selected_stattrak = 0;
                s_stattrak_enabled = false;
                std::memset(s_custom_name_buffer, 0, sizeof(s_custom_name_buffer));
                s_show_editor = true;
            }
        }
        ImGui::EndTable();
    }
}

void SkinPanel::RenderKnivesTab() {
    auto items = ItemDatabase::Search(s_search_buffer, ItemType::Knife);

    if (ImGui::BeginTable("KnivesTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, 300))) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("DefIndex", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 120);
        ImGui::TableHeadersRow();

        for (const auto& item : items) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", item.name.c_str());

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%d", item.def_index);

            ImGui::TableSetColumnIndex(2);
            if (ImGui::SmallButton(("Edit##" + std::to_string(item.def_index)).c_str())) {
                s_selected_item = &item;
                s_current_config.definition_index = item.def_index;
                s_current_config.paint_kit_id = 0;
                s_current_config.seed = 0;
                s_current_config.wear = 0.0001f;
                s_current_config.stattrak = -1;
                s_current_config.custom_name.clear();
                s_current_config.enabled = true;
                s_selected_paint_kit = 0;
                s_selected_wear = 0.0001f;
                s_selected_seed = 0;
                s_selected_stattrak = 0;
                s_stattrak_enabled = false;
                std::memset(s_custom_name_buffer, 0, sizeof(s_custom_name_buffer));
                s_show_editor = true;
            }
        }
        ImGui::EndTable();
    }
}

void SkinPanel::RenderGlovesTab() {
    auto items = ItemDatabase::Search(s_search_buffer, ItemType::Glove);

    if (ImGui::BeginTable("GlovesTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, 300))) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("DefIndex", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 120);
        ImGui::TableHeadersRow();

        for (const auto& item : items) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", item.name.c_str());

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%d", item.def_index);

            ImGui::TableSetColumnIndex(2);
            if (ImGui::SmallButton(("Edit##" + std::to_string(item.def_index)).c_str())) {
                s_selected_item = &item;
                s_current_config.definition_index = item.def_index;
                s_current_config.paint_kit_id = 0;
                s_current_config.seed = 0;
                s_current_config.wear = 0.0001f;
                s_current_config.stattrak = -1;
                s_current_config.custom_name.clear();
                s_current_config.enabled = true;
                s_selected_paint_kit = 0;
                s_selected_wear = 0.0001f;
                s_selected_seed = 0;
                s_selected_stattrak = 0;
                s_stattrak_enabled = false;
                std::memset(s_custom_name_buffer, 0, sizeof(s_custom_name_buffer));
                s_show_editor = true;
            }
        }
        ImGui::EndTable();
    }
}

void SkinPanel::RenderAgentsTab() {
    ImGui::TextColored(ImVec4(1, 0.8f, 0.3f, 1), "Agent support requires model hooking.");
    ImGui::Text("TODO: RE REQUIRED - Agent model changing via SetModel hook.");
    ImGui::Text("Hook OnFrameStageNotify at FRAME_START and call SetModel on pawn entity.");
}

void SkinPanel::RenderMusicKitsTab() {
    ImGui::TextColored(ImVec4(1, 0.8f, 0.3f, 1), "Music Kit support requires sound event hooking.");
    ImGui::Text("TODO: RE REQUIRED - Music Kit application via sound event hook.");
    ImGui::Text("Hook sound event system and write player controller music kit field.");
}

void SkinPanel::RenderSkinEditor(const DumpedItemDef& item) {
    ImGui::Text("Editing: %s (DefIndex: %d)", item.name.c_str(), item.def_index);
    ImGui::Separator();

    const auto& paint_kits = ItemDatabase::GetPaintKits();

    // Paint Kit dropdown
    ImGui::Text("Paint Kit:");
    ImGui::SetNextItemWidth(-1);
    const char* current_kit = "Stock";
    if (!paint_kits.empty() && s_selected_paint_kit < static_cast<int>(paint_kits.size()))
        current_kit = paint_kits[s_selected_paint_kit].description.c_str();
    if (ImGui::BeginCombo("##paintkit", current_kit)) {
        for (size_t i = 0; i < paint_kits.size(); ++i) {
            bool is_selected = (s_selected_paint_kit == static_cast<int>(i));
            if (ImGui::Selectable(paint_kits[i].description.c_str(), is_selected))
                s_selected_paint_kit = static_cast<int>(i);
            if (is_selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    // Wear slider
    ImGui::Text("Wear: %.3f", s_selected_wear);
    ImGui::SetNextItemWidth(-1);
    ImGui::SliderFloat("##wear", &s_selected_wear, 0.0f, 1.0f, "%.3f");

    const char* wear_names[] = {"Factory New", "Minimal Wear", "Field-Tested", "Well-Worn", "Battle-Scarred"};
    float wear_presets[] = {0.0f, 0.07f, 0.15f, 0.38f, 0.45f};
    ImGui::SameLine();
    for (int i = 0; i < 5; ++i) {
        if (ImGui::SmallButton(wear_names[i])) {
            s_selected_wear = wear_presets[i];
        }
        if (i < 4) ImGui::SameLine();
    }

    // Seed
    ImGui::Text("Seed:");
    ImGui::SetNextItemWidth(-1);
    ImGui::SliderInt("##seed", &s_selected_seed, 0, 1000);

    // StatTrak
    ImGui::Checkbox("StatTrak", &s_stattrak_enabled);
    if (s_stattrak_enabled) {
        ImGui::SameLine();
        ImGui::Text("Count:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        ImGui::InputInt("##stattrak", &s_selected_stattrak, 0, 0);
    }

    // Custom name
    ImGui::Text("Nametag:");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##customname", s_custom_name_buffer, sizeof(s_custom_name_buffer));

    // Enabled
    ImGui::Checkbox("Enabled", &s_enabled);

    ImGui::Separator();

    // Buttons
    if (ImGui::Button("Apply", ImVec2(100, 0))) {
        s_current_config.paint_kit_id = paint_kits.empty() ? 0 : paint_kits[s_selected_paint_kit].id;
        s_current_config.seed = s_selected_seed;
        s_current_config.wear = s_selected_wear;
        s_current_config.stattrak = s_stattrak_enabled ? s_selected_stattrak : -1;
        s_current_config.custom_name = s_custom_name_buffer;
        s_current_config.enabled = s_enabled;

        if (SkinChanger::ApplySkin(s_current_config)) {
            s_status_log = "Applied: " + item.name;
            SkinChanger::ForceViewModelUpdate();
            SC_LOG_UI("Apply", item.name + " (kit:" + std::to_string(s_current_config.paint_kit_id) + ")");
        } else {
            s_status_log = "Failed to apply skin";
            SC_LOG_WARN("UI: Failed to apply skin for " + item.name);
        }
        ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(100, 0))) {
        SC_LOG_UI("Cancel", "Skin editor closed for " + item.name);
        ImGui::CloseCurrentPopup();
    }
}

void SkinPanel::RenderStatusPanel() {
    ImGui::Separator();
    ImGui::BeginChild("StatusPanel", ImVec2(0, 80), true);

    ImGui::TextColored(ImVec4(0.3f, 1, 0.3f, 1), "Status: %s", SkinChanger::GetStatusMessage().c_str());
    ImGui::Text("Active skins: %d", SkinChanger::GetActiveSkinCount());
    ImGui::Text("Last action: %s", s_status_log.c_str());

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5);

    if (ImGui::Button("Save Config", ImVec2(120, 0))) {
        if (SkinChanger::SaveConfig()) {
            s_status_log = "Config saved";
            SC_LOG_UI("SaveConfig", "Configuration saved successfully");
        } else {
            s_status_log = "Failed to save config";
            SC_LOG_ERROR("UI: Failed to save config");
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Load Config", ImVec2(120, 0))) {
        if (SkinChanger::LoadConfig()) {
            s_status_log = "Config loaded";
            SC_LOG_UI("LoadConfig", "Configuration loaded successfully");
        } else {
            s_status_log = "Failed to load config";
            SC_LOG_ERROR("UI: Failed to load config");
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Force Update VM", ImVec2(140, 0))) {
        SkinChanger::ForceViewModelUpdate();
        s_status_log = "ViewModel update forced";
        SC_LOG_UI("ForceUpdateVM", "ViewModel update triggered");
    }

    ImGui::SameLine();
    if (ImGui::Button("Remove All", ImVec2(120, 0))) {
        SkinChanger::RemoveAllSkins();
        s_status_log = "All skins removed";
        SC_LOG_UI("RemoveAllSkins", "All skins removed");
    }

    ImGui::EndChild();
}

}
