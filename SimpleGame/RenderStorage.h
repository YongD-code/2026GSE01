#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <fstream>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <utility>

// Derived, disposable data only. Save files are never stored here.
namespace RenderStorage
{
inline std::wstring Folder(const wchar_t* child)
{
    wchar_t base[32768] = {};
    DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", base, 32768);
    if (!length || length >= 32768)
        return {};
    std::wstring path = std::wstring(base) + L"/2026GSE01";
    CreateDirectoryW(path.c_str(), nullptr);
    path += L"/";
    path += child;
    CreateDirectoryW(path.c_str(), nullptr);
    return path;
}

inline bool Enabled(const wchar_t* name)
{
    wchar_t value[16] = {};
    return !(GetEnvironmentVariableW(name, value, 16) == 1 && value[0] == L'0');
}

inline std::uint64_t Hash(const void* bytes, size_t size)
{
    auto data = static_cast<const unsigned char*>(bytes);
    std::uint64_t hash = 14695981039346656037ull;
    for (size_t i = 0; i < size; ++i)
        hash = (hash ^ data[i]) * 1099511628211ull;
    return hash;
}

inline std::wstring MeshPath(const std::string& key)
{
    const auto folder = Folder(L"MeshCache-v1");
    if (folder.empty())
        return {};
    wchar_t name[64] = {};
    swprintf_s(
        name, L"/%016llx.mesh", static_cast<unsigned long long>(Hash(key.data(), key.size())));
    return folder + name;
}

template <class Vertex> bool Read(const std::string& key, std::vector<Vertex>& vertices)
{
    if (!Enabled(L"GSE_MESH_DISK_CACHE"))
        return false;
    const auto path = MeshPath(key);
    if (path.empty())
        return false;
    std::ifstream input(path.c_str(), std::ios::binary);
    std::uint32_t header[4] = {};
    std::uint64_t checksum = 0;
    if (!input.read(reinterpret_cast<char*>(header), sizeof(header)) ||
        !input.read(reinterpret_cast<char*>(&checksum), sizeof(checksum)) ||
        header[0] != 0x3148534d || header[1] != sizeof(Vertex) || header[2] != key.size() ||
        header[3] == 0 || header[3] > 1000000 || header[3] % 3)
        return false;
    std::string stored(header[2], '\0');
    if (!input.read(&stored[0], stored.size()) || stored != key)
        return false;
    std::vector<Vertex> loaded(header[3]);
    if (!input.read(reinterpret_cast<char*>(loaded.data()), loaded.size() * sizeof(Vertex)) ||
        input.peek() != std::char_traits<char>::eof() ||
        Hash(loaded.data(), loaded.size() * sizeof(Vertex)) != checksum)
        return false;
    for (const auto& vertex : loaded)
        if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y) || !std::isfinite(vertex.r) ||
            !std::isfinite(vertex.g) || !std::isfinite(vertex.b) || !std::isfinite(vertex.a))
            return false;
    vertices = std::move(loaded);
    return true;
}

template <class Vertex> bool Write(const std::string& key, const std::vector<Vertex>& vertices)
{
    if (!Enabled(L"GSE_MESH_DISK_CACHE") || vertices.empty())
        return false;
    const auto path = MeshPath(key);
    if (path.empty())
        return false;
    // Stop adding derived files at the budget; existing game saves are unrelated.
    const auto folder = Folder(L"MeshCache-v1");
    WIN32_FIND_DATAW entry = {};
    HANDLE find = FindFirstFileW((folder + L"/*.mesh").c_str(), &entry);
    unsigned long long bytes = 0;
    if (find != INVALID_HANDLE_VALUE)
    {
        do
        {
            bytes +=
                (static_cast<unsigned long long>(entry.nFileSizeHigh) << 32) | entry.nFileSizeLow;
        } while (FindNextFileW(find, &entry));
        FindClose(find);
    }
    if (bytes + vertices.size() * sizeof(Vertex) > 256ull * 1024 * 1024)
        return false;
    const auto temporary = path + L"." + std::to_wstring(GetCurrentProcessId()) + L".tmp";
    std::ofstream output(temporary.c_str(), std::ios::binary | std::ios::trunc);
    const std::uint32_t header[] = {0x3148534d,
                                    sizeof(Vertex),
                                    static_cast<std::uint32_t>(key.size()),
                                    static_cast<std::uint32_t>(vertices.size())};
    const auto checksum = Hash(vertices.data(), vertices.size() * sizeof(Vertex));
    output.write(reinterpret_cast<const char*>(header), sizeof(header));
    output.write(reinterpret_cast<const char*>(&checksum), sizeof(checksum));
    output.write(key.data(), key.size());
    output.write(reinterpret_cast<const char*>(vertices.data()), vertices.size() * sizeof(Vertex));
    output.close();
    if (!output || !MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING))
    {
        DeleteFileW(temporary.c_str());
        return false;
    }
    return true;
}
} // namespace RenderStorage
