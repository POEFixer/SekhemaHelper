#pragma once
// RoomClassifier.h — DAT FK -> room type / affliction / reward. The pure helpers
// (ExtractRoomType / MapReward + string utils) are SDK-free and standalone-tested;
// ClassifyFk (needs Mem) is implemented in the .cpp with Mem forward-declared.
#include "Model.h"
#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace sekhema {

class Mem; // forward decl — keeps this header SDK-free

inline std::string LowerCopy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return s;
}
inline bool HasSub(const std::string& hay, const char* needle) {
    return hay.find(needle) != std::string::npos;
}
inline bool HasSubI(const std::string& hay, const char* needle) {
    return LowerCopy(hay).find(LowerCopy(needle)) != std::string::npos;
}
inline std::vector<std::string> SplitOn(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) { if (c == sep) { out.push_back(cur); cur.clear(); } else cur.push_back(c); }
    out.push_back(cur);
    return out;
}

// "Caverns_Arena_03" -> parts[1]="Arena" -> "Hourglass"; unknown tokens identity.
inline std::string ExtractRoomType(const std::string& id) {
    auto parts = SplitOn(id, '_');
    if (parts.size() < 2) return "";
    const std::string& tok = parts[1];
    if (tok == "Arena")   return "Hourglass";
    if (tok == "Lair")    return "Chalice";
    if (tok == "Explore") return "Escape";
    return tok; // Ritual / Gauntlet / Boss already match their display name
}

// lowercase, first-match-wins (order matters). "" if nothing matches (-> base weight).
inline std::string MapReward(const std::string& id) {
    std::string s = LowerCopy(id);
    if (HasSub(s, "key"))
        return HasSub(s, "gold") ? "Gold Key" : HasSub(s, "silver") ? "Silver Key" : "Bronze Key";
    if (HasSub(s, "chest") || HasSub(s, "cache"))
        return HasSub(s, "gold") ? "Golden Cache" : HasSub(s, "silver") ? "Silver Cache" : "Bronze Cache";
    if (HasSub(s, "water") || HasSub(s, "fountain"))
        return (HasSub(s, "legend") || HasSub(s, "major") || HasSub(s, "large")) ? "Large Fountain" : "Fountain";
    if (HasSub(s, "merchant")) return "Merchant";
    if (HasSub(s, "pledge"))   return "Pledge to Kochai";
    if (HasSub(s, "honor") || HasSub(s, "honour")) return "Honour";
    if (HasSub(s, "boon"))     return "Boon";
    if (HasSub(s, "curse"))    return "Curse";
    if (HasSub(s, "random"))   return "Random";
    return "";
}

// Reads one content FK pair and writes type/affliction/reward onto the room.
// table+0x08 -> path string; "SanctumPersistentEffects" -> affliction @ row+0x28;
// "SanctumRooms" -> id @ row+0x00 -> Treasure?MapReward:ExtractRoomType.
void ClassifyFk(SekhemaRoom& room, uintptr_t rowPtr, uintptr_t tablePtr, const Mem& mem);

} // namespace sekhema
