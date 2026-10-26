#pragma once
#include "World.h"

// The first authored encounter occupies the existing reserved story corridor.
// Coordinates stay independent of quest row indices and serialized quest IDs.
namespace CemeteryRoom
{
inline WorldObject Bounds()
{
    return {0, 1108, -440, 440, 280, 0};
}

inline WorldPoint Gate()
{
    return {1148, -160};
}

inline WorldPoint Entry()
{
    return {1148, -205};
}

inline WorldPoint Outside()
{
    return {1134, -110};
}

inline WorldPoint Boss()
{
    return {1380, -335};
}

inline WorldPoint Seal()
{
    return {1460, -235};
}

inline bool Contains(WorldPoint p, double margin = 0)
{
    const auto b = Bounds();
    return p.x >= b.x - margin && p.x <= b.x + b.w + margin && p.y >= b.y - margin &&
           p.y <= b.y + b.d + margin;
}

inline const std::vector<WorldObject>& Scenery()
{
    static const std::vector<WorldObject> objects = {{0, 1100, -448, 8, 296, 65},
                                                     {0, 1108, -448, 440, 8, 82},
                                                     {0, 1548, -448, 8, 296, 65},
                                                     {0, 1108, -160, 12, 8, 46},
                                                     {0, 1180, -160, 368, 8, 46},
                                                     {1, 1112, -172, 8, 22, 82},
                                                     {1, 1180, -172, 8, 22, 82},
                                                     {2, 1184, -411, 18, 16, 37},
                                                     {2, 1242, -411, 18, 16, 46},
                                                     {2, 1458, -410, 18, 16, 42}};
    return objects;
}

inline bool Blocked(WorldPoint p, bool gateClosed)
{
    if (!Contains(p, 30))
    {
        return false;
    }
    for (const auto& o : Scenery())
    {
        if (p.x > o.x - 12 && p.x < o.x + o.w + 12 && p.y > o.y - 12 && p.y < o.y + o.d + 12)
        {
            return true;
        }
    }
    return gateClosed && p.x > 1108 && p.x < 1192 && p.y > -184 && p.y < -140;
}
} // namespace CemeteryRoom
