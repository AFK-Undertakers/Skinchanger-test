#include "schema.hpp"
#include "offsets.hpp"
#include "../core/memory.hpp"

namespace cs2 {

void* SchemaParser::m_schema_system = nullptr;
std::unordered_map<std::string, SchemaClass_t> SchemaParser::m_classes;
std::shared_mutex SchemaParser::m_mutex;
std::string SchemaParser::m_last_error;

bool SchemaParser::Initialize() {
    std::unique_lock lock(m_mutex);

    HMODULE hSchema = PatternScanner::GetModuleHandle(L"schemasystem.dll");
    if (!hSchema) {
        hSchema = LoadLibraryW(L"schemasystem.dll");
    }
    if (!hSchema) {
        m_last_error = "schemasystem.dll not found";
        return false;
    }

    using CreateInterfaceFn = void* (*)(const char*, int*);
    auto CreateInterface = reinterpret_cast<CreateInterfaceFn>(
        GetProcAddress(hSchema, "CreateInterface"));
    if (!CreateInterface) {
        m_last_error = "CreateInterface not found in schemasystem.dll";
        return false;
    }

    m_schema_system = CreateInterface("SchemaSystem_001", nullptr);
    if (!m_schema_system) {
        m_last_error = "Failed to get SchemaSystem_001 interface";
        return false;
    }

    // TODO: RE REQUIRED - Full schema class enumeration
    // The schema system stores classes in a CUtlVector<SchemaClassInfo_t*>
    // accessible via the ISchemaSystem interface.
    //
    // Structure (as of CS2 2024+):
    // ISchemaSystem -> m_Classes (CUtlVector<SchemaClassInfo_t*>)
    // SchemaClassInfo_t: m_pszName, m_pszModule, m_nFieldCount, m_pFields, m_nStaticFieldCount, m_pStaticFields
    //
    // For now, we populate from the dumped offsets which are reliable
    // and update them via pattern scanning when available.

    // Populate known classes from dumped offsets
    auto add_class = [&](const char* name, const std::vector<std::pair<const char*, uint32_t>>& fields) {
        SchemaClass_t cls;
        cls.name = name;
        for (const auto& [fname, foffset] : fields) {
            cls.fields.push_back({fname, foffset, 0});
        }
        m_classes[name] = std::move(cls);
    };

    add_class("C_EconItemView", {
        {"m_iItemDefinitionIndex", 0x1BA},
        {"m_iEntityQuality", 0x1BC},
        {"m_iEntityLevel", 0x1C0},
        {"m_iItemID", 0x1C8},
        {"m_iItemIDHigh", 0x1D0},
        {"m_iItemIDLow", 0x1D4},
        {"m_iAccountID", 0x1D8},
        {"m_bInitialized", 0x1E8},
        {"m_bDisallowSOC", 0x1E9},
        {"m_szCustomName", 0x2F8}
    });

    add_class("C_AttributeContainer", {
        {"m_Item", 0x50}
    });

    add_class("C_EconEntity", {
        {"m_AttributeManager", 0x1378},
        {"m_OriginalOwnerXuidLow", 0x1848},
        {"m_OriginalOwnerXuidHigh", 0x184C},
        {"m_nFallbackPaintKit", 0x1850},
        {"m_nFallbackSeed", 0x1854},
        {"m_flFallbackWear", 0x1858},
        {"m_nFallbackStatTrak", 0x185C}
    });

    add_class("C_CSWeaponBase", {
        {"m_iOriginalTeamNumber", 0x19F8},
        {"m_hPrevOwner", 0x1AB4}
    });

    add_class("C_CSPlayerPawn", {
        {"m_bNeedToReApplyGloves", 0x188D},
        {"m_EconGloves", 0x1890}
    });

    add_class("CGameSceneNode", {
        {"m_pOwner", 0x30},
        {"m_pParent", 0x38},
        {"m_pChild", 0x40},
        {"m_pNextSibling", 0x48}
    });

    return true;
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

const SchemaClass_t* SchemaParser::FindClass(std::string_view name) {
    std::shared_lock lock(m_mutex);
    auto it = m_classes.find(std::string(name));
    if (it != m_classes.end()) return &it->second;
    return nullptr;
}

uint32_t SchemaParser::GetOffset(std::string_view class_name, std::string_view field_name) {
    std::shared_lock lock(m_mutex);
    auto it = m_classes.find(std::string(class_name));
    if (it == m_classes.end()) return 0;

    for (auto& field : it->second.fields) {
        if (field.name == field_name) return field.offset;
    }
    return 0;
}

}
