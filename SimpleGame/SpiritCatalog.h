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
    Eater,
    Stag,
    Hound,
    Count
};

constexpr int StagSkills = 5;
constexpr int SkillSetCount = 6;

struct Definition
{
    const char* name;
    const char* story;
    const wchar_t* sprite;
    float health;
    float size;
    int motion;
    const wchar_t* model;
    int combatType, skillSpecies;
    bool passive;
    int gait; // 0: hover, 1: biped, 2: quadruped.
    const char* disposition;
    bool bossOnly = false;
};

inline const Definition& Get(int species)
{
    static const std::array<Definition, Count> definitions = {
        {{"남겨진 자",
          "눈 오는 공동묘지에서 돌아오지 않는 부모를 기다리던 아이.",
          L"Assets/Spirits/ash_lantern_4dir.png",
          48,
          72,
          0,
          L"Assets/Spirits/Runtime/forgotten.meshbin",
          0,
          Forgotten,
          false,
          0,
          "gentle"},
         {"목 없는 순례자",
          "목적지를 잃은 채 수도원으로 향하는 순례자의 기억.",
          L"Assets/Spirits/mourning_bell_4dir.png",
          58,
          72,
          1,
          L"Assets/Spirits/Runtime/pilgrim.meshbin",
          1,
          Pilgrim,
          false,
          0,
          "ambivalent"},
         {"기도하는 손",
          "응답 없는 기도가 모여 누군가를 지키려는 손이 되었다.",
          L"Assets/Spirits/briar_fox_4dir.png",
          68,
          67,
          2,
          L"Assets/Spirits/Runtime/hands.meshbin",
          2,
          Hands,
          false,
          2,
          "gentle"},
         {"기도하지 않는 성자",
          "인간의 실험으로 훼손된 신앙의 주령. 빼앗긴 기도 대신 종소리로 침입자를 심판한다.",
          L"Assets/Spirits/unpraying_saint_4dir.png",
          125,
          88,
          -1,
          L"Assets/Spirits/Runtime/saint.meshbin",
          3,
          Saint,
          false,
          0,
          "dangerous",
          true},
         {"울음 먹는 자",
          "길 잃은 이의 울음을 삼키며 해진 가면 뒤에서 눈물을 흘리는 주령.",
          L"Assets/Spirits/Models/eater.png",
          85,
          86,
          -1,
          L"Assets/Spirits/Runtime/eater.meshbin",
          0,
          Forgotten,
          false,
          1,
          "dangerous",
          true},
         {"길지기 사슴",
          "가지뿔에 작은 방울과 기원 천을 매달고 잊힌 길목을 지키는 선한 주령.",
          L"Assets/Spirits/Models/stag.png",
          64,
          80,
          -1,
          L"Assets/Spirits/Runtime/stag.meshbin",
          0,
          Hands,
          true,
          2,
          "gentle"},
         {"재 먹는 들개",
          "타버린 기억의 재를 먹고 붉은 균열을 드러내며 달려드는 주령.",
          L"Assets/Spirits/Models/hound.png",
          62,
          67,
          2,
          L"Assets/Spirits/Runtime/hound.meshbin",
          0,
          Pilgrim,
          false,
          2,
          "dangerous"}}};
    return definitions[species >= 0 && species < Count ? species : Forgotten];
}
} // namespace SpiritCatalog
