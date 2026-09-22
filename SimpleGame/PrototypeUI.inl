// Member definitions included inside Prototype; all layout lives here.
void WrappedText(
    float x, float y, float maxWidth, const std::string& value, Color color, int maxLines = 3)
{
    std::string line;
    float used = 0;
    int row = 0;
    for (size_t i = 0; i < value.size();)
    {
        const unsigned char lead = static_cast<unsigned char>(value[i]);
        const size_t bytes =
            lead < 128 ? 1 : ((lead & 0xe0) == 0xc0 ? 2 : ((lead & 0xf0) == 0xe0 ? 3 : 4));
        const float advance = lead < 128 ? 8.f : 16.f;
        if (used + advance > maxWidth || value[i] == '\n')
        {
            if (++row >= maxLines)
            {
                r.Text(x, y, line + "…", color);
                return;
            }
            r.Text(x, y, line, color);
            line.clear();
            used = 0;
            y += 23;
        }
        if (value[i] != '\n')
        {
            line += value.substr(i, bytes);
            used += advance;
        }
        i += bytes;
    }
    if (!line.empty())
        r.Text(x, y, line, color);
}

std::string ObjectiveHint() const
{
    const auto* quest = chapter.Current();
    if (!quest)
        return chapter.quests.empty() ? "퀘스트 데이터를 읽지 못했습니다." : "1장 완료 · 자유 탐험";
    const double dx = (quest->destination.x - player.x - quest->destination.y + player.y) * .85;
    const double dy = (quest->destination.x - player.x + quest->destination.y - player.y) * .43;
    const char* direction =
        std::abs(dx) > std::abs(dy) ? (dx > 0 ? "오른쪽" : "왼쪽") : (dy > 0 ? "아래쪽" : "위쪽");
    return quest->objective + " · " + direction + " " +
           std::to_string(static_cast<int>(Distance(player, quest->destination)));
}

UIRect JournalBounds() const
{
    const float w = (std::min)(940.f, float(width) - 40);
    const float h = (std::min)(690.f, float(height) - 40);
    return {(width - w) / 2, (height - h) / 2, w, h};
}

UIRect JournalTab(int tab) const
{
    const auto b = JournalBounds();
    const float w = (b.w - 64) / 5;
    return {b.x + 16 + tab * (w + 8), b.y + 48, w, 36};
}

UIRect JournalPageButton(int side) const
{
    const auto b = JournalBounds();
    return {side == 0 ? b.x + 16 : b.x + b.w - 116, b.y + b.h - 92, 100, 32};
}

std::vector<std::string> JournalEntries()
{
    std::vector<std::string> rows;
    auto add = [&](const std::string& text)
    {
        rows.push_back(text);
    };
    if (menuTab == 0)
    {
        add("성장 · 레벨 " + std::to_string(levelOne.level) + " / 경험치 " +
            std::to_string(levelOne.experience) + " / " +
            std::to_string(levelOne.RequiredExperience()));
        add("생명 · 체력 " + std::to_string(int(levelOne.health)) + " / " +
            std::to_string(int(levelOne.MaximumHealth())));
        add("공격 · 피해 " + std::to_string(int(levelOne.Damage())) + " / 무기 강화 +" +
            std::to_string(levelOne.weapon));
        add("발사 · 간격 " + std::to_string(levelOne.Interval()).substr(0, 4) + "초 / 사거리 " +
            std::to_string(int(levelOne.Range())));
        add("수집 · 주령 흡수 범위 " + std::to_string(int(levelOne.MagnetRange())) + " / 처치 " +
            std::to_string(levelOne.kills));
        add(levelOne.automatic ? "자동 발사 · 켜짐" : "자동 발사 · 꺼짐 / Space로 공격");
        add("주령 능력 · V 붙잡는 손 / 대기 " +
            std::to_string(int(std::ceil(levelOne.skillCooldown))) + "초");
        add("지역 · " +
            std::string(World::Village(player.x, player.y) ? "벨른 마을" : world.Region(player)));
    }
    else if (menuTab == 1)
    {
        add("봉인권 · 낡은 " + std::to_string(levelOne.tickets[0]) + " / 일반 " +
            std::to_string(levelOne.tickets[1]));
        add("봉인권 · 고급 " + std::to_string(levelOne.tickets[2]) + " / 인연 " +
            std::to_string(levelOne.tickets[3]) + " (사용처 잠김)");
        add("강화 재료 · 주령석 " + std::to_string(levelOne.stones) + " / G로 강화");
        add("이야기 물품 · 성자의 잔재 " + std::to_string(levelOne.saintRemains));
        if (levelOne.resolvedBosses.count(1))
            add("이야기 물품 · 검은 파편 / 특수 봉인 조각");
        if (levelOne.complete)
            add("성유물 · 일곱 번째 종이 울리는 날, 문이 열린다.");
    }
    else if (menuTab == 2)
    {
        add("수집 · " + std::to_string(levelOne.collection.size()) + "개체 / N으로 동행 변경");
        add(captured ? std::string("현재 동행 · ") + SpiritName(capturedSpirit) + " / 강화 +" +
                           std::to_string(levelOne.spiritRank)
                     : "아직 동행 주령이 없습니다.");
        add("고유 능력 · 남겨진 자는 동행 중 주변 전리품을 흡수합니다.");
        for (size_t i = 0; i < levelOne.collection.size(); ++i)
        {
            const auto& entry = levelOne.collection[i];
            add(std::to_string(i + 1) + " · " + SpiritName(entry.species) + " / 별 " +
                std::to_string(entry.rarity) + (entry.memory ? " / 다른 기억" : ""));
        }
    }
    else if (menuTab == 3)
    {
        const auto* quest = chapter.Current();
        add(quest ? "현재 목표 · " + quest->title : "현재 목표 · 1장 완료");
        add(ObjectiveHint());
        if (quest)
            add("현재 이야기 · " + quest->dialogue);
        add("완료한 이벤트 · " + std::to_string(chapter.step) + " / " +
            std::to_string(chapter.quests.size()));
        for (int i = 0; i < chapter.step && i < static_cast<int>(chapter.quests.size()); ++i)
            add("완료 · " + chapter.quests[i].title);
    }
    else
    {
        add("이동 · WASD / 방향키 / Shift 달리기");
        add("전투 · Space 자동 조준 연사 / F 자동 발사 / Ctrl 회피");
        add("포획 · E 대화·포획 / X 정화 / T 봉인권 선택");
        add("수집 · Z 아이템 줍기 / 남겨진 자 동행 시 자동 흡수");
        add("주령 · Q 조종 / V 능력 / G 강화 / 도감에서 N 동행 변경");
        add("기록 · Tab 기록창 / J 주령 / 1~5 탭 / [ ] 페이지");
        add("화면 · H HUD / K 피격 흔들림 / P 후처리 / B 블룸");
        add("저장 · F5 저장 / F9 불러오기 / 안전할 때 자동 저장");
        add("저장 슬롯 · 최대 3개 / 현재 슬롯 " + std::to_string(activeSlot));
        add("메뉴 · ESC 열기·돌아가기 / 항목 마우스 클릭");
    }
    // Split long entries into two-line cards without dropping their remaining text.
    std::vector<std::string> cards;
    const float limit = JournalBounds().w - 72;
    for (const auto& text : rows)
    {
        std::string card;
        float used = 0;
        int lines = 1;
        for (size_t i = 0; i < text.size();)
        {
            const auto lead = static_cast<unsigned char>(text[i]);
            const size_t bytes = lead < 128              ? 1
                                 : (lead & 0xe0) == 0xc0 ? 2
                                 : (lead & 0xf0) == 0xe0 ? 3
                                                         : 4;
            const float advance = lead < 128 ? 8.f : 16.f;
            if (used + advance > limit || text[i] == '\n')
            {
                if (lines == 2)
                {
                    cards.push_back(card);
                    card.clear();
                    lines = 1;
                }
                else
                {
                    card += '\n';
                    ++lines;
                }
                used = 0;
            }
            if (text[i] != '\n')
            {
                card += text.substr(i, bytes);
                used += advance;
            }
            i += bytes;
        }
        if (!card.empty())
            cards.push_back(card);
    }
    return cards;
}

int JournalCapacity() const
{
    return (std::max)(1, int((JournalBounds().h - 212) / 64));
}

void ChangeJournalPage(int delta)
{
    const int count =
        (std::max)(1, (int(JournalEntries().size()) + JournalCapacity() - 1) / JournalCapacity());
    journalPage = (std::max)(0, (std::min)(count - 1, journalPage + delta));
}

void JournalClick(int x, int y)
{
    for (int i = 0; i < 5; ++i)
        if (JournalTab(i).Contains(x, y))
        {
            menuTab = i;
            journalPage = 0;
            return;
        }
    for (int i = 0; i < 2; ++i)
        if (JournalPageButton(i).Contains(x, y))
            ChangeJournalPage(i == 0 ? -1 : 1);
}

void DrawJournal()
{
    const auto b = JournalBounds();
    const Color ivory(.88f, .87f, .81f), gold(.85f, .7f, .43f), muted(.62f, .68f, .69f);
    r.Rect(0, 0, float(width), float(height), Color(0, 0, 0, .76f));
    Panel(b.x, b.y, b.w, b.h);
    r.Text(b.x + 20, b.y + 29, "여정 기록", gold, true);
    const char* tabs[] = {"능력치", "소지품", "주령", "이벤트", "도움"};
    for (int i = 0; i < 5; ++i)
        UIButton(JournalTab(i), tabs[i], menuTab == i);
    const auto rows = JournalEntries();
    const int capacity = JournalCapacity();
    const int pages = (std::max)(1, (int(rows.size()) + capacity - 1) / capacity);
    journalPage = (std::min)(journalPage, pages - 1);
    for (int i = 0; i < capacity && journalPage * capacity + i < int(rows.size()); ++i)
    {
        const float y = b.y + 104 + i * 64;
        r.Rect(b.x + 16, y, b.w - 32, 54, Color(.075f, .115f, .14f));
        r.Rect(b.x + 16, y, 2, 54, Color(.29f, .40f, .42f));
        WrappedText(b.x + 28, y + 21, b.w - 56, rows[journalPage * capacity + i], ivory, 2);
    }
    UIButton(JournalPageButton(0), "이전 [");
    UIButton(JournalPageButton(1), "다음 ]");
    r.Text(b.x + b.w * .5f - 25,
           b.y + b.h - 70,
           std::to_string(journalPage + 1) + " / " + std::to_string(pages),
           gold);
    r.Rect(b.x + 16, b.y + b.h - 48, b.w - 32, 32, Color(.06f, .09f, .11f));
    WrappedText(b.x + 28, b.y + b.h - 26, b.w - 56, saveStatus, muted, 1);
}

void HUD()
{
    if (titleScreen || slotMode != SlotMode::Closed)
    {
        DrawFront();
        return;
    }
    const Color ivory(.88f, .85f, .77f), muted(.60f, .67f, .69f), gold(.85f, .7f, .43f);
    const float sw = float(width), sh = float(height);
    if (width < 480 || height < 360)
    {
        r.Text(12, 28, "창을 넓혀 주세요. ESC 메뉴", ivory);
        return;
    }
    if (pauseMenu && !journal)
    {
        DrawPauseMenu();
        return;
    }
    if (journal)
    {
        DrawJournal();
        return;
    }
    if (chapter.choicePending || levelOne.Dead())
    {
        r.Rect(0, 0, sw, sh, Color(0, 0, 0, .76f));
        const float w = (std::min)(640.f, sw - 40), h = chapter.choicePending ? 254.f : 190.f;
        const float x = (sw - w) / 2, y = (sh - h) / 2;
        Panel(x, y, w, h);
        r.Text(x + 24,
               y + 36,
               chapter.choicePending ? "아이에게 건넬 말" : "불씨가 꺼졌습니다",
               gold,
               true);
        if (chapter.choicePending)
        {
            const char* choices[] = {"1  안심시킨다", "2  무시한다", "3  대가를 요구한다"};
            for (int i = 0; i < 3; ++i)
                UIButton({x + 24, y + 64 + i * 56, w - 48, 44}, choices[i]);
        }
        else
        {
            UIButton({x + 24, y + 62, w - 48, 44}, "F9  저장된 여정 불러오기");
            UIButton({x + 24, y + 120, w - 48, 44}, "R  처음부터 시작");
        }
        return;
    }
    if (hudVisible)
    {
        const float hpw = width >= 760 ? 260.f : 200.f;
        Panel(20, 20, hpw, 112);
        r.Text(34, 43, "성장 " + std::to_string(levelOne.level), gold);
        r.Text(34,
               67,
               "체력 " + std::to_string(int(levelOne.health)) + " / " +
                   std::to_string(int(levelOne.MaximumHealth())),
               ivory);
        r.Rect(34, 77, hpw - 28, 8, Color(.2f, .1f, .12f));
        r.Rect(34,
               77,
               (hpw - 28) * levelOne.health / levelOne.MaximumHealth(),
               8,
               Color(.72f, .3f, .28f));
        r.Rect(34,
               99,
               (hpw - 28) * (levelOne.level == 20
                                 ? 1.f
                                 : float(levelOne.experience) / levelOne.RequiredExperience()),
               4,
               gold);
        r.Rect(34,
               115,
               (hpw - 28) *
                   (1.f - (std::min)(1.f, levelOne.dodgeCooldown / levelOne.combat.dodgeWait)),
               4,
               Color(.3f, .75f, .9f));
        const float w = (std::min)(420.f, sw - hpw - 60);
        const float x = sw - w - 20;
        Panel(x, 20, w, 112);
        const auto* quest = chapter.Current();
        WrappedText(x + 16, 46, w - 32, quest ? quest->title : "탐험 기록", gold, 1);
        WrappedText(x + 16, 74, w - 32, ObjectiveHint(), ivory, 2);
    }
    const float dockW = (std::min)(850.f, sw - 40), dockX = (sw - dockW) / 2;
    if (saveStatusTime > 0)
    {
        Panel(dockX, sh - 70, dockW, 38);
        WrappedText(dockX + 14, sh - 45, dockW - 28, "저장 · " + saveStatus, muted, 1);
    }
    if (hudVisible)
        r.Text(20, sh - 12, "ESC 메뉴 · Ctrl 회피 · Z 줍기 · Tab 기록", muted);

    std::string heading, body;
    const int weak = dialoguePending ? -1 : levelOne.Capturable(player);
    if (weak >= 0)
    {
        const char* tickets[] = {"낡은 봉인권", "봉인권", "고급 봉인권"};
        const float chance = levelOne.collection.empty() ? 1.f
                             : levelOne.enemies[weak].boss == 4
                                 ? levelOne.captureRules.bossChance
                                 : levelOne.captureRules.normalChance;
        heading = "약화된 주령 · 포획 " + std::to_string(int(chance * 100)) + "%";
        body = std::string("T 선택 · ") + tickets[levelOne.selectedTicket] + " " +
               std::to_string(levelOne.tickets[levelOne.selectedTicket]) + "장\nE 포획 / X 정화";
    }
    else if (messageTime > 0)
    {
        heading = speaker + (dialoguePending ? " · E / Enter 계속" : " · 알림");
        body = message;
    }
    else
    {
        const WorldPoint collector = controlling ? spirit : player;
        const LevelOne::Loot* nearest = nullptr;
        double distance = 65;
        for (const auto& item : levelOne.loot)
        {
            const double current = Distance(item.position, collector);
            if (current < distance)
            {
                nearest = &item;
                distance = current;
            }
        }
        if (nearest)
        {
            const char* names[] = {"경험치 조각", "무기 강화석", "회복약", "주령석", "봉인권"};
            heading = "주변 전리품";
            body = std::string(names[static_cast<int>(nearest->kind)]) + " · Z로 주변 아이템 줍기";
        }
        else if (Nearby() >= 0)
        {
            const char* hints[] = {"E · 아이와 대화", "E · 라엔과 대화", "", "E · 불가에서 회복"};
            heading = "상호작용";
            body = hints[Nearby()];
        }
    }
    if (!body.empty())
    {
        const float w = (std::min)(760.f, sw - 40), x = (sw - w) / 2, y = sh - 216;
        Panel(x, y, w, 130);
        r.Text(x + 16, y + 27, heading, gold);
        r.Rect(x + 16, y + 38, w - 32, 1, Color(.28f, .34f, .35f));
        WrappedText(x + 16, y + 61, w - 32, body, ivory, 3);
    }
}
