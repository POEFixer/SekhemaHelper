#pragma once
// ChestTypes.h — registry of every Trial of the Sekhemas chest content type.
//
// Source of truth: Chests.dat (Metadata/Chests/MarakethSanctum/*). The last
// path segment encodes tier + content + quality, e.g. "GoldChestRadiusJewels1"
// -> tier Gold, content "RadiusJewels", quality 1. Quality digit: 1 = base,
// 2 = "Superior", 3 = "Prime" (a few one-offs like GoldChestGrandSpectrum have
// no digit). "Burial" is the synthetic id for the untiered
// SmallChests/BurialChamber{Pot,SmallChest,Urn}_NN breakables.
//
// Registry order = default highlight priority (top = most valuable).
#include <imgui.h>
#include <cstddef>
#include <string>

namespace sekhema {

struct ChestTypeInfo {
    const char* id;        // exact content token parsed from the metadata path
    const char* uiName;    // Overlays-tab row name (in-game cache name)
    const char* mapLabel;  // short label drawn on the map
    ImVec4      defaultColor;
    bool        defaultShow;
    bool        defaultHighlight;
};

inline const ChestTypeInfo* ChestTypeRegistry(size_t& count) {
    static const ChestTypeInfo kTypes[] = {
        { "GrandSpectrum", "Spectrum Cache (Grand Spectrum)",  "Spectrum", ImVec4(1.00f, 0.30f, 1.00f, 1.0f), true,  true  },
        { "RadiusJewels",  "Time-Lost Cache (Radius Jewel)",   "TimeLost", ImVec4(0.65f, 0.40f, 1.00f, 1.0f), true,  true  },
        { "LargeRelic",    "Large Relic Cache",                "Relic L",  ImVec4(0.30f, 1.00f, 0.90f, 1.0f), true,  true  },
        { "Jewels",        "Royal Cache (Jewels)",             "Jewels",   ImVec4(0.55f, 0.55f, 1.00f, 1.0f), true,  true  },
        { "Currency",      "Arcanist's Cache (Currency)",      "Currency", ImVec4(1.00f, 0.85f, 0.30f, 1.0f), true,  false },
        { "MediumRelic",   "Medium Relic Cache",               "Relic M",  ImVec4(0.25f, 0.85f, 0.75f, 1.0f), true,  false },
        { "Maps",          "Cartographer's Cache (Waystones)", "Maps",     ImVec4(1.00f, 0.60f, 0.20f, 1.0f), true,  false },
        { "Ring",          "Ring Cache",                       "Ring",     ImVec4(1.00f, 0.50f, 0.70f, 1.0f), true,  false },
        { "Amulet",        "Amulet Cache",                     "Amulet",   ImVec4(1.00f, 0.55f, 0.45f, 1.0f), true,  false },
        { "SmallRelic",    "Small Relic Cache",                "Relic S",  ImVec4(0.20f, 0.70f, 0.60f, 1.0f), true,  false },
        { "Gold",          "Hoarder's Cache (Gold)",           "Gold",     ImVec4(0.95f, 0.80f, 0.10f, 1.0f), true,  false },
        { "Belt",          "Belt Cache",                       "Belt",     ImVec4(0.85f, 0.70f, 0.50f, 1.0f), true,  false },
        { "BodyArmour",    "Body Armour Cache",                "Body",     ImVec4(0.90f, 0.35f, 0.30f, 1.0f), true,  false },
        { "Helmet",        "Helmet Cache",                     "Helm",     ImVec4(0.95f, 0.50f, 0.40f, 1.0f), true,  false },
        { "Gloves",        "Gloves Cache",                     "Gloves",   ImVec4(0.90f, 0.60f, 0.50f, 1.0f), true,  false },
        { "Boots",         "Boots Cache",                      "Boots",    ImVec4(0.80f, 0.55f, 0.35f, 1.0f), true,  false },
        { "Shield",        "Shield Cache",                     "Shield",   ImVec4(0.50f, 0.70f, 0.90f, 1.0f), true,  false },
        { "MeleeWeapons",  "Warrior's Cache (Melee)",          "Melee",    ImVec4(0.85f, 0.30f, 0.50f, 1.0f), true,  false },
        { "RangedWeapons", "Ranger's Cache (Ranged)",          "Ranged",   ImVec4(0.40f, 0.85f, 0.40f, 1.0f), true,  false },
        { "CasterWeapons", "Mage's Cache (Caster)",            "Caster",   ImVec4(0.35f, 0.60f, 1.00f, 1.0f), true,  false },
        { "Generic",       "Mysterious Cache (Generic)",       "Myst",     ImVec4(0.75f, 0.75f, 0.75f, 1.0f), true,  false },
        { "Burial",        "Pots / Urns / Small Chests",       "Pot",      ImVec4(0.50f, 0.50f, 0.50f, 1.0f), false, false },
    };
    count = sizeof(kTypes) / sizeof(kTypes[0]);
    return kTypes;
}

inline const ChestTypeInfo* FindChestTypeInfo(const std::string& id) {
    size_t n = 0;
    const ChestTypeInfo* reg = ChestTypeRegistry(n);
    for (size_t i = 0; i < n; ++i)
        if (id == reg[i].id) return &reg[i];
    return nullptr;
}

} // namespace sekhema
