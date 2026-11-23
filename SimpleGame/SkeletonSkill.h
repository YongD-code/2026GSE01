#pragma once
#include "World.h"
#include <algorithm>
#include <cmath>

namespace SkeletonSkill
{
constexpr float Charge = .4f;
constexpr float Animation = 1.f;
constexpr float Travel = .46f;
constexpr float Fade = .3f;
constexpr float Duration = Charge + Travel + Fade;
constexpr float Cooldown = 5.f;
constexpr float Reach = 280.f;
constexpr float Radius = 11.f;
constexpr float Damage = 1.8f;
constexpr float Stagger = .22f;
constexpr float Height = 55.f;

inline float Progress(float age)
{
    return (std::max)(0.f, (std::min)(1.f, (age - Charge) / Travel));
}

inline WorldPoint Direction(WorldPoint aim, int shard)
{
    const double angle = (shard - 1) * .13;
    return {aim.x * std::cos(angle) - aim.y * std::sin(angle),
            aim.x * std::sin(angle) + aim.y * std::cos(angle)};
}

inline WorldPoint Point(WorldPoint origin, WorldPoint aim, int shard, float progress)
{
    const auto direction = Direction(aim, shard);
    const double forward = 20 + (Reach - 20) * progress;
    const double side = (shard - 1) * 6 * (1 - progress);
    return {origin.x + direction.x * forward - aim.y * side,
            origin.y + direction.y * forward + aim.x * side};
}

// Earliest circle contact on the swept segment; -1 means no contact.
inline double Contact(WorldPoint center, WorldPoint a, WorldPoint b)
{
    const double x = a.x - center.x, y = a.y - center.y;
    const double vx = b.x - a.x, vy = b.y - a.y;
    const double c = x * x + y * y - Radius * Radius;
    if (c <= 0)
    {
        return 0;
    }
    const double length = vx * vx + vy * vy;
    const double dot = x * vx + y * vy;
    const double discriminant = dot * dot - length * c;
    if (length < .000001 || discriminant < 0)
    {
        return -1;
    }
    const double t = (-dot - std::sqrt(discriminant)) / length;
    return t >= 0 && t <= 1 ? t : -1;
}
} // namespace SkeletonSkill
