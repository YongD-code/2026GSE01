#pragma once
#include "World.h"

// The first authored encounter occupies a reserved clearing north of the story road.
// Coordinates stay independent of quest row indices and serialized quest IDs.
namespace CemeteryRoom
{
inline WorldObject Bounds()
{
    return World::CemeteryBounds();
}

inline WorldPoint Gate()
{
    const auto b = Bounds();
    return {b.x + 120, b.y + b.d};
}

inline WorldPoint Entry()
{
    const auto gate = Gate();
    return {gate.x, gate.y - 70};
}

inline WorldPoint Outside()
{
    const auto gate = Gate();
    return {gate.x, gate.y + 70};
}

inline WorldPoint Boss()
{
    const auto b = Bounds();
    return {b.x + b.w * .60, b.y + b.d * .38};
}

inline WorldPoint Seal()
{
    const auto b = Bounds();
    return {b.x + b.w - 130, b.y + 42};
}

inline WorldObject GateFootprint()
{
    const auto gate = Gate();
    return {0, gate.x - 56, gate.y - 4, 112, 8, 84};
}

inline bool Contains(WorldPoint p, double margin = 0)
{
    const auto b = Bounds();
    return p.x >= b.x - margin && p.x <= b.x + b.w + margin && p.y >= b.y - margin &&
           p.y <= b.y + b.d + margin;
}

inline const std::vector<WorldObject>& Scenery()
{
    static const std::vector<WorldObject> objects = []
    {
        const auto b = Bounds();
        const auto gate = GateFootprint();
        return std::vector<WorldObject>{
            {0, b.x - 8, b.y - 8, 8, b.d + 16, 65},
            {0, b.x, b.y - 8, b.w, 8, 82},
            {0, b.x + b.w, b.y - 8, 8, b.d + 16, 65},
            {0, b.x, gate.y, gate.x - b.x, 8, 46},
            {0, gate.x + gate.w, gate.y, b.x + b.w - gate.x - gate.w, 8, 46},
            {1, gate.x - 8, gate.y - 8, 8, 22, 82},
            {1, gate.x + gate.w, gate.y - 8, 8, 22, 82},
            // Small edge decorations have no gameplay collision.
            {2, b.x + 80, b.y + 8, 18, 16, 30},
            {2, b.x + 140, b.y + 8, 18, 16, 36},
            {2, b.x + b.w - 64, b.y + 8, 18, 16, 32}};
    }();
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
        if (o.kind == 2)
        {
            continue;
        }
        if (p.x > o.x - 12 && p.x < o.x + o.w + 12 && p.y > o.y - 12 && p.y < o.y + o.d + 12)
        {
            return true;
        }
    }
    const auto gate = GateFootprint();
    return gateClosed && p.x > gate.x - 12 && p.x < gate.x + gate.w + 12 && p.y > gate.y - 20 &&
           p.y < gate.y + gate.d + 16;
}
} // namespace CemeteryRoom
