#pragma once
#include "World.h"
#include <algorithm>
#include <cmath>

namespace StagSkill
{
constexpr float Charge = .62f;
constexpr float Travel = .58f;
constexpr float Fade = .8f;
constexpr float BloomRise = .28f;
constexpr float VineHeight = 84.f;
constexpr float Duration = Charge + Travel + BloomRise + Fade;
constexpr float Animation = 1.35f;
constexpr float Cooldown = 7.f;
constexpr float Reach = 360.f;
constexpr float Radius = 18.f;
constexpr float RootTime = 1.1f;
constexpr int Segments = 48;
constexpr int BloomPoints = 8;

struct VinePoint
{
    double x, y, z;
};

inline float Rise(float age, float end)
{
    const float t = (std::max)(0.f, (std::min)(1.f, (age - Charge - Travel * end) / BloomRise));
    return 1 - (1 - t) * (1 - t) * (1 - t);
}

inline VinePoint Stem(WorldPoint at, int branch, int strand, float t)
{
    const double angle = strand * 2.094395102 + branch * .8 + t * 2.7;
    const double spread = (4 + 8 * std::sin(t * 3.141592654)) * (.4 + .6 * t);
    return {at.x + std::cos(angle) * spread + std::sin(branch * 1.1 + strand) * t * t * 9,
            at.y + std::sin(angle) * spread + std::cos(branch + strand * .7) * t * t * 6,
            (VineHeight - strand * 8) * t};
}

inline float Growth(float age)
{
    return (std::max)(0.f, (std::min)(1.f, (age - Charge) / Travel));
}

inline WorldPoint Point(WorldPoint origin, WorldPoint aim, int branch, float t)
{
    const double split = (std::max)(0., (std::min)(1., (t - .2) / .8));
    const double side =
        std::sin(t * 3.141592653589793) * 12 + (branch - 1) * 62 * split * split * (3 - 2 * split);
    return {origin.x + aim.x * Reach * t - aim.y * side,
            origin.y + aim.y * Reach * t + aim.x * side};
}

inline float BloomAt(float end, int index)
{
    return end * (.05f + .95f * index / (BloomPoints - 1));
}

inline bool CoveredByCenter(WorldPoint origin, WorldPoint aim, int branch, float t, float centerEnd)
{
    if (branch == 1 || centerEnd <= .1f)
    {
        return false;
    }
    const auto at = Point(origin, aim, branch, t);
    for (int i = 0; i < BloomPoints; ++i)
    {
        const auto center = Point(origin, aim, 1, BloomAt(centerEnd, i));
        const double dx = at.x - center.x, dy = at.y - center.y;
        if (dx * dx + dy * dy < 22 * 22)
        {
            return true;
        }
    }
    return false;
}

inline double SegmentDistance(WorldPoint p, WorldPoint a, WorldPoint b)
{
    const double x = b.x - a.x, y = b.y - a.y;
    const double t = (std::max)(
        0., (std::min)(1., ((p.x - a.x) * x + (p.y - a.y) * y) / (x * x + y * y + .000001)));
    return std::hypot(p.x - a.x - t * x, p.y - a.y - t * y);
}
} // namespace StagSkill
