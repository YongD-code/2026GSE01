#pragma once
#include "SpiritCatalog.h"
#include <array>

// Encounter IDs remain stable in Chapter1.tsv and saved progress.
namespace StoryEncounter
{
constexpr int SpriteCount = 4;

struct Definition
{
    const char* name;
    int species, sprite;
    float size;
    bool capturable;
    float walkFrameDistance = 0;
};

inline const Definition& Get(int id)
{
    static const std::array<Definition, 13> definitions = {
        {{"배회 주령", SpiritCatalog::Forgotten, -1, 72, true},
         {"울음 먹는 자", SpiritCatalog::Forgotten, 1, 125, false, 16},
         {"수도원 수호자", SpiritCatalog::Forgotten, 2, 112, false, 15},
         {"눈먼 수도사", SpiritCatalog::Forgotten, 3, 103, false, 13},
         {"기도하지 않는 성자", SpiritCatalog::Saint, -1, 110, true},
         {"남겨진 자", SpiritCatalog::Forgotten, -1, 82, true},
         {"굶주린 들개", SpiritCatalog::Forgotten, 0, 58, false, 11},
         {"수레를 맴도는 들개", SpiritCatalog::Forgotten, 0, 62, false, 11},
         {"묘지 길목의 들개", SpiritCatalog::Forgotten, 0, 66, false, 11},
         {"목 없는 순례자", SpiritCatalog::Pilgrim, -1, 86, true},
         {"기도하는 손", SpiritCatalog::Hands, -1, 86, true},
         {"결박된 순례자", SpiritCatalog::Pilgrim, -1, 94, true},
         {"돌아가지 못한 사람", SpiritCatalog::Forgotten, -1, 88, false}}};
    return definitions[id >= 0 && id < int(definitions.size()) ? id : 0];
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
