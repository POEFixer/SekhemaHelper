#include "AfflictionIcons.h"

#include <d3d11.h>
#include <fstream>

// stb_image implementation — define once in this TU
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include <stb_image.h>

namespace sekhema {

void AfflictionIcons::SetSources(void* d3dDevice, const std::filesystem::path& pluginDir) {
    if (d3dDevice) m_device = d3dDevice;
    if (m_dir.empty() && !pluginDir.empty())
        m_dir = pluginDir / "resources" / "afflictions";
}

ImTextureID AfflictionIcons::Get(const char* iconFile) {
    if (!iconFile || !iconFile[0] || !m_device || m_dir.empty()) return ImTextureID{};
    auto it = m_cache.find(iconFile);
    if (it != m_cache.end()) return it->second;

    ImTextureID tex{};
    // fs::path overload keeps the read Unicode-safe (the plugin may live under a
    // non-ASCII user directory); decode from memory so stbi never opens a file.
    std::ifstream f(m_dir / iconFile, std::ios::binary);
    if (f.is_open()) {
        std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(f)),
                                         std::istreambuf_iterator<char>());
        int w = 0, h = 0;
        unsigned char* rgba = bytes.empty() ? nullptr
            : stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()),
                                    &w, &h, nullptr, 4);
        if (rgba) {
            D3D11_TEXTURE2D_DESC desc = {};
            desc.Width  = static_cast<UINT>(w);
            desc.Height = static_cast<UINT>(h);
            desc.MipLevels = 1;
            desc.ArraySize = 1;
            desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            desc.SampleDesc.Count = 1;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

            D3D11_SUBRESOURCE_DATA init = {};
            init.pSysMem = rgba;
            init.SysMemPitch = static_cast<UINT>(w * 4);

            auto* device = static_cast<ID3D11Device*>(m_device);
            ID3D11Texture2D* tex2d = nullptr;
            if (SUCCEEDED(device->CreateTexture2D(&desc, &init, &tex2d))) {
                D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
                srvDesc.Format = desc.Format;
                srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                srvDesc.Texture2D.MipLevels = 1;
                ID3D11ShaderResourceView* srv = nullptr;
                if (SUCCEEDED(device->CreateShaderResourceView(tex2d, &srvDesc, &srv))) {
                    m_srvs.push_back(srv);
                    tex = reinterpret_cast<ImTextureID>(srv);
                }
                tex2d->Release();
            }
            stbi_image_free(rgba);
        }
    }
    m_cache.emplace(iconFile, tex);
    return tex;
}

void AfflictionIcons::ReleaseAll() {
    for (auto* srv : m_srvs)
        if (srv) srv->Release();
    m_srvs.clear();
    m_cache.clear();
}

} // namespace sekhema
