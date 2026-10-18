#pragma once
#include <array>

// Stable species IDs are persisted in saves. Append new species; never reorder existing rows.
namespace SpiritCatalog
{
enum Species
{
    Forgotten,
    Pilgrim,
    Hands,
    Saint,
    Count
};

struct Definition
{
    const char* name;
    const char* story;
    const wchar_t* sprite;
    float health;
    float size;
    int motion;
};

inline const Definition& Get(int species)
{
    static const std::array<Definition, Count> definitions = {
        {{"남겨진 자",
          "눈 오는 공동묘지에서 돌아오지 않는 부모를 기다리던 아이.",
          L"Assets/Spirits/ash_lantern_4dir.png",
          48,
          72,
          0},
         {"목 없는 순례자",
          "목적지를 잃은 채 수도원으로 향하는 순례자의 기억.",
          L"Assets/Spirits/mourning_bell_4dir.png",
          58,
          72,
          1},
         {"기도하는 손",
          "응답 없는 기도가 모여 누군가를 지키려는 손이 되었다.",
          L"Assets/Spirits/briar_fox_4dir.png",
          68,
          67,
          2},
         {"기도하지 않는 성자",
          "인간의 실험으로 훼손된 신앙의 주령. 빼앗긴 기도 대신 종소리로 침입자를 심판한다.",
          L"Assets/Spirits/unpraying_saint_4dir.png",
          125,
          88,
          -1}}};
    return definitions[species >= 0 && species < Count ? species : Forgotten];
}
} // namespace SpiritCatalog
