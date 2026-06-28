#include "ResourceReader.h"
#include "MemoryLayout.h"

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

// Parse the integer shown by a leaf UI element. The displayed number is a
// std::wstring at leaf+0x4C0 and may carry a delta suffix like "152(+0)" or an
// icon glyph, so parse the LEADING digit run only (skipping thousands
// separators) and stop at the first terminator — NOT strip-all, which would read
// "152(+0)" as 1520. The host GetText() reads a different field (+0x4D0) that is
// wrong for these trial-HUD elements, so read +0x4C0 directly.
static int ParseUiInt(const PluginSDK::Context* ctx, uintptr_t el) {
    if (!ctx || !el) return 0;
    std::wstring w = ctx->Memory.ReadStdWString(el + layout::UiLeaf_TextWString);
    long long v = 0; bool started = false;
    for (wchar_t wc : w) {
        if (wc >= L'0' && wc <= L'9') {
            v = v * 10 + (wc - L'0'); started = true;
            if (v > 1'000'000'000LL) break;
        } else if (wc == L',' || wc == L'.' || wc == L' ' || wc == 0x00A0) {
            continue;            // thousands separator within the number
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
