// Included inside Prototype. The pause menu groups existing game features by purpose.
struct PauseBounds
{
    float x, y, w, h, columnWidth, rowHeight;
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
    const float w = (std::min)(1160.f, float(width) - 36.f);
    const float h = (std::min)(590.f, float(height) - 30.f);
    return {(width - w) * .5f, (height - h) * .5f, w, h, w / 6.f, 43.f};
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

int MenuActionAt(int x, int y) const
{
    const auto box = MenuBounds();
    if (exitConfirm)
    {
        const int row = int((y - box.y - 88.f) / box.rowHeight);
        return x >= box.x + 28 && x <= box.x + box.w - 28 && row >= 0 && row < 2 ? row : -1;
    }

    if (y >= box.y + box.h - 39.f && y <= box.y + box.h - 13.f)
    {
        return 0;
    }

    const int column = int((x - box.x) / box.columnWidth);
    const int row = int((y - box.y - 87.f) / box.rowHeight);
    if (column < 0 || column >= 6 || row < 0)
        return -1;
    const PauseColumn& panel = PauseColumns()[column];
    return row < panel.count ? panel.entries[row].action : -1;
}

void ActivateMenu(int item)
{
    ClearInput();
    if (exitConfirm)
    {
        if (item == 0)
        {
            if (!SaveProgress(true))
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
        break;
    case 5:
        SaveProgress(true);
        break;
    case 6:
        LoadProgress();
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
    case 13:
        hudVisible = !hudVisible;
        break;
    default:
        break;
    }
}

void DrawPauseMenu()
{
    const auto box = MenuBounds();
    const Color ivory(.91f, .94f, .91f), muted(.56f, .71f, .73f), cyan(.26f, .77f, .84f);
    r.Rect(0, 0, float(width), float(height), Color(.01f, .04f, .06f, .72f));
    r.Rect(box.x, box.y, box.w, box.h, Color(.05f, .15f, .18f, .96f));
    r.Rect(box.x, box.y, box.w, 42, Color(.08f, .48f, .57f, .96f));
    r.Text(box.x + 17, box.y + 28, exitConfirm ? "게임 종료" : "메뉴", ivory, true);

    if (exitConfirm)
    {
        r.Text(box.x + 30, box.y + 72, "여정을 마칠까요?", cyan, true);
        const char* choices[] = {"저장하고 종료", "저장하지 않고 종료"};
        for (int i = 0; i < 2; ++i)
        {
            const float y = box.y + 88.f + i * box.rowHeight;
            r.Rect(box.x + 28, y, box.w - 56, box.rowHeight - 4, Color(.12f, .25f, .29f, .96f));
            r.Text(box.x + 43, y + 28, choices[i], pauseSelection == i ? cyan : ivory);
        }
        WrappedText(box.x + 30,
                    box.y + 205,
                    box.w - 60,
                    CanSaveProgress() ? "저장하지 않고 종료하면 마지막 저장 이후의 진행을 잃습니다."
                                      : "현재는 저장할 수 없습니다. 전투가 끝난 뒤 저장하고 "
                                        "종료하거나 저장하지 않고 종료하세요.",
                    ivory,
                    3);
    }
    else
    {
        const PauseColumn* columns = PauseColumns();
        for (int column = 0; column < 6; ++column)
        {
            const float x = box.x + column * box.columnWidth;
            const PauseColumn& panel = columns[column];
            r.Rect(
                x + 3, box.y + 47, box.columnWidth - 6, box.h - 96, Color(.08f, .2f, .23f, .94f));
            r.Rect(x + 3, box.y + 47, box.columnWidth - 6, 34, Color(.08f, .43f, .51f, .98f));
            r.Text(x + 14, box.y + 70, panel.title, ivory, true);
            for (int row = 0; row < panel.count; ++row)
            {
                const float y = box.y + 87.f + row * box.rowHeight;
                const bool selected = pauseSelection == panel.entries[row].action;
                r.Rect(x + 10,
                       y,
                       box.columnWidth - 20,
                       box.rowHeight - 4,
                       selected ? Color(.16f, .43f, .48f, .98f) : Color(.12f, .28f, .31f, .9f));
                r.Text(x + 20, y + 28, panel.entries[row].label, selected ? cyan : ivory);
            }
        }
        r.Rect(box.x + 18, box.y + box.h - 39, box.w - 36, 26, Color(.07f, .32f, .38f, .94f));
        r.Text(box.x + 30, box.y + box.h - 20, "계속하기 · 클릭 또는 ESC", muted);
    }
    WrappedText(box.x + 20, box.y + box.h - 68, box.w - 40, saveStatus, ivory, 1);
}
