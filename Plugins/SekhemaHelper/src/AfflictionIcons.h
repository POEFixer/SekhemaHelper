#pragma once
// AfflictionIcons.h — lazy PNG -> D3D11 texture cache for the Profiles-tab
// affliction icons (resources/afflictions/*.png, extracted from the game by
// tools/sekhema/extract_affliction_icons.py). Render-thread only: textures are
// created on first Get() during DrawSettings and live until ReleaseAll().
#include <imgui.h>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

struct ID3D11ShaderResourceView;

namespace sekhema {

class AfflictionIcons {
public:
    // Idempotent; the device comes from ctx()->D3DDevice, dir is the plugin folder.
    void SetSources(void* d3dDevice, const std::filesystem::path& pluginDir);

    // Texture for a catalog iconFile ("cursedarkpiticon.png"); ImTextureID{} if
    // missing or failed — failures are cached so the disk is hit once per file.
    ImTextureID Get(const char* iconFile);

    void ReleaseAll();
    ~AfflictionIcons() { ReleaseAll(); }

private:
    void* m_device = nullptr;
    std::filesystem::path m_dir;                          // <plugin>/resources/afflictions
    std::unordered_map<std::string, ImTextureID> m_cache; // iconFile -> tex (0 = failed)
    std::vector<ID3D11ShaderResourceView*> m_srvs;
};

} // namespace sekhema
