#pragma once

#include <cstddef>
#include <cstdint>

namespace cs2 {
namespace offsets {

namespace client_dll {
    constexpr std::ptrdiff_t dwCSGOInput = 0x231E330;
    constexpr std::ptrdiff_t dwEntityList = 0x24B3268;
    constexpr std::ptrdiff_t dwGameEntitySystem = 0x24B3268;
    constexpr std::ptrdiff_t dwGameEntitySystem_highestEntityIndex = 0x20A0;
    constexpr std::ptrdiff_t dwGameRules = 0x2311ED0;
    constexpr std::ptrdiff_t dwGlobalVars = 0x2062540;
    constexpr std::ptrdiff_t dwLocalPlayerController = 0x22F8028;
    constexpr std::ptrdiff_t dwLocalPlayerPawn = 0x206D9E0;
    constexpr std::ptrdiff_t dwViewMatrix = 0x2313F10;
    constexpr std::ptrdiff_t dwViewAngles = 0x231E9B8;
    constexpr std::ptrdiff_t Source2Client002 = 0x230CDD0;
    constexpr std::ptrdiff_t Source2ClientPrediction001 = 0x206D8F0;
}

namespace engine2_dll {
    constexpr std::ptrdiff_t GameResourceServiceClientV001 = 0x614A30;
    constexpr std::ptrdiff_t Source2EngineToClient001 = 0x611BE0;
}

namespace schemasystem_dll {
    constexpr std::ptrdiff_t SchemaSystem_001 = 0x76730;
}

namespace filesystem_stdio_dll {
    constexpr std::ptrdiff_t VFileSystem017 = 0x214730;
}

namespace inputsystem_dll {
    constexpr std::ptrdiff_t InputSystemVersion001 = 0x45AD0;
}

namespace tier0_dll {
    constexpr std::ptrdiff_t VEngineCvar007 = 0x3A33B0;
}

namespace materialsystem2_dll {
    constexpr std::ptrdiff_t VMaterialSystem2_001 = 0x163E80;
}

namespace localize_dll {
    constexpr std::ptrdiff_t Localize_001 = 0x57E20;
}

// C_EconItemView schema offsets (client_dll)
namespace C_EconItemView {
    constexpr std::ptrdiff_t m_iItemDefinitionIndex = 0x1BA;
    constexpr std::ptrdiff_t m_iEntityQuality = 0x1BC;
    constexpr std::ptrdiff_t m_iEntityLevel = 0x1C0;
    constexpr std::ptrdiff_t m_iItemID = 0x1C8;
    constexpr std::ptrdiff_t m_iItemIDHigh = 0x1D0;
    constexpr std::ptrdiff_t m_iItemIDLow = 0x1D4;
    constexpr std::ptrdiff_t m_iAccountID = 0x1D8;
    constexpr std::ptrdiff_t m_iInventoryPosition = 0x1DC;
    constexpr std::ptrdiff_t m_bInitialized = 0x1E8;
    constexpr std::ptrdiff_t m_bDisallowSOC = 0x1E9;
    constexpr std::ptrdiff_t m_bIsStoreItem = 0x1EA;
    constexpr std::ptrdiff_t m_bIsTradeItem = 0x1EB;
    constexpr std::ptrdiff_t m_AttributeList = 0x208;
    constexpr std::ptrdiff_t m_NetworkedDynamicAttributes = 0x280;
    constexpr std::ptrdiff_t m_szCustomName = 0x2F8;
    constexpr std::ptrdiff_t m_szCustomNameOverride = 0x399;
}

// C_AttributeContainer schema offsets
namespace C_AttributeContainer {
    constexpr std::ptrdiff_t m_Item = 0x50;
}

// C_EconEntity schema offsets
namespace C_EconEntity {
    constexpr std::ptrdiff_t m_AttributeManager = 0x1378;
    constexpr std::ptrdiff_t m_OriginalOwnerXuidLow = 0x1848;
    constexpr std::ptrdiff_t m_OriginalOwnerXuidHigh = 0x184C;
    constexpr std::ptrdiff_t m_nFallbackPaintKit = 0x1850;
    constexpr std::ptrdiff_t m_nFallbackSeed = 0x1854;
    constexpr std::ptrdiff_t m_flFallbackWear = 0x1858;
    constexpr std::ptrdiff_t m_nFallbackStatTrak = 0x185C;
    constexpr std::ptrdiff_t m_vecAttachedModels = 0x18A8;
}

// C_CSWeaponBase schema offsets
namespace C_CSWeaponBase {
    constexpr std::ptrdiff_t m_iOriginalTeamNumber = 0x19F8;
    constexpr std::ptrdiff_t m_hPrevOwner = 0x1AB4;
}

// C_CSPlayerPawn schema offsets
namespace C_CSPlayerPawn {
    constexpr std::ptrdiff_t m_bNeedToReApplyGloves = 0x188D;
    constexpr std::ptrdiff_t m_EconGloves = 0x1890;
    constexpr std::ptrdiff_t m_pClippingWeapon = 0x3DC0;
}

// CCSPlayerController schema offsets
namespace CCSPlayerController {
    constexpr std::ptrdiff_t m_pInGameMoneyServices = 0x808;
    constexpr std::ptrdiff_t m_pInventoryServices = 0x810;
    constexpr std::ptrdiff_t m_hPlayerPawn = 0x90C;
    constexpr std::ptrdiff_t m_hOriginalControllerOfCurrentPawn = 0x930;
}

// CGameSceneNode schema offsets
namespace CGameSceneNode {
    constexpr std::ptrdiff_t m_pOwner = 0x30;
    constexpr std::ptrdiff_t m_pParent = 0x38;
    constexpr std::ptrdiff_t m_pChild = 0x40;
    constexpr std::ptrdiff_t m_pNextSibling = 0x48;
    constexpr std::ptrdiff_t m_hParent = 0x78;
}

// C_BaseEntity schema offsets
namespace C_BaseEntity {
    constexpr std::ptrdiff_t m_pGameSceneNode = 0x338;
    constexpr std::ptrdiff_t m_iHealth = 0x344;
    constexpr std::ptrdiff_t m_lifeState = 0x348;
    constexpr std::ptrdiff_t m_iTeamNum = 0x3E3;
}

// C_BaseViewModel schema offsets
namespace C_BaseViewModel {
    constexpr std::ptrdiff_t m_hWeapon = 0x10E8;
    constexpr std::ptrdiff_t m_nViewModelIndex = 0x10F0;
    constexpr std::ptrdiff_t m_nAnimationParity = 0x10F4;
    constexpr std::ptrdiff_t m_hControlPanel = 0x10FC;
}

// C_CSGOViewModel schema offsets
namespace C_CSGOViewModel {
    constexpr std::ptrdiff_t m_bShouldIgnoreOffsetAndAccuracy = 0x1118;
}

// CCSPlayer_ViewModelServices
namespace CCSPlayer_ViewModelServices {
    constexpr std::ptrdiff_t m_hViewModel = 0x40;
}

// CEntityIdentity schema offsets
namespace CEntityIdentity {
    constexpr std::ptrdiff_t m_designerName = 0x20;
    constexpr std::ptrdiff_t m_flags = 0x18;
    constexpr std::ptrdiff_t m_worldGroupId = 0x48;
    constexpr std::ptrdiff_t m_fDataObjectTypes = 0x1C;
}

// CEntityInstance schema offsets
namespace CEntityInstance {
    constexpr std::ptrdiff_t m_pEntity = 0x10;
    constexpr std::ptrdiff_t m_CScriptComponent = 0x38;
}

// GameEntitySystem
namespace CGameEntitySystem {
    constexpr std::ptrdiff_t highestEntityIndex = 0x20A0;
}

}
}
