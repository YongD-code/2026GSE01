#pragma once
#include "SpiritCatalog.h"
#include "UILayout.h"
#include <algorithm>
#include <cstdint>

namespace Bestiary
{
static_assert(SpiritCatalog::Count < 64, "Bestiary species IDs must fit the saved mask.");
constexpr std::uint64_t KnownSpecies = (std::uint64_t{1} << SpiritCatalog::Count) - 1;

constexpr std::uint64_t Bit(int species)
{
    return std::uint64_t{1} << species;
}

struct Description
{
    const char* role;
    const char* habitat;
    const char* capture;
};

inline const Description& Describe(int species)
{
    static const std::array<Description, SpiritCatalog::Count> entries = {{
        {"제어 · 비명과 결박",
         "벨른 공동묘지와 마을 밖 길목",
         "처치 후 남은 잔영에 접근해 봉인권을 사용합니다. 첫 봉인은 라엔이 돕습니다."},
        {"공격 · 왕복하는 윤환",
         "폐수도원으로 향하는 길",
         "처치 후 일반 잔영은 30초, 희귀 잔영은 60초 동안 포획할 수 있습니다."},
        {"지원 · 보호와 결박",
         "폐수도원 안뜰과 마을 밖 필드",
         "이야기 속 희귀 개체의 잔영은 60초간 남습니다. 봉인권으로 포획합니다."},
        {"공격 · 성흔과 종소리",
         "폐수도원 예배당",
         "성자를 쓰러뜨린 뒤 T로 고급 봉인권을 선택하고 F로 잔영을 포획합니다."},
        {"공격 · 울음을 삼키는 주령",
         "울음을 삼킨 전용 묘역",
         "현재 묘역 사건에서는 포획할 수 없습니다. 격파 후 현장의 봉인 흔적을 조사합니다."},
        {"지원 · 회복과 길목 제어",
         "마을 밖의 숲과 길목",
         "비적대 개체입니다. 공격해 쓰러뜨리면 잔영을 봉인할 수 있습니다."},
        {"공격 · 빠른 접근",
         "묘지로 향하는 길과 마을 밖 필드",
         "이야기 속 들개는 포획 대상에서 제외됩니다. 일반 필드 개체는 잔영을 포획합니다."},
        {"지원 · 기도하는 손 계열 술식",
         "마을에서 떨어진 일부 필드 캠프",
         "긴 팔의 공격을 피한 뒤 처치하고, 일반 봉인권으로 잔영을 포획합니다."},
    }};
    static const Description fallback = {
        "생태 기록 준비 중", "조사 중", "포획 조건을 조사하고 있습니다."};
    return entries[species].role ? entries[species] : fallback;
}

struct Layout
{
    UIRect book, list, detail;
    bool compact;
    int columns, capacity;

    explicit Layout(UIRect bounds) : book(bounds)
    {
        compact = bounds.w < 760 || bounds.h < 560;
        const float top = bounds.y + 102;
        const float h = bounds.h - 166;
        const float gutter = bounds.x + bounds.w * .4f;
        list = {bounds.x + 24, top, gutter - bounds.x - 44, h};
        detail = {gutter + 24, top, bounds.x + bounds.w - gutter - 48, h};
        if (compact)
        {
            list = detail = {bounds.x + 24, top, bounds.w - 48, h};
        }
        columns = compact ? (list.w >= 640 ? 4 : list.w >= 430 ? 3 : 2) : 2;
        const int rows = (std::max)(1, int((list.h - 36) / 116));
        capacity = rows * columns;
    }

    UIRect Card(int local) const
    {
        const float w = (list.w - (columns - 1) * 8) / columns;
        return {
            list.x + (local % columns) * (w + 8), list.y + 32 + (local / columns) * 116, w, 112};
    }

    UIRect Tab(int index) const
    {
        const float w = (detail.w - 16) / 3;
        return {detail.x + index * (w + 8), detail.y + (compact ? 44 : 136), w, 30};
    }

    UIRect Text() const
    {
        const float top = detail.y + (compact ? 84 : 176);
        return {detail.x, top, detail.w, detail.y + detail.h - 38 - top};
    }

    UIRect DetailButton(int side) const
    {
        return {side == 0 ? detail.x : detail.x + detail.w - 74, detail.y + detail.h - 30, 74, 28};
    }

    UIRect ListBack() const
    {
        return {detail.x + detail.w - 74, detail.y + 2, 74, 30};
    }

    UIRect ListButton(int side) const
    {
        return {side == 0 ? list.x : list.x + list.w - 100, book.y + book.h - 62, 100, 26};
    }
};
} // namespace Bestiary
