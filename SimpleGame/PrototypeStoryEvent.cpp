#include "stdafx.h"
#include "Prototype.h"

bool Prototype::CemeteryCleared() const
{
    return levelOne.resolvedBosses.count(1) != 0 || chapter.step > chapter.Index("P007");
}

bool Prototype::StoryVisible(const StoryLayout::Site& site) const
{
    return StoryLayout::Visible(site, chapter) && (site.id != "seal" || CemeteryCleared());
}

WorldPoint Prototype::StoryPosition(const StoryLayout::Site& site) const
{
    return site.id == "seal" ? CemeteryRoom::Seal() : site.position;
}

WorldPoint Prototype::StoryObjectivePosition(const Chapter::Quest& quest) const
{
    if (quest.id == "P007")
    {
        return cemeteryEngaged ? CemeteryRoom::Boss() : CemeteryRoom::Gate();
    }
    if (quest.id == "P007_SEAL")
    {
        return CemeteryRoom::Seal();
    }
    return quest.trigger == "skill" ? player : quest.destination;
}

std::string Prototype::StoryObjectiveText(const Chapter::Quest& quest) const
{
    if (quest.id == "P007")
    {
        return cemeteryEngaged ? "묘역 안의 울음 먹는 자를 격파하기"
                               : "공동묘지 북쪽 봉인문에서 F로 묘역에 진입하기";
    }
    if (quest.id == "P007_SEAL")
    {
        return "묘역 동쪽 제단의 봉인 흔적을 조사해 두 파편 확보하기";
    }
    return quest.objective;
}

bool Prototype::UpdateStoryEvent()
{
    const auto* quest = chapter.Current();
    if (!quest || (quest->id != "P007" && quest->id != "P007_SEAL"))
    {
        return false;
    }
    // This event owns its actual completion transitions; acknowledging a briefing
    // must neither spawn the boss in the road nor finish the investigation.
    chapter.rewardPending = false;
    if (quest->id == "P007" && levelOne.resolvedBosses.count(1))
    {
        levelOne.health = (std::max)(1.f, levelOne.health);
        StartStoryScene(StoryScene::RoomVictory);
    }
    return true;
}

bool Prototype::InteractStoryEvent()
{
    const auto* quest = chapter.Current();
    if (quest && quest->id == "P007_SEAL" && Distance(player, CemeteryRoom::Seal()) < 62)
    {
        StartStoryScene(StoryScene::SealDiscovery);
        return true;
    }
    if (Distance(player, CemeteryRoom::Gate()) >= 70)
    {
        return false;
    }
    if (quest && quest->id == "P007" && !cemeteryEngaged)
    {
        EnterCemetery();
    }
    else if (!CemeteryCleared())
    {
        Say("봉인된 묘역",
            cemeteryEngaged ? "울음 먹는 자가 문을 붙잡고 있다. 먼저 격파해야 한다."
                            : "안쪽에서 울음소리가 들린다. 라엔과 첫 봉인을 마치고 돌아오자.");
    }
    else
    {
        return false;
    }
    return true;
}

void Prototype::EnterCemetery()
{
    // Keep the checkpoint outside the encounter. Saving can fail without taking
    // away the in-session retry path or overwriting another slot.
    SaveProgress(false);
    ClearInput();
    controlling = false;
    levelOne.damageSpirit = -1;
    player = CemeteryRoom::Entry();
    spirit = {player.x + 36, player.y - 15};
    levelOne.arenaBoss = 1;
    levelOne.EnsureBoss(1,
                        CemeteryRoom::Boss(),
                        player,
                        [this](WorldPoint p)
                        {
                            return Blocked(p);
                        });
    const auto boss = std::find_if(levelOne.enemies.begin(),
                                   levelOne.enemies.end(),
                                   [](const LevelOne::Enemy& enemy)
                                   {
                                       return enemy.boss == 1 && enemy.health > 0;
                                   });
    if (boss == levelOne.enemies.end())
    {
        levelOne.arenaBoss = 0;
        player = CemeteryRoom::Outside();
        spirit = player;
        Say("묘역", "전투 장소를 준비하지 못했습니다. 입구에서 다시 시도하세요.");
        return;
    }
    cemeteryEngaged = true;
    cemeteryBossPosition = boss->position;
    boss->alerted = false;
    StartStoryScene(StoryScene::RoomEntrance);
}

void Prototype::RetryCemetery()
{
    if (!cemeteryEngaged || CemeteryCleared())
    {
        return;
    }
    levelOne.enemies.erase(std::remove_if(levelOne.enemies.begin(),
                                          levelOne.enemies.end(),
                                          [](const LevelOne::Enemy& enemy)
                                          {
                                              return enemy.boss == 1;
                                          }),
                           levelOne.enemies.end());
    ResetStoryEvent();
    ResetControls();
    levelOne.health = levelOne.MaximumHealth();
    levelOne.hurtTime = levelOne.hitStop = levelOne.invulnerability = 0;
    levelOne.dodgeTime = levelOne.dodgeCooldown = 0;
    levelOne.cooldown = levelOne.companionCooldown = 0;
    for (auto& member : levelOne.collection)
    {
        member.health = 100;
    }
    player = CemeteryRoom::Outside();
    spirit = player;
    controlling = false;
    dialoguePending = false;
    camera = player;
    ClearInput();
    Say("묘역 입구",
        "전투를 재정비했습니다. F로 다시 진입할 수 있습니다. 소모한 물약은 돌아오지 않습니다.");
}

void Prototype::ResetStoryEvent()
{
    storyScene = StoryScene::None;
    storySceneTime = 0;
    cemeteryBossPosition = CemeteryRoom::Boss();
    cemeteryEngaged = false;
    levelOne.arenaBoss = 0;
    levelOne.projectiles.erase(std::remove_if(levelOne.projectiles.begin(),
                                              levelOne.projectiles.end(),
                                              [](const LevelOne::Projectile& shot)
                                              {
                                                  return CemeteryRoom::Contains(shot.position, 160);
                                              }),
                               levelOne.projectiles.end());
}

void Prototype::RestoreStoryEvent()
{
    ResetStoryEvent();
    // Older snapshots may contain the road-spawned boss or stand on a new wall.
    // Preserve completed quest IDs/rewards, but restart an unfinished fight at its gate.
    if (!CemeteryCleared())
    {
        levelOne.enemies.erase(std::remove_if(levelOne.enemies.begin(),
                                              levelOne.enemies.end(),
                                              [](const LevelOne::Enemy& enemy)
                                              {
                                                  return enemy.boss == 1;
                                              }),
                               levelOne.enemies.end());
    }
    if ((!CemeteryCleared() && CemeteryRoom::Contains(player, 15)) ||
        CemeteryRoom::Blocked(player, !CemeteryCleared()))
    {
        player = CemeteryRoom::Outside();
        spirit = player;
        controlling = false;
    }
    if (CemeteryRoom::Blocked(spirit, !CemeteryCleared()))
    {
        spirit = player;
        controlling = false;
    }
}

void Prototype::StartStoryScene(StoryScene kind)
{
    if (storyScene != StoryScene::None)
    {
        return;
    }
    storyScene = kind;
    storySceneTime = 0;
    if (kind == StoryScene::RoomVictory)
    {
        for (const auto& enemy : levelOne.enemies)
        {
            if (enemy.boss == 1)
            {
                cemeteryBossPosition = enemy.position;
                break;
            }
        }
    }
    dialoguePending = chapter.choicePending = false;
    messageTime = 0;
    pauseMenu = journal = false;
    ClearInput();
    skillEffects.clear();
    levelOne.projectiles.erase(std::remove_if(levelOne.projectiles.begin(),
                                              levelOne.projectiles.end(),
                                              [](const LevelOne::Projectile& shot)
                                              {
                                                  return CemeteryRoom::Contains(shot.position, 160);
                                              }),
                               levelOne.projectiles.end());
    levelOne.hitFeedback.clear();
    levelOne.hitStop = levelOne.hurtTime = purpleShake = domainShake = 0;
    levelOne.castTime = spiritCastPose = 0;
    protagonistStopTime = .08f;
    protagonistAnimation.moving = companionAnimation.moving = false;
}

void Prototype::UpdateStoryScene(float dt)
{
    const float elapsed = (std::max)(0.f, (std::min)(dt, .05f));
    storySceneTime += elapsed;
    time += elapsed;
    UpdateCharacterPresentation(elapsed, 0, 0, 0);
    const float duration = storyScene == StoryScene::RoomEntrance  ? 3.5f
                           : storyScene == StoryScene::RoomVictory ? 8.f
                                                                   : 6.f;
    WorldPoint focus = cemeteryBossPosition;
    if (storyScene == StoryScene::RoomVictory && storySceneTime >= 2.8f)
    {
        focus = storySceneTime < 5.6f ? CemeteryRoom::Seal() : player;
    }
    else if (storyScene == StoryScene::SealDiscovery)
    {
        focus = CemeteryRoom::Seal();
    }
    const float t = 1 - std::exp(-elapsed * 2.2f);
    camera.x += (focus.x - camera.x) * t;
    camera.y += (focus.y - camera.y) * t;
    if (storySceneTime >= duration)
    {
        FinishStoryScene();
    }
}

void Prototype::FinishStoryScene()
{
    const StoryScene finished = storyScene;
    if (finished == StoryScene::None)
    {
        return;
    }
    storyScene = StoryScene::None;
    storySceneTime = 0;
    ClearInput();
    const auto* quest = chapter.Current();
    if (finished == StoryScene::RoomEntrance)
    {
        for (auto& enemy : levelOne.enemies)
        {
            if (enemy.boss == 1 && enemy.health > 0)
            {
                enemy.alerted = true;
            }
        }
        levelOne.invulnerability = .5f;
    }
    else if (finished == StoryScene::RoomVictory)
    {
        cemeteryEngaged = false;
        levelOne.arenaBoss = 0;
        if (quest && quest->id == "P007" && levelOne.resolvedBosses.count(1))
        {
            AdvanceChapter();
            chapter.rewardPending = false;
        }
        autoSaveTime = 30;
    }
    else if (finished == StoryScene::SealDiscovery && quest && quest->id == "P007_SEAL")
    {
        AdvanceChapter();
        // The fragments are permanent story evidence represented by this saved
        // completion step; they cannot be sold, consumed or granted a second time.
        autoSaveTime = 30;
    }
    camera = controlling ? spirit : player;
}

UIRect Prototype::StorySkipBounds() const
{
    const float w = (std::min)(220.f, (std::max)(1.f, float(width) - 32));
    return {width - w - 16, 16, w, 32};
}

void Prototype::DrawStorySceneOverlay()
{
    const float bar = (std::min)(72.f, height * .12f);
    const float bottom = (std::min)(132.f, height * .28f);
    r.Rect(0, 0, float(width), bar, Color(.015f, .018f, .025f, .96f));
    r.Rect(0, height - bottom, float(width), bottom, Color(.015f, .018f, .025f, .96f));
    std::string title, caption;
    if (storyScene == StoryScene::RoomEntrance)
    {
        title = "울음을 삼킨 묘역 · 울음 먹는 자";
        caption = "문이 닫혔다. 묘비 사이에서 아이들의 울음이 한꺼번에 터져 나온다.";
    }
    else if (storyScene == StoryScene::RoomVictory)
    {
        title = "끊어진 울음";
        caption = storySceneTime < 2.8f ? "괴물의 형체가 무너지자, 삼켜졌던 울음이 흩어진다."
                  : storySceneTime < 5.6f
                      ? "울음이 사라진 자리. 제단 아래에 검은 봉인 자국이 드러났다."
                      : "누군가 주령을 이곳에 묶어 두었다. 남은 흔적을 조사하자.";
    }
    else
    {
        title = "자연적으로 생긴 흔적이 아니다";
        caption = storySceneTime < 3.f ? "검은 파편에는 정교하게 새긴 봉인 문양이 남아 있다."
                                       : "특수 봉인 파편도 확보했다. 누군가의 개입을 보여 주는 "
                                         "물증이다. 벨른으로 돌아가자.";
    }
    WrappedText(24, bar + 30, float(width) - 48, title, Color(.95f, .81f, .53f), 2);
    WrappedText(28, height - bottom + 28, float(width) - 56, caption, Color(.94f, .94f, .9f), 3);
    UIButton(StorySkipBounds(), "Space / ESC · 건너뛰기");
}

void Prototype::BindCemeteryRoom()
{
    const auto b = CemeteryRoom::Bounds();
    const Point center = Screen(b.x + b.w * .5, b.y + b.d * .5);
    if (center.x < -650 || center.x > width + 650 || center.y < -450 || center.y > height + 500)
    {
        return;
    }
    scene.Ensure("cemetery", "world");
    auto& floor = scene.Ensure("cemetery/floor", "cemetery");
    floor.layer = ActorLayer::Terrain;
    floor.depthOffset = 1.e13;
    floor.draw = [this, b](const Actor&)
    {
        const Point origin = Screen(b.x, b.y);
        const std::string key = "cemetery:floor-v1";
        if (r.BeginCachedMesh(key, origin))
        {
            Ground(b.x, b.y, b.w, b.d, Color(.13f, .17f, .17f));
            for (int row = 0; row < 7; ++row)
            {
                for (int col = 0; col < 11; ++col)
                {
                    const double x = b.x + col * 40 + 1, y = b.y + row * 40 + 1;
                    const float tone = float((row * 7 + col * 3) % 5) * .012f;
                    Ground(x, y, 38, 38, Color(.23f + tone, .26f + tone, .25f + tone));
                    if ((row + col) % 4 == 0)
                    {
                        r.Line(Screen(x + 5, y + 7),
                               Screen(x + 29, y + 24),
                               1,
                               Color(.10f, .14f, .14f));
                    }
                }
            }
            r.EndCachedMesh();
        }
        r.DrawCachedMesh(key, origin);
    };
    const auto& scenery = CemeteryRoom::Scenery();
    for (size_t i = 0; i < scenery.size(); ++i)
    {
        const auto o = scenery[i];
        auto& actor = scene.Ensure("cemetery/stone/" + std::to_string(i), "cemetery");
        actor.SetWorldPosition({o.x, o.y, 0});
        actor.layer = ActorLayer::World;
        actor.footprint = {0, 0, o.w, o.d, true};
        actor.draw = [this, o, i](const Actor&)
        {
            const Point origin = Screen(o.x, o.y);
            const std::string key = "cemetery:stone-v1:" + std::to_string(i);
            if (r.BeginCachedMesh(key, origin))
            {
                if (o.kind == 2)
                {
                    Stone({2, o.x, o.y, o.w, o.d, o.h}, 1);
                }
                else
                {
                    Box(o.x,
                        o.y,
                        o.w,
                        o.d,
                        o.h,
                        Color(.35f, .38f, .37f),
                        Color(.23f, .27f, .27f),
                        Color(.16f, .20f, .21f));
                    Box(o.x - 2,
                        o.y - 2,
                        o.w + 4,
                        o.d + 4,
                        8,
                        Color(.40f, .42f, .39f),
                        Color(.29f, .31f, .30f),
                        Color(.22f, .25f, .25f));
                    for (double z = 15; z < o.h; z += 17)
                    {
                        r.Line(Screen(o.x, o.y + o.d, z),
                               Screen(o.x + o.w, o.y + o.d, z),
                               1,
                               Color(.11f, .15f, .15f));
                        for (double x = o.x + 16 + int(z / 17) % 2 * 16; x < o.x + o.w; x += 32)
                        {
                            r.Line(Screen(x, o.y + o.d, z - 14),
                                   Screen(x, o.y + o.d, z),
                                   1,
                                   Color(.13f, .17f, .17f));
                        }
                    }
                }
                r.EndCachedMesh();
            }
            r.DrawCachedMesh(key, origin);
        };
    }
    auto& gate = scene.Ensure("cemetery/gate", "cemetery");
    gate.SetWorldPosition({1120, -164, 0});
    gate.layer = ActorLayer::World;
    gate.footprint = {0, 0, 60, 8, true};
    gate.draw = [this](const Actor&)
    {
        r.Line(Screen(1116, -160, 84), Screen(1184, -160, 84), 7, Color(.30f, .23f, .19f));
        if (!CemeteryCleared())
        {
            for (int i = 0; i < 8; ++i)
            {
                const double x = 1123 + i * 7;
                r.Line(Screen(x, -160, 5), Screen(x, -160, 76), 3, Color(.24f, .20f, .20f));
            }
            r.Line(Screen(1120, -160, 24), Screen(1180, -160, 24), 4, Color(.35f, .25f, .21f));
            r.Line(Screen(1120, -160, 60), Screen(1180, -160, 60), 4, Color(.35f, .25f, .21f));
        }
    };
    auto& label = scene.Ensure("cemetery/label", "cemetery");
    label.layer = ActorLayer::WorldLabel;
    label.SetWorldPosition({CemeteryRoom::Gate().x, CemeteryRoom::Gate().y, 0});
    label.draw = [this](const Actor&)
    {
        if (Distance(player, CemeteryRoom::Gate()) < 220)
        {
            DrawWorldLabel(CemeteryRoom::Gate(), 94, "울음을 삼킨 묘역", Color(.85f, .76f, .58f));
        }
    };
    if (storyScene == StoryScene::RoomVictory && storySceneTime < 2.8f)
    {
        auto& remains = scene.Ensure<ModelActor>("cemetery/departure", "cemetery");
        remains.SetWorldPosition({cemeteryBossPosition.x, cemeteryBossPosition.y, 0});
        remains.layer = ActorLayer::Character;
        remains.footprint = {0, 0, 0, 0, true};
        remains.draw = [this](const Actor& node)
        {
            const float fade = (std::max)(0.f, 1 - storySceneTime / 2.8f);
            SpriteAnimation animation;
            animation.facing = Facing::SouthWest;
            const Point at =
                Screen(cemeteryBossPosition.x, cemeteryBossPosition.y, storySceneTime * 8);
            const Color tint(.7f, .8f, .9f, fade);
            if (!DrawSpiritModel(SpiritCatalog::Eater,
                                 at,
                                 125,
                                 animation,
                                 tint,
                                 time,
                                 -1,
                                 &static_cast<const ModelActor&>(node).modelState))
            {
                spiritSheets[SpiritCatalog::Eater].Draw(r, at, 125, animation, tint, width, height);
            }
        };
    }
}
