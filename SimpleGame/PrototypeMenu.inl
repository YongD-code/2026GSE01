// Included inside Prototype. Menu actions keep dialogue and quest choices intact.
struct PauseBounds
{
    float x, y, w, h, step;
};

PauseBounds MenuBounds() const
{
    const float w = (std::min)(560.f, float(width) - 36);
    const float h = (std::min)(580.f, float(height) - 24);
    return {(width - w) * .5f, (height - h) * .5f, w, h, (h - 130) / 9};
}

void ActivateMenu(int item)
{
    ClearInput();
    if (exitConfirm)
    {
        if (item == 0)
        {
            if (!SaveProgress(true))
            {
                return;
            }
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
    default:
        break;
    }
}

void DrawPauseMenu()
{
    const auto box = MenuBounds();
    const Color ivory(.88f, .85f, .77f), muted(.57f, .63f, .65f), gold(.85f, .7f, .43f);
    r.Rect(0, 0, float(width), float(height), Color(0, 0, 0, .72f));
    Panel(box.x, box.y, box.w, box.h);
    r.Text(
        box.x + 22, box.y + 32, exitConfirm ? "게임을 종료할까요?" : "잠시 쉬어가기", gold, true);
    const char* entries[] = {"1 계속하기",
                             "2 인벤토리",
                             "3 주령 도감",
                             "4 이벤트·퀘스트",
                             "5 능력치",
                             "6 저장",
                             "7 불러오기",
                             "8 조작 안내",
                             "9 게임 종료"};
    const char* exits[] = {"1 저장하고 종료", "2 저장하지 않고 종료"};
    const int count = exitConfirm ? 2 : 9;
    for (int i = 0; i < count; ++i)
    {
        const float y = box.y + 55 + i * box.step;
        if (pauseSelection == i)
        {
            r.Rect(box.x + 16, y, box.w - 32, box.step - 2, Color(.19f, .22f, .24f, .95f));
        }
        r.Text(box.x + 28,
               y + box.step * .72f,
               exitConfirm ? exits[i] : entries[i],
               pauseSelection == i ? gold : ivory);
    }
    if (exitConfirm)
    {
        WrappedText(box.x + 22,
                    box.y + 65 + box.step * 3,
                    box.w - 44,
                    CanSaveProgress() ? "저장하지 않고 종료하면 마지막 저장 이후의 진행을 잃습니다."
                                      : "현재는 저장할 수 없습니다. 돌아가 전투를 마치거나 마지막 "
                                        "저장을 남기고 종료하세요.",
                    ivory,
                    3);
    }
    r.Text(box.x + 22, box.y + box.h - 70, "클릭 / 숫자 / 방향키·Enter · ESC 돌아가기", muted);
    WrappedText(box.x + 22, box.y + box.h - 45, box.w - 44, saveStatus, ivory, 2);
}
