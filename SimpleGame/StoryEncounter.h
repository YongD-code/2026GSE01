#pragma once
#include "SpiritCatalog.h"
#include <array>

// Encounter IDs remain stable in Chapter1.tsv and saved progress.
namespace StoryEncounter
{
constexpr int SpriteCount = 4;
constexpr int SaintBoss = 4;
constexpr int OrderSwordsman = 13;
constexpr int OrderArcher = 14;
constexpr int OrderSealer = 15;
constexpr int ShrineGuard = 16;
constexpr int BellWraith = 17;
constexpr int Gar = 18;
constexpr int MaxId = Gar;

inline bool ShrineEncounter(int id)
{
    return id == ShrineGuard || id == BellWraith;
}

struct Definition
{
    const char* name;
    int species, sprite;
    float size;
    bool capturable;
    float walkFrameDistance = 0;
    int humanAttack = -1;
};

inline const Definition& Get(int id)
{
    static const std::array<Definition, MaxId + 1> definitions = {
        {{"배회 주령", SpiritCatalog::Forgotten, -1, 72, true},
         {"울음 먹는 자", SpiritCatalog::Eater, -1, 125, false, 16},
         {"수도원 수호자", SpiritCatalog::Forgotten, 2, 112, false, 15},
         {"눈먼 수도사", SpiritCatalog::Forgotten, 3, 103, false, 13},
         {"기도하지 않는 성자", SpiritCatalog::Saint, -1, 110, true},
         {"남겨진 자", SpiritCatalog::Forgotten, -1, 82, true},
         {"굶주린 들개", SpiritCatalog::Hound, -1, 58, false, 11},
         {"수레를 맴도는 들개", SpiritCatalog::Hound, -1, 62, false, 11},
         {"묘지 길목의 들개", SpiritCatalog::Hound, -1, 66, false, 11},
         {"목 없는 순례자", SpiritCatalog::Pilgrim, -1, 86, true},
         {"기도하는 손", SpiritCatalog::Hands, -1, 86, true},
         {"결박된 순례자", SpiritCatalog::Pilgrim, -1, 94, true},
         {"돌아가지 못한 사람", SpiritCatalog::Forgotten, -1, 88, false},
         {"성흔회 검병", SpiritCatalog::Forgotten, -1, 78, false, 10, 0},
         {"성흔회 궁수", SpiritCatalog::Forgotten, -1, 74, false, 10, 2},
         {"성흔회 봉인사", SpiritCatalog::Forgotten, -1, 78, false, 10, 2},
         {"성흔회 포획기사", SpiritCatalog::Forgotten, -1, 80, false, 10, 0},
         {"종을 듣는 자", SpiritCatalog::Pilgrim, -1, 104, true, 12},
         {"성흔기사 가르", SpiritCatalog::Forgotten, -1, 94, false, 12, 0}}};
    return definitions[id >= 0 && id < int(definitions.size()) ? id : 0];
}

inline int AttackType(int id, int species)
{
    if (id == BellWraith)
    {
        return 3;
    }
    const int humanAttack = Get(id).humanAttack;
    return humanAttack >= 0 ? humanAttack : SpiritCatalog::Get(species).combatType;
}

inline int RangedSpreadSteps(int id)
{
    return id == OrderArcher || id == OrderSealer ? 0 : 1;
}

inline const wchar_t* SpritePath(int index)
{
    static const wchar_t* paths[SpriteCount] = {L"Assets/Enemies/Story/hungry_hound_walk.png",
                                                L"Assets/Enemies/Story/weeping_devourer_walk.png",
                                                L"Assets/Enemies/Story/monastery_warden_walk.png",
                                                L"Assets/Enemies/Story/blind_monk_walk.png"};
    return index >= 0 && index < SpriteCount ? paths[index] : L"";
}
} // namespace StoryEncounter
