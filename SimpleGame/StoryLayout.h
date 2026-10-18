#pragma once
#include "Chapter.h"
#include <array>

namespace StoryLayout
{
constexpr int NpcCount = 8;

struct NpcSprite
{
    const char* id;
    const wchar_t* path;
    bool reversedSides = false;
};

inline const std::array<NpcSprite, NpcCount>& NpcSprites()
{
    static const std::array<NpcSprite, NpcCount> sprites = {
        {{"sibling", L"Assets/Characters/Story/sibling.png"},
         {"parent", L"Assets/Characters/Story/parent.png"},
         {"merchant", L"Assets/Characters/Story/merchant.png"},
         {"child", L"Assets/Characters/Story/child.png"},
         {"stranger", L"Assets/Characters/Story/stranger.png"},
         {"raen", L"Assets/Characters/Story/raen.png"},
         {"sealer", L"Assets/Characters/Story/sealer.png"},
         {"smith", L"Assets/Characters/Story/smith.png", true}}};
    return sprites;
}

inline int NpcSpriteIndex(const std::string& id)
{
    const std::string identity = id.compare(0, 5, "raen_") == 0 ? "raen" : id;
    const auto& sprites = NpcSprites();
    for (int i = 0; i < NpcCount; ++i)
    {
        if (identity == sprites[i].id)
        {
            return i;
        }
    }
    return -1;
}

enum class Kind
{
    Adult,
    Child,
    Evidence
};

struct Site
{
    std::string id, name;
    Kind kind;
    WorldPoint position;
    std::string from, until;
    int spriteIndex = NpcSpriteIndex(id);
};

inline const std::vector<Site>& Sites()
{
    static const std::vector<Site> sites = {
        {"sibling", "동생", Kind::Child, {-220, -65}, "P000", "P001"},
        {"parent", "부모", Kind::Adult, {-180, -65}, "P000", ""},
        {"merchant", "잡화상", Kind::Adult, {210, -80}, "P000", ""},
        {"child", "마을 아이", Kind::Child, {-100, 90}, "P000", ""},
        {"mark", "둘만의 길 표시", Kind::Evidence, {100, 90}, "P000", ""},
        {"belonging", "동생의 소지품", Kind::Evidence, {440, 0}, "P001", ""},
        {"cart", "버려진 수레", Kind::Evidence, {740, 75}, "P000", ""},
        {"stranger", "돌아가지 못한 사람", Kind::Adult, {1030, 0}, "P001", "P003_TRIAL"},
        {"raen_gate", "라엔", Kind::Adult, {1000, -45}, "P004", "P008_VILLAGER"},
        {"grave", "이름 없는 묘비", Kind::Evidence, {1140, 80}, "P000", ""},
        {"seal", "봉인 제단", Kind::Evidence, {1320, 70}, "P000", ""},
        {"room", "동생의 물건 상자", Kind::Evidence, {-260, -65}, "P000", ""},
        {"raen_home", "라엔", Kind::Adult, {-120, -65}, "P008_VILLAGER", "C100"},
        {"sealer", "봉인사", Kind::Adult, {310, -90}, "P000", ""},
        {"smith", "대장장이", Kind::Adult, {360, -90}, "P000", ""},
        {"raen_hub", "라엔", Kind::Adult, {270, -90}, "C100", "C102"},
        {"field_record", "봉인 기록대", Kind::Evidence, {1690, 70}, "P000", ""},
        {"cell", "감금실의 결박", Kind::Evidence, {1980, -55}, "P000", ""},
        {"ledger", "운송 장부", Kind::Evidence, {2570, 70}, "P000", ""},
        {"relic", "검은 성유물", Kind::Evidence, {3290, 65}, "C108", ""},
        {"raen_chapel", "라엔", Kind::Adult, {3320, 0}, "C109_RELIC", ""}};
    return sites;
}

inline bool Visible(const Site& site, const Chapter& chapter)
{
    return chapter.step >= chapter.Index(site.from) &&
           (site.until.empty() || chapter.step < chapter.Index(site.until));
}

inline const std::vector<WorldObject>& Buildings()
{
    static const std::vector<WorldObject> buildings = {{0, 140, -220, 115, 100, 85},
                                                       {0, 650, -180, 95, 75, 70},
                                                       {0, 940, -240, 130, 110, 105},
                                                       {2, 970, 70, 22, 22, 70},
                                                       {2, 1080, 70, 22, 22, 70},
                                                       {2, 1160, -120, 20, 22, 38},
                                                       {2, 1210, -120, 20, 22, 38},
                                                       {2, 1260, -120, 20, 22, 38},
                                                       {2, 1310, -120, 20, 22, 38},
                                                       {0, 1570, -230, 130, 110, 100},
                                                       {0, 1900, -230, 165, 130, 115},
                                                       {0, 2200, -240, 155, 110, 100},
                                                       {0, 2440, -260, 200, 130, 125},
                                                       {0, 2740, -260, 185, 125, 125},
                                                       {0, 3060, -300, 260, 150, 160},
                                                       {2, 3090, 95, 24, 24, 80},
                                                       {2, 3350, 95, 24, 24, 80}};
    return buildings;
}
} // namespace StoryLayout
