// Member menu implementation. Controls share layout between drawing and input.
struct PauseBounds
{
    float x, y, w, h;
    int columns, rows;
};

struct PauseEntry
{
    const char* label;
    int action;
};

struct PauseColumn
{
    const char* title;
    const PauseEntry* entries;
    int count;
};

PauseBounds MenuBounds() const
{
    const float w = (std::min)(1100.f, float(width) - 40);
    const float h = (std::min)(650.f, float(height) - 40);
    return {(width - w) / 2,
            (height - h) / 2,
            w,
            h,
            w >= 960   ? 3
            : w >= 640 ? 2
                       : 1,
            h >= 570 ? 2 : 1};
}

const PauseColumn* PauseColumns() const
{
    static const PauseEntry character[] = {{"능력치", 4}, {"기본 화면", 13}};
    static const PauseEntry inventory[] = {{"인벤토리", 1}, {"주령석 강화", 9}};
    static const PauseEntry combat[] = {{"자동 발사", 10}, {"주령 능력", 11}, {"주령 조종", 12}};
    static const PauseEntry adventure[] = {{"이벤트 · 퀘스트", 3}};
    static const PauseEntry records[] = {{"주령 도감", 2}, {"저장", 5}, {"불러오기", 6}};
    static const PauseEntry other[] = {{"조작 안내", 7}, {"게임 종료", 8}};
    static const PauseColumn columns[] = {{"캐릭터", character, 2},
                                          {"소지품", inventory, 2},
                                          {"전투", combat, 3},
                                          {"모험", adventure, 1},
                                          {"기록", records, 3},
                                          {"기타", other, 2}};
    return columns;
}

UIRect MenuCategory(int index) const
{
    const auto b = MenuBounds();
    const int slot = index % (b.columns * b.rows);
    const float w = (b.w - 32 - (b.columns - 1) * 18) / b.columns;
    const float h = (b.h - 158 - (b.rows - 1) * 18) / b.rows;
    return {
        b.x + 16 + (slot % b.columns) * (w + 18), b.y + 58 + (slot / b.columns) * (h + 18), w, h};
}

UIRect MenuEntryRect(int category, int row) const
{
    const auto p = MenuCategory(category);
    const float step = (std::min)(48.f, (p.h - 42) / 3);
    return {p.x + 10, p.y + 36 + row * step, p.w - 20, step - 6};
}

UIRect MenuFooter(int index) const
{
    const auto b = MenuBounds();
    const float w = (b.w - 56) / 3;
    return {b.x + 16 + index * (w + 12), b.y + b.h - 44, w, 32};
}

int MenuPages() const
{
    const auto b = MenuBounds();
    return (6 + b.columns * b.rows - 1) / (b.columns * b.rows);
}

void ChangeMenuPage(int direction)
{
    menuPage = (menuPage + direction + MenuPages()) % MenuPages();
    const auto b = MenuBounds();
    pauseSelection = PauseColumns()[menuPage * b.columns * b.rows].entries[0].action;
}

void MoveMenuSelection(int direction)
{
    if (exitConfirm)
    {
        pauseSelection = (pauseSelection + 1) % 2;
        return;
    }
    const auto b = MenuBounds();
    const int size = b.columns * b.rows;
    const int first = (std::min)(menuPage, MenuPages() - 1) * size;
    std::vector<int> actions;
    for (int c = first; c < (std::min)(6, first + size); ++c)
        for (int i = 0; i < PauseColumns()[c].count; ++i)
            actions.push_back(PauseColumns()[c].entries[i].action);
    actions.push_back(0);
    const auto found = std::find(actions.begin(), actions.end(), pauseSelection);
    const int index = found == actions.end() ? 0 : int(found - actions.begin());
    pauseSelection = actions[(index + direction + int(actions.size())) % int(actions.size())];
}

int MenuActionAt(int x, int y) const
{
    const auto b = MenuBounds();
    if (exitConfirm)
    {
        for (int i = 0; i < 2; ++i)
            if (UIRect{b.x + 24, b.y + 64 + i * 62, b.w - 48, 48}.Contains(x, y))
                return i;
        return -1;
    }
    for (int i = 0; i < 3; ++i)
        if (MenuFooter(i).Contains(x, y))
            return i == 1 ? 0 : i == 0 ? 14 : 15;
    const int pageSize = b.columns * b.rows;
    const int first = (std::min)(menuPage, MenuPages() - 1) * pageSize;
    for (int c = first; c < (std::min)(6, first + pageSize); ++c)
        for (int row = 0; row < PauseColumns()[c].count; ++row)
            if (MenuEntryRect(c, row).Contains(x, y))
                return PauseColumns()[c].entries[row].action;
    return -1;
}

void ActivateMenu(int item)
{
    ClearInput();
    if (exitConfirm)
    {
        if (item == 0)
        {
            OpenSlots(SlotMode::Save);
            exitAfterSave = true;
            return;
        }
        else if (item != 1)
        {
            return;
        }
        skipExitSave = true;
        exitRequested = true;
        return;
    }

    pauseSelection = item;
    switch (item)
    {
    case 0:
        pauseMenu = false;
        break;
    case 1:
    case 2:
    case 3:
    case 4:
    case 7:
        menuTab = item == 1 ? 1 : item == 2 ? 2 : item == 3 ? 3 : item == 4 ? 0 : 4;
        journal = true;
        journalPage = 0;
        break;
    case 5:
        OpenSlots(SlotMode::Save);
        break;
    case 6:
        OpenSlots(SlotMode::Load);
        break;
    case 8:
        exitConfirm = true;
        pauseSelection = 0;
        break;
    case 9:
        levelOne.Upgrade(captured);
        break;
    case 10:
        levelOne.automatic = !levelOne.automatic;
        Say("자동 발사", levelOne.automatic ? "자동 발사를 켰습니다." : "자동 발사를 껐습니다.");
        break;
    case 11:
        levelOne.UseSkill(player);
        break;
    case 12:
        if (captured)
        {
            controlling = !controlling;
            Say("조종 전환", controlling ? "주령을 조종합니다." : "방랑자의 몸으로 돌아왔습니다.");
        }
        else
        {
            Say("조종 전환", "봉인한 주령이 있어야 조종할 수 있습니다.");
        }
        break;
    case 14:
        ChangeMenuPage(-1);
        break;
    case 15:
        ChangeMenuPage(1);
        break;
    case 13:
        hudVisible = !hudVisible;
        break;
    default:
        break;
    }
}

void DrawPauseMenu()
{
    const auto b = MenuBounds();
    const Color ivory(.88f, .88f, .82f), gold(.85f, .7f, .43f), muted(.6f, .67f, .69f);
    r.Rect(0, 0, float(width), float(height), Color(0, 0, 0, .76f));
    Panel(b.x, b.y, b.w, b.h);
    r.Text(b.x + 20, b.y + 32, exitConfirm ? "여정 종료" : "메뉴", gold, true);
    if (exitConfirm)
    {
        const char* labels[] = {"1  저장하고 종료", "2  저장하지 않고 종료"};
        for (int i = 0; i < 2; ++i)
            UIButton({b.x + 24, b.y + 64 + i * 62, b.w - 48, 48}, labels[i], pauseSelection == i);
        WrappedText(b.x + 24,
                    b.y + 200,
                    b.w - 48,
                    "저장하지 않으면 마지막 저장 이후의 진행을 잃습니다. ESC로 돌아갑니다.",
                    ivory,
                    2);
    }
    else
    {
        menuPage = (std::min)(menuPage, MenuPages() - 1);
        const int pageSize = b.columns * b.rows;
        for (int c = menuPage * pageSize; c < (std::min)(6, (menuPage + 1) * pageSize); ++c)
        {
            const auto p = MenuCategory(c);
            Panel(p.x, p.y, p.w, p.h);
            r.Text(p.x + 14, p.y + 24, PauseColumns()[c].title, gold);
            for (int row = 0; row < PauseColumns()[c].count; ++row)
            {
                const auto& entry = PauseColumns()[c].entries[row];
                UIButton(MenuEntryRect(c, row), entry.label, pauseSelection == entry.action);
            }
        }
        UIButton(MenuFooter(0), "이전 [");
        UIButton(MenuFooter(1), "계속 · ESC");
        UIButton(MenuFooter(2), "다음 ]");
    }
    r.Rect(b.x + 16, b.y + b.h - 88, b.w - 32, 32, Color(.08f, .12f, .14f));
    WrappedText(b.x + 28,
                b.y + b.h - 66,
                b.w - 56,
                (exitConfirm
                     ? ""
                     : std::to_string(menuPage + 1) + "/" + std::to_string(MenuPages()) + " · ") +
                    saveStatus,
                muted,
                1);
}
