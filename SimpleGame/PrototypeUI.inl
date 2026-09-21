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

void HUD()
{
    if (titleScreen || slotMode != SlotMode::Closed)
    {
        DrawFront();
        return;
    }
    const Color ivory(.88f, .85f, .77f), muted(.57f, .63f, .65f), gold(.85f, .7f, .43f);
    const float screenWidth = static_cast<float>(width), screenHeight = static_cast<float>(height);
    if (width < 480 || height < 360)
    {
        r.Text(12, 28, "창을 넓혀 주세요. ESC 메뉴 / H 화면 정보", ivory);
        return;
    }
    if (pauseMenu && !journal)
    {
        DrawPauseMenu();
        return;
    }
    if (!journal && saveStatusTime > 0)
    {
        const float statusWidth = (std::min)(850.f, screenWidth - 36);
        Panel(18, screenHeight - 54, statusWidth, 29);
        WrappedText(28, screenHeight - 34, statusWidth - 20, saveStatus, ivory, 1);
    }
    if (!journal && hudVisible)
    {
        Panel(18, 18, 235, 64);
        r.Text(32,
               40,
               "성장 " + std::to_string(levelOne.level) + "   체력 " +
                   std::to_string(static_cast<int>(levelOne.health)) + " / " +
                   std::to_string(static_cast<int>(levelOne.MaximumHealth())),
               ivory);
        r.Rect(32, 52, 204, 6, Color(.16f, .12f, .13f));
        r.Rect(32, 52, 204 * levelOne.health / levelOne.MaximumHealth(), 6, Color(.72f, .3f, .28f));
        r.Rect(32,
               65,
               204 * (levelOne.level == 20
                          ? 1.f
                          : float(levelOne.experience) / levelOne.RequiredExperience()),
               2,
               gold);
        r.Rect(32,
               73,
               204 * (1.f - (std::min)(1.f, levelOne.dodgeCooldown / levelOne.combat.dodgeWait)),
               3,
               Color(.3f, .75f, .9f));
        const float objectiveWidth = (std::min)(420.f, screenWidth - 36);
        // At narrow widths, stack the objective below health rather than overlapping it.
        const float ox = width >= 760 ? screenWidth - objectiveWidth - 18 : 18;
        const float oy = width >= 760 ? 18 : 92;
        Panel(ox, oy, objectiveWidth, 66);
        const auto* quest = chapter.Current();
        r.Text(ox + 12, oy + 22, quest ? quest->title : "탐험 기록", gold);
        WrappedText(ox + 12, oy + 44, objectiveWidth - 24, ObjectiveHint(), ivory, 1);
        r.Text(20, screenHeight - 18, "ESC 메뉴 · Ctrl 회피 · Tab 기록창", muted);
    }
    if (!journal)
    {
        const int weak = dialoguePending ? -1 : levelOne.Capturable(player);
        if (weak >= 0)
        {
            const char* ticketNames[] = {"낡은 봉인권", "봉인권", "고급 봉인권"};
            const float w = (std::min)(600.f, screenWidth - 36);
            const float x = (screenWidth - w) * .5f;
            Panel(x, screenHeight - 145, w, 100);
            const float chance =
                levelOne.collection.empty()
                    ? 1.f
                    : (levelOne.enemies[weak].boss == 4 ? levelOne.captureRules.bossChance
                                                        : levelOne.captureRules.normalChance);
            r.Text(x + 14,
                   screenHeight - 120,
                   "약화된 주령 · 포획 확률 " + std::to_string(int(chance * 100)) + "%",
                   gold);
            WrappedText(x + 14,
                        screenHeight - 94,
                        w - 28,
                        std::string("T 선택: ") + ticketNames[levelOne.selectedTicket] + " (" +
                            std::to_string(levelOne.tickets[levelOne.selectedTicket]) +
                            ")   E 포획 · X 정화",
                        ivory,
                        2);
            if (messageTime > 0 && speaker == "기록")
                WrappedText(x + 14, screenHeight - 62, w - 28, message, gold, 1);
        }
        else if (Nearby() >= 0 && !levelOne.Dead() && messageTime <= 0)
        {
            const char* hints[] = {"E · 아이와 대화", "E · 라엔과 대화", "", "E · 불가에서 회복"};
            r.Text(screenWidth * .5f - 90, screenHeight - 65, hints[Nearby()], gold);
        }
        if (messageTime > 0 && weak < 0)
        {
            const float w = (std::min)(720.f, screenWidth - 36);
            const float x = (screenWidth - w) * .5f;
            Panel(x, screenHeight - 163, w, 115);
            r.Text(x + 14,
                   screenHeight - 138,
                   speaker + (dialoguePending ? " · E / Enter 계속" : ""),
                   gold);
            WrappedText(x + 14, screenHeight - 112, w - 28, message, ivory, 3);
        }
    }
    if (journal)
    {
        r.Rect(0, 0, screenWidth, screenHeight, Color(0, 0, 0, .62f));
        const float w = (std::min)(850.f, screenWidth - 36);
        const float h = (std::min)(590.f, screenHeight - 36);
        const float x = (screenWidth - w) * .5f, y = (screenHeight - h) * .5f;
        Panel(x, y, w, h);
        const char* tabs[] = {"1 능력치", "2 소지품", "3 주령", "4 이벤트", "5 도움"};
        for (int i = 0; i < 5; ++i)
            r.Text(x + 16 + i * (w - 32) / 5, y + 30, tabs[i], i == menuTab ? gold : muted);
        r.Rect(x + 16, y + 44, w - 32, 1, Color(.3f, .3f, .28f));
        float lineY = y + 76;
        auto line = [&](const std::string& text)
        {
            if (lineY < y + h - 62)
                WrappedText(x + 20, lineY, w - 40, text, ivory, 1);
            lineY += 29;
        };
        if (menuTab == 0)
        {
            line("성장 " + std::to_string(levelOne.level) + " · 경험치 " +
                 std::to_string(levelOne.experience) + "/" +
                 std::to_string(levelOne.RequiredExperience()));
            line("체력 " + std::to_string(int(levelOne.health)) + " / " +
                 std::to_string(int(levelOne.MaximumHealth())));
            line("공격력 " + std::to_string(int(levelOne.Damage())) + " · 무기 강화 +" +
                 std::to_string(levelOne.weapon));
            line("발사 간격 " + std::to_string(levelOne.Interval()).substr(0, 4) + "초 · 사거리 " +
                 std::to_string(int(levelOne.Range())));
            line("흡수 범위 " + std::to_string(int(levelOne.MagnetRange())) + " · 처치 " +
                 std::to_string(levelOne.kills));
            line(levelOne.automatic ? "자동 발사: 켜짐" : "자동 발사: 꺼짐 (Space로 공격)");
            line("V 붙잡는 손 · 남은 대기 " +
                 std::to_string(int(std::ceil(levelOne.skillCooldown))) + "초");
            line("현재 지역: " + std::string(World::Village(player.x, player.y)
                                                 ? "벨른 마을"
                                                 : world.Region(player)));
        }
        else if (menuTab == 1)
        {
            line("낡은 봉인권 " + std::to_string(levelOne.tickets[0]));
            line("봉인권 " + std::to_string(levelOne.tickets[1]));
            line("고급 봉인권 " + std::to_string(levelOne.tickets[2]));
            line("인연의 봉인권 " + std::to_string(levelOne.tickets[3]) + " · 사용처 잠김");
            line("주령석 " + std::to_string(levelOne.stones) + " · 게임 중 G로 강화");
            line("성자의 잔재 " + std::to_string(levelOne.saintRemains));
            if (levelOne.resolvedBosses.count(1))
                line("검은 파편 · 특수 봉인 조각");
            if (levelOne.complete)
                line("검게 변한 성유물 · 일곱 번째 종이 울리는 날, 문이 열린다.");
        }
        else if (menuTab == 2)
        {
            line("보유 개체 " + std::to_string(levelOne.collection.size()) +
                 " · [ / ] 페이지 · N 동행 변경");
            const int perPage = (std::max)(1, static_cast<int>((h - 180) / 29));
            const int begin = collectionPage * perPage;
            for (int i = begin;
                 i < static_cast<int>(levelOne.collection.size()) && i < begin + perPage;
                 ++i)
            {
                const auto& spiritRecord = levelOne.collection[i];
                line(std::to_string(i + 1) + ". " + SpiritName(spiritRecord.species) + " · 별 " +
                     std::to_string(spiritRecord.rarity) +
                     (spiritRecord.memory ? " · 다른 기억" : ""));
            }
            if (levelOne.collection.empty())
                line("약화된 주령에게 봉인권을 사용해 첫 기록을 남겨 보세요.");
            if (captured)
                line(std::string("동행: ") + SpiritName(capturedSpirit) + " · 강화 +" +
                     std::to_string(levelOne.spiritRank));
        }
        else if (menuTab == 3)
        {
            const auto* quest = chapter.Current();
            line(quest ? quest->title : "1장 완료 · 다음 이야기를 기다리며");
            WrappedText(x + 20, lineY, w - 40, ObjectiveHint(), gold, 2);
            lineY += 56;
            line("완료된 이벤트 " + std::to_string(chapter.step) + " / " +
                 std::to_string(chapter.quests.size()) + " · [ / ] 기록 페이지");
            const int end = (std::min)(chapter.step, static_cast<int>(chapter.quests.size()));
            const int begin = eventPage * 3;
            for (int i = begin; i < end && i < begin + 3; ++i)
            {
                line("완료 · " + chapter.quests[i].title);
            }
            if (end == 0)
            {
                line("아직 완료한 이벤트가 없습니다.");
            }
            if (quest)
            {
                line("현재 이야기");
                if (lineY + 46 < y + h - 62)
                {
                    WrappedText(x + 20, lineY, w - 40, quest->dialogue, ivory, 2);
                }
            }
        }
        else
        {
            line("Ctrl 회피 · K 피격 화면 흔들림 켜기/끄기");
            line("WASD / 방향키 이동 · Shift 달리기 · Space 자동 조준 연사");
            line("E 대화/포획 · X 약화된 주령 정화 · T 봉인권 선택");
            line("Q 동행 주령 조종 · V 붙잡는 손 · G 주령석 강화");
            line("F 자동 발사 · H 기본 HUD 켜기/끄기");
            line("Tab 통합 기록창 · J 주령 탭 · 1~5 탭 선택");
            line("P 후처리 · B 블룸 · ESC 일시정지 메뉴");
            line("파랑 경험치 · 금색 무기 · 초록 회복 · 보라 주령석 · 흰색 봉인권");
            line("약화된 적은 자동 공격에서 제외됩니다. E 포획 / X 정화를 선택하세요.");
            line("F5 저장 · F9 불러오기 · 안전할 때 30초마다/정상 종료 시 자동 저장");
            line("저장 슬롯 3개 · 현재 슬롯 " + std::to_string(activeSlot));
        }
        WrappedText(x + 20, y + h - 23, w - 40, saveStatus + " · F5 저장 / F9 불러오기", muted, 1);
    }
    if (chapter.choicePending && !journal)
    {
        r.Rect(0, 0, screenWidth, screenHeight, Color(0, 0, 0, .6f));
        const float w = (std::min)(650.f, screenWidth - 36), x = (screenWidth - w) * .5f;
        Panel(x, screenHeight * .5f - 75, w, 150);
        r.Text(x + 18, screenHeight * .5f - 43, "불안해하는 아이에게 어떻게 답할까?", gold);
        WrappedText(x + 18,
                    screenHeight * .5f - 8,
                    w - 36,
                    "1 안심시킨다     2 무시한다     3 대가를 요구한다",
                    ivory,
                    2);
    }
    if (levelOne.Dead() && !journal)
    {
        const float w = (std::min)(500.f, screenWidth - 36), x = (screenWidth - w) * .5f;
        Panel(x, screenHeight * .5f - 50, w, 100);
        r.Text(x + 18, screenHeight * .5f - 12, "불씨가 꺼졌습니다", gold, true);
        r.Text(x + 18, screenHeight * .5f + 20, "F9 · 저장 불러오기 / R · 처음부터 시작", ivory);
    }
}
