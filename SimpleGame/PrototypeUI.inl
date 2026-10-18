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
    return quest->objective;
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
        add("소모품 · 회복물약 " + std::to_string(levelOne.potions) + "개 / H 사용");
        add("기타 · 강화석 " + std::to_string(levelOne.weaponStones) + " / 전리품 " +
            std::to_string(levelOne.spoils));
        add("소지금 · " + std::to_string(levelOne.gold) + "금 / 대장장이·잡화상에서 거래");
        add("공격 · 피해 " + std::to_string(int(levelOne.Damage())) + " / 무기 강화 +" +
            std::to_string(levelOne.weapon));
        add("발사 · 간격 " + std::to_string(levelOne.Interval()).substr(0, 4) + "초 / 사거리 " +
            std::to_string(int(levelOne.Range())));
        add("수집 · 자동 흡수 범위 " + std::to_string(int(levelOne.MagnetRange())) + " / 처치 " +
            std::to_string(levelOne.kills));
        add("기본 공격 · 좌클릭 / 커서 방향 연사");
        add(std::string("주인공 외형 · ") +
            (useProtagonistModel && protagonistModel.Ready() ? "입체 모델" : "기존 외형") +
            " / M 전환");
        add(std::string("채집꾼 외형 · ") +
            (useGathererStudy && gathererSheet.Ready() ? "1차 보행 후보" : "기존 외형") +
            " / 게임 중 N 전환");
        add(std::string("주령 외형 · ") + (UsingSpiritModels() ? "2.5D 모델" : "기존 원화") +
            " / ESC 전투 메뉴에서 전환");
        for (int i = 0; i < 4; ++i)
        {
            add(std::string("QWER").substr(i, 1) + " · " + SkillName(SkillSet(), i) + " / 대기 " +
                std::to_string(int(std::ceil(skillTimers[SkillSet()][i]))) + "초");
            add(SkillDescription(SkillSet(), i));
        }
        add("지역 · " +
            std::string(World::Village(player.x, player.y) ? "벨른 마을" : world.Region(player)));
    }
    else if (menuTab == 1)
    {
        add("소모품 · 회복물약 " + std::to_string(levelOne.potions) + " / H로 본체 회복");
        add("기타 · 강화석 " + std::to_string(levelOne.weaponStones) + " / 주령석 " +
            std::to_string(levelOne.stones));
        add("기타 · 전리품 " + std::to_string(levelOne.spoils) + " / 소지금 " +
            std::to_string(levelOne.gold));
        add("봉인권 · 낡은 " + std::to_string(levelOne.tickets[0]) + " / 일반 " +
            std::to_string(levelOne.tickets[1]));
        add("봉인권 · 고급 " + std::to_string(levelOne.tickets[2]) + " / 인연 " +
            std::to_string(levelOne.tickets[3]) + " (사용처 잠김)");
        add("강화 재료 · 주령석 " + std::to_string(levelOne.stones) + " / 봉인사에게 강화");
        add("이야기 물품 · 성자의 잔재 " + std::to_string(levelOne.saintRemains));
        if (levelOne.resolvedBosses.count(1))
            add("이야기 물품 · 검은 파편 / 특수 봉인 조각");
        if (levelOne.complete || chapter.step > chapter.Index("C109_RELIC"))
            add("성유물 · 일곱 번째 종이 울리는 날, 문이 열린다.");
    }
    else if (menuTab == 2)
    {
        add("수집 · " + std::to_string(levelOne.collection.size()) + "개체 / N으로 동행 변경");
        add(captured ? std::string("현재 동행 · ") + SpiritName(capturedSpirit) + " / 강화 +" +
                           std::to_string(levelOne.spiritRank)
                     : "아직 동행 주령이 없습니다.");
        add("아이템 · 주령 종류와 관계없이 가까운 전리품을 자동 흡수합니다.");
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
        add("초록색 목표 표시는 미니맵을 확인하세요. 범위 밖 목표는 화살표로 표시됩니다.");
        add("완료한 이벤트 · " + std::to_string(chapter.step) + " / " +
            std::to_string(chapter.quests.size()));
        for (int i = 0; i < chapter.step && i < static_cast<int>(chapter.quests.size()); ++i)
        {
            add("완료 · " + chapter.quests[i].title);
            add(chapter.quests[i].dialogue);
        }
    }
    else
    {
        add("이동 · 우클릭 목적지 / S 정지 / Shift 달리기");
        add("전투 · 좌클릭 공격 / QWER 스킬 / Ctrl 회피");
        add("포획 · F 대화·조사·포획 / X 정화 / T 봉인권 선택");
        add("수집 · 가까운 아이템 자동 흡수 / 조종 중인 캐릭터 기준");
        add("주령 · B 조종과 QWER 전환 / 봉인사 강화 / 도감 N 동행 변경");
        add("기록 · Tab 기록창 / J 주령 / 1~5 탭 / [ ] 페이지");
        add("화면 · U HUD / K 피격 흔들림 / P 후처리 / O 블룸");
        add("주인공 외형 · M 입체 모델과 기존 외형 비교 / ESC 캐릭터 메뉴에서도 전환");
        add("채집꾼 외형 · 게임 중 N 기존 외형과 1차 보행 후보 비교 / 도감의 N은 동행 변경");
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

bool VisualJournal() const
{
    return menuTab == 1 || menuTab == 2;
}

int JournalColumns() const
{
    return JournalBounds().w >= 700 ? 3 : 2;
}

int VisualCapacity() const
{
    return JournalColumns() * (std::max)(1, int((JournalBounds().h - 224) / 126));
}

int JournalItemCount()
{
    return menuTab == 1   ? 11
           : menuTab == 2 ? int(levelOne.collection.size())
                          : int(JournalEntries().size());
}

UIRect JournalSlot(int index) const
{
    const auto b = JournalBounds();
    const int cols = JournalColumns();
    const float w = (b.w - 44 - (cols - 1) * 12) / cols;
    return {b.x + 22 + (index % cols) * (w + 12), b.y + 108 + (index / cols) * 126, w, 114};
}

void DrawJournalSlot(int index, int local)
{
    const auto box = JournalSlot(local);
    const bool spirits = menuTab == 2;
    const bool selected = spirits
                              ? captured && index == selectedCompanion
                              : index >= 4 && index <= 6 && index - 4 == levelOne.selectedTicket;
    const Color accents[] = {Color(.85f, .30f, .42f),
                             Color(.26f, .55f, .86f),
                             Color(.55f, .35f, .82f),
                             Color(.20f, .64f, .52f)};
    const Color accent = accents[index % 4];
    r.Rect(box.x + 2, box.y + 3, box.w, box.h, Color(.18f, .23f, .35f, .18f));
    r.Rect(box.x, box.y, box.w, box.h, selected ? Color(1, .92f, .67f) : Color(.98f, .98f, 1));
    r.Rect(box.x, box.y, box.w, 4, accent);
    r.Rect(box.x + 10, box.y + 14, 62, 70, Color(.87f, .91f, .98f));
    SpriteAnimation pose;
    std::string name, info, action;
    if (spirits)
    {
        const auto& entry = levelOne.collection[index];
        if (!DrawSpiritModel(entry.species, {box.x + 41, box.y + 81}, 62, pose, Color(1, 1, 1), 0))
        {
            spiritSheets[entry.species].Draw(
                r, {box.x + 41, box.y + 81}, 62, pose, Color(1, 1, 1), width, height);
        }
        name = SpiritName(entry.species);
        for (int star = 0; star < entry.rarity; ++star)
        {
            const float sx = box.x + 17 + star * 13;
            r.Quad({sx, box.y + 82},
                   {sx + 4, box.y + 87},
                   {sx, box.y + 92},
                   {sx - 4, box.y + 87},
                   Color(.94f, .64f, .10f));
        }
        info =
            "등급 " + std::to_string(entry.rarity) + " / 체력 " + std::to_string(int(entry.health));
        action = selected ? "동행 중" : "클릭하여 동행";
        if (entry.memory)
            action += " · 기억";
    }
    else
    {
        const char* names[] = {"회복물약",
                               "강화석",
                               "주령석",
                               "전리품",
                               "낡은 부적",
                               "봉인 부적",
                               "고급 부적",
                               "인연 부적",
                               "성자의 잔재",
                               "검은 파편",
                               "성유물"};
        const int amounts[] = {
            levelOne.potions,
            levelOne.weaponStones,
            levelOne.stones,
            levelOne.spoils,
            levelOne.tickets[0],
            levelOne.tickets[1],
            levelOne.tickets[2],
            levelOne.tickets[3],
            levelOne.saintRemains,
            int(levelOne.resolvedBosses.count(1)),
            int(levelOne.complete || chapter.step > chapter.Index("C109_RELIC"))};
        const int icons[] = {2, 1, 3, 0, 4, 4, 4, 4, 3, 3, 3};
        lootSheets[icons[index]].Draw(
            r, {box.x + 41, box.y + 78}, 48, pose, Color(1, 1, 1), width, height);
        name = names[index];
        info = std::to_string(amounts[index]) + "개";
        action = index == 0                 ? "클릭 사용 · H"
                 : index >= 4 && index <= 6 ? (selected ? "선택한 부적" : "클릭 선택")
                 : index == 7               ? "사용처 잠김"
                 : index >= 8               ? "이야기 물품"
                                            : "재료 · 상점 거래";
    }
    const Color ink(.16f, .21f, .32f);
    WrappedText(box.x + 82, box.y + 32, box.w - 92, name, ink, 2);
    WrappedText(box.x + 82, box.y + 75, box.w - 92, info, ink, 1);
    r.Text(box.x + 10, box.y + 103, action, accent);
}

int JournalCapacity() const
{
    return VisualJournal() ? VisualCapacity() : (std::max)(1, int((JournalBounds().h - 212) / 64));
}

void ChangeJournalPage(int delta)
{
    const int count =
        (std::max)(1, (JournalItemCount() + JournalCapacity() - 1) / JournalCapacity());
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
    if (VisualJournal())
    {
        for (int i = 0; i < JournalCapacity(); ++i)
        {
            const int index = journalPage * JournalCapacity() + i;
            if (index >= JournalItemCount() || !JournalSlot(i).Contains(x, y))
                continue;
            if (menuTab == 1 && index == 0)
                levelOne.UsePotion();
            else if (menuTab == 1 && index >= 4 && index <= 6)
                levelOne.selectedTicket = index - 4;
            else if (menuTab == 2 && !controlling)
            {
                selectedCompanion = index;
                capturedSpirit = levelOne.collection[index].species;
                captured = true;
            }
            return;
        }
    }
    for (int i = 0; i < 2; ++i)
        if (JournalPageButton(i).Contains(x, y))
            ChangeJournalPage(i == 0 ? -1 : 1);
}

void DrawJournal()
{
    const auto b = JournalBounds();
    const Color ivory(.16f, .21f, .32f), gold(.23f, .34f, .58f), muted(.38f, .43f, .54f);
    r.Rect(0, 0, float(width), float(height), Color(0, 0, 0, .76f));
    Panel(b.x, b.y, b.w, b.h);
    r.Rect(b.x + 4, b.y + 4, b.w - 8, b.h - 8, Color(.91f, .94f, .99f));
    r.Text(b.x + 20, b.y + 29, "여정 · 가방과 주령", gold, true);
    const char* tabs[] = {"능력치", "소지품", "주령", "이벤트", "도움"};
    for (int i = 0; i < 5; ++i)
        UIButton(JournalTab(i), tabs[i], menuTab == i);
    const auto rows = VisualJournal() ? std::vector<std::string>() : JournalEntries();
    const int capacity = JournalCapacity();
    const int pages = (std::max)(1, (JournalItemCount() + capacity - 1) / capacity);
    journalPage = (std::min)(journalPage, pages - 1);
    if (VisualJournal())
    {
        r.Text(b.x + 24,
               b.y + 105,
               "소지금 " + std::to_string(levelOne.gold) + "금 · 아이콘을 클릭하세요",
               gold);
        for (int i = 0; i < capacity && journalPage * capacity + i < JournalItemCount(); ++i)
            DrawJournalSlot(journalPage * capacity + i, i);
        if (JournalItemCount() == 0)
            r.Text(b.x + 30, b.y + 155, "포획한 주령이 여기에 표시됩니다.", muted);
    }
    else
        for (int i = 0; i < capacity && journalPage * capacity + i < int(rows.size()); ++i)
        {
            const float y = b.y + 104 + i * 64;
            r.Rect(b.x + 16, y, b.w - 32, 54, Color(.98f, .98f, 1));
            r.Rect(b.x + 16, y, 2, 54, Color(.29f, .40f, .42f));
            WrappedText(b.x + 28, y + 21, b.w - 56, rows[journalPage * capacity + i], ivory, 2);
        }
    UIButton(JournalPageButton(0), "이전 [");
    UIButton(JournalPageButton(1), "다음 ]");
    r.Text(b.x + b.w * .5f - 25,
           b.y + b.h - 70,
           std::to_string(journalPage + 1) + " / " + std::to_string(pages),
           gold);
    r.Rect(b.x + 16, b.y + b.h - 48, b.w - 32, 32, Color(.83f, .88f, .96f));
    WrappedText(b.x + 28, b.y + b.h - 26, b.w - 56, saveStatus, muted, 1);
}

UIRect MinimapBounds() const
{
    const float size = (std::min)(180.f, float(height) * .32f);
    return {float(width) - size - 12, 12, size, size};
}

void DrawMinimap()
{
    const auto box = MinimapBounds();
    Panel(box.x, box.y, box.w, box.h);
    const float cx = box.x + box.w * .5f;
    const float cy = box.y + box.h * .55f;
    const float radius = box.w * .5f - 12;
    const float scale = radius / 460.f;
    auto project = [&](WorldPoint at)
    {
        return Point{cx + float((at.x - player.x - at.y + player.y) * .85) * scale,
                     cy + float((at.x - player.x + at.y - player.y) * .43) * scale};
    };
    auto inside = [&](Point p)
    {
        return p.x > box.x + 8 && p.x < box.x + box.w - 8 && p.y > box.y + 27 &&
               p.y < box.y + box.h - 20;
    };
    // The same road predicate used by world terrain; no distance text.
    const auto tx = static_cast<World::Coordinate>(std::floor(player.x / 64));
    const auto ty = static_cast<World::Coordinate>(std::floor(player.y / 64));
    for (int y = -12; y <= 12; ++y)
    {
        for (int x = -12; x <= 12; ++x)
        {
            const Point p = project({double(tx + x) * 64 + 32, double(ty + y) * 64 + 32});
            if (inside(p) && World::Road(tx + x, ty + y))
            {
                r.Rect(p.x - 2, p.y - 2, 4, 4, Color(.34f, .36f, .30f));
            }
        }
    }
    for (const auto& object : villageObjects)
    {
        const Point p = project({object.x + object.w * .5, object.y + object.d * .5});
        if (inside(p))
        {
            r.Rect(p.x - 3, p.y - 3, 6, 6, Color(.65f, .59f, .43f));
        }
    }
    for (const auto& site : StoryLayout::Sites())
    {
        const Point p = project(site.position);
        if (inside(p) && StoryLayout::Visible(site, chapter))
        {
            r.Ellipse(p.x, p.y, 2, 2, Color(.56f, .69f, .76f));
        }
    }
    if (const auto* quest = chapter.Current())
    {
        WorldPoint target = quest->trigger == "skill" ? player : quest->destination;
        for (const auto& enemy : levelOne.enemies)
        {
            if ((quest->trigger == "fight" || quest->trigger == "first_capture" ||
                 quest->trigger == "survive") &&
                enemy.boss == quest->amount && enemy.health > 0)
            {
                target = enemy.position;
                break;
            }
        }
        const Point p = project(target);
        const Color green(.2f, 1.f, .36f);
        if (inside(p))
        {
            r.Ellipse(p.x, p.y, 4, 4, green);
        }
        else
        {
            const float dx = p.x - cx;
            const float dy = p.y - cy;
            const float horizontal = radius / (std::max)(.001f, std::abs(dx));
            const float vertical = (radius - 18) / (std::max)(.001f, std::abs(dy));
            const float t = (std::min)(horizontal, vertical);
            const float length = (std::max)(.001f, std::hypot(dx, dy));
            const float ux = dx / length;
            const float uy = dy / length;
            const Point tip = {cx + dx * t, cy + dy * t};
            r.Triangle(tip,
                       {tip.x - ux * 10 - uy * 5, tip.y - uy * 10 + ux * 5},
                       {tip.x - ux * 10 + uy * 5, tip.y - uy * 10 - ux * 5},
                       green);
        }
    }
    r.Triangle({cx, cy - 5}, {cx - 4, cy + 4}, {cx + 4, cy + 4}, Color(1, 1, 1));
    r.Text(box.x + 10, box.y + 20, "주변 지도", Color(.85f, .83f, .72f));
    r.Text(box.x + 8, box.y + box.h - 5, "초록: 목표", Color(.2f, 1.f, .36f));
}

UIRect QuestTrackerBounds() const
{
    const float available = MinimapBounds().x - 32;
    return {20, 144, (std::min)(height < 550 ? 200.f : 340.f, available), 126};
}

void DrawQuestTracker()
{
    const auto box = QuestTrackerBounds();
    const auto* quest = chapter.Current();
    const Color text(.9f, .9f, .85f), muted(.65f, .72f, .76f), gold(.95f, .79f, .48f);
    float progress = 0;
    std::string counter = "0 / 1";
    if (!quest)
    {
        progress = chapter.quests.empty() ? 0.f : 1.f;
        counter = chapter.quests.empty() ? "확인 필요" : "100%";
    }
    else if (quest->trigger == "survive")
    {
        progress = chapter.rewardPending ? 0.f : (std::min)(1.f, storyTrialTime / 5.f);
        counter = std::to_string(int(progress * 100)) + "%";
    }
    else
    {
        const bool encounter = quest->trigger == "fight" || quest->trigger == "first_capture";
        const bool complete = encounter ? levelOne.resolvedBosses.count(quest->amount) != 0
                                        : quest->trigger == "skill" && !chapter.rewardPending &&
                                              levelOne.skillsUsed > 0;
        progress = complete ? 1.f : 0.f;
        counter = complete ? "1 / 1" : "0 / 1";
    }
    Panel(box.x, box.y, box.w, box.h);
    r.Text(box.x + 12, box.y + 21, "현재 퀘스트", gold);
    r.Text(box.x + box.w - 90, box.y + 21, counter, gold);
    WrappedText(box.x + 12,
                box.y + 44,
                box.w - 24,
                quest                    ? quest->title
                : chapter.quests.empty() ? "퀘스트 데이터 없음"
                                         : "1장 완료",
                text,
                1);
    WrappedText(
        box.x + 12, box.y + 66, box.w - 24, quest ? quest->objective : ObjectiveHint(), muted, 2);
    r.Rect(box.x + 12, box.y + 101, box.w - 24, 5, Color(.16f, .20f, .23f));
    if (progress > 0)
    {
        r.Rect(box.x + 12, box.y + 101, (box.w - 24) * progress, 5, gold);
    }
    r.Text(box.x + 12,
           box.y + 120,
           "완료 " + std::to_string(chapter.step) + " / " + std::to_string(chapter.quests.size()),
           muted);
}

UIRect ChildDialogueButton(int index) const
{
    const float w = (std::min)(640.f, float(width) - 40);
    return {(width - w) / 2 + 24, (height - 190.f) / 2 + 64 + index * 56, w - 48, 44};
}

UIRect MessageBounds() const
{
    const bool beside = hudVisible && height < 550;
    const float left = beside ? QuestTrackerBounds().x + QuestTrackerBounds().w + 12 : 20;
    const float w = (std::min)(760.f, width - left - 20);
    const float y = beside ? (std::max)(height - 262.f, MinimapBounds().y + MinimapBounds().h + 12)
                           : height - 262.f;
    return {beside ? left : (width - w) / 2, y, w, 130};
}

std::vector<std::string> DialoguePages() const
{
    const float limit = (std::max)(16.f, MessageBounds().w - 32);
    std::vector<std::string> lines;
    std::string line;
    float used = 0;
    for (size_t i = 0; i < message.size();)
    {
        const unsigned char lead = static_cast<unsigned char>(message[i]);
        const size_t bytes =
            lead < 128 ? 1 : ((lead & 0xe0) == 0xc0 ? 2 : ((lead & 0xf0) == 0xe0 ? 3 : 4));
        const float advance = lead < 128 ? 8.f : 16.f;
        if (message[i] == '\n' || (!line.empty() && used + advance > limit))
        {
            lines.push_back(line);
            line.clear();
            used = 0;
        }
        if (message[i] != '\n')
        {
            line += message.substr(i, bytes);
            used += advance;
        }
        i += bytes;
    }
    if (!line.empty() || lines.empty())
        lines.push_back(line);
    std::vector<std::string> pages;
    for (size_t i = 0; i < lines.size(); ++i)
    {
        if (i % 3 == 0)
            pages.emplace_back();
        else
            pages.back() += '\n';
        pages.back() += lines[i];
    }
    return pages;
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
    if (!serviceNpc.empty())
    {
        DrawService();
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
        const float w = (std::min)(640.f, sw - 40), h = 190.f;
        const float x = (sw - w) / 2, y = (sh - h) / 2;
        Panel(x, y, w, h);
        r.Text(x + 24,
               y + 36,
               chapter.choicePending ? "아이와 대화" : "불씨가 꺼졌습니다",
               gold,
               true);
        if (chapter.choicePending)
        {
            const char* choices[] = {"1  대화하기", "2  종료하기"};
            for (int i = 0; i < 2; ++i)
                UIButton(ChildDialogueButton(i), choices[i]);
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
        const float shownHealth =
            controlling ? levelOne.collection[selectedCompanion].health : levelOne.health;
        const float shownMaximum = controlling ? 100.f : levelOne.MaximumHealth();
        Panel(20, 20, hpw, 112);
        r.Text(34,
               43,
               controlling ? "주령 조종 · 본체 무적" : "성장 " + std::to_string(levelOne.level),
               gold);
        r.Text(34,
               67,
               (controlling ? "주령 체력 " : "체력 ") + std::to_string(int(shownHealth)) + " / " +
                   std::to_string(int(shownMaximum)),
               ivory);
        r.Rect(34, 77, hpw - 28, 8, Color(.2f, .1f, .12f));
        r.Rect(34, 77, (hpw - 28) * shownHealth / shownMaximum, 8, Color(.72f, .3f, .28f));
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
    }
    if (hudVisible)
    {
        DrawQuestTracker();
    }
    DrawMinimap();
    DrawSkillBar();
    const float dockW = (std::min)(850.f, sw - 40), dockX = (sw - dockW) / 2;
    if (saveStatusTime > 0)
    {
        const float statusX =
            hudVisible && sh < 410 ? QuestTrackerBounds().x + QuestTrackerBounds().w + 12 : dockX;
        const float statusW = (std::min)(dockW, sw - statusX - 20);
        Panel(statusX, sh - 122, statusW, 38);
        WrappedText(statusX + 14, sh - 97, statusW - 28, "저장 · " + saveStatus, muted, 1);
    }
    if (hudVisible)
        r.Text(20, sh - 12, "ESC 메뉴 · Ctrl 회피 · 아이템 자동 흡수 · Tab 기록", muted);

    std::string heading, body;
    const auto* nearbyStory = NearbyStory();
    const int weak =
        dialoguePending || IsStoryObjective(nearbyStory) ? -1 : levelOne.Capturable(player);
    if (weak >= 0)
    {
        const char* tickets[] = {"낡은 봉인권", "봉인권", "고급 봉인권"};
        const float chance = levelOne.enemies[weak].boss == 5 ? 1.f
                             : levelOne.enemies[weak].boss == 4
                                 ? levelOne.captureRules.bossChance
                                 : levelOne.captureRules.normalChance;
        heading = "주령 잔영 · 포획 " + std::to_string(int(chance * 100)) + "%";
        heading +=
            levelOne.enemies[weak].remnantTime < 0
                ? " · 퀘스트 보호"
                : " · " + std::to_string(int(std::ceil(levelOne.enemies[weak].remnantTime))) + "초";
        body = levelOne.enemies[weak].boss == 5
                   ? "낡은 봉인권 · 라엔 지원\nF 첫 봉인 / 튜토리얼 잔영은 사라지지 않습니다."
                   : std::string("T 선택 · ") + tickets[levelOne.selectedTicket] + " " +
                         std::to_string(levelOne.tickets[levelOne.selectedTicket]) +
                         "장\nF 포획 / X 정화";
    }
    else if (messageTime > 0 || dialoguePending)
    {
        heading = speaker + (dialoguePending ? (talkingResident >= 0 ? " · F / Enter 대화 종료"
                                                                     : " · F / Enter 계속")
                                             : " · 알림");
        body = message;
        if (dialoguePending)
        {
            const auto pages = DialoguePages();
            dialoguePage = (std::min)(dialoguePage, int(pages.size()) - 1);
            body = pages[dialoguePage];
            heading = speaker + " · F / Enter " +
                      (dialoguePage + 1 < int(pages.size()) ? "다음" : "대화 종료") + " · " +
                      std::to_string(dialoguePage + 1) + " / " + std::to_string(pages.size());
        }
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
            const char* names[] = {"경험치 조각", "무기 강화석", "회복물약", "주령석", "봉인권"};
            heading = "주변 전리품";
            body = std::string(names[static_cast<int>(nearest->kind)]) + " · 가까이 가면 자동 흡수";
        }
        else if (const auto* site = nearbyStory)
        {
            heading = site->name;
            body = site->kind == StoryLayout::Kind::Evidence ? "F · 조사" : "F · 대화";
        }
        else if (NearbyResident())
        {
            heading = "마을 주민";
            body = "F · 대화하기";
        }
        else if (Nearby() >= 0)
        {
            const char* hints[] = {"F · 아이와 대화", "F · 라엔과 대화", "", "F · 불가에서 회복"};
            heading = "상호작용";
            body = hints[Nearby()];
        }
    }
    if (!body.empty())
    {
        const auto box = MessageBounds();
        Panel(box.x, box.y, box.w, box.h);
        WrappedText(box.x + 16, box.y + 27, box.w - 32, heading, gold, 1);
        r.Rect(box.x + 16, box.y + 38, box.w - 32, 1, Color(.28f, .34f, .35f));
        WrappedText(box.x + 16, box.y + 61, box.w - 32, body, ivory, 3);
    }
}
