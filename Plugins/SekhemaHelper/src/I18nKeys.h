#pragma once
// I18nKeys.h — the lang/en.json keys the plugin builds at run time for its own English
// catalogs: afflictions (AfflictionCatalog.h), room types and rewards (RoomClassifier.h,
// WeightProfiles.h), floors and chest types (ChestTypes.h). The run history, the weight
// tables and the worker keep the English names; Text.h turns them into the program
// language when they are drawn. The prefixes are listed in lang/dynamic_prefixes.txt, and
// tests/test_i18n.cpp walks every catalog and checks each key against lang/en.json.
// STL only (the tests build it without the SDK).
#include <string>
#include <string_view>

namespace sekhema::keys {

inline constexpr std::string_view kAfflictionName = "affliction_name_";
inline constexpr std::string_view kAfflictionDesc = "affliction_desc_";
inline constexpr std::string_view kRoomType       = "room_";
inline constexpr std::string_view kReward         = "reward_";
inline constexpr std::string_view kFloor          = "floor_";
inline constexpr std::string_view kChestName      = "chest_name_";
inline constexpr std::string_view kChestLabel     = "chest_label_";

// "Tradition's Demand" -> "traditions_demand", "GrandSpectrum" -> "grand_spectrum",
// "No-Hit" -> "no_hit": ASCII letters and digits, lower-cased; a word break (a space, a
// punctuation mark, a lower-case letter or digit followed by a capital) becomes one '_';
// apostrophes are dropped. Anything else (a name read in another game language) leaves
// no trace, so such a name builds no key of lang/en.json.
inline std::string Slug(std::string_view text) {
    std::string out;
    bool wordBreak = false;
    char previous = 0;
    for (const char c : text) {
        if (c == '\'') continue;
        const bool lower = c >= 'a' && c <= 'z';
        const bool upper = c >= 'A' && c <= 'Z';
        const bool digit = c >= '0' && c <= '9';
        if (!lower && !upper && !digit) {
            wordBreak = true;
            previous = c;
            continue;
        }
        if (upper && ((previous >= 'a' && previous <= 'z') || (previous >= '0' && previous <= '9')))
            wordBreak = true;
        if (wordBreak && !out.empty()) out.push_back('_');
        wordBreak = false;
        out.push_back(upper ? static_cast<char>(c - 'A' + 'a') : c);
        previous = c;
    }
    return out;
}

// The key of an English catalog name under one of the prefixes above.
inline std::string Make(std::string_view prefix, std::string_view english) {
    return std::string(prefix) + Slug(english);
}

// The first "<number>%" of a catalog description ("100%"), "" when it has none: the
// {percent} argument of its affliction_desc_ text. A text never holds a percent sign
// before a word ("100% increased"): the translation gate reads "% i" as a printf
// conversion, so the number comes from the catalog through the placeholder.
inline std::string FirstPercent(std::string_view text) {
    const size_t percent = text.find('%');
    if (percent == std::string_view::npos) return std::string();
    size_t start = percent;
    while (start > 0 && ((text[start - 1] >= '0' && text[start - 1] <= '9') || text[start - 1] == '.'))
        --start;
    return std::string(text.substr(start, percent + 1 - start));
}

} // namespace sekhema::keys
