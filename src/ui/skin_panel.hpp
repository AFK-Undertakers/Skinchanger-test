#pragma once

#include <string>
#include <vector>
#include "../features/skinchanger/items.hpp"

namespace cs2 {

class SkinPanel {
public:
    static void Render();
    static void Initialize();

private:
    static void RenderWeaponsTab();
    static void RenderKnivesTab();
    static void RenderGlovesTab();
    static void RenderAgentsTab();
    static void RenderMusicKitsTab();
    static void RenderStatusPanel();
    static void RenderSkinEditor(const DumpedItem& item);

    static char s_search_buffer[256];
    static SkinConfig s_current_config;
    static const DumpedItem* s_selected_item;
    static bool s_show_editor;
    static std::string s_status_log;
    static int s_selected_paint_kit;
    static float s_selected_wear;
    static int s_selected_seed;
    static int s_selected_stattrak;
    static char s_custom_name_buffer[128];
    static bool s_stattrak_enabled;
    static bool s_enabled;
};

}
