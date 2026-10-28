#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <fstream>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <utility>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <exception>

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

inline const std::wstring& MeshFolder()
{
    static const auto folder = Folder(L"MeshCache-v4");
    return folder;
}

inline std::wstring MeshPath(const std::string& key)
{
    const auto& folder = MeshFolder();
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

struct CacheBudget
{
    std::mutex mutex;
    unsigned long long bytes = 0;
    std::chrono::steady_clock::time_point scanned;
};

inline CacheBudget& MeshBudget()
{
    static CacheBudget budget;
    return budget;
}

template <class Vertex> bool Write(const std::string& key, const std::vector<Vertex>& vertices)
{
    if (!Enabled(L"GSE_MESH_DISK_CACHE") || vertices.empty())
        return false;
    const auto path = MeshPath(key);
    if (path.empty())
        return false;
    // Cache the directory total between writes. Periodically reconcile other processes.
    auto& budget = MeshBudget();
    std::lock_guard<std::mutex> lock(budget.mutex);
    const auto now = std::chrono::steady_clock::now();
    if (budget.scanned == std::chrono::steady_clock::time_point{} ||
        now - budget.scanned >= std::chrono::seconds(30))
    {
        budget.bytes = 0;
        WIN32_FIND_DATAW entry = {};
        HANDLE find = FindFirstFileW((MeshFolder() + L"/*.mesh").c_str(), &entry);
        if (find != INVALID_HANDLE_VALUE)
        {
            do
            {
                budget.bytes += (static_cast<unsigned long long>(entry.nFileSizeHigh) << 32) |
                                entry.nFileSizeLow;
            } while (FindNextFileW(find, &entry));
            FindClose(find);
        }
        budget.scanned = now;
    }
    WIN32_FILE_ATTRIBUTE_DATA previous = {};
    const auto oldBytes = GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &previous)
                              ? (static_cast<unsigned long long>(previous.nFileSizeHigh) << 32) |
                                    previous.nFileSizeLow
                              : 0ull;
    const auto fileBytes = sizeof(std::uint32_t) * 4 + sizeof(std::uint64_t) + key.size() +
                           vertices.size() * sizeof(Vertex);
    const auto retained = budget.bytes - (oldBytes < budget.bytes ? oldBytes : budget.bytes);
    if (retained + fileBytes > 256ull * 1024 * 1024)
    {
        return false;
    }
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
    budget.bytes = retained + fileBytes;
    return true;
}

// Only disposable mesh cache data is written here. No GL or world state is used by the worker.
template <class Vertex> class MeshWriter
{
    struct Job
    {
        std::string key;
        std::vector<Vertex> vertices;
    };

    std::mutex mutex;
    std::condition_variable ready;
    std::deque<Job> pending;
    size_t queuedBytes = 0;
    bool stopping = false;
    std::thread worker;

    void Run()
    {
        for (;;)
        {
            Job job;
            {
                std::unique_lock<std::mutex> lock(mutex);
                ready.wait(lock,
                           [this]
                           {
                               return stopping || !pending.empty();
                           });
                if (stopping)
                {
                    return;
                }
                job = std::move(pending.front());
                queuedBytes -= job.vertices.size() * sizeof(Vertex);
                pending.pop_front();
            }
            try
            {
                Write(job.key, job.vertices);
            }
            catch (const std::exception&)
            {
                // A disposable cache failure must not stop the game or the worker.
            }
        }
    }

  public:
    MeshWriter()
        : worker(
              [this]
              {
                  Run();
              })
    {
    }

    ~MeshWriter()
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            stopping = true;
            pending.clear();
            queuedBytes = 0;
        }
        ready.notify_one();
        worker.join();
    }

    bool Submit(const std::string& key, const std::vector<Vertex>& vertices)
    {
        const size_t bytes = vertices.size() * sizeof(Vertex);
        {
            std::lock_guard<std::mutex> lock(mutex);
            // Bound memory and latency even if storage is slow or unavailable.
            if (stopping || pending.size() >= 32 || queuedBytes + bytes > 16 * 1024 * 1024)
            {
                return false;
            }
            pending.push_back({key, vertices});
            queuedBytes += bytes;
        }
        ready.notify_one();
        return true;
    }
};

template <class Vertex> bool QueueWrite(const std::string& key, const std::vector<Vertex>& vertices)
{
    if (!Enabled(L"GSE_MESH_DISK_CACHE") || vertices.empty())
    {
        return false;
    }
    // These shared resources must outlive the writer during static destruction.
    (void)MeshFolder();
    (void)MeshBudget();
    try
    {
        static MeshWriter<Vertex> writer;
        return writer.Submit(key, vertices);
    }
    catch (const std::exception&)
    {
        return false;
    }
}
} // namespace RenderStorage
