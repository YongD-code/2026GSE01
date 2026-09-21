#pragma once
#include "Renderer.h"
#include "World.h"
#include "SpriteSheet.h"
#include "LevelOne.h"
#include "SaveGame.h"
#include "SceneGraph.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include <sstream>

// Authored starting village surrounded by deterministic streamed chunks.
class Prototype
{
    using Object = WorldObject;
    World world;
    LevelOne levelOne;
    Chapter chapter;
    int menuTab = 0, collectionPage = 0, selectedCompanion = 0;
    bool hudVisible = true;
    bool dodgeHeld = false, screenShake = true;
    bool dialoguePending = false;
    bool pauseMenu = false, exitConfirm = false, exitRequested = false, skipExitSave = false;
    enum class SlotMode
    {
        Closed,
        NewGame,
        Save,
        Load
    };
    bool titleScreen = true, overwriteConfirm = false, exitAfterSave = false;
    SlotMode slotMode = SlotMode::Closed;
    int activeSlot = 0, frontSelection = 0;
    std::string slotLabels[3];
    int pauseSelection = 0, eventPage = 0;
    bool automaticSaveAllowed = true;
    float autoSaveTime = 0, saveStatusTime = 0;
    std::string saveStatus = "자동 저장 대기";
    std::vector<Object> villageObjects;
    Renderer& r;
    SpriteSheet protagonistSheet, childSheet, keeperSheet;
    bool blenderChild = false, blenderKeeper = false;
    SpriteAnimation protagonistAnimation;
    SpriteSheet attackSheet, protagonistRunSheet;
    bool blenderProtagonist = false, protagonistSprinting = false;
    SpriteSheet spiritSheets[3];
    SpriteAnimation companionAnimation;
    const WorldPoint wildSpirits[3] = {{300, 160}, {-220, 250}, {220, 360}};
    int capturedSpirit = 0;

    const char* SpiritName(int type) const
    {
        static const char* names[] = {
            "남겨진 자", "목 없는 순례자", "기도하는 손", "기도하지 않는 성자"};
        return names[(std::max)(0, (std::min)(type, 3))];
    }

    const char* SpiritStory(int type) const
    {
        static const char* stories[] = {"눈 오는 공동묘지에서 돌아오지 않는 부모를 기다리던 아이.",
                                        "목적지를 잃은 채 수도원으로 향하는 순례자의 기억.",
                                        "응답 없는 기도가 모여 누군가를 지키려는 손이 되었다.",
                                        "기도를 멈춘 성자. 검게 변한 성유물의 문장을 기억한다."};
        return stories[(std::max)(0, (std::min)(type, 3))];
    }

    int width = 1280, height = 800;
    bool keys[256] = {};
    bool arrows[4] = {};
    SceneGraph scene;
    bool sceneReady = false, requestedSprint = false;
    unsigned long long nextSceneId = 1;
    WorldPoint player = {70, 160}, spirit = {300, 160}, camera = {70, 160};
    const WorldPoint child = {-100, 90}, fire = {0, 0};
    WorldPoint keeper = {150, -100};
    float time = 0, walk = 0, messageTime = 0;
    bool moving = false, captured = false, controlling = false, journal = false, toySword = false;
    int childTalk = 0;
    std::string speaker, message;

    static double Distance(WorldPoint a, WorldPoint b)
    {
        double x = a.x - b.x, y = a.y - b.y;
        return std::sqrt(x * x + y * y);
    }

    Point Screen(double x, double y, double z = 0) const
    {
        x -= camera.x;
        if (screenShake && levelOne.hurtTime > 0)
            x += std::sin(time * 110) * levelOne.hurtTime * 18;
        y -= camera.y;
        return {static_cast<float>(width * .5 + (x - y) * .85),
                static_cast<float>(height * .51 + (x + y) * .43 - z)};
    }

    void Ground(double x, double y, double w, double d, Color c)
    {
        r.Quad(Screen(x, y), Screen(x + w, y), Screen(x + w, y + d), Screen(x, y + d), c);
    }

    void Box(double x, double y, double w, double d, double h, Color top, Color left, Color right)
    {
        Point a = Screen(x, y, h), b = Screen(x + w, y, h), c = Screen(x + w, y + d, h),
              e = Screen(x, y + d, h);
        r.Quad(e, c, Screen(x + w, y + d), Screen(x, y + d), left);
        r.Quad(b, Screen(x + w, y), Screen(x + w, y + d), c, right);
        r.Quad(a, b, c, e, top);
    }

    void Say(const std::string& name, const std::string& text)
    {
        speaker = name;
        message = text;
        messageTime = 8;
    }

    bool Blocked(WorldPoint p)
    {
        if (world.Blocked(p))
            return true;
        for (const Object& o : villageObjects)
        {
            if (p.x > o.x - 12 && p.x < o.x + o.w + 12 && p.y > o.y - 12 && p.y < o.y + o.d + 12)
                return true;
        }
        return false;
    }

    int Nearby() const
    {
        if (controlling)
            return -1;
        if (Distance(player, child) < 85)
            return 0;
        if (Distance(player, keeper) < 85)
            return 1;
        if (Distance(player, fire) < 85)
            return 3;
        return -1;
    }

    void Interact()
    {
        if (levelOne.Capturable(player) >= 0)
        {
            levelOne.Capture(player, false);
            return;
        }
        const auto* quest = chapter.Current();
        const int target = Nearby();
        if (target == 0)
        {
            if (quest && quest->trigger == "villager")
            {
                if (chapter.step == 0)
                {
                    chapter.choicePending = true;
                    Say("벨른 마을의 아이",
                        "공동묘지 근처에서 봤다는 사람이 있어. 요즘 밤에는 거기로 가지 마….");
                }
                else
                {
                    Say("벨른 마을의 아이", "동생? 너한테 동생이 있었다고? 라엔에게 물어봐.");
                    chapter.Advance(levelOne.kills, static_cast<int>(levelOne.collection.size()));
                }
            }
            else
                Say("벨른 마을의 아이", "죽은 사람이 돌아왔다는 소문이 있어. 조심해.");
        }
        else if (target == 1)
        {
            if (quest && quest->trigger == "guide")
            {
                Say("라엔", quest->dialogue);
                chapter.Advance(levelOne.kills, static_cast<int>(levelOne.collection.size()));
            }
            else
                Say("라엔", "주령은 장소에 남은 감정이야. 힘만으로는 그 사연을 알 수 없어.");
        }
        else if (target == 3)
        {
            levelOne.health = levelOne.MaximumHealth();
            Say("벨른의 불씨", "잠시 쉬어 체력을 회복했습니다.");
        }
    }

    void UpdateChapter()
    {
        if (levelOne.Dead())
            return;
        const auto* quest = chapter.Current();
        if (!quest)
        {
            if (chapter.rewardPending && !chapter.quests.empty())
            {
                chapter.rewardPending = false;
                levelOne.complete = true;
                levelOne.tickets[1] += 3;
                ++levelOne.tickets[2];
                ++levelOne.tickets[3];
                ++levelOne.stones;
                Say("1장 완료 · 저주를 보는 자",
                    "검게 변한 성유물을 얻었다. 일곱 번째 종이 울리는 날, 문이 열린다. 다음 장의 "
                    "이야기는 아직 준비 중입니다.");
            }
            return;
        }
        if (chapter.rewardPending)
        {
            chapter.rewardPending = false;
            Say(quest->title, quest->dialogue);
            dialoguePending = true;
            if (quest->trigger == "guide")
                keeper = quest->destination;
            if (quest->id == "P005")
            {
                levelOne.captureUnlocked = true;
                ++levelOne.tickets[0];
            }
            if (quest->id == "C102")
                levelOne.tickets[1] += 3;
        }
        int boss =
            quest->trigger == "boss"
                ? 1
                : (quest->trigger == "elite"
                       ? 2
                       : (quest->trigger == "monk" ? 3 : (quest->trigger == "saint" ? 4 : 0)));
        if (boss && Distance(player, quest->destination) < 700)
            levelOne.EnsureBoss(boss,
                                quest->destination,
                                [this](WorldPoint p)
                                {
                                    return Blocked(p);
                                });
        bool done = quest->trigger == "reach" && Distance(player, quest->destination) < 80;
        done = done || (quest->trigger == "kills" &&
                        levelOne.kills - chapter.baselineKills >= quest->amount);
        done = done || (quest->trigger == "capture" &&
                        static_cast<int>(levelOne.collection.size()) - chapter.baselineCaptures >=
                            quest->amount);
        done = done || (quest->trigger == "skill" && levelOne.skillsUsed > 0);
        done = done || (boss && levelOne.resolvedBosses.count(boss));
        if (done)
            chapter.Advance(levelOne.kills, static_cast<int>(levelOne.collection.size()));
        if (!levelOne.collection.empty() && !captured)
        {
            captured = true;
            capturedSpirit = levelOne.collection.front().species;
            spirit = player;
        }
    }

    void House(const Object& o);
    void Tree(const Object& o);
    void Person(WorldPoint pos, Color coat, bool isPlayer, bool childSize = false);
    void Wisp(int type, WorldPoint position);
    void DrawEnemy(const LevelOne::Enemy& enemy);
    void DrawCombatWarnings();
    void Hearth(WorldPoint position);

    void Panel(float x, float y, float w, float h)
    {
        r.Rect(x, y, w, h, Color(.035f, .052f, .068f, .94f));
        r.Rect(x, y, w, 1, Color(.48f, .43f, .30f, .65f));
        r.Rect(x, y + h - 1, w, 1, Color(.28f, .30f, .29f, .6f));
    }

#include "PrototypeSave.inl"
#include "PrototypeMenu.inl"
#include "PrototypeUI.inl"

  public:
    explicit Prototype(Renderer& renderer) : r(renderer)
    {
        villageObjects = {{0, -300, -210, 140, 110, 92},
                          {0, -70, -360, 155, 115, 115},
                          {0, -440, 90, 130, 100, 85},
                          {0, 270, -250, 145, 110, 98},
                          {2, 330, 240, 65, 35, 30},
                          {2, 390, 270, 30, 35, 52},
                          {2, 280, 280, 25, 25, 24}};
        // Temporary authored landmarks; keep the y=0 quest route clear.
        for (int i = 0; i < 6; ++i)
            villageObjects.push_back({2, 1080. + i * 45, -90, 18, 16, 38});
        villageObjects.push_back({0, 2930, -250, 210, 110, 140});
        villageObjects.push_back({2, 3180, -100, 25, 25, 90});
        villageObjects.push_back({2, 3260, -100, 25, 25, 90});
        SpriteLayout eightDirections;
        eightDirections.rows = 8;
        eightDirections.directionRows = {{0, 1, 2, 3, 4, 5, 6, 7}};
        SpriteLayout protagonistLayout = eightDirections;
        protagonistLayout.anchorAtTorso = true;
        protagonistLayout.pingPongWalk = true;
        SpriteLayout blenderLayout = eightDirections;
        blenderLayout.columns = 9;
        blenderLayout.firstWalkFrame = 1;
        blenderLayout.walkFrames = 8;
        blenderLayout.fixedCanvas = true;
        blenderProtagonist =
            protagonistSheet.Load(L"Assets/Characters/Blender/walk.png", blenderLayout);
        if (blenderProtagonist)
        {
            protagonistRunSheet.Load(L"Assets/Characters/Blender/run.png", blenderLayout);
        }
        else if (!protagonistSheet.Load(L"Assets/Characters/protagonist.png", protagonistLayout))
        {
            std::cerr << "주인공 스프라이트를 불러오지 못해 임시 도형을 사용합니다." << std::endl;
        }
        SpriteLayout npcIdleLayout = blenderLayout;
        npcIdleLayout.columns = 8;
        npcIdleLayout.firstWalkFrame = 0;
        npcIdleLayout.walkFrames = 8;
        blenderChild = childSheet.Load(L"Assets/Characters/Blender/npc1/idle.png", npcIdleLayout);
        blenderKeeper = keeperSheet.Load(L"Assets/Characters/Blender/npc2/idle.png", npcIdleLayout);
        if (!blenderChild && !childSheet.Load(L"Assets/Characters/mira.png", eightDirections))
            std::cerr << "NPC1 스프라이트 로드 실패: 임시 외형을 사용합니다." << std::endl;
        if (!blenderKeeper && !keeperSheet.Load(L"Assets/Characters/keeper.png", eightDirections))
            std::cerr << "NPC2 스프라이트 로드 실패: 임시 외형을 사용합니다." << std::endl;
        SpriteLayout attackLayout;
        attackLayout.firstWalkFrame = 0;
        attackLayout.walkFrames = 4;
        attackLayout.anchorAtFeet = true;
        if (blenderProtagonist)
        {
            attackLayout = eightDirections;
            attackLayout.firstWalkFrame = 0;
            attackLayout.walkFrames = 4;
            attackLayout.fixedCanvas = true;
        }
        if (!attackSheet.Load(blenderProtagonist ? L"Assets/Characters/Blender/cast.png"
                                                 : L"Assets/Characters/protagonist_attack.png",
                              attackLayout))
            std::cerr << "공격 스프라이트 로드 실패: 기본 외형으로 시전합니다." << std::endl;
        SpriteLayout spiritLayout;
        spiritLayout.rows = 4;
        spiritLayout.directionRows = {{0, 1, 1, 1, 3, 2, 2, 2}};
        spiritLayout.firstWalkFrame = 1;
        spiritLayout.walkFrames = 3;
        const wchar_t* spiritFiles[] = {L"Assets/Spirits/ash_lantern_4dir.png",
                                        L"Assets/Spirits/mourning_bell_4dir.png",
                                        L"Assets/Spirits/briar_fox_4dir.png"};
        for (int i = 0; i < 3; ++i)
            if (!spiritSheets[i].Load(spiritFiles[i], spiritLayout))
                std::cerr << "주령 이미지 로드 실패: " << SpiritName(i) << std::endl;
        chapter.Load();
        levelOne.captureRules.Load();
        levelOne.LoadCombat();
        world.Stream(camera, player, width, height);
    }

    void ProgressKey(bool load)
    {
        if (slotMode != SlotMode::Closed || exitConfirm)
            return;
        if (!titleScreen || load)
            OpenSlots(load ? SlotMode::Load : SlotMode::Save);
    }

    void SaveBeforeClose()
    {
        if (!titleScreen && !skipExitSave && automaticSaveAllowed && CanSaveProgress() &&
            !SaveProgress(false))
        {
            std::cerr << "종료 저장 실패: 마지막 저장 기록을 유지합니다." << std::endl;
        }
    }

    bool WantsExit() const
    {
        return exitRequested;
    }

    void PointerDown(int x, int y)
    {
        scene.PointerDown(x, y);
    }

    void MenuClick(int x, int y)
    {
        if (!pauseMenu || width < 480 || height < 360)
        {
            return;
        }
        if (journal)
        {
            const float w = (std::min)(850.f, float(width) - 36);
            const float h = (std::min)(590.f, float(height) - 36);
            const float left = (width - w) * .5f;
            const float top = (height - h) * .5f;
            if (y >= top && y < top + 44 && x >= left + 16 && x < left + w - 16)
            {
                menuTab = (std::min)(4, int((x - left - 16) / ((w - 32) / 5)));
            }
            return;
        }
        const int action = MenuActionAt(x, y);
        if (action >= 0)
            ActivateMenu(action);
    }

    void Resize(int w, int h)
    {
        width = w;
        height = h;
        r.Resize(w, h);
    }

    void Key(unsigned char key, bool down)
    {
        if (key >= 'A' && key <= 'Z')
            key = static_cast<unsigned char>(key - 'A' + 'a');
        bool fresh = down && !keys[key];
        keys[key] = down;
        if (!fresh)
            return;
        if (titleScreen || slotMode != SlotMode::Closed)
        {
            FrontKey(key);
            return;
        }
        if (key == 27)
        {
            ClearInput();
            if (exitConfirm)
            {
                exitConfirm = false;
            }
            else if (journal)
            {
                journal = false;
                pauseMenu = true;
            }
            else
            {
                pauseMenu = !pauseMenu;
            }
            return;
        }
        if (pauseMenu && !journal)
        {
            const int count = exitConfirm ? 2 : 9;
            if (key >= '1' && key < '1' + count)
            {
                ActivateMenu(key - '1');
            }
            else if (key == 13)
            {
                ActivateMenu(pauseSelection);
            }
            return;
        }
        if (chapter.choicePending && !journal)
        {
            if (key >= '1' && key <= '3')
            {
                if (key == '1')
                    ++chapter.empathy;
                if (key == '2')
                    ++chapter.indifference;
                if (key == '3')
                    ++chapter.greed;
                chapter.choicePending = false;
                chapter.Advance(levelOne.kills, static_cast<int>(levelOne.collection.size()));
            }
            return;
        }
        if (!journal && dialoguePending && (key == 'e' || key == 13))
        {
            dialoguePending = false;
            messageTime = 0;
            return;
        }
        if (key == '\t')
        {
            journal = !journal;
            return;
        }
        if (key == 'j')
        {
            if (journal && menuTab == 2)
                journal = false;
            else
            {
                journal = true;
                menuTab = 2;
            }
            return;
        }
        if (key == 'h')
        {
            hudVisible = !hudVisible;
            return;
        }
        if (dialoguePending && !journal)
            return;
        if (journal)
        {
            if (key >= '1' && key <= '5')
                menuTab = key - '1';
            const int perPage =
                (std::max)(1, static_cast<int>(((std::min)(590, height - 36) - 180) / 29));
            const int lastPage = levelOne.collection.empty()
                                     ? 0
                                     : (static_cast<int>(levelOne.collection.size()) - 1) / perPage;
            if (menuTab == 3 && (key == '[' || key == ']'))
            {
                const int last = (std::max)(0, (chapter.step - 1) / 3);
                eventPage = (std::max)(0, (std::min)(last, eventPage + (key == ']' ? 1 : -1)));
            }
            if (menuTab == 2 && key == '[')
                collectionPage = (std::max)(0, collectionPage - 1);
            if (menuTab == 2 && key == ']')
                collectionPage = (std::min)(lastPage, collectionPage + 1);
            if (menuTab == 2 && key == 'n' && !levelOne.collection.empty())
            {
                selectedCompanion =
                    (selectedCompanion + 1) % static_cast<int>(levelOne.collection.size());
                capturedSpirit = levelOne.collection[selectedCompanion].species;
            }
            return;
        }
        if (key == 'p')
        {
            r.postProcess.enabled = !r.postProcess.enabled;
            Say("화면 효과", r.postProcess.enabled ? "후처리를 켰습니다." : "후처리를 껐습니다.");
        }
        if (key == 'b')
        {
            r.postProcess.bloomEnabled = !r.postProcess.bloomEnabled;
            Say("빛 번짐",
                r.postProcess.bloomEnabled ? "블룸을 켰습니다. 후처리도 켜져 있어야 적용됩니다."
                                           : "블룸을 껐습니다.");
        }

        if (key == 'r' && levelOne.Dead())
        {
            levelOne = LevelOne();
            levelOne.captureRules.Load();
            levelOne.LoadCombat();
            chapter = Chapter();
            chapter.Load();
            captured = false;
            selectedCompanion = 0;
            collectionPage = 0;
            eventPage = 0;
            player = {70, 160};
            spirit = player;
            camera = player;
            controlling = false;
            keeper = {150, -100};
            ClearInput();
            Say("레벨 1 · 다시 피는 불씨",
                "성장을 초기화했습니다. 마을 밖에서 파밍을 다시 시작하세요.");
            return;
        }
        if (levelOne.Dead())
            return;
        if (key == 'f')
            levelOne.automatic = !levelOne.automatic;
        if (key == 't')
            levelOne.selectedTicket = (levelOne.selectedTicket + 1) % 3;
        if (key == 'x')
            levelOne.Capture(player, true);
        if (key == 'v')
            levelOne.UseSkill(player);
        if (key == 'g')
            levelOne.Upgrade(captured);
        if (key == 'e')
            Interact();
        if (key == 'k')
            screenShake = !screenShake;
        if (key == 'q')
        {
            if (captured)
            {
                controlling = !controlling;
                Say("조종 전환",
                    controlling ? "주령을 조종합니다. WASD로 이동하고 Q로 본체에 돌아갑니다."
                                : "방랑자의 몸으로 돌아왔습니다.");
            }
            else
                Say("조종 전환", "라엔에게 봉인을 배운 뒤 약화된 주령에게 봉인권을 사용하세요.");
        }
    }

    void Arrow(int key, bool down)
    {
        if (titleScreen || slotMode != SlotMode::Closed)
        {
            if (down && !overwriteConfirm && (key == 1 || key == 3))
                frontSelection = (frontSelection + (key == 1 ? 2 : 1)) % 3;
            return;
        }
        if (pauseMenu && !journal)
        {
            if (down && (key == 1 || key == 3))
            {
                const int count = exitConfirm ? 2 : 9;
                pauseSelection = (pauseSelection + (key == 1 ? count - 1 : 1)) % count;
            }
            return;
        }
        if (key >= 0 && key < 4)
            arrows[key] = down;
    }

    void ClearInput()
    {
        for (bool& k : keys)
            k = false;
        for (bool& k : arrows)
            k = false;
    }

    void Update(float dt, bool sprint = false);
    void Draw();

  private:
    void OpenSlots(SlotMode mode);
    void FrontKey(unsigned char key);
    bool FrontClick(int x, int y);
    void SelectFront(int item);
    void CompleteSlot();
    void DrawFront();
    void NewGame();
    void UpdateSimulation(float dt, bool sprint);
    void SynchronizeScene();
    void DrawTerrain(const World::Chunk& chunk, ActorPosition position);
    void DrawObject(const Object& object);
};
