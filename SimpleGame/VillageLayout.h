#pragma once
#include "StoryLayout.h"
#include <array>
#include <string>

// Stable addresses are shared by collision, Actors, simulation, saves and telemetry.
namespace VillageLayout
{
constexpr int HomesPerVillage = 32;
constexpr int PopulationPerVillage = 30;
constexpr int StoryResidents = StoryLayout::NpcCount;
constexpr int ResidentCount = World::VillageCount * PopulationPerVillage - StoryResidents;
constexpr int HomeCount = World::VillageCount * HomesPerVillage;

struct House
{
    int id, village;
    WorldObject building;
    WorldPoint entrance;
};

inline const std::vector<WorldObject>& AuthoredBuildings()
{
    static const std::vector<WorldObject> objects = []
    {
        std::vector<WorldObject> result = {{0, -300, -210, 140, 110, 92},
                                           {0, -70, -360, 155, 115, 115},
                                           {0, -440, 90, 130, 100, 85},
                                           {0, 270, -250, 145, 110, 98},
                                           {2, 330, 240, 65, 35, 30},
                                           {2, 390, 270, 30, 35, 52},
                                           {2, 280, 280, 25, 25, 24}};
        result.insert(
            result.end(), StoryLayout::Buildings().begin(), StoryLayout::Buildings().end());
        for (int i = 0; i < 6; ++i)
        {
            result.push_back({2, 1080. + i * 45, -90, 18, 16, 38});
        }
        result.push_back({0, 2930, -250, 210, 110, 140});
        result.push_back({2, 3180, -100, 25, 25, 90});
        result.push_back({2, 3260, -100, 25, 25, 90});
        return result;
    }();
    return objects;
}

inline bool Overlap(const WorldObject& a, const WorldObject& b, double margin = 24)
{
    return a.x < b.x + b.w + margin && a.x + a.w + margin > b.x && a.y < b.y + b.d + margin &&
           a.y + a.d + margin > b.y;
}

inline const std::vector<House>& Houses()
{
    static const std::vector<House> houses = []
    {
        std::vector<House> result;
        result.reserve(HomeCount);
        for (int village = 0; village < World::VillageCount; ++village)
        {
            const auto at = World::Settlements()[village];
            int slot = 0;
            for (int row = 0; row < 9 && slot < HomesPerVillage; ++row)
            {
                for (int col = 0; col < 9 && slot < HomesPerVillage; ++col)
                {
                    const double x = -740 + col * 180, y = -740 + row * 180;
                    // Keep the story plaza, workshops and both axial gate roads open.
                    if ((x < 560 && x + 100 > -560 && y < 400 && y + 72 > -500) ||
                        (x < 52 && x + 100 > -52) || (y < 52 && y + 72 > -52))
                    {
                        continue;
                    }
                    WorldObject building = {0, at.x + x, at.y + y, 100, 72, 82. + (slot % 4) * 7};
                    bool occupied = false;
                    for (const auto& authored : AuthoredBuildings())
                    {
                        if (Overlap(building, authored))
                        {
                            occupied = true;
                            break;
                        }
                    }
                    if (occupied)
                    {
                        continue;
                    }
                    result.push_back({village * HomesPerVillage + slot,
                                      village,
                                      building,
                                      {building.x + 50, building.y + building.d + 26}});
                    ++slot;
                }
            }
        }
        return result;
    }();
    return houses;
}

inline const House* Home(int id)
{
    const auto& houses = Houses();
    if (id >= 0 && id < int(houses.size()) && houses[id].id == id)
    {
        return &houses[id];
    }
    return nullptr;
}

inline std::string Address(int id)
{
    const auto* home = Home(id);
    return home ? std::string(World::VillageName(home->village)) + " " +
                      std::to_string(id % HomesPerVillage + 1) + "번집"
                : "거주지 미배정";
}

inline int NearestVillage(WorldPoint point)
{
    int result = 0;
    double best = 1.e30;
    for (int i = 0; i < World::VillageCount; ++i)
    {
        const auto at = World::Settlements()[i];
        const double distance = std::hypot(point.x - at.x, point.y - at.y);
        if (distance < best)
        {
            result = i;
            best = distance;
        }
    }
    return result;
}

inline int NextVillage(int from, int to)
{
    if (from == to)
    {
        return to;
    }
    std::array<int, World::VillageCount> parent;
    parent.fill(-1);
    std::array<int, World::VillageCount> queue = {};
    int head = 0, tail = 0;
    queue[tail++] = from;
    parent[from] = from;
    while (head < tail)
    {
        const int current = queue[head++];
        for (const auto& road : World::Connections())
        {
            const int next = road.first == current    ? road.second
                             : road.second == current ? road.first
                                                      : -1;
            if (next >= 0 && parent[next] < 0)
            {
                parent[next] = current;
                queue[tail++] = next;
            }
        }
    }
    int next = to;
    while (parent[next] >= 0 && parent[next] != from && parent[next] != next)
    {
        next = parent[next];
    }
    return parent[next] == from ? next : from;
}

inline WorldPoint RoutingGoal(WorldPoint start, WorldPoint destination)
{
    const int from = NearestVillage(start), to = NearestVillage(destination);
    if (from == to)
    {
        return destination;
    }
    const auto hub = World::Settlements()[from];
    const auto next = World::Settlements()[NextVillage(from, to)];
    // Reach the trunk road before setting off; local navigation handles individual buildings.
    if ((hub.x == next.x && std::abs(start.x - hub.x) > 8) ||
        (hub.y == next.y && std::abs(start.y - hub.y) > 8))
    {
        return hub;
    }
    return next;
}

inline const std::vector<WorldObject>& Buildings()
{
    static const std::vector<WorldObject> buildings = []
    {
        auto result = AuthoredBuildings();
        for (const auto& house : Houses())
        {
            result.push_back(house.building);
        }
        for (int village = 1; village < World::VillageCount; ++village)
        {
            const auto at = World::Settlements()[village];
            const double shopX[] = {-420, -220, 180};
            for (int shop = 0; shop < 3; ++shop)
            {
                WorldObject building = {0, at.x + shopX[shop], at.y - 220, 120, 100, 95};
                bool occupied = false;
                for (const auto& authored : AuthoredBuildings())
                {
                    occupied = occupied || Overlap(building, authored);
                }
                if (!occupied)
                {
                    result.push_back(building);
                }
            }
        }
        return result;
    }();
    return buildings;
}
} // namespace VillageLayout
