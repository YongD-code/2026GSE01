#pragma once
#include "Renderer.h"
#include "World.h"
#include "SpriteSheet.h"
#include "CharacterModel.h"
#include "SpiritAppearance.h"
#include "LevelOne.h"
#include "SaveGame.h"
#include "SceneGraph.h"
#include "UILayout.h"
#include "StoryLayout.h"
#include "ClickNavigation.h"
#include "Monitoring.h"
#include "Population.h"
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
    int dialoguePage = 0;
    bool pauseMenu = false, exitConfirm = false, exitRequested = false, skipExitSave = false;
    enum class SlotMode
    {
        Closed,
        NewGame,
        Save,
        Load
    };
    bool titleScreen = true, overwriteConfirm = false, exitAfterSave = false;
    bool openingPlaying = false, openingSkipping = false, openingPending = false;
    float frontTime = 0, openingTime = 0, openingExitTime = 0, openingFade = 0;
    SpriteSheet presentationSheets[4];
    SlotMode slotMode = SlotMode::Closed;
    int activeSlot = 0, frontSelection = 0;
    std::string slotLabels[3];
    int pauseSelection = 0, eventPage = 0;
    int menuPage = 0, journalPage = 0;
    bool automaticSaveAllowed = true;
    float autoSaveTime = 0, saveStatusTime = 0;
    std::string saveStatus = "자동 저장 대기";
    std::vector<Object> villageObjects;
    std::map<World::Key, std::vector<size_t>> villageCollision;
    Renderer& r;
    SpriteSheet protagonistSheet, domainPortrait, spiritPortraits[4], ghostFace, ghostScreamPoses,
        ghostVariants[3], spiritSpellArt[3], spiritRealms[4];
    SpriteSheet storyNpcSheets[StoryLayout::NpcCount];
    SpriteAnimation protagonistAnimation;
    CharacterModel protagonistModel;
    bool useProtagonistModel = false;
    float protagonistYaw = 2.35619449f, protagonistGait = 0;
    float protagonistModelTime = 0, protagonistCastAge = 1;
    WorldPoint movementVelocity = {0, 0};
    bool movementWasControlling = false;
    float protagonistStopTime = .08f;
    SpriteSheet attackSheet, protagonistRunSheet;
    bool blenderProtagonist = false, protagonistSprinting = false;
    SpriteSheet spiritSheets[SpiritCatalog::Count];
    SpriteSheet spiritModelSheets[SpiritCatalog::Count];
    bool useSpiritModels = true;
    SpriteSheet storyEnemySheets[StoryEncounter::SpriteCount];
    SpriteSheet lootSheets[5];
    SpriteAnimation companionAnimation;
    std::string serviceNpc;
    std::vector<UIRect> occupiedLabels;
    int capturedSpirit = 0;

    const char* SpiritName(int type) const
    {
        return SpiritCatalog::Get(type).name;
    }

    const char* SpiritStory(int type) const
    {
        return SpiritCatalog::Get(type).story;
    }

    bool UsingSpiritModel(int species) const
    {
        return useSpiritModels && species >= 0 && species < SpiritCatalog::Count &&
               spiritModelSheets[species].Ready();
    }

    bool UsingSpiritModels() const
    {
        for (int species = 0; species < SpiritCatalog::Count; ++species)
        {
            if (UsingSpiritModel(species))
            {
                return true;
            }
        }
        return false;
    }

    int width = 1280, height = 800;
    bool keys[256] = {};
    SceneGraph scene;
    bool sceneReady = false, requestedSprint = false;
    unsigned long long nextSceneId = 1;
    WorldPoint player = {70, 160}, spirit = {300, 160}, camera = {70, 160};
    const WorldPoint child = {-100, 90}, fire = {0, 0};
    WorldPoint keeper = {150, -100};
    float time = 0, messageTime = 0;
    bool moving = false, captured = false, controlling = false, journal = false;
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
        if (screenShake && domainShake > 0)
        {
            x += std::sin(time * 63) * domainShake * 13;
            y += std::cos(time * 79) * domainShake * 8;
        }
        if (screenShake && purpleShake > 0)
        {
            x += std::sin(time * 91) * purpleShake * 20;
            y += std::cos(time * 77) * purpleShake * 10;
        }
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
        talkingResident = -1;
        speaker = name;
        message = text;
        dialoguePage = 0;
        messageTime = 8;
    }

    double SimulationRadius() const
    {
        // Include the projected viewport, sprite height and camera-follow margin on wide displays.
        return (std::max)(1400., std::hypot(width / 1.7, height / .86) * .70710678 + 350);
    }

    bool Blocked(WorldPoint p)
    {
        if (world.Blocked(p))
            return true;
        const auto nearby = villageCollision.find({World::Index(p.x), World::Index(p.y)});
        if (nearby == villageCollision.end())
        {
            return false;
        }
        for (const auto index : nearby->second)
        {
            const auto& o = villageObjects[index];
            if (p.x > o.x - 12 && p.x < o.x + o.w + 12 && p.y > o.y - 12 && p.y < o.y + o.d + 12)
                return true;
        }
        return false;
    }

    float storyTrialTime = 0;

    const StoryLayout::Site* NearbyStory() const
    {
        if (controlling)
        {
            return nullptr;
        }
        const auto* quest = chapter.Current();
        const StoryLayout::Site* closest = nullptr;
        double nearest = 62;
        for (const auto& site : StoryLayout::Sites())
        {
            if (!StoryLayout::Visible(site, chapter))
            {
                continue;
            }
            const double distance = Distance(player, site.position);
            if (distance < 62 && quest &&
                quest->trigger.substr(quest->trigger.find(':') + 1) == site.id)
            {
                return &site;
            }
            if (distance < nearest)
            {
                nearest = distance;
                closest = &site;
            }
        }
        return closest;
    }

    bool IsStoryObjective(const StoryLayout::Site* site) const
    {
        const auto* quest = chapter.Current();
        return site && quest &&
               (quest->trigger == "talk:" + site->id || quest->trigger == "inspect:" + site->id ||
                quest->trigger == "choice:" + site->id);
    }

    void AdvanceChapter()
    {
        chapter.Advance(levelOne.kills, static_cast<int>(levelOne.collection.size()));
        storyTrialTime = 0;
        const auto* quest = chapter.Current();
        if (quest && quest->trigger == "skill")
            levelOne.skillsUsed = 0;
    }

    int Nearby() const
    {
        return !controlling && Distance(player, fire) < 65 ? 3 : -1;
    }

    void Interact()
    {
        if (controlling)
        {
            return;
        }
        const auto* quest = chapter.Current();
        const auto* site = NearbyStory();
        const bool storyObjective = IsStoryObjective(site);
        if (!storyObjective && levelOne.Capturable(player) >= 0)
        {
            levelOne.Capture(player, false);
            return;
        }
        if (site)
        {
            if (storyObjective)
            {
                ClearInput();
                if (quest->trigger == "choice:child")
                {
                    Say(site->name, "아이에게 동생의 행방을 물어볼까요?");
                    chapter.choicePending = true;
                    return;
                }
                Say(site->name, quest->dialogue);
                if (quest->id == "C100")
                {
                    levelOne.tickets[1] += 3;
                }
                if (quest->id == "C100_SMITH")
                {
                    levelOne.weapon = (std::min)(20, levelOne.weapon + 1);
                    levelOne.health = levelOne.MaximumHealth();
                }
                if (quest->id == "P004")
                {
                    levelOne.health = levelOne.MaximumHealth();
                }
                dialoguePending = true;
                AdvanceChapter();
                autoSaveTime = 30;
            }
            else
            {
                if (site->id == "smith" || site->id == "merchant" || site->id == "sealer")
                {
                    serviceNpc = site->id;
                    ClearInput();
                    return;
                }
                Say(site->name, StoryDialogue(*site));
            }
            return;
        }
        if (const auto* resident = NearbyResident())
        {
            TalkResident(resident->id);
            return;
        }
        if (Nearby() == 3)
        {
            levelOne.health = levelOne.MaximumHealth();
            Say("벨른의 불씨", "잠시 쉬어 체력을 회복했습니다.");
        }
    }

    std::string StoryDialogue(const StoryLayout::Site& site) const
    {
        const bool forgotten = chapter.step >= chapter.Index("P008_VILLAGER");
        if (site.id == "sibling")
            return "길이 엇갈리면 돌담의 표시를 따라와. 우리 같이 집으로 돌아가야지.";
        if (site.id == "parent")
            return forgotten ? "너는 외동이야. 창고의 옷과 물건도 네가 어릴 때 쓰던 거잖니."
                   : chapter.step >= chapter.Index("P001")
                       ? "아직 동생을 못 찾았니? 마을 아이가 공동묘지 길에서 봤다고 하더구나."
                       : "심부름 잘 다녀오렴. 너희 둘이 함께 가니 마음이 놓이는구나.";
        if (site.id == "child")
            return forgotten ? "동생이라니? 난 네가 혼자 다니는 모습만 기억하는데."
                             : "공동묘지로 가는 길은 무서워. 물어볼 게 있으면 이야기해.";
        if (site.id.compare(0, 5, "raen_") == 0)
            return forgotten
                       ? "나도 네 동생을 기억해. 사람들이 잊었다고 해서 남은 흔적까지 사라지진 "
                         "않아."
                       : "주령은 강한 기억과 감정에서 태어나. 저 목소리만으로 사람이라 믿지는 마.";
        if (site.id == "merchant")
            return forgotten ? "지난번에도 혼자 꾸러미를 가져왔잖니. 필요한 물건이 있으면 말하렴."
                             : "동생이 이웃 몫까지 챙겼더구나. 둘이 길을 조심해서 다니렴.";
        if (site.id == "stranger")
            return "집에 가야 하는데…. 돌아가는 길이 기억나지 않아….";
        const auto* quest = chapter.Current();
        for (int i = 0; i < chapter.step && i < int(chapter.quests.size()); ++i)
        {
            if (chapter.quests[i].trigger == "inspect:" + site.id)
                return chapter.quests[i].dialogue;
        }
        return quest ? "아직 이 흔적의 의미를 알 수 없다. 현재 조사는 Tab 기록창에서 확인하자."
                     : "수도원에서 얻은 기록과 성유물이 다음 의식 장소를 가리키고 있다.";
    }

    void ChooseChildDialogue(bool talk)
    {
        const auto* quest = chapter.Current();
        if (!chapter.choicePending || !quest || quest->id != "P001_WITNESS")
            return;
        chapter.choicePending = false;
        ClearInput();
        if (!talk)
        {
            dialoguePending = false;
            messageTime = 0;
            return;
        }
        Say("마을 아이", quest->dialogue);
        dialoguePending = true;
        AdvanceChapter();
        autoSaveTime = 30;
    }

    void UpdateChapter()
    {
        const auto* quest = chapter.Current();
        levelOne.storyTrial = quest && quest->trigger == "survive";
        if (levelOne.Dead())
        {
            return;
        }
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
                    "성유물을 확보했다. 수도원 밖을 성흔회가 포위한다. 여기까지가 이번 여정이다. "
                    "자유 탐험과 저장을 계속할 수 있다.");
                dialoguePending = true;
                ClearInput();
            }
            return;
        }
        if (chapter.rewardPending)
        {
            chapter.rewardPending = false;
            const bool battle = quest->trigger == "fight" || quest->trigger == "first_capture" ||
                                quest->trigger == "survive" || quest->trigger == "skill";
            Say(quest->title, battle ? quest->dialogue : quest->objective);
            dialoguePending = true;
            ClearInput();
            if (quest->id == "P005")
            {
                levelOne.captureUnlocked = true;
                ++levelOne.tickets[0];
                levelOne.selectedTicket = 0;
            }
            storyTrialTime = 0;
            return;
        }
        const bool encounter = quest->trigger == "fight" || quest->trigger == "first_capture" ||
                               quest->trigger == "survive";
        if (encounter && Distance(player, quest->destination) < 300)
        {
            levelOne.EnsureBoss(quest->amount,
                                quest->destination,
                                player,
                                [this](WorldPoint p)
                                {
                                    return Blocked(p);
                                });
        }
        bool done = encounter && levelOne.resolvedBosses.count(quest->amount) != 0;
        if (quest->trigger == "survive" && storyTrialTime >= 5)
        {
            for (auto& enemy : levelOne.enemies)
            {
                if (enemy.boss == 12 && enemy.health > 0)
                {
                    levelOne.Resolve(enemy, false);
                }
            }
            levelOne.projectiles.clear();
            done = true;
        }
        if (quest->trigger == "skill")
        {
            done = levelOne.skillsUsed > 0;
        }
        if (done)
        {
            if (quest->trigger == "first_capture" && !levelOne.collection.empty())
            {
                for (int i = int(levelOne.collection.size()) - 1; i >= 0; --i)
                {
                    if (levelOne.collection[i].species == SpiritCatalog::Forgotten)
                    {
                        selectedCompanion = i;
                        break;
                    }
                }
                capturedSpirit = levelOne.collection[selectedCompanion].species;
                captured = true;
                controlling = false;
                movementWasControlling = false;
                spirit = player;
                ClearInput();
            }
            AdvanceChapter();
        }
        if (!levelOne.collection.empty() && !captured)
        {
            captured = true;
            capturedSpirit = levelOne.collection.front().species;
            spirit = player;
        }
    }

    void House(const Object& o);
    void Tree(const Object& o, int variant);
    void Stone(const Object& o, int variant);
    void DrawPlayer(WorldPoint pos);
    void StoryProp(const StoryLayout::Site& site, WorldPoint at);
    void StoryFacade(const Object& building);
    void StoryPerson(const StoryLayout::Site& site, WorldPoint position);
    bool CharacterInView(WorldPoint position, float bodyHeight) const;
    bool StoryPersonVisible(const StoryLayout::Site& site, WorldPoint position) const;
    void DrawWorldLabel(WorldPoint position, float elevation, const std::string& text, Color color);
    void Wisp(int type, WorldPoint position);
    bool DrawSpiritModel(int species,
                         Point feet,
                         float size,
                         SpriteAnimation animation,
                         Color tint,
                         float motionClock,
                         float castProgress = -1.f);

    struct EnemyVisual
    {
        WorldPoint position;
        SpriteAnimation animation;
        float health = 0, maximum = 1, flash = 0;
        WorldPoint hitDirection = {0, 0};
        float hitStrength = 0;
        float castPose = 0;
        float remnantTime = -2;
        int type = 0, boss = 0;
        bool alerted = false, weakened = false;
    };

    void DrawEnemy(const EnemyVisual& enemy);
    void DrawCombatWarnings();
    void Hearth(WorldPoint position);

    void Panel(float x, float y, float w, float h)
    {
        r.Rect(x + 4, y + 5, w, h, Color(0, 0, 0, .32f));
        r.Rect(x, y, w, h, Color(.07f, .12f, .24f, .96f));
        r.Rect(x, y, 1, h, Color(.32f, .38f, .39f, .8f));
        r.Rect(x + w - 1, y, 1, h, Color(.32f, .38f, .39f, .8f));
        r.Rect(x, y, w, 1, Color(.48f, .43f, .30f, .65f));
        r.Rect(x, y + h - 1, w, 1, Color(.28f, .30f, .29f, .6f));
    }

    void UIButton(UIRect box, const std::string& label, bool selected = false)
    {
        r.Rect(box.x,
               box.y,
               box.w,
               box.h,
               selected ? Color(.36f, .30f, .67f, .98f) : Color(.19f, .36f, .57f, .98f));
        r.Rect(box.x,
               box.y,
               selected ? 3.f : 1.f,
               box.h,
               selected ? Color(.85f, .7f, .43f) : Color(.28f, .36f, .38f));
        const bool hover = box.Contains(cursorX, cursorY);
        r.Rect(box.x + 1, box.y + 1, box.w - 2, 2, Color(.65f, .8f, 1, hover ? .85f : .4f));
        if (hover)
            r.Rect(box.x + 2, box.y + 3, box.w - 4, box.h - 5, Color(.6f, .8f, 1, .12f));
        WrappedText(box.x + 12,
                    box.y + box.h * .5f + 6,
                    box.w - 24,
                    label,
                    selected ? Color(.92f, .78f, .52f) : Color(.85f, .86f, .82f),
                    1);
    }

#include "PrototypePopulation.inl"
#include "PrototypeControls.inl"
#include "PrototypeSave.inl"
#include "PrototypeMenu.inl"
#include "PrototypeUI.inl"
#include "PrototypeServices.inl"
#include "PrototypeMonitoring.inl"

  public:
    explicit Prototype(Renderer& renderer) : r(renderer)
    {
        villageObjects = VillageLayout::Buildings();
        for (size_t index = 0; index < villageObjects.size(); ++index)
        {
            const auto& object = villageObjects[index];
            for (auto y = World::Index(object.y - 12); y <= World::Index(object.y + object.d + 12);
                 ++y)
            {
                for (auto x = World::Index(object.x - 12);
                     x <= World::Index(object.x + object.w + 12);
                     ++x)
                {
                    villageCollision[{x, y}].push_back(index);
                }
            }
        }
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
        blenderLayout.detectLocomotionColumns = true;
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
        if (!protagonistModel.Load(L"Assets/Characters/Runtime/protagonist.meshbin"))
            std::cerr << "주인공 입체 모델을 불러오지 못해 기존 외형으로 표시합니다." << std::endl;
        SpriteLayout storyLayout;
        storyLayout.columns = 2;
        storyLayout.rows = 2;
        storyLayout.directionRows = {{0, 0, 0, 0, 1, 1, 1, 1}};
        storyLayout.directionIdleColumns = {{0, 1, 1, 1, 1, 0, 0, 0}};
        storyLayout.firstWalkFrame = 0;
        storyLayout.walkFrames = 1;
        storyLayout.anchorAtFeet = true;
        const auto& npcSprites = StoryLayout::NpcSprites();
        const wchar_t* residentWalkFiles[] = {L"Assets/Characters/Townsfolk/farmer_walk.png",
                                              L"Assets/Characters/Townsfolk/merchant_walk.png",
                                              L"Assets/Characters/Townsfolk/artisan_walk.png",
                                              L"Assets/Characters/Townsfolk/hunter_walk.png"};
        for (int i = 0; i < 4; ++i)
        {
            SpriteLayout residentLayout = storyLayout;
            residentLayout.columns = residentLayout.rows = 4;
            residentLayout.directionRows.fill(i);
            residentLayout.directionIdleColumns = {{0, 2, 2, 2, 3, 1, 1, 1}};
            residentLayout.anchorAtFeet = false;
            residentLayout.anchorAtTorso = true;
            residentLayout.normalizeFrameHeight = true;
            if (!populationSheets[i].Load(L"Assets/Characters/Townsfolk/residents.png",
                                          residentLayout))
                std::cerr << "생활 NPC 외형 로드 실패: " << i << std::endl;
            SpriteLayout walkLayout;
            walkLayout.columns = 8;
            walkLayout.rows = 4;
            walkLayout.directionRows = {{0, 2, 2, 2, 3, 1, 1, 1}};
            walkLayout.directionIdleColumns.fill(-1);
            walkLayout.firstWalkFrame = 0;
            walkLayout.walkFrames = 8;
            walkLayout.anchorAtTorso = true;
            walkLayout.normalizeFrameHeight = true;
            walkLayout.blendWalkFrames = false;
            if (!populationWalkSheets[i].Load(residentWalkFiles[i], walkLayout))
                std::cerr << "생활 NPC 걷기 원화 로드 실패: " << i << std::endl;
        }
        SpriteLayout gathererLayout;
        gathererLayout.columns = 33;
        gathererLayout.rows = 8;
        // The Blender camera's right axis is (-X,+Y), opposite Screen's (+X,-Y).
        gathererLayout.directionRows = {{0, 7, 6, 5, 4, 3, 2, 1}};
        gathererLayout.firstWalkFrame = 1;
        gathererLayout.walkFrames = 32;
        gathererLayout.fixedCanvas = true;
        gathererLayout.canvasAnchorY = .8394f;
        gathererLayout.canvasCharacterHeight = .6120f;
        gathererLayout.blendWalkFrames = false;
        if (!gathererSheet.Load(L"Assets/Characters/Townsfolk/Gatherer/walk.png", gathererLayout))
        {
            std::cerr << "채집꾼 입체 보행 원화 로드 실패: 기존 외형을 사용합니다." << std::endl;
        }
        for (int i = 0; i < StoryLayout::NpcCount; ++i)
        {
            SpriteLayout characterLayout = storyLayout;
            if (npcSprites[i].reversedSides)
            {
                // The smith sheet places the right-facing view in its upper-right cell.
                characterLayout.directionRows = {{0, 1, 1, 1, 1, 0, 0, 0}};
                characterLayout.directionIdleColumns = {{0, 0, 0, 0, 1, 1, 1, 1}};
            }
            if (!storyNpcSheets[i].Load(npcSprites[i].path, characterLayout))
            {
                std::cerr << "이야기 NPC 전용 스프라이트 로드 실패: " << i << std::endl;
            }
        }
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
        spiritLayout.pingPongWalk = true;
        spiritLayout.anchorAtTorso = true;
        for (int i = 0; i < SpiritCatalog::Count; ++i)
        {
            SpriteLayout speciesLayout = spiritLayout;
            if (i == SpiritCatalog::Saint)
            {
                // The saint glides. Alternate illustration poses are not aligned walk frames.
                speciesLayout.firstWalkFrame = 0;
                speciesLayout.walkFrames = 1;
                speciesLayout.pingPongWalk = false;
            }
            if (!spiritSheets[i].Load(SpiritCatalog::Get(i).sprite, speciesLayout))
                std::cerr << "주령 이미지 로드 실패: " << SpiritName(i) << std::endl;
        }
        for (int species = 0; species < SpiritCatalog::Count; ++species)
        {
            const auto& appearance = SpiritAppearance::Get(species);
            SpriteLayout modelLayout;
            modelLayout.columns = SpiritAppearance::Columns;
            modelLayout.rows = SpiritAppearance::Rows;
            modelLayout.directionRows = {{0, 1, 2, 3, 4, 5, 6, 7}};
            modelLayout.firstWalkFrame = 0;
            modelLayout.walkFrames = SpiritAppearance::Columns;
            modelLayout.fixedCanvas = true;
            modelLayout.blendWalkFrames = false;
            modelLayout.canvasAnchorY = appearance.anchorY;
            modelLayout.canvasCharacterHeight = appearance.characterHeight;
            if (!spiritModelSheets[species].Load(appearance.sprite, modelLayout))
            {
                std::cerr << "주령 입체 외형 로드 실패, 기존 원화 사용: " << SpiritName(species)
                          << std::endl;
            }
        }
        SpriteLayout encounterLayout;
        encounterLayout.directionRows = {{0, 2, 2, 2, 3, 1, 1, 1}};
        encounterLayout.firstWalkFrame = 0;
        encounterLayout.walkFrames = 4;
        encounterLayout.anchorAtTorso = true;
        encounterLayout.normalizeFrameHeight = true;
        encounterLayout.blendWalkFrames = false;
        for (int i = 0; i < StoryEncounter::SpriteCount; ++i)
        {
            if (!storyEnemySheets[i].Load(StoryEncounter::SpritePath(i), encounterLayout))
                std::cerr << "이야기 몬스터 전용 외형 로드 실패: " << i << std::endl;
        }
        chapter.Load();
        SpriteLayout lootLayout;
        lootLayout.columns = lootLayout.rows = 1;
        lootLayout.directionRows.fill(0);
        lootLayout.firstWalkFrame = 0;
        lootLayout.walkFrames = 1;
        LoadPresentation(lootLayout);
        if (!domainPortrait.Load(L"Assets/Effects/domain_portrait.png", lootLayout))
        {
            std::cerr << "영역 컷인 이미지 로드 실패" << std::endl;
        }
        const wchar_t* portraits[] = {L"Assets/Effects/forgotten_ultimate.png",
                                      L"Assets/Effects/pilgrim_ultimate.png",
                                      L"Assets/Effects/hands_ultimate.png",
                                      L"Assets/Effects/saint_ultimate.png"};
        for (int i = 0; i < 4; ++i)
            spiritPortraits[i].Load(portraits[i], lootLayout);
        ghostFace.Load(L"Assets/Effects/ghost_scream.png", lootLayout);
        ghostScreamPoses.Load(L"Assets/Effects/ghost_scream_poses.png", lootLayout);
        const wchar_t* realmFiles[] = {L"Assets/Effects/Realms/snow_graveyard.png",
                                       L"Assets/Effects/Realms/endless_pilgrimage.png",
                                       L"Assets/Effects/Realms/praying_sanctuary.png",
                                       L"Assets/Effects/Realms/inverted_belfry.png"};
        for (int i = 0; i < 4; ++i)
            if (!spiritRealms[i].Load(realmFiles[i], lootLayout))
                std::cerr << "주령 영역 이미지 로드 실패: " << i << std::endl;
        const wchar_t* ghostFiles[] = {L"Assets/Effects/ghost_binding.png",
                                       L"Assets/Effects/ghost_drift.png",
                                       L"Assets/Effects/ghost_choir.png"};
        for (int i = 0; i < 3; ++i)
            ghostVariants[i].Load(ghostFiles[i], lootLayout);
        const wchar_t* spellArt[] = {L"Assets/Effects/pilgrim_skills.png",
                                     L"Assets/Effects/hands_skills.png",
                                     L"Assets/Effects/saint_skills.png"};
        for (int i = 0; i < 3; ++i)
        {
            if (!spiritSpellArt[i].Load(spellArt[i], lootLayout))
                std::cerr << "주령 스킬 이미지 로드 실패: " << i << std::endl;
        }
        lootLayout.anchorAtFeet = true;
        const wchar_t* lootFiles[] = {L"Assets/Items/experience.png",
                                      L"Assets/Items/weapon_stone.png",
                                      L"Assets/Items/healing.png",
                                      L"Assets/Items/spirit_stone.png",
                                      L"Assets/Items/seal_ticket.png"};
        for (int i = 0; i < 5; ++i)
        {
            if (!lootSheets[i].Load(lootFiles[i], lootLayout))
                std::cerr << "드롭 이미지 로드 실패: 종류 " << i
                          << " · 실행 폴더의 Assets/Items를 확인하세요." << std::endl;
        }
        levelOne.captureRules.Load();
        levelOne.LoadCombat();
        world.Stream(camera, player, width, height);
    }

    void ProgressKey(bool load)
    {
        if (openingPlaying || slotMode != SlotMode::Closed || exitConfirm)
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
        MouseButton(0, true, x, y);
    }

    void PointerMove(int x, int y)
    {
        cursorX = x;
        cursorY = y;
        if (titleScreen && !openingPlaying && slotMode == SlotMode::Closed)
        {
            for (int i = 0; i < 3; ++i)
                if (FrontButton(i).Contains(x, y))
                    frontSelection = i;
        }
    }

    void MouseButton(int button, bool down, int x, int y)
    {
        PointerMove(x, y);
        if (button == 0 && !down)
        {
            attackHeld = false;
            return;
        }
        if (!down)
        {
            return;
        }
        if (openingPlaying)
        {
            if (button == 0 && OpeningSkipBounds().Contains(x, y))
                SkipOpening();
            return;
        }
        if (button == 0 && scene.PointerDown(x, y))
        {
            return;
        }
        if (!ControlsActive())
        {
            return;
        }
        if (MinimapBounds().Contains(x, y) || SkillBarBounds().Contains(x, y) ||
            (hudVisible && QuestTrackerBounds().Contains(x, y)))
        {
            return;
        }
        if (button == 2)
        {
            MoveTo(CursorWorld());
        }
        if (button == 0)
        {
            StopMovement();
            attackHeld = true;
            BasicAttack(true);
        }
    }

    void MenuClick(int x, int y)
    {
        if ((!pauseMenu && !journal) || width < 480 || height < 360)
        {
            return;
        }
        if (journal)
        {
            JournalClick(x, y);
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
        if (openingPlaying)
        {
            if (key == 27 || key == 13 || key == ' ')
                SkipOpening();
            keys[key] = true;
            return;
        }
        if (titleScreen || slotMode != SlotMode::Closed)
        {
            FrontKey(key);
            keys[key] = true;
            return;
        }
        if (!serviceNpc.empty())
        {
            if (key == 27)
                serviceNpc.clear();
            else if (key >= '1' && key <= '6')
                ServiceAction(key - '1');
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
            if (!exitConfirm && (key == '[' || key == ']'))
            {
                ChangeMenuPage(key == ']' ? 1 : -1);
                return;
            }
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
            if (key == '1' || key == '2')
            {
                ChooseChildDialogue(key == '1');
            }
            return;
        }
        if (!journal && dialoguePending && (key == 'f' || key == 13))
        {
            const int pages = int(DialoguePages().size());
            dialoguePage = (std::min)(dialoguePage, pages - 1);
            if (dialoguePage + 1 < pages)
            {
                ++dialoguePage;
                return;
            }
            dialoguePending = false;
            dialoguePage = 0;
            talkingResident = -1;
            messageTime = 0;
            return;
        }
        if (key == '\t')
        {
            ClearInput();
            journal = !journal;
            return;
        }
        if (key == 'j')
        {
            ClearInput();
            if (journal && menuTab == 2)
                journal = false;
            else
            {
                journal = true;
                menuTab = 2;
            }
            return;
        }
        if (key == 'u')
        {
            hudVisible = !hudVisible;
            return;
        }
        if (dialoguePending && !journal)
            return;
        if (journal)
        {
            if (key >= '1' && key <= '5')
            {
                menuTab = key - '1';
                journalPage = 0;
            }
            if (key == '[' || key == ']')
                ChangeJournalPage(key == ']' ? 1 : -1);
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
        if (key == 'o')
        {
            r.postProcess.bloomEnabled = !r.postProcess.bloomEnabled;
            Say("빛 번짐",
                r.postProcess.bloomEnabled ? "블룸을 켰습니다. 후처리도 켜져 있어야 적용됩니다."
                                           : "블룸을 껐습니다.");
        }

        if (key == 'r' && levelOne.Dead())
        {
            population.people.clear();
            population.society = Population::Society();
            levelOne = LevelOne();
            ResetControls();
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
        if (key == 't')
            levelOne.selectedTicket = (levelOne.selectedTicket + 1) % 3;
        if (key == 'x')
            levelOne.Capture(player, true);
        if (key == 'h')
            levelOne.UsePotion();
        if (key == 'f')
            Interact();
        if (key == 'k')
            screenShake = !screenShake;
        if (key == 'm')
            ToggleProtagonistModel();
        if (key == 'n')
            ToggleGathererStudy();
        const std::string skillKeys = "qwer";
        const auto skillSlot = skillKeys.find(char(key));
        if (skillSlot != std::string::npos)
        {
            CastSkill(static_cast<int>(skillSlot));
        }
        if (key == 's')
        {
            bufferedCommand.action = -1;
            StopMovement();
            attackHeld = false;
        }
        if (key == 'b')
        {
            if (PreparingSkill())
                return;
            if (captured)
            {
                if (!controlling && levelOne.collection[selectedCompanion].health < 25.f)
                {
                    Say("조종 전환", "주령 체력이 25 이상 회복되면 조종할 수 있습니다.");
                    return;
                }
                controlling = !controlling;
                ClearInput();
                movementWasControlling = controlling;
                Say("조종 전환",
                    controlling ? "주령을 조종합니다. 우클릭으로 이동하고 주령 QWER을 사용합니다. "
                                  "B로 본체에 돌아갑니다."
                                : "방랑자의 몸으로 돌아와 기본 QWER을 사용합니다.");
            }
            else
                Say("조종 전환", "라엔에게 봉인을 배운 뒤 약화된 주령에게 봉인권을 사용하세요.");
        }
    }

    void Arrow(int key, bool down)
    {
        if (openingPlaying)
            return;
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
                MoveMenuSelection(key == 1 ? -1 : 1);
            }
            return;
        }
    }

    void ClearInput()
    {
        for (bool& k : keys)
            k = false;
        bufferedCommand.action = -1;
        StopMovement();
        attackHeld = false;
    }

    void Update(float dt, bool sprint = false);
    void Draw();

  private:
    void LoadPresentation(const SpriteLayout& layout);
    void StartOpening();
    void UpdatePresentation(float dt);
    void SkipOpening();
    UIRect PresentationBounds() const;
    UIRect OpeningSkipBounds() const;
    void DrawPresentationBackground();
    void DrawOpening();
    void DrawPresentationImage(int image, const UIRect& bounds, float progress);
    UIRect FrontBounds() const;
    UIRect FrontButton(int index) const;
    bool GameUIClick(int x, int y);
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
