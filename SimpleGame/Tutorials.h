#pragma once
#include "Chapter.h"
#include "LevelOne.h"
#include <array>
#include <cstdint>

namespace Tutorials
{
using Mask = std::uint32_t;

// The numeric IDs are part of save format 10. Append new entries.
enum Id
{
    Potion,
    WeaponStone,
    SpiritStone,
    Techniques,
    Capture,
    Companion,
    StandardTicket,
    AdvancedTicket,
    BondTicket,
    SaintSeal,
    OrderSeal,
    BestiaryBook,
    Count
};

constexpr float DisplaySeconds = 5.f;
static_assert(Count < 32, "Tutorial mask must fit in the save field.");
constexpr Mask Known = (Mask{1} << Count) - 1;

struct Tip
{
    const char* title;
    const char* instruction;
};

inline constexpr std::array<Tip, Count> Tips = {{
    {"회복물약", "H를 누르면 본체 체력을 회복합니다. Tab 소지품에서도 사용할 수 있습니다."},
    {"무기 강화석", "벨른 대장장이에게 가져가 무기를 강화하거나 판매할 수 있습니다."},
    {"주령석", "벨른 봉인사에게 가져가 동행 주령을 강화할 수 있습니다."},
    {"술식 해금", "커서로 겨누고 Q·W·E로 술식, R로 영역 전개를 사용합니다."},
    {"주령 포획", "잔영에 가까이 가서 F로 포획합니다. T로 사용할 봉인권을 바꿉니다."},
    {"주령 조종", "B로 직접 조종하거나 본체로 돌아옵니다. Tab 주령 탭에서 동행을 바꿉니다."},
    {"일반 봉인권", "T로 선택한 뒤 주령 잔영 가까이에서 F로 사용합니다."},
    {"고급 봉인권", "T로 고급 봉인권을 선택하면 성자의 잔영을 포획할 수 있습니다."},
    {"인연 부적", "이야기에서 사용처가 열릴 때까지 소지품에 보관됩니다."},
    {"성자의 봉인 파훼", "봉인이 빛날 때 가까이서 F를 누르거나 남겨진 자의 W를 맞히세요."},
    {"주령 봉쇄", "5초간 본체로 싸우거나 Tab 주령 탭에서 다른 개체를 고르세요."},
    {"주령 도감", "J로 도감을 열어 생태와 술식을 읽고 포획 기록에서 동행을 고르세요."},
}};

struct History
{
    Mask pending = 0;
    Mask shown = 0;

    void Discover(Id id)
    {
        const Mask bit = Mask{1} << id;
        pending |= bit & ~shown;
    }

    void Remember(Mask seen)
    {
        shown |= seen;
        pending &= ~shown;
    }

    bool Valid() const
    {
        return ((shown | pending) & ~Known) == 0 && (shown & pending) == 0;
    }
};

inline void DiscoverInventory(History& history, const LevelOne& level)
{
    if (level.potions > 0)
        history.Discover(Potion);
    if (level.weaponStones > 0)
        history.Discover(WeaponStone);
    if (level.stones > 0)
        history.Discover(SpiritStone);
    if (!level.collection.empty())
    {
        history.Discover(Companion);
        history.Discover(BestiaryBook);
    }
    if (level.tickets[1] > 0)
        history.Discover(StandardTicket);
    if (level.tickets[2] > 0)
        history.Discover(AdvancedTicket);
    if (level.tickets[3] > 0)
        history.Discover(BondTicket);
}

inline History RestoreLegacy(const LevelOne& level,
                             const Chapter& progress,
                             const Chapter& authored)
{
    History known;
    DiscoverInventory(known, level);
    const int storyWeaponBonus = progress.step > authored.Index("C100_SMITH") ? 1 : 0;
    if (level.weapon > storyWeaponBonus)
        known.Discover(WeaponStone);
    const int techniqueStep = authored.Index("P005");
    if (progress.step >= techniqueStep && techniqueStep < int(authored.quests.size()))
        known.Discover(Techniques);
    if (level.captureUnlocked)
        known.Discover(Capture);
    if (level.spiritRank > 0 || level.failedUpgrades > 0 || level.complete)
        known.Discover(SpiritStone);
    if (progress.step > authored.Index("C100") || level.complete)
        known.Discover(StandardTicket);
    if (level.resolvedBosses.count(3) || level.complete)
        known.Discover(AdvancedTicket);
    if (level.complete)
        known.Discover(BondTicket);
    if (progress.saintSealUnderstood || progress.step > authored.Index("C108"))
        known.Discover(SaintSeal);
    if (level.resolvedBosses.count(StoryEncounter::OrderSealer))
        known.Discover(OrderSeal);
    known.Remember(known.pending);
    return known;
}
} // namespace Tutorials
