#pragma once
#include "Renderer.h"
#include "World.h"
#include "SpriteSheet.h"
#include "LevelOne.h"
#include "SaveGame.h"
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
    std::vector<Object> objects;
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

    void House(const Object& o)
    {
        Box(o.x,
            o.y,
            o.w,
            o.d,
            o.h,
            Color(.23f, .25f, .25f),
            Color(.19f, .21f, .22f),
            Color(.12f, .15f, .17f));
        Point a = Screen(o.x - 12, o.y - 12, o.h), b = Screen(o.x + o.w + 12, o.y - 12, o.h);
        Point c = Screen(o.x + o.w + 12, o.y + o.d + 12, o.h),
              d = Screen(o.x - 12, o.y + o.d + 12, o.h);
        Point ridgeA = Screen(o.x - 12, o.y + o.d * .5f, o.h + 65),
              ridgeB = Screen(o.x + o.w + 12, o.y + o.d * .5f, o.h + 65);
        r.Triangle(a, d, ridgeA, Color(.16f, .19f, .20f));
        r.Quad(ridgeA, ridgeB, c, d, Color(.27f, .19f, .18f));
        r.Triangle(b, c, ridgeB, Color(.19f, .13f, .14f));
        r.Line(ridgeA, ridgeB, 3, Color(.38f, .29f, .24f));
        for (int i = 1; i < 5; ++i)
        {
            float u = float(i) / 5;
            r.Line({ridgeA.x + (ridgeB.x - ridgeA.x) * u, ridgeA.y + (ridgeB.y - ridgeA.y) * u},
                   {d.x + (c.x - d.x) * u, d.y + (c.y - d.y) * u},
                   2,
                   Color(.16f, .13f, .14f));
        }
        // Front-wall door and windows lie on the same projected plane as the wall.
        double front = o.y + o.d;
        r.Quad(Screen(o.x + o.w * .42f, front, 0),
               Screen(o.x + o.w * .65f, front, 0),
               Screen(o.x + o.w * .65f, front, 48),
               Screen(o.x + o.w * .42f, front, 48),
               Color(.07f, .09f, .10f));
        r.Quad(Screen(o.x + 18, front, 38),
               Screen(o.x + 40, front, 38),
               Screen(o.x + 40, front, 62),
               Screen(o.x + 18, front, 62),
               Color(2.2f, 1.25f, .45f));
        Box(o.x + o.w * .7f,
            o.y + 18,
            18,
            20,
            o.h + 70,
            Color(.32f, .32f, .29f),
            Color(.23f, .23f, .23f),
            Color(.16f, .17f, .18f));
    }

    void Tree(const Object& o)
    {
        Point p = Screen(o.x + o.w * .5f, o.y + o.d * .5f);
        r.Ellipse(p.x, p.y, 27, 11, Color(0, 0, 0, .25f));
        r.Line(p, {p.x - 4, static_cast<float>(p.y - o.h)}, 7, Color(.17f, .16f, .17f));
        for (int i = 0; i < 4; ++i)
        {
            float side = i % 2 ? 1.f : -1.f, level = static_cast<float>(p.y - o.h * .35 - i * 15);
            Point tip = {p.x + side * (23 + i * 3), level - 28};
            r.Line({p.x - 2, level}, tip, 4, Color(.19f, .18f, .19f));
            r.Line(tip, {tip.x + side * 10, tip.y - 21}, 2, Color(.22f, .21f, .22f));
        }
    }

    void Person(WorldPoint pos, Color coat, bool isPlayer, bool childSize = false)
    {
        Point p = Screen(pos.x, pos.y);
        const float spriteHeight = childSize ? 55.f : 78.f;
        if (p.x < -80 || p.x > width + 80 || p.y < -20 || p.y > height + 100)
            return;
        r.Ellipse(p.x, p.y + 1, childSize ? 10.f : 14.f, 5, Color(0, 0, 0, .35f));
        if (isPlayer && !controlling)
            r.Ellipse(p.x, p.y, 18, 7, Color(.75f, .69f, .45f, .20f));
        SpriteAnimation animation = protagonistAnimation;
        SpriteSheet* sheet = isPlayer && protagonistSprinting && protagonistRunSheet.Ready()
                                 ? &protagonistRunSheet
                                 : &protagonistSheet;
        Color tint = isPlayer && levelOne.dodgeTime > 0  ? Color(.5f, 1.5f, 2.f)
                     : isPlayer && levelOne.hurtTime > 0 ? Color(1.5f, .55f, .55f)
                                                         : Color(1, 1, 1);
        if (isPlayer && levelOne.castTime > 0 && !protagonistAnimation.moving &&
            attackSheet.Ready())
        {
            sheet = &attackSheet;
            animation.facing = levelOne.castFacing;
            animation.moving = true;
            animation.phase = (std::min)(3.f, std::floor((.18f - levelOne.castTime) / .045f));
        }
        if (!isPlayer)
        {
            sheet = childSize ? &childSheet : &keeperSheet;
            // Until a dedicated image is supplied, share the protagonist texture as a placeholder.
            if (!sheet->Ready())
            {
                sheet = &protagonistSheet;
                tint = childSize ? Color(1.f, .78f, .66f) : Color(.72f, .9f, .78f);
            }
            animation = SpriteAnimation();
            if (Distance(pos, player) < 160)
            {
                double dx = player.x - pos.x, dy = player.y - pos.y;
                animation.Update((dx - dy) * .85, (dx + dy) * .43, 0, false);
                animation.moving = false;
            }
            if (childSize ? blenderChild : blenderKeeper)
            {
                // Idle sheets contain a full breathing loop for each facing.
                animation.moving = true;
                animation.phase = std::fmod(time * 4.f + (childSize ? 0.f : 3.f), 8.f);
            }
        }
        if (sheet->Ready())
            sheet->Draw(r,
                        p,
                        spriteHeight,
                        animation,
                        tint,
                        width,
                        height,
                        isPlayer && levelOne.castTime > 0 && !protagonistAnimation.moving ? 1.4f
                                                                                          : 0.f);
        else
        {
            // Keep the actor visible if an asset is missing or PNG decoding fails.
            r.Triangle({p.x, p.y - 43}, {p.x - 14, p.y - 6}, {p.x + 14, p.y - 6}, coat);
            r.Ellipse(p.x, p.y - 44, 8, 10, Color(.61f, .53f, .43f));
        }
    }

    void Wisp(int type, WorldPoint position)
    {
        Point p = Screen(position.x, position.y);
        if (p.x < -100 || p.x > width + 100 || p.y < -50 || p.y > height + 130)
            return;
        r.Ellipse(p.x, p.y, 18, 6, Color(0, 0, 0, .3f));
        SpriteAnimation animation;
        if (captured && capturedSpirit == type)
            animation = companionAnimation;
        float bob = type % 3 == 2 ? 0.f : std::sin(time * 2 + type) * 3.f;
        if (spiritSheets[type % 3].Ready())
            spiritSheets[type % 3].Draw(r,
                                        {p.x, p.y - 4 + bob},
                                        type == 2 ? 67.f : 72.f,
                                        animation,
                                        Color(1, 1, 1),
                                        width,
                                        height,
                                        type == 0 ? 2.f : (type == 1 ? 1.f : .15f));
        else
            r.Ellipse(p.x, p.y - 25, 12, 18, Color(.4f, .7f, .8f));
    }

    void DrawEnemy(const LevelOne::Enemy& enemy)
    {
        const Point p = Screen(enemy.position.x, enemy.position.y);
        r.Ellipse(p.x, p.y, 20, 7, Color(.65f, .08f, .1f, .6f));
        if (spiritSheets[enemy.type].Ready())
        {
            spiritSheets[enemy.type].Draw(
                r,
                p,
                enemy.boss > 0 ? 100.f : 68.f,
                enemy.animation,
                enemy.flash > 0
                    ? Color(2, 2, 2)
                    : (enemy.returning ? Color(.55f, .55f, .65f) : Color(1.25f, .6f, .65f)),
                width,
                height,
                .4f);
        }
        else
        {
            r.Ellipse(p.x, p.y - 25, 16, 24, Color(.8f, .2f, .3f));
        }
        if (!enemy.alerted && !enemy.weakened && enemy.flash <= 0 && enemy.boss == 0)
            return;
        const float barY = p.y - (enemy.boss > 0 ? 111.f : 79.f);
        r.Rect(p.x - 20, barY, 40, 4, Color(.15f, .08f, .09f));
        r.Rect(p.x - 20,
               barY,
               40 * (std::max)(0.f, enemy.health) / enemy.maximum,
               4,
               Color(.9f, .25f, .25f));
    }

    void DrawCombatWarnings()
    {
        for (const auto& enemy : levelOne.enemies)
        {
            if (!enemy.alerted || enemy.weakened || enemy.health <= 0 ||
                (enemy.phase != 1 && enemy.phase != 2))
                continue;
            const Color warning =
                enemy.phase == 2 ? Color(2.f, .2f, .1f, .6f) : Color(1.f, .24f, .07f, .45f);
            const auto origin = enemy.position;
            if (enemy.type == 1)
            {
                const double reach = levelOne.combat.chargeSpeed * .48;
                const double nx = -enemy.aim.y * 30, ny = enemy.aim.x * 30;
                r.Quad(Screen(origin.x + nx, origin.y + ny),
                       Screen(origin.x - nx, origin.y - ny),
                       Screen(origin.x + enemy.aim.x * reach - nx,
                              origin.y + enemy.aim.y * reach - ny),
                       Screen(origin.x + enemy.aim.x * reach + nx,
                              origin.y + enemy.aim.y * reach + ny),
                       warning);
            }
            else if (enemy.type == 0)
            {
                const double base = std::atan2(enemy.aim.y, enemy.aim.x);
                for (int i = 0; i < 16; ++i)
                {
                    const double a = base - 1.213 + i * 2.426 / 16;
                    const double b = base - 1.213 + (i + 1) * 2.426 / 16;
                    const Point center = Screen(origin.x, origin.y);
                    r.Quad(center,
                           Screen(origin.x + std::cos(a) * 85, origin.y + std::sin(a) * 85),
                           Screen(origin.x + std::cos(b) * 85, origin.y + std::sin(b) * 85),
                           center,
                           warning);
                }
            }
            else
            {
                for (int i = -1; i <= 1; ++i)
                {
                    const double a = i * .18, c = std::cos(a), s = std::sin(a);
                    r.Line(Screen(origin.x, origin.y),
                           Screen(origin.x + (enemy.aim.x * c - enemy.aim.y * s) * 520,
                                  origin.y + (enemy.aim.x * s + enemy.aim.y * c) * 520),
                           2,
                           warning);
                }
            }
        }
        if (levelOne.dodgeTime > 0)
        {
            const Point p = Screen(player.x, player.y);
            r.Ellipse(p.x, p.y, 25, 10, Color(.25f, 1.3f, 2.f, .7f));
        }
    }

    void DrawLootAndProjectiles()
    {
        for (const LevelOne::Loot& item : levelOne.loot)
        {
            const Point p = Screen(item.position.x, item.position.y, 9 + std::sin(time * 4) * 2);
            const Color colors[] = {Color(.2f, 1.2f, 2.f),
                                    Color(2.f, 1.2f, .2f),
                                    Color(.3f, 1.8f, .5f),
                                    Color(1.6f, .4f, 2.f),
                                    Color(1.6f, 1.6f, 1.6f)};
            const Color color = colors[static_cast<int>(item.kind)];
            r.Quad({p.x, p.y - 6}, {p.x + 5, p.y}, {p.x, p.y + 6}, {p.x - 5, p.y}, color);
        }
        for (const LevelOne::Projectile& shot : levelOne.projectiles)
        {
            const Point p = Screen(shot.position.x, shot.position.y, 32);
            const Point tail = Screen(shot.position.x - shot.velocity.x * .03,
                                      shot.position.y - shot.velocity.y * .03,
                                      32);
            const Color color = shot.hostile     ? Color(3.f, .35f, .15f)
                                : shot.companion ? Color(1.8f, .5f, 2.f)
                                                 : Color(.4f, 1.5f, 3.f);
            r.Line(tail, p, 4, color);
            r.Ellipse(p.x, p.y, 5, 5, color);
        }
    }

    void Hearth()
    {
        Point p = Screen(0, 0);
        r.Ellipse(p.x, p.y, 28, 13, Color(.29f, .28f, .26f));
        r.Ellipse(p.x, p.y, 22, 9, Color(.09f, .10f, .11f));
        r.Line({p.x - 15, p.y + 3}, {p.x + 12, p.y - 5}, 6, Color(.36f, .23f, .16f));
        float flicker = std::sin(time * 13) * 3;
        r.Triangle({p.x - 13, p.y},
                   {p.x + 11, p.y},
                   {p.x + 3, p.y - 40 - flicker},
                   Color(2.4f, .9f, .25f, .9f));
        r.Triangle(
            {p.x - 7, p.y}, {p.x + 8, p.y}, {p.x - 2, p.y - 25 + flicker}, Color(3.f, 1.8f, .6f));
        for (int i = 0; i < 7; ++i)
        {
            float t = std::fmod(time * 18 + i * 12.f, 85.f);
            r.Ellipse(p.x + std::sin(t * .08f + i) * 12,
                      p.y - t,
                      1.3f,
                      2,
                      Color(1, .66f, .30f, 1 - t / 85));
        }
    }

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
        if (!LoadProgress(true))
        {
            world.Stream(camera, player, width, height);
            UpdateChapter();
        }
    }

    void ProgressKey(bool load)
    {
        if (load)
        {
            LoadProgress();
        }
        else
        {
            SaveProgress(true);
        }
    }

    void SaveBeforeClose()
    {
        if (!skipExitSave && automaticSaveAllowed && CanSaveProgress() && !SaveProgress(false))
        {
            std::cerr << "종료 저장 실패: 마지막 저장 기록을 유지합니다." << std::endl;
        }
    }

    bool WantsExit() const
    {
        return exitRequested;
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
        const auto box = MenuBounds();
        if (x < box.x + 16 || x > box.x + box.w - 16)
        {
            return;
        }
        const int row = int(std::floor((y - box.y - 55) / box.step));
        if (row >= 0 && row < (exitConfirm ? 2 : 9))
        {
            ActivateMenu(row);
        }
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

    void Update(float dt, bool sprint = false)
    {
        dt = (std::max)(0.f, (std::min)(dt, .05f));
        const WorldPoint previousPlayer = player;
        const WorldPoint previousSpirit = spirit;
        time += dt;
        saveStatusTime = (std::max)(0.f, saveStatusTime - dt);
        if (!pauseMenu && !journal)
        {
            autoSaveTime += dt;
        }
        if (!dialoguePending && !journal && !pauseMenu)
            messageTime = (std::max)(0.f, messageTime - dt);
        moving = false;
        const bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
        const bool dodgePressed = ctrl && !dodgeHeld;
        dodgeHeld = ctrl;
        if (!pauseMenu && !journal && !chapter.choicePending && !dialoguePending &&
            !levelOne.Dead())
        {
            float sx = float(keys['d'] || arrows[2]) - float(keys['a'] || arrows[0]);
            float sy = float(keys['s'] || arrows[3]) - float(keys['w'] || arrows[1]);
            // Invert the isometric projection so keys correspond to screen directions.
            float dx = sx / .85f + sy / .43f, dy = -sx / .85f + sy / .43f;
            float length = std::sqrt(sx * sx + sy * sy);
            if (!controlling)
                levelOne.Dodge(dt,
                               player,
                               {dx, dy},
                               dodgePressed,
                               [this](WorldPoint p)
                               {
                                   return Blocked(p);
                               });
            if (length > 0 && (controlling || levelOne.dodgeTime <= 0))
            {
                const float pace = sprint ? 1.8f : 1.f;
                float speed = (controlling ? 110.f : 90.f) * pace;
                dx = dx * .5f / length * speed * dt;
                dy = dy * .5f / length * speed * dt;
                WorldPoint& actor = controlling ? spirit : player;
                LevelOne::Move(actor,
                               {dx, dy},
                               [this](WorldPoint p)
                               {
                                   return Blocked(p);
                               });
                moving = true;
                walk += dt * 10 * pace;
            }
            if (captured && !controlling)
            {
                WorldPoint target = {player.x + 35, player.y + 20};
                float t = 1 - std::exp(-dt * 3);
                LevelOne::Move(spirit,
                               {(target.x - spirit.x) * t, (target.y - spirit.y) * t},
                               [this](WorldPoint p)
                               {
                                   return Blocked(p);
                               });
            }
        }
        if (!pauseMenu && !journal && !chapter.choicePending && !dialoguePending)
        {
            UpdateChapter();
            if (!dialoguePending)
                levelOne.Update(dt,
                                player,
                                spirit,
                                captured,
                                controlling,
                                keys[' '],
                                world.seed,
                                [this](WorldPoint p)
                                {
                                    return Blocked(p);
                                });
            if (!levelOne.notice.empty())
            {
                Say("기록", levelOne.notice);
                messageTime = 3;
                levelOne.notice.clear();
            }
        }
        double actualX = player.x - previousPlayer.x, actualY = player.y - previousPlayer.y;
        protagonistSprinting =
            sprint && !controlling && (std::abs(actualX) + std::abs(actualY) > .001);
        // One pose per 13 screen pixels: sprinting and wall sliding follow actual travel.
        // Keep the stride phase when switching between walking and sprinting.
        if (!pauseMenu && !journal && !chapter.choicePending && !dialoguePending)
            protagonistAnimation.Update((actualX - actualY) * .85,
                                        (actualX + actualY) * .43,
                                        dt,
                                        sprint,
                                        blenderProtagonist ? (sprint ? 7.f : 5.f) : 13.f);
        const double spiritDX = spirit.x - previousSpirit.x, spiritDY = spirit.y - previousSpirit.y;
        double screenDX = (spiritDX - spiritDY) * .85, screenDY = (spiritDX + spiritDY) * .43;
        // Ignore the tiny convergence tail of the following interpolation.
        if (!controlling && screenDX * screenDX + screenDY * screenDY < .0025)
        {
            screenDX = 0;
            screenDY = 0;
        }
        if (!pauseMenu && !journal && !chapter.choicePending && !dialoguePending)
            companionAnimation.Update(screenDX, screenDY, dt, controlling && sprint);
        WorldPoint target = controlling ? spirit : player;
        if (Distance(target, camera) > 2000)
            camera = target; // Distant vessel switch.
        float t = 1 - std::exp(-dt * 6);
        camera.x += (target.x - camera.x) * t;
        camera.y += (target.y - camera.y) * t;
        world.Stream(camera, target, width, height);
        if (!pauseMenu && !journal && automaticSaveAllowed && autoSaveTime >= 30 &&
            CanSaveProgress())
        {
            SaveProgress(false);
        }
    }

    void Draw()
    {
        r.Begin(Color(.035f, .052f, .069f));
        objects = villageObjects;
        for (const auto& pair : world.chunks)
        {
            const World::Chunk& chunk = pair.second;
            for (const Object& o : chunk.objects)
                objects.push_back(o);
            const std::string terrainKey = "terrain:" + std::to_string(chunk.key.first) + ":" +
                                           std::to_string(chunk.key.second) + ":" +
                                           std::to_string(world.seed);
            const Point terrainOrigin = Screen(double(chunk.key.first) * World::ChunkSize,
                                               double(chunk.key.second) * World::ChunkSize);
            if (r.BeginCachedMesh(terrainKey, terrainOrigin))
            {
                for (int y = 0; y < 8; ++y)
                    for (int x = 0; x < 8; ++x)
                    {
                        World::Coordinate tx = chunk.key.first * 8 + x,
                                          ty = chunk.key.second * 8 + y;
                        double wx = double(tx) * 64, wy = double(ty) * 64;
                        Point p = Screen(wx, wy);

                        auto hash = World::Hash(tx, ty, 42);
                        float shade = float(hash % 15) * .002f;
                        Color color = chunk.biome == 0
                                          ? Color(.095f + shade, .125f + shade, .13f + shade)
                                          : (chunk.biome == 1
                                                 ? Color(.12f + shade, .145f + shade, .145f + shade)
                                                 : Color(.15f + shade, .14f + shade, .14f + shade));
                        if (World::Village(wx, wy))
                            color = Color(.11f + shade, .13f + shade, .13f + shade);
                        bool road = World::Road(tx, ty);
                        if (road)
                            color = Color(.23f + shade, .23f + shade, .20f + shade);
                        Ground(wx, wy, 64, 64, color);
                        if (road)
                            Ground(wx + 12 + double(hash % 17),
                                   wy + 15,
                                   16,
                                   10,
                                   Color(.32f, .31f, .26f, .5f));
                        else
                            r.Line(p, {p.x + 3, p.y - 4}, 1, Color(.24f, .27f, .24f, .4f));
                    }
                r.EndCachedMesh();
            }
            r.DrawCachedMesh(terrainKey, terrainOrigin);
        }
        Ground(-165, -145, 330, 290, Color(.24f, .24f, .21f));
        DrawCombatWarnings();

        struct Entry
        {
            double depth;
            int index;
        };

        std::vector<Entry> order;
        for (size_t i = 0; i < objects.size(); ++i)
        {
            const Object& o = objects[i];
            Point bounds = Screen(o.x + o.w * .5, o.y + o.d * .5);
            if (bounds.x < -300 || bounds.x > width + 300 || bounds.y < -250 ||
                bounds.y > height + 350)
                continue;
            order.push_back({o.x + o.y + o.w + o.d, static_cast<int>(i)});
        }
        order.push_back({0, -1});
        order.push_back({child.x + child.y, -2});
        order.push_back({keeper.x + keeper.y, -3});
        order.push_back({player.x + player.y, -4});
        if (captured)
            order.push_back({spirit.x + spirit.y, -8});
        for (size_t i = 0; i < levelOne.enemies.size(); ++i)
            order.push_back({levelOne.enemies[i].position.x + levelOne.enemies[i].position.y,
                             -1000 - static_cast<int>(i)});
        std::stable_sort(order.begin(),
                         order.end(),
                         [](const Entry& a, const Entry& b)
                         {
                             return a.depth < b.depth;
                         });
        for (const Entry& entry : order)
        {
            if (entry.index >= 0)
            {
                const Object& o = objects[entry.index];
                const std::string meshKey = "object:" + std::to_string(o.kind) + ":" +
                                            std::to_string(o.w) + ":" + std::to_string(o.d) + ":" +
                                            std::to_string(o.h);
                const Point origin = Screen(o.x, o.y);
                if (r.BeginCachedMesh(meshKey, origin))
                {
                    if (o.kind == 0)
                        House(o);
                    else if (o.kind == 1)
                        Tree(o);
                    else
                        Box(o.x,
                            o.y,
                            o.w,
                            o.d,
                            o.h,
                            Color(.33f, .36f, .35f),
                            Color(.23f, .27f, .28f),
                            Color(.17f, .21f, .23f));
                    r.EndCachedMesh();
                }
                r.DrawCachedMesh(meshKey, origin);
            }
            else if (entry.index == -1)
                Hearth();
            else if (entry.index == -2)
                Person(child, Color(.48f, .36f, .29f), false, true);
            else if (entry.index == -3)
                Person(keeper, Color(.33f, .37f, .34f), false);
            else if (entry.index == -4)
                Person(player, Color(.44f, .49f, .52f), true);
            else if (entry.index <= -1000)
                DrawEnemy(levelOne.enemies[-entry.index - 1000]);
            else if (entry.index == -8)
                Wisp(capturedSpirit, spirit);
        }
        DrawLootAndProjectiles();
        // Moving low fog and airborne ash, deliberately subtle over the scene.
        for (int i = 0; i < 9; ++i)
        {
            float x = std::fmod(i * 197.f + time * 9, width + 350.f) - 175;
            r.Ellipse(x, height * .25f + i * 67, 210, 20, Color(.38f, .48f, .51f, .025f));
        }
        for (int i = 0; i < 40; ++i)
        {
            float x = std::fmod(i * 131.f + time * 13, width + 10.f);
            float y = std::fmod(i * 73.f + time * 8, height + 10.f);
            r.Rect(x, y, 2, 2, Color(.67f, .65f, .55f, .2f));
        }
        r.FinishScene(); // Post-process the world before drawing crisp UI and text.
        // Framing bars keep the world visually quiet behind the interface.
        r.Rect(0, 0, float(width), 10, Color(.02f, .03f, .04f));
        HUD();
        r.Flush();
    }
};
