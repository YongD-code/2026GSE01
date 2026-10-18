#pragma once
#include <cstdint>
#include <cmath>
#include <map>
#include <set>
#include <vector>
#include <utility>
#include <random>
#include <array>
#include <algorithm>

struct WorldPoint
{
    double x, y;
};

struct WorldObject
{
    int kind;
    double x, y, w, d, h;
};

// Version 1 generation: coordinate hashes, never visitation order, define a chunk.
class World
{
  public:
    static constexpr int ChunkSize = 512;
    using Coordinate = std::int64_t;
    using Key = std::pair<Coordinate, Coordinate>;

    struct Chunk
    {
        Key key;
        int biome;
        std::vector<WorldObject> objects;
    };

    std::map<Key, Chunk> chunks;
    std::uint64_t seed = std::random_device{}();
    std::vector<Key> streamAnchors;
    std::array<Coordinate, 4> streamBounds = {{0, 0, 0, 0}};
    std::uint64_t streamSeed = 0;
    unsigned streamPass = 0;

    static Coordinate Index(double value)
    {
        return static_cast<Coordinate>(std::floor(value / ChunkSize));
    }

    static std::uint64_t Hash(Coordinate x, Coordinate y, std::uint64_t salt = 0)
    {
        std::uint64_t n = static_cast<std::uint64_t>(x) * 0x9e3779b97f4a7c15ULL;
        n ^= static_cast<std::uint64_t>(y) * 0xbf58476d1ce4e5b9ULL;
        n ^= 2026ULL + salt * 0x94d049bb133111ebULL;
        n = (n ^ (n >> 30)) * 0xbf58476d1ce4e5b9ULL;
        n = (n ^ (n >> 27)) * 0x94d049bb133111ebULL;
        return n ^ (n >> 31);
    }

    static bool Road(Coordinate tileX, Coordinate tileY)
    {
        auto lane = [](Coordinate v)
        {
            Coordinate m = (v % 8 + 8) % 8;
            return m == 0 || m == 7;
        };
        return lane(tileX) || lane(tileY);
    }

    static bool Village(double x, double y)
    {
        return std::abs(x) < 512 && std::abs(y) < 512;
    }

    static const std::array<WorldPoint, 5>& Settlements()
    {
        static const std::array<WorldPoint, 5> hubs = {
            {{0, 0}, {-1280, 0}, {0, 1280}, {0, -1280}, {2560, 768}}};
        return hubs;
    }

    static bool Settlement(double x, double y)
    {
        for (const auto& hub : Settlements())
            if (std::abs(x - hub.x) < 280 && std::abs(y - hub.y) < 300)
                return true;
        return false;
    }

    Chunk& Ensure(Coordinate x, Coordinate y)
    {
        Key key = {x, y};
        auto found = chunks.find(key);
        if (found != chunks.end())
            return found->second;
        Chunk c;
        c.key = key;
        // Regions cover four chunks, including correctly grouped negative coordinates.
        Coordinate rx = static_cast<Coordinate>(std::floor(double(x) / 4));
        Coordinate ry = static_cast<Coordinate>(std::floor(double(y) / 4));
        c.biome = static_cast<int>(Hash(rx, ry, 9 + seed) % 3);
        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < 3; ++col)
            {
                auto h = Hash(x, y, seed + 1 + row * 3 + col);
                if (h % 5 == 0)
                    continue;
                // Each obstacle stays inside its own cell. Inflated collision boxes retain
                // at least 30 units between cells and never enter the boundary road network.
                double px = double(x) * ChunkSize + 85 + col * 112 + double((h >> 8) % 12);
                double py = double(y) * ChunkSize + 85 + row * 112 + double((h >> 16) % 12);
                if (Village(px, py) || Settlement(px, py) ||
                    (px >= 500 && px <= 3500 && py >= -450 && py <= 400))
                    continue;
                int kind = c.biome == 2 ? 2 : 1;
                double w = kind == 1 ? 18 : 30, d = w,
                       height = kind == 1 ? 75 + double(h % 55) : 20 + double(h % 35);
                // Rare roadside cottages; bounded footprint cannot block connecting roads.
                if (col == 1 && row == 1 && h % 11 == 0)
                {
                    kind = 0;
                    w = 46;
                    d = 46;
                    height = 70;
                }
                c.objects.push_back({kind, px, py, w, d, height});
            }
        return chunks.emplace(key, std::move(c)).first->second;
    }

    void Stream(WorldPoint camera,
                WorldPoint actor,
                int width,
                int height,
                const std::vector<WorldPoint>& residents = {})
    {
        // Inverse projection of screen bounds plus roof-height and movement margins.
        double reach = width / 3.4 + height / 1.72 + 260;
        Coordinate minX = Index(camera.x - reach), maxX = Index(camera.x + reach);
        Coordinate minY = Index(camera.y - reach), maxY = Index(camera.y + reach);
        const std::array<Coordinate, 4> bounds = {{minX, maxX, minY, maxY}};
        std::vector<Key> anchors;
        anchors.reserve(residents.size() + 1);
        anchors.push_back({Index(actor.x), Index(actor.y)});
        for (const auto& resident : residents)
            anchors.push_back({Index(resident.x), Index(resident.y)});
        std::sort(anchors.begin(), anchors.end());
        anchors.erase(std::unique(anchors.begin(), anchors.end()), anchors.end());
        // Movement within the same chunks does not change residency. Periodically trim chunks
        // created by collision probes even when the anchors have not changed.
        if (!chunks.empty() && streamSeed == seed && streamBounds == bounds &&
            streamAnchors == anchors && ++streamPass < 30)
            return;
        streamAnchors = anchors;
        streamBounds = bounds;
        streamSeed = seed;
        streamPass = 0;
        std::set<Key> needed;
        for (Coordinate y = minY; y <= maxY; ++y)
            for (Coordinate x = minX; x <= maxX; ++x)
                needed.insert({x, y});
        for (const auto& anchor : anchors)
        {
            for (int y = -1; y <= 1; ++y)
                for (int x = -1; x <= 1; ++x)
                    needed.insert({anchor.first + x, anchor.second + y});
        }
        for (const Key& key : needed)
            Ensure(key.first, key.second);
        for (auto it = chunks.begin(); it != chunks.end();)
        {
            if (needed.find(it->first) == needed.end())
                it = chunks.erase(it);
            else
                ++it;
        }
    }

    bool Blocked(WorldPoint p)
    {
        Coordinate cx = Index(p.x), cy = Index(p.y);
        // Ensure() places every procedural footprint, including its 12-unit collision margin,
        // strictly inside its own chunk. Authored buildings are checked separately by the host.
        for (const WorldObject& o : Ensure(cx, cy).objects)
            if (p.x > o.x - 12 && p.x < o.x + o.w + 12 && p.y > o.y - 12 && p.y < o.y + o.d + 12)
                return true;
        return false;
    }

    const char* Region(WorldPoint p)
    {
        if (Village(p.x, p.y))
            return "잿빛 마을";
        switch (Ensure(Index(p.x), Index(p.y)).biome)
        {
        case 0:
            return "메마른 숲";
        case 1:
            return "안개 황야";
        default:
            return "잊힌 폐허";
        }
    }
};
