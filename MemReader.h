#pragma once
// MemReader.h — typed raw-memory reads for the SekhemaHelper FloorData walk,
// wrapping the host's ctx()->Memory service. Separate from MemoryLayout.h so the
// pure offset constants + VecCount stay SDK-free / standalone-testable.
#include "MemoryLayout.h"
#include "sdk/PluginSDK.h"

#include <Windows.h>
#include <cstdint>
#include <string>
#include <vector>

namespace sekhema {

class Mem {
    const PluginSDK::Context* m_ctx = nullptr;
public:
    explicit Mem(const PluginSDK::Context* ctx) : m_ctx(ctx) {}

    bool Valid() const { return m_ctx != nullptr; }

    template <class T>
    T Read(uintptr_t addr) const {
        T v{};
        if (m_ctx && addr) m_ctx->Memory.Read(addr, &v, sizeof(T));
        return v;
    }

    uintptr_t Ptr(uintptr_t addr) const { return Read<uintptr_t>(addr); }

    std::vector<uint8_t> ReadBytes(uintptr_t addr, int n) const {
        std::vector<uint8_t> b;
        if (!m_ctx || !addr || n <= 0 || n > (1 << 16)) return b;
        b.resize(static_cast<size_t>(n));
        if (!m_ctx->Memory.Read(addr, b.data(), static_cast<size_t>(n))) b.clear();
        return b;
    }

    StdVec ReadVec(uintptr_t addr) const {
        StdVec v;
        v.First = Read<uintptr_t>(addr + 0x00);
        v.Last  = Read<uintptr_t>(addr + 0x08);
        return v;
    }

    // Raw null-terminated wchar_t[] at ptr (NOT a std::wstring) -> UTF-8.
    std::string ReadWide(uintptr_t ptr, int maxChars) const {
        if (!m_ctx || !ptr || maxChars <= 0 || maxChars > 4096) return {};
        std::vector<wchar_t> buf(static_cast<size_t>(maxChars) + 1, L'\0');
        if (!m_ctx->Memory.Read(ptr, buf.data(),
                                static_cast<size_t>(maxChars) * sizeof(wchar_t)))
            return {};
        buf[maxChars] = L'\0';
        size_t len = 0;
        while (len < static_cast<size_t>(maxChars) && buf[len] != L'\0') ++len;
        if (len == 0) return {};
        int need = ::WideCharToMultiByte(CP_UTF8, 0, buf.data(), static_cast<int>(len),
                                         nullptr, 0, nullptr, nullptr);
        if (need <= 0) return {};
        std::string out(static_cast<size_t>(need), '\0');
        ::WideCharToMultiByte(CP_UTF8, 0, buf.data(), static_cast<int>(len),
                              out.data(), need, nullptr, nullptr);
        return out;
    }
};

} // namespace sekhema
