#include "ResourceReader.h"

#include <algorithm>
#include <initializer_list>
#include <string>
#include <vector>

namespace sekhema {

static uintptr_t Follow(const PluginSDK::Context* ctx, uintptr_t root,
                        std::initializer_list<int> path) {
    if (!ctx || !root) return 0;
    std::vector<int> idx(path);
    return ctx->Ui.FollowPath(root, idx.data(), static_cast<int>(idx.size()));
}

// Parse the integer shown by a leaf UI element. The displayed number is the
// leaf's StringId StdWString, read host-side and delivered as UTF-8 by the
// Sekhema service (Ui.GetStringId reads the same canonical field but narrows
// non-ASCII to '?', which would mangle NBSP thousands separators). The string
// may carry a delta suffix like "152(+0)" or an icon glyph, so parse the
// LEADING digit run only (skipping thousands separators, incl. the UTF-8 NBSP
// pair 0xC2 0xA0) and stop at the first terminator — NOT strip-all, which
// would read "152(+0)" as 1520.
static int ParseUiInt(const PluginSDK::Context* ctx, uintptr_t el) {
    if (!ctx || !el) return 0;
    std::string s = ctx->Sekhema.GetUiStringId(el);
    long long v = 0; bool started = false;
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c >= '0' && c <= '9') {
            v = v * 10 + (c - '0'); started = true;
            if (v > 1'000'000'000LL) break;
        } else if (c == ',' || c == '.' || c == ' ') {
            continue;            // thousands separator within the number
        } else if (c == 0xC2 && i + 1 < s.size() &&
                   static_cast<unsigned char>(s[i + 1]) == 0xA0) {
            ++i;                 // UTF-8 NBSP thousands separator
            continue;
        } else if (started) {
            break;               // terminator after the number (delta suffix / glyph)
        }
    }
    return started ? static_cast<int>(v) : 0;
}

SekhemaResources ReadResources(const PluginSDK::Context* ctx, uintptr_t /*trialPanel*/) {
    SekhemaResources r;
    if (!ctx) return r;

    // CE-verified: the trial resource HUD lives under the "GameUi anchor" = the
    // inventory root's parent. anchor.child[13] = { water@0, keys@1..3, honour@5 }.
    uintptr_t inv    = ctx->Ui.GetUiRoot();
    uintptr_t anchor = inv ? ctx->Ui.Read(inv).ParentAddress : 0;
    if (!anchor) return r;

    // Honour %: bar fill width / frame (fill's parent) width.
    uintptr_t fill = Follow(ctx, anchor, {13, 5, 1});
    if (fill) {
        PluginSDK::UiElement fe = ctx->Ui.Read(fill);
        PluginSDK::UiElement fr = ctx->Ui.Read(fe.ParentAddress);
        if (fr.UnscaledWidth > 0.0f) {
            r.honourPct = std::clamp(100.0f * fe.UnscaledWidth / fr.UnscaledWidth, 0.0f, 100.0f);
            r.valid = true;
        }
    }

    r.sacredWater = ParseUiInt(ctx, Follow(ctx, anchor, {13, 0, 1}));
    r.keysBronze  = ParseUiInt(ctx, Follow(ctx, anchor, {13, 1, 1}));
    r.keysSilver  = ParseUiInt(ctx, Follow(ctx, anchor, {13, 2, 1}));
    r.keysGold    = ParseUiInt(ctx, Follow(ctx, anchor, {13, 3, 1}));
    return r;
}

} // namespace sekhema
