#include "stdafx.h"
#include "Prototype.h"

void Prototype::OpenSlots(SlotMode mode)
{
    ClearInput();
    slotMode = mode;
    overwriteConfirm = false;
    exitAfterSave = false;
    frontSelection = activeSlot > 0 ? activeSlot - 1 : 0;
    for (int i = 0; i < 3; ++i)
    {
        SaveGame::State state;
        const auto result = SaveGame::Load(chapter, state, i + 1);
        slotLabels[i] = result == SaveGame::Result::Missing ? "빈 슬롯"
                        : result == SaveGame::Result::Invalid
                            ? "읽을 수 없는 기록"
                            : "레벨 " + std::to_string(state.level.level) + " · 목표 " +
                                  std::to_string(state.chapter.step) + "개 완료" +
                                  (result == SaveGame::Result::Backup ? " · 백업 복구 가능" : "");
    }
    ProgressNotice(mode == SlotMode::NewGame ? "새 여정을 기록할 슬롯을 선택하세요."
                   : mode == SlotMode::Save  ? "저장할 슬롯을 선택하세요."
                                             : "이어갈 여정을 선택하세요.");
}

void Prototype::NewGame()
{
    world = World();
    levelOne = LevelOne();
    levelOne.captureRules.Load();
    levelOne.LoadCombat();
    chapter = Chapter();
    chapter.Load();
    player = {70, 160};
    spirit = player;
    camera = player;
    keeper = {150, -100};
    captured = controlling = dialoguePending = false;
    selectedCompanion = capturedSpirit = collectionPage = eventPage = 0;
    protagonistAnimation = SpriteAnimation();
    companionAnimation = SpriteAnimation();
    moving = protagonistSprinting = dodgeHeld = false;
    speaker.clear();
    message.clear();
    messageTime = 0;
    autoSaveTime = 0;
    activeSlot = 0;
    automaticSaveAllowed = false;
    titleScreen = pauseMenu = journal = exitConfirm = false;
    skipExitSave = exitRequested = false;
    ClearInput();
    world.Stream(camera, player, width, height);
    UpdateChapter();
}

void Prototype::CompleteSlot()
{
    const int slot = frontSelection + 1;
    if (slotMode == SlotMode::Load)
    {
        if (!LoadProgress(titleScreen, slot))
            return;
    }
    else
    {
        if (slotMode == SlotMode::NewGame)
            NewGame();
        if (!SaveProgress(true, slot))
        {
            // NewGame has already started, but never automatically write to an
            // old slot after a failed first save. Let the player explicitly retry.
            if (slotMode == SlotMode::NewGame)
                slotMode = SlotMode::Save;
            overwriteConfirm = false;
            return;
        }
    }
    slotMode = SlotMode::Closed;
    overwriteConfirm = false;
    if (exitAfterSave)
    {
        skipExitSave = true;
        exitRequested = true;
    }
    exitAfterSave = false;
    ClearInput();
}

void Prototype::SelectFront(int item)
{
    if (slotMode == SlotMode::Closed)
    {
        if (item == 0)
            OpenSlots(SlotMode::NewGame);
        else if (item == 1)
            OpenSlots(SlotMode::Load);
        else if (item == 2)
        {
            skipExitSave = true;
            exitRequested = true;
        }
        return;
    }
    if (overwriteConfirm)
    {
        if (item == 0)
            CompleteSlot();
        else if (item == 1)
            overwriteConfirm = false;
        return;
    }
    if (item < 0 || item > 2)
        return;
    frontSelection = item;
    if (slotMode != SlotMode::Load)
    {
        if (slotMode == SlotMode::Save && !CanSaveProgress())
        {
            ProgressNotice("전투·귀환·사망 중에는 저장할 수 없습니다.");
            return;
        }
        const std::wstring path = SaveGame::Path(item + 1);
        auto absent = [](const std::wstring& file)
        {
            if (GetFileAttributesW(file.c_str()) != INVALID_FILE_ATTRIBUTES)
                return false;
            const DWORD error = GetLastError();
            return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
        };
        if (path.empty() || !absent(path) || !absent(path + L".bak"))
        {
            overwriteConfirm = true;
            return;
        }
    }
    CompleteSlot();
}

void Prototype::FrontKey(unsigned char key)
{
    ClearInput();
    if (key == 27)
    {
        if (overwriteConfirm)
            overwriteConfirm = false;
        else
        {
            slotMode = SlotMode::Closed;
            exitAfterSave = false;
        }
        return;
    }
    if (key == 13)
        SelectFront(overwriteConfirm ? 0 : frontSelection);
    else if (key >= '1' && key <= (overwriteConfirm ? '2' : '3'))
        SelectFront(key - '1');
}

UIRect Prototype::FrontBounds() const
{
    const float w = (std::min)(660.f, float(width) - 32);
    const float h = (std::min)(460.f, float(height) - 32);
    return {(width - w) / 2, (height - h) / 2, w, h};
}

UIRect Prototype::FrontButton(int index) const
{
    const auto b = FrontBounds();
    const float step = (b.h - 190) / 3;
    return {b.x + 24, b.y + 88 + index * step, b.w - 48, step - 10};
}

bool Prototype::FrontClick(int x, int y)
{
    if (width < 480 || height < 360)
        return true;
    const auto b = FrontBounds();
    if (slotMode != SlotMode::Closed &&
        UIRect{b.x + 24, b.y + b.h - 92, b.w - 48, 36}.Contains(x, y))
    {
        FrontKey(27);
        return true;
    }
    for (int i = 0; i < (overwriteConfirm ? 2 : 3); ++i)
        if (FrontButton(i).Contains(x, y))
        {
            SelectFront(i);
            break;
        }
    return true;
}

bool Prototype::GameUIClick(int x, int y)
{
    if ((!chapter.choicePending && !levelOne.Dead()) || width < 480 || height < 360)
        return false;
    const bool choice = chapter.choicePending;
    const float w = (std::min)(640.f, float(width) - 40), h = choice ? 254.f : 190.f;
    const float left = (width - w) / 2, top = (height - h) / 2;
    for (int i = 0; i < (choice ? 3 : 2); ++i)
    {
        const UIRect button = {
            left + 24, top + (choice ? 64 : 62) + i * (choice ? 56 : 58), w - 48, 44};
        if (!button.Contains(x, y))
            continue;
        ClearInput();
        if (choice)
            Key(static_cast<unsigned char>('1' + i), true);
        else if (i == 0)
            ProgressKey(true);
        else
            Key('r', true);
        ClearInput();
        break;
    }
    return true;
}

void Prototype::DrawFront()
{
    const Color ivory(.9f, .88f, .8f), gold(.85f, .69f, .41f), muted(.6f, .65f, .66f);
    r.Rect(0, 0, float(width), float(height), Color(.015f, .025f, .04f, .86f));
    if (width < 480 || height < 360)
    {
        r.Text(16, 32, "창을 넓혀 주세요.", ivory);
        return;
    }
    const auto b = FrontBounds();
    Panel(b.x, b.y, b.w, b.h);
    const bool choosing = slotMode != SlotMode::Closed;
    const std::string heading = !choosing                       ? "마지막 불씨"
                                : overwriteConfirm              ? "기존 여정을 덮어쓸까요?"
                                : slotMode == SlotMode::NewGame ? "새 여정 · 슬롯 선택"
                                : slotMode == SlotMode::Save    ? "여정 저장"
                                                                : "여정 불러오기";
    r.Text(b.x + 24, b.y + 32, heading, gold, true);
    WrappedText(b.x + 24,
                b.y + 60,
                b.w - 48,
                overwriteConfirm
                    ? "선택한 슬롯 " + std::to_string(frontSelection + 1) + "의 기록을 바꿉니다."
                : choosing ? "여정을 보관할 공간을 선택하세요."
                           : "어둠 속에서도, 이야기는 이어집니다.",
                muted,
                1);
    r.Rect(b.x + 24, b.y + 74, b.w - 48, 1, Color(.28f, .34f, .35f));
    const char* main[] = {"처음부터 시작", "불러오기", "종료"};
    const char* confirm[] = {"덮어쓰기", "취소"};
    for (int i = 0; i < (overwriteConfirm ? 2 : 3); ++i)
    {
        const std::string label = overwriteConfirm ? confirm[i]
                                  : choosing
                                      ? "슬롯 " + std::to_string(i + 1) + " · " + slotLabels[i]
                                      : main[i];
        UIButton(FrontButton(i), label, overwriteConfirm ? i == 0 : frontSelection == i);
    }
    if (choosing)
        UIButton({b.x + 24, b.y + b.h - 92, b.w - 48, 36}, "돌아가기 · ESC");
    r.Rect(b.x + 24, b.y + b.h - 44, b.w - 48, 28, Color(.06f, .09f, .11f));
    WrappedText(b.x + 36,
                b.y + b.h - 24,
                b.w - 72,
                choosing ? saveStatus : "마우스 클릭 · 방향키 / Enter · 숫자 1~3",
                muted,
                1);
}
