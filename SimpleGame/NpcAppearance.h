#pragma once
#include <array>
#include <cstdint>

// Slot order and indices match Art/Blender/ModularCollection/catalog.json.
namespace NpcAppearance
{
struct Definition
{
    std::array<int, 4> partIndices = {{0, 0, 0, -1}};
    float scale = 1;
};

inline Definition Story(int identity)
{
    static const std::array<Definition, 8> people = {{
        {{{1, 2, 0, 5}}, .84f},
        {{{1, 4, 2, 0}}, 1.f},
        {{{0, 0, 1, 4}}, 1.05f},
        {{{0, 1, 0, 0}}, .82f},
        {{{0, 3, 6, -1}}, .98f},
        {{{1, 5, 3, 6}}, 1.f},
        {{{1, 4, 5, 3}}, 1.f},
        {{{0, 0, 2, 2}}, 1.07f},
    }};
    return people[identity >= 0 && identity < int(people.size()) ? identity : 0];
}

inline Definition Resident(int id, int role)
{
    std::uint32_t variant = (std::uint32_t(id) + 1) * 2654435761u;
    variant ^= variant >> 16;
    variant *= 2246822519u;
    variant ^= variant >> 13;
    Definition result;
    result.partIndices[0] = int(variant & 1);
    result.partIndices[1] = int((variant >> 3) % 7);
    result.partIndices[2] = role == 1 ? 1 : role == 2 ? 2 : role == 3 ? 4 : 0;
    result.partIndices[3] = role == 3 ? 1 : role == 2 ? 7 : role == 1 ? 4 : 0;
    if (role == 3 && (variant >> 9) % 3 == 0)
    {
        result.partIndices[2] = 3;
        result.partIndices[3] = 6;
    }
    result.scale = .95f + float((variant >> 12) % 11) * .01f;
    return result;
}
} // namespace NpcAppearance
