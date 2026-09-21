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

bool Prototype::FrontClick(int x, int y)
{
    const float w = (std::min)(600.f, float(width) - 32);
    const float left = (width - w) * .5f, top = (height - 330.f) * .5f;
    if (x < left + 24 || x >= left + w - 24)
        return true;
    if (slotMode != SlotMode::Closed && y >= top + 258 && y < top + 288)
    {
        FrontKey(27);
        return true;
    }
    const int count = overwriteConfirm ? 2 : 3;
    for (int i = 0; i < count; ++i)
    {
        const float row = top + 86 + i * 54;
        if (y >= row && y < row + 46)
        {
            SelectFront(i);
            break;
        }
    }
    return true;
}

void Prototype::DrawFront()
{
    const Color ivory(.9f, .88f, .8f), gold(.85f, .69f, .41f), muted(.6f, .65f, .66f);
    const float w = (std::min)(600.f, float(width) - 32);
    const float left = (width - w) * .5f, top = (height - 330.f) * .5f;
    r.Rect(0, 0, float(width), float(height), Color(.015f, .025f, .04f, .86f));
    Panel(left, top, w, 330);
    const bool choosing = slotMode != SlotMode::Closed;
    const std::string heading = !choosing                       ? "마지막 불씨"
                                : overwriteConfirm              ? "기존 여정을 덮어쓸까요?"
                                : slotMode == SlotMode::NewGame ? "새 여정 · 슬롯 선택"
                                : slotMode == SlotMode::Save    ? "여정 저장"
                                                                : "여정 불러오기";
    r.Text(left + 26, top + 36, heading, gold, true);
    r.Text(left + 26,
           top + 65,
           overwriteConfirm
               ? "선택한 슬롯 " + std::to_string(frontSelection + 1) + "의 기록을 바꿉니다."
           : choosing ? "세 개의 슬롯에 각각 여정을 남길 수 있습니다."
                      : "어둠 속에서도, 이야기는 이어집니다.",
           muted);
    const char* main[] = {"처음부터 시작", "불러오기", "종료"};
    const char* confirm[] = {"덮어쓰기", "취소"};
    const int count = overwriteConfirm ? 2 : 3;
    for (int i = 0; i < count; ++i)
    {
        const float y = top + 86 + i * 54;
        const bool selected = overwriteConfirm ? i == 0 : frontSelection == i;
        r.Rect(left + 24,
               y,
               w - 48,
               46,
               selected ? Color(.2f, .27f, .29f, .98f) : Color(.09f, .13f, .16f, .98f));
        const std::string label = overwriteConfirm ? confirm[i]
                                  : choosing
                                      ? "슬롯 " + std::to_string(i + 1) + " · " + slotLabels[i]
                                      : main[i];
        WrappedText(left + 38, y + 29, w - 76, label, selected ? gold : ivory, 1);
    }
    if (choosing)
    {
        r.Rect(left + 24, top + 258, w - 48, 30, Color(.1f, .15f, .18f));
        r.Text(left + 38, top + 279, "돌아가기 · ESC", ivory);
        WrappedText(left + 24, top + 312, w - 48, saveStatus, muted, 1);
    }
    else
        r.Text(left + 26, top + 297, "마우스 클릭 · 방향키 / Enter · 숫자 1~3", muted);
}
