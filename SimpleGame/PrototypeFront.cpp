#include "stdafx.h"
#include "Prototype.h"

namespace
{
struct OpeningShot
{
    int image;
    float duration;
    const char* caption;
    const char* secondCaption;
};

const OpeningShot openingShots[] = {
    {1, 6.5f, "길이 엇갈리면, 이 표시를 따라와.", "우리는 같은 길로 돌아오기로 약속했다."},
    {2, 7.f, "동생은 돌아오지 않았다.", "동생이라니… 무슨 소리니?"},
    {3, 7.5f, "망각의 바깥에서, 누군가 나를 기다리고 있었다.", "나도 기억해. 네가 찾는 아이를."},
    {0, 9.f, "모두가 잊어도, 나는 기억한다.", "이름을 빼앗긴 자들의 이야기가 시작된다."}};
const float openingDuration = 30.f;
} // namespace

void Prototype::LoadPresentation(const SpriteLayout& layout)
{
    const wchar_t* images[] = {L"Assets/Presentation/title_domain.png",
                               L"Assets/Presentation/opening_memory.png",
                               L"Assets/Presentation/opening_absence.png",
                               L"Assets/Presentation/opening_cemetery.png"};
    for (int i = 0; i < 4; ++i)
    {
        if (!presentationSheets[i].Load(images[i], layout))
            std::cerr << "타이틀·오프닝 원화 로드 실패: " << i << std::endl;
    }
}

UIRect Prototype::PresentationBounds() const
{
    const float w = (std::min)(float(width), float(height) * 16.f / 9.f);
    const float h = w * 9.f / 16.f;
    return {(width - w) * .5f, (height - h) * .5f, w, h};
}

UIRect Prototype::OpeningSkipBounds() const
{
    const float w = (std::min)(180.f, (std::max)(1.f, float(width) - 32));
    return {width - w - 16, (std::max)(0.f, float(height) - 46), w, 30};
}

void Prototype::StartOpening()
{
    ClearInput();
    openingPending = false;
    openingPlaying = true;
    openingSkipping = false;
    openingTime = openingExitTime = openingFade = 0;
}

void Prototype::SkipOpening()
{
    if (!openingPlaying || openingSkipping || openingTime < .75f)
        return;
    openingSkipping = true;
    openingExitTime = 0;
    ClearInput();
}

void Prototype::UpdatePresentation(float dt)
{
    const float elapsed = (std::max)(0.f, (std::min)(dt, .1f));
    frontTime += elapsed;
    openingFade = (std::max)(0.f, openingFade - elapsed);
    if (!openingPlaying)
        return;
    openingTime += elapsed;
    if (openingSkipping)
        openingExitTime += elapsed;
    if (openingTime >= openingDuration || (openingSkipping && openingExitTime >= .4f))
    {
        openingPlaying = openingSkipping = false;
        openingExitTime = 0;
        openingFade = titleScreen ? 0 : .8f;
        if (titleScreen)
            frontTime = 0;
        // Keep held key states until key-up so the skip key cannot advance the first dialogue.
        bufferedCommand.action = -1;
        attackHeld = false;
    }
}

void Prototype::DrawPresentationImage(int image, const UIRect& frame, float progress)
{
    const bool warm = image == 1;
    const float drift = std::sin(frontTime * .17f) * .003f;
    presentationSheets[image].DrawCinematic(r,
                                            frame.x,
                                            frame.y,
                                            frame.w,
                                            frame.h,
                                            1.025f + progress * .035f,
                                            drift,
                                            (progress - .5f) * .012f);
    // A few soft layers move independently of the painted scene. No world simulation is needed.
    for (int i = 0; i < 3; ++i)
    {
        const float phase = frontTime * .13f + i * 1.9f;
        r.EnergyParticle({frame.x + frame.w * (.28f + i * .23f + std::sin(phase) * .04f),
                          frame.y + frame.h * (.84f + i * .028f)},
                         frame.w * .22f,
                         frame.h * .035f,
                         .03f * std::sin(phase),
                         warm ? Color(.65f, .41f, .3f, .045f) : Color(.32f, .38f, .65f, .065f),
                         phase,
                         true);
    }
    for (int i = 0; i < 16; ++i)
    {
        const float phase = std::fmod(frontTime * (.018f + i % 3 * .004f) + i * .137f, 1.f);
        const float x = frame.x + frame.w * (.04f + std::fmod(i * .618034f, .92f)) +
                        std::sin(frontTime * .6f + i) * 6;
        const float y = frame.y + frame.h * (1 - phase);
        const float alpha = std::sin(phase * 3.14159265f) * .35f;
        r.EnergyParticle({x, y},
                         2.f + i % 3,
                         1.4f,
                         i * .7f,
                         warm ? Color(1.f, .63f, .28f, alpha) : Color(.63f, .57f, 1.f, alpha),
                         phase);
    }
    if (!presentationSheets[image].Ready())
    {
        r.Rect(frame.x, frame.y, frame.w, frame.h, Color(.025f, .018f, .055f));
        r.Text(frame.x + 24,
               frame.y + 34,
               "원화 파일을 불러오지 못했습니다.",
               Color(.85f, .72f, .82f));
    }
    // Letterbox/pillarbox bars also clip the soft foreground particles at the image edges.
    r.Rect(0, 0, float(width), frame.y, Color(0, 0, 0));
    r.Rect(0,
           frame.y + frame.h,
           float(width),
           (std::max)(0.f, height - frame.y - frame.h),
           Color(0, 0, 0));
    r.Rect(0, frame.y, frame.x, frame.h, Color(0, 0, 0));
    r.Rect(frame.x + frame.w,
           frame.y,
           (std::max)(0.f, width - frame.x - frame.w),
           frame.h,
           Color(0, 0, 0));
}

void Prototype::DrawPresentationBackground()
{
    const auto frame = PresentationBounds();
    DrawPresentationImage(0, frame, (.5f + .5f * std::sin(frontTime * .09f)));
    for (int i = 0; i < 12; ++i)
    {
        const float alpha = .48f * (1.f - i / 12.f);
        r.Rect(frame.x + frame.w * .045f * i,
               frame.y,
               frame.w * .046f,
               frame.h,
               Color(.012f, .008f, .03f, alpha));
    }
}

void Prototype::DrawOpening()
{
    const auto frame = PresentationBounds();
    int index = 0;
    float local = openingTime;
    while (index < 3 && local >= openingShots[index].duration)
    {
        local -= openingShots[index].duration;
        ++index;
    }
    const auto& shot = openingShots[index];
    const float progress = (std::min)(1.f, local / shot.duration);
    DrawPresentationImage(shot.image, frame, progress);
    const float visibility =
        (std::max)(0.f, (std::min)(1.f, (std::min)(local / .6f, (shot.duration - local) / .6f)));
    const float midpoint = shot.duration * .5f;
    const bool second = local >= midpoint;
    const float captionTime = second ? local - midpoint : local;
    const float captionRemaining = second ? shot.duration - local : midpoint - local;
    const float captionAlpha = (std::max)(
        0.f, (std::min)(visibility, (std::min)(captionTime / .5f, captionRemaining / .4f)));
    const std::string caption = second ? shot.secondCaption : shot.caption;
    float units = 0;
    for (size_t i = 0; i < caption.size();)
    {
        const unsigned char c = static_cast<unsigned char>(caption[i]);
        const size_t length = c < 128 ? 1 : ((c & 0xe0) == 0xc0 ? 2 : ((c & 0xf0) == 0xe0 ? 3 : 4));
        units += c < 128 ? .55f : 1.f;
        i += length;
    }
    const int size = (std::max)(12, (std::min)(23, int(frame.w / 55.f)));
    const float x = (std::max)(frame.x + 16, frame.x + (frame.w - units * size) * .5f);
    const float baseline = frame.y + frame.h * .905f;
    r.Rect(frame.x, frame.y + frame.h * .845f, frame.w, frame.h * .155f, Color(0, 0, 0, .35f));
    r.Text(x + 1, baseline + 1, caption, Color(0, 0, 0, captionAlpha), false, size);
    r.Text(x, baseline, caption, Color(.94f, .92f, .98f, captionAlpha), false, size);
    if (index == 3 && local > 2)
    {
        const float alpha = (std::min)(1.f, (local - 2) / 1.2f);
        const float left = frame.x + frame.w * .075f;
        r.Text(left,
               frame.y + frame.h * .4f,
               "주흔",
               Color(.96f, .93f, 1.f, alpha),
               true,
               (std::max)(36, (std::min)(140, int(frame.h * .17f))),
               true);
        r.Text(left,
               frame.y + frame.h * .48f,
               "잊힌 이름",
               Color(.8f, .72f, .96f, alpha),
               false,
               (std::max)(14, int(frame.h * .04f)),
               true);
    }
    r.Rect(0, 0, float(width), float(height), Color(0, 0, 0, 1 - visibility));
    if (openingSkipping)
        r.Rect(0,
               0,
               float(width),
               float(height),
               Color(0, 0, 0, (std::min)(1.f, openingExitTime / .4f)));
    const auto skip = OpeningSkipBounds();
    r.Rect(skip.x, skip.y, skip.w, skip.h, Color(.02f, .015f, .035f, .72f));
    r.Text(skip.x + 10, skip.y + 21, "건너뛰기 · ESC / Enter", Color(.8f, .77f, .88f));
}

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
    openingPending = true;
    openingPlaying = openingSkipping = false;
    openingTime = openingExitTime = openingFade = 0;
    storyTrialTime = 0;
    world = World();
    levelOne = LevelOne();
    ResetControls();
    population.people.clear();
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
    movementVelocity = {0, 0};
    movementWasControlling = false;
    protagonistStopTime = .08f;
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
    const bool newJourney = slotMode == SlotMode::NewGame || openingPending;
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
    if (newJourney && !exitRequested)
        StartOpening();
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
    if (slotMode == SlotMode::Closed && key == 'o')
    {
        StartOpening();
        return;
    }
    if (key == 27)
    {
        if (overwriteConfirm)
            overwriteConfirm = false;
        else
        {
            slotMode = SlotMode::Closed;
            exitAfterSave = false;
            openingPending = false;
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
    if (titleScreen && slotMode == SlotMode::Closed)
    {
        const auto frame = PresentationBounds();
        return {frame.x + frame.w * .065f,
                frame.y + frame.h * (.58f + index * .088f),
                frame.w * .32f,
                frame.h * .072f};
    }
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
    if (!serviceNpc.empty())
    {
        for (int i = 0; i < 6; ++i)
        {
            if (ServiceButton(i).Contains(x, y))
            {
                ServiceAction(i);
                break;
            }
        }
        return true;
    }
    if ((!chapter.choicePending && !levelOne.Dead()) || width < 480 || height < 360)
        return false;
    const bool choice = chapter.choicePending;
    const float w = (std::min)(640.f, float(width) - 40), h = 190.f;
    const float left = (width - w) / 2, top = (height - h) / 2;
    for (int i = 0; i < 2; ++i)
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
    if (titleScreen)
        DrawPresentationBackground();
    if (width < 480 || height < 360)
    {
        r.Text(16, 32, "창을 넓혀 주세요.", Color(.9f, .88f, .8f));
        return;
    }
    if (titleScreen && slotMode == SlotMode::Closed)
    {
        const auto frame = PresentationBounds();
        const float x = frame.x + frame.w * .07f;
        const int titleSize = (std::max)(40, (std::min)(160, int(frame.h * .18f)));
        const int subtitleSize = (std::max)(13, (std::min)(30, int(frame.h * .043f)));
        const float reveal = (std::min)(1.f, frontTime / 1.2f);
        r.Text(x + 2,
               frame.y + frame.h * .365f + 2,
               "주흔",
               Color(.35f, .16f, .7f, reveal * .6f),
               true,
               titleSize,
               true);
        r.Text(x,
               frame.y + frame.h * .365f,
               "주흔",
               Color(.97f, .95f, 1.f, reveal),
               true,
               titleSize,
               true);
        r.Text(x,
               frame.y + frame.h * .445f,
               "잊힌 이름",
               Color(.82f, .76f, .94f, reveal),
               false,
               subtitleSize,
               true);
        r.Line({x, frame.y + frame.h * .48f},
               {x + frame.w * .21f, frame.y + frame.h * .48f},
               1,
               Color(.57f, .35f, .84f, reveal * .6f));
        const char* labels[] = {"새로운 이야기", "이어하기", "종료"};
        const int menuSize = (std::max)(14, (std::min)(25, int(frame.h * .035f)));
        for (int i = 0; i < 3; ++i)
        {
            const auto button = FrontButton(i);
            const bool active = frontSelection == i;
            const float baseline = button.y + button.h * .68f;
            if (active)
            {
                r.EnergyParticle({button.x + button.w * .42f, button.y + button.h * .94f},
                                 button.w * .5f,
                                 4,
                                 0,
                                 Color(.55f, .23f, 1.f, .28f),
                                 frontTime * .2f);
                r.Line({button.x, button.y + button.h * .94f},
                       {button.x + button.w * .85f, button.y + button.h * .94f},
                       1,
                       Color(.65f, .42f, 1.f, .75f));
            }
            r.Text(button.x + 14,
                   baseline,
                   labels[i],
                   active ? Color(.98f, .93f, 1.f, reveal) : Color(.66f, .66f, .75f, reveal),
                   active,
                   menuSize);
        }
        r.Text(x,
               frame.y + frame.h * .94f,
               "클릭 · 방향키 / Enter · 1~3",
               Color(.58f, .6f, .7f, reveal));
        r.Text((std::max)(x, frame.x + frame.w - 180),
               frame.y + frame.h * .94f,
               "오프닝 보기 · O",
               Color(.73f, .68f, .86f, reveal));
        if (!presentationSheets[0].Ready())
            r.Text(x,
                   frame.y + frame.h * .52f,
                   "타이틀 원화를 불러오지 못했습니다.",
                   Color(1.f, .65f, .6f));
        return;
    }
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
    const std::string heading = !choosing                       ? "주흔 — 잊힌 이름"
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
