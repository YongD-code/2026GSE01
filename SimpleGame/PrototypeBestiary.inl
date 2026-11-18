// Seven UI layers: book, frame, tabs, cards, portraits, badges, text.
void ResetBestiary()
{
    bestiarySpecies = bestiaryTab = bestiaryDetailPage = bestiarySkill = bestiarySpecimen = 0;
    bestiaryDetailOpen = false;
    bestiaryCollectionSize = static_cast<size_t>(-1);
}

void RefreshBestiary()
{
    if (bestiaryCollectionSize != levelOne.collection.size())
    {
        for (auto& specimens : bestiarySpecimens)
        {
            specimens.clear();
        }
        for (int i = 0; i < int(levelOne.collection.size()); ++i)
        {
            const int species = levelOne.collection[i].species;
            bestiarySpecimens[species].push_back(i);
            levelOne.discoveredSpecies |= Bestiary::Bit(species);
        }
        bestiaryCollectionSize = levelOne.collection.size();
    }
    for (const auto& enemy : levelOne.enemies)
    {
        if (enemy.health > 0 && StoryEncounter::Get(enemy.boss).humanAttack < 0 &&
            CharacterInView(enemy.position, SpiritCatalog::Get(enemy.type).size))
        {
            levelOne.discoveredSpecies |= Bestiary::Bit(enemy.type);
        }
    }
}

bool BestiaryKnown(int species) const
{
    return (levelOne.discoveredSpecies & Bestiary::Bit(species)) != 0;
}

void BestiaryButton(UIRect box, const std::string& label, bool selected = false)
{
    const bool hover = box.Contains(cursorX, cursorY);
    const Color edge = selected ? Color(.25f, .34f, .49f) : Color(.69f, .60f, .64f);
    r.Rect(box.x, box.y, box.w, box.h, edge);
    r.Rect(box.x + 1,
           box.y + 1,
           box.w - 2,
           box.h - 2,
           selected ? Color(.25f, .34f, .49f)
           : hover  ? Color(1.f, .9f, .92f)
                    : Color(.95f, .94f, .9f));
    WrappedText(box.x + 8,
                box.y + box.h * .5f + 6,
                box.w - 16,
                label,
                selected ? Color(1.f, .96f, .97f) : Color(.23f, .26f, .32f),
                1);
}

void BestiaryPortrait(int species, Point feet, float size, bool known, bool animate)
{
    auto& sheet =
        spiritModelSheets[species].Ready() ? spiritModelSheets[species] : spiritSheets[species];
    const float bob = animate && known ? std::sin(frontTime * 1.8f) * 2.f : 0;
    sheet.DrawPortrait(r,
                       {feet.x, feet.y - size * .5f + bob},
                       size,
                       size,
                       known ? Color(1, 1, 1) : Color(0, 0, 0, .3f));
}

UIRect BestiaryEquipButton(const Bestiary::Layout& layout) const
{
    return {
        layout.detail.x + 82, layout.detail.y + layout.detail.h - 30, layout.detail.w - 164, 28};
}

std::vector<std::string> BestiaryLines(const std::string& text, float maxWidth) const
{
    std::vector<std::string> lines;
    std::string line;
    float used = 0;
    for (size_t i = 0; i < text.size();)
    {
        const unsigned char lead = static_cast<unsigned char>(text[i]);
        const size_t bytes = lead < 128 ? 1 : (lead < 224 ? 2 : (lead < 240 ? 3 : 4));
        const float advance = lead < 128 ? 8.f : 16.f;
        if (text[i] == '\n' || used + advance > maxWidth)
        {
            lines.push_back(line);
            line.clear();
            used = 0;
        }
        if (text[i] != '\n')
        {
            line += text.substr(i, bytes);
            used += advance;
        }
        i += bytes;
    }
    if (!line.empty())
    {
        lines.push_back(line);
    }
    return lines;
}

std::string BestiaryEntryText() const
{
    const int species = bestiarySpecies;
    if (!BestiaryKnown(species))
    {
        return "미발견 주령\n세계에서 주령을 만나면 이름과 생태가 기록됩니다. 포획하면 술식과 개체 "
               "기록을 열 수 있습니다.";
    }
    const auto& entry = SpiritCatalog::Get(species);
    const auto& info = Bestiary::Describe(species);
    const auto& specimens = bestiarySpecimens[species];
    if (bestiaryTab == 0)
    {
        return std::string(entry.story) + "\n\n" + info.role + "\n서식지 · " + info.habitat +
               "\n\n포획 안내\n" + info.capture;
    }
    if (specimens.empty())
    {
        return "포획 후 기록이 열립니다.\n\n" + std::string(info.capture);
    }
    if (bestiaryTab == 1)
    {
        const int set =
            species == SpiritCatalog::Stag ? SpiritCatalog::StagSkills : 1 + entry.skillSpecies;
        const char key = "QWER"[bestiarySkill];
        std::string text = std::string(1, key) + " · " + SkillName(set, bestiarySkill) + "\n\n" +
                           SkillDescription(set, bestiarySkill);
        if (species == SpiritCatalog::Hound || species == SpiritCatalog::Eater ||
            species == SpiritCatalog::Skeleton)
        {
            text += "\n\n현재 조종 술식은 " +
                    std::string(entry.skillSpecies == SpiritCatalog::Pilgrim ? "목 없는 순례자"
                                : entry.skillSpecies == SpiritCatalog::Hands ? "기도하는 손"
                                                                             : "남겨진 자") +
                    " 계열을 공유합니다.";
        }
        return text;
    }
    const int page = (std::min)(bestiarySpecimen, int(specimens.size()) - 1);
    const int index = specimens[page];
    const auto& specimen = levelOne.collection[index];
    std::string text = "보유 " + std::to_string(specimens.size()) + "개체 · " +
                       std::to_string(page + 1) + "번째 기록\n등급 " +
                       std::to_string(specimen.rarity) + "성 · 체력 " +
                       std::to_string(int(specimen.health)) + " / 100\n";
    text += specimen.memory ? "다른 기억을 품은 개체" : "기본 기억을 품은 개체";
    if (levelOne.orderSeal.Blocks(index))
    {
        text += "\n성흔 봉쇄 중 · 전투로 돌아가면 시간이 흐릅니다.";
    }
    else if (captured && index == selectedCompanion)
    {
        text += "\n현재 동행 중";
    }
    return text;
}

void ChangeBestiaryDetailPage(int delta)
{
    const Bestiary::Layout layout(JournalBounds());
    const auto box = layout.Text();
    const int capacity = (std::max)(1, int(box.h / 23));
    const int pages = (std::max)(
        1, (int(BestiaryLines(BestiaryEntryText(), box.w).size()) + capacity - 1) / capacity);
    if (bestiaryDetailPage + delta >= 0 && bestiaryDetailPage + delta < pages)
    {
        bestiaryDetailPage += delta;
        return;
    }
    const auto count = bestiarySpecimens[bestiarySpecies].size();
    if (bestiaryTab == 1 && count > 0)
    {
        bestiarySkill = (bestiarySkill + delta + 4) % 4;
    }
    else if (bestiaryTab == 2 && count > 0)
    {
        bestiarySpecimen = (bestiarySpecimen + delta + int(count)) % int(count);
    }
    else
    {
        return;
    }
    const int lines = int(BestiaryLines(BestiaryEntryText(), box.w).size());
    bestiaryDetailPage = delta > 0 ? 0 : (std::max)(0, (lines - 1) / capacity);
}

void BestiaryClick(int x, int y)
{
    const Bestiary::Layout layout(JournalBounds());
    RefreshBestiary();
    if (layout.compact && bestiaryDetailOpen && layout.ListBack().Contains(x, y))
    {
        bestiaryDetailOpen = false;
        return;
    }
    if (!layout.compact || bestiaryDetailOpen)
    {
        for (int tab = 0; tab < 3; ++tab)
        {
            if (layout.Tab(tab).Contains(x, y))
            {
                bestiaryTab = tab;
                bestiaryDetailPage = bestiarySpecimen = 0;
                return;
            }
        }
        for (int side = 0; side < 2; ++side)
        {
            if (layout.DetailButton(side).Contains(x, y))
            {
                ChangeBestiaryDetailPage(side == 0 ? -1 : 1);
                return;
            }
        }
        const auto& specimens = bestiarySpecimens[bestiarySpecies];
        if (bestiaryTab == 2 && !specimens.empty() && BestiaryEquipButton(layout).Contains(x, y))
        {
            const int page = (std::min)(bestiarySpecimen, int(specimens.size()) - 1);
            SelectCompanion(specimens[page]);
            return;
        }
    }
    if (!layout.compact || !bestiaryDetailOpen)
    {
        for (int local = 0; local < layout.capacity; ++local)
        {
            const int species = journalPage * layout.capacity + local;
            if (species < SpiritCatalog::Count && layout.Card(local).Contains(x, y))
            {
                bestiarySpecies = species;
                bestiaryDetailPage = bestiarySkill = bestiarySpecimen = 0;
                bestiaryDetailOpen = true;
                return;
            }
        }
        for (int side = 0; side < 2; ++side)
        {
            if (layout.ListButton(side).Contains(x, y))
            {
                ChangeJournalPage(side == 0 ? -1 : 1);
                return;
            }
        }
    }
}

void DrawBestiaryDetail(const Bestiary::Layout& layout)
{
    const auto area = layout.detail;
    const bool known = BestiaryKnown(bestiarySpecies);
    const Color ink(.19f, .22f, .29f), muted(.43f, .44f, .48f), teal(.25f, .34f, .49f);
    r.Rect(area.x - 8, area.y - 6, area.w + 16, area.h + 8, Color(1.f, .985f, .96f, .74f));
    const float portraitSize = layout.compact ? 32.f : 112.f;
    BestiaryPortrait(
        bestiarySpecies,
        {layout.compact ? area.x + 18 : area.x + area.w - 60, area.y + (layout.compact ? 34 : 121)},
        portraitSize,
        known,
        true);
    WrappedText(area.x + (layout.compact ? 42 : 0),
                area.y + 24,
                area.w - (layout.compact ? 128 : 132),
                known ? SpiritName(bestiarySpecies) : "미발견",
                ink,
                layout.compact ? 1 : 2);
    if (layout.compact)
    {
        BestiaryButton(layout.ListBack(), "목록");
    }
    else
    {
        const auto count = bestiarySpecimens[bestiarySpecies].size();
        r.Text(area.x,
               area.y + 76,
               count > 0 ? "포획 완료"
               : known   ? "발견 기록"
                         : "기록 없음",
               teal);
        r.Text(area.x, area.y + 103, "도감 " + std::to_string(bestiarySpecies + 1), muted);
    }
    const char* tabs[] = {"생태", "QWER", "포획 기록"};
    for (int tab = 0; tab < 3; ++tab)
    {
        BestiaryButton(layout.Tab(tab), tabs[tab], bestiaryTab == tab);
    }

    const auto textBox = layout.Text();
    const int lineCapacity = (std::max)(1, int(textBox.h / 23));
    const auto lines = BestiaryLines(BestiaryEntryText(), textBox.w);
    int pages = (std::max)(1, (int(lines.size()) + lineCapacity - 1) / lineCapacity);
    const bool specimens = known && bestiaryTab == 2 && !bestiarySpecimens[bestiarySpecies].empty();
    bestiaryDetailPage = (std::min)(bestiaryDetailPage, pages - 1);
    const int start = bestiaryDetailPage * lineCapacity;
    for (int line = 0; line < lineCapacity && start + line < int(lines.size()); ++line)
    {
        r.Text(textBox.x, textBox.y + 17 + line * 23, lines[start + line], ink);
    }
    BestiaryButton(layout.DetailButton(0), "이전");
    BestiaryButton(layout.DetailButton(1), "다음");
    if (specimens)
    {
        const int index = bestiarySpecimens[bestiarySpecies][bestiarySpecimen];
        const char* action = controlling                              ? "본체 복귀 후 선택"
                             : levelOne.orderSeal.Blocks(index)       ? "봉쇄 중"
                             : captured && index == selectedCompanion ? "동행 중"
                                                                      : "동행하기";
        BestiaryButton(BestiaryEquipButton(layout), action, captured && index == selectedCompanion);
    }
    else
    {
        const auto previous = layout.DetailButton(0);
        const std::string counter = bestiaryTab == 1 ? std::string(1, "QWER"[bestiarySkill])
                                                     : std::to_string(bestiaryDetailPage + 1) +
                                                           " / " + std::to_string(pages);
        r.Text(previous.x + previous.w + 18, previous.y + 20, counter, muted);
    }
}

void DrawBestiary()
{
    RefreshBestiary();
    const Bestiary::Layout layout(JournalBounds());
    const auto b = layout.book;
    const Color ink(.19f, .22f, .29f), teal(.25f, .34f, .49f);
    r.Rect(0, 0, float(width), float(height), Color(.03f, .04f, .06f, .82f));
    r.Rect(b.x, b.y, b.w, b.h, Color(.97f, .96f, .92f));
    bestiaryBook.DrawPanel(r, b.x, b.y, b.w, b.h, 1.f);
    r.Rect(b.x + 20, b.y + 14, b.w - 40, 74, Color(1.f, .985f, .96f, .78f));
    r.Text(b.x + 28, b.y + 35, "주령 도감", ink, true);
    const char* tabs[] = {"능력치", "소지품", "주령", "이벤트", "도움"};
    for (int tab = 0; tab < 5; ++tab)
    {
        BestiaryButton(JournalTab(tab), tabs[tab], tab == 2);
    }
    if (!layout.compact || !bestiaryDetailOpen)
    {
        int seen = 0, owned = 0;
        for (int species = 0; species < SpiritCatalog::Count; ++species)
        {
            seen += BestiaryKnown(species);
            owned += !bestiarySpecimens[species].empty();
        }
        r.Rect(layout.list.x - 8,
               layout.list.y - 6,
               layout.list.w + 16,
               layout.list.h + 8,
               Color(1.f, .985f, .96f, .74f));
        const std::string counter = "발견 " + std::to_string(seen) + " · 포획 " +
                                    std::to_string(owned) + " / " +
                                    std::to_string(SpiritCatalog::Count);
        r.Text(layout.list.x, layout.list.y + 19, counter, teal);
        const int pages = (SpiritCatalog::Count + layout.capacity - 1) / layout.capacity;
        journalPage = (std::max)(0, (std::min)(journalPage, pages - 1));
        for (int local = 0; local < layout.capacity; ++local)
        {
            const int species = journalPage * layout.capacity + local;
            if (species >= SpiritCatalog::Count)
            {
                break;
            }
            const auto card = layout.Card(local);
            const bool known = BestiaryKnown(species);
            const auto count = bestiarySpecimens[species].size();
            const bool selected = species == bestiarySpecies;
            const Color accents[] = {Color(.7f, .42f, .53f),
                                     Color(.25f, .34f, .49f),
                                     Color(.53f, .52f, .65f),
                                     Color(.4f, .57f, .45f)};
            const Color accent = selected ? teal : accents[species % 4];
            r.Rect(card.x, card.y, card.w, card.h, selected ? teal : Color(.76f, .7f, .72f));
            r.Rect(card.x + 2,
                   card.y + 2,
                   card.w - 4,
                   card.h - 4,
                   selected ? Color(.99f, .89f, .92f) : Color(.98f, .98f, .95f));
            r.Rect(card.x + 2, card.y + 2, 3, card.h - 4, accent);
            BestiaryPortrait(species, {card.x + card.w * .5f, card.y + 61}, 54, known, false);
            r.Text(card.x + 8, card.y + 18, std::to_string(species + 1), Color(.45f, .43f, .37f));
            WrappedText(card.x + 6,
                        card.y + 77,
                        card.w - 12,
                        known ? SpiritName(species) : "미발견",
                        ink,
                        2);
            r.Text(card.x + card.w - 58,
                   card.y + 18,
                   count > 0 ? "포획"
                   : known   ? "발견"
                             : "?",
                   accent);
        }
        BestiaryButton(layout.ListButton(0), "이전 [");
        BestiaryButton(layout.ListButton(1), "다음 ]");
        r.Text(layout.list.x + layout.list.w * .5f - 20,
               layout.book.y + layout.book.h - 43,
               std::to_string(journalPage + 1) + " / " + std::to_string(pages),
               teal);
    }
    if (!layout.compact || bestiaryDetailOpen)
    {
        DrawBestiaryDetail(layout);
    }
    WrappedText(b.x + 24,
                b.y + b.h - 24,
                b.w - 48,
                layout.compact ? "카드 선택 · 목록으로 돌아가기 / Tab 닫기"
                               : "카드 선택 · 포획 기록에서 동행 선택 / Tab 닫기",
                ink,
                1);
}
