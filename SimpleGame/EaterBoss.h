#pragma once
#include "World.h"
#include <array>
#include <algorithm>
#include <cmath>

// Session-only attack state. Simulation and ground warnings share these shapes/timings.
namespace EaterBoss
{
constexpr int Encounter = 1;
constexpr double Tau = 6.283185307179586;
constexpr float TransitionTime = 1.5f;

enum class Pattern
{
    Sweep,
    Rush,
    Scream,
    Grief
};

struct Plan
{
    float windup, active, recovery, damage;
    float reach, radius, speed, halfArc;
};

inline Plan Describe(Pattern pattern)
{
    switch (pattern)
    {
    case Pattern::Sweep:
        return {.72f, .26f, 1.15f, 22, 118, 0, 0, 1.15f};
    case Pattern::Rush:
        return {.95f, .58f, 1.5f, 28, 0, 34, 480, 0};
    case Pattern::Scream:
        return {1.1f, 1.05f, 1.4f, 24, 265, 20, 0, 0};
    default:
        return {1.15f, .36f, 1.5f, 25, 0, 58, 0, 0};
    }
}

struct State
{
    Pattern pattern = Pattern::Sweep;
    Plan plan = Describe(Pattern::Sweep);
    std::array<int, 4> lastUse = {{-4, -4, -4, -4}};
    int sequence = 0, followups = 0;
    bool phaseTwo = false, hit = false, hasAttack = false;
    WorldPoint origin = {0, 0};
    std::array<WorldPoint, 3> marks = {};
    int markCount = 0;
};

inline float Progress(float duration, float remaining)
{
    return 1 - (std::max)(0.f, (std::min)(1.f, remaining / duration));
}

inline double WaveRadius(const Plan& plan, float progress)
{
    return 24 + (plan.reach - 24) * progress;
}

inline bool ConeContains(WorldPoint origin, WorldPoint aim, WorldPoint target, const Plan& plan)
{
    const double dx = target.x - origin.x, dy = target.y - origin.y;
    const double length = std::hypot(dx, dy);
    return length <= plan.reach && dx * aim.x + dy * aim.y >= length * std::cos(plan.halfArc);
}

inline double SegmentDistance(WorldPoint p, WorldPoint a, WorldPoint b)
{
    const double dx = b.x - a.x, dy = b.y - a.y;
    const double length2 = dx * dx + dy * dy;
    const double t =
        length2 > .000001
            ? (std::max)(0., (std::min)(1., ((p.x - a.x) * dx + (p.y - a.y) * dy) / length2))
            : 0;
    return std::hypot(p.x - a.x - dx * t, p.y - a.y - dy * t);
}

inline Pattern Select(const State& state, double distance)
{
    int chosen = 3;
    double best = -1;
    for (int i = 0; i < 4; ++i)
    {
        const int age = state.sequence - state.lastUse[i];
        if (age <= 1 || (i == 0 && distance > 132) || (i == 2 && distance > 270))
        {
            continue;
        }
        const double preference = i == 0 ? 32 : i == 1 ? (distance > 180 ? 28 : 8) : 18;
        const double score =
            preference + (std::min)(age, 8) * 9 + double(World::Hash(state.sequence, i, 81) % 7);
        if (score > best)
        {
            chosen = i;
            best = score;
        }
    }
    return static_cast<Pattern>(chosen);
}

inline const char* Name(Pattern pattern)
{
    switch (pattern)
    {
    case Pattern::Sweep:
        return "통곡 할퀴기";
    case Pattern::Rush:
        return "빈 가면 돌진";
    case Pattern::Scream:
        return "삼킨 울음";
    default:
        return "눈물 낙인";
    }
}
} // namespace EaterBoss
