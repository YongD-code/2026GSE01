#include "stdafx.h"
#include "Prototype.h"

bool Prototype::CemeteryCleared() const
{
    return levelOne.resolvedBosses.count(1) != 0 || chapter.step > chapter.Index("P007");
}

bool Prototype::StoryVisible(const StoryLayout::Site& site) const
{
    if ((site.id == "gar_index" && levelOne.garChoice == 2) ||
        (site.id == "gar_external" && levelOne.garChoice != 2) ||
        (site.id == "gar_after" && levelOne.garChoice == 2))
    {
        return false;
    }
    return StoryLayout::Visible(site, chapter) && (site.id != "seal" || CemeteryCleared());
}

WorldPoint Prototype::StoryPosition(const StoryLayout::Site& site) const
{
    return site.id == "seal" ? CemeteryRoom::Seal() : site.position;
}

WorldPoint Prototype::StoryObjectivePosition(const Chapter::Quest& quest) const
{
    if (quest.id == "C205")
    {
        if (garEngaged)
        {
            for (const auto& enemy : levelOne.enemies)
                if (enemy.boss == StoryEncounter::Gar && enemy.health > 0)
                    return enemy.position;
            return GarCourt::Boss;
        }
        return GarCourt::Gate;
    }
    if (quest.id == "C207")
    {
        return GarRecordPosition();
    }
    if (quest.id == "C203")
    {
        if (!(levelOne.shrineFrames & 1))
        {
            return StoryLayout::ShrineFrameA;
        }
        if (!(levelOne.shrineFrames & 2))
        {
            return StoryLayout::ShrineFrameB;
        }
        for (const auto& enemy : levelOne.enemies)
        {
            if (enemy.boss == StoryEncounter::ShrineGuard && enemy.health > 0)
            {
                return enemy.position;
            }
        }
    }
    if (quest.id == "C203" || quest.id == "C203_RESULT")
    {
        for (const auto& enemy : levelOne.enemies)
        {
            if (enemy.boss == StoryEncounter::BellWraith && enemy.health > 0)
            {
                return enemy.position;
            }
        }
    }
    if (quest.id == "P007")
    {
        return cemeteryEngaged ? CemeteryRoom::Boss() : CemeteryRoom::Gate();
    }
    if (quest.id == "P007_SEAL")
    {
        return CemeteryRoom::Seal();
    }
    if (quest.id == "C201")
    {
        const LevelOne::Enemy* closest = nullptr;
        const WorldPoint focus = controlling ? spirit : player;
        for (const auto& enemy : levelOne.enemies)
        {
            if (enemy.health > 0 && StoryEncounter::Get(enemy.boss).humanAttack >= 0 &&
                (!closest || Distance(focus, enemy.position) < Distance(focus, closest->position)))
            {
                closest = &enemy;
            }
        }
        return closest ? closest->position : quest.destination;
    }
    return quest.trigger == "skill" ? player : quest.destination;
}

std::string Prototype::StoryObjectiveText(const Chapter::Quest& quest) const
{
    if (quest.id == "C205")
        return garEngaged ? "가르 체력을 10%까지 낮추기" : "결투장 입구에서 F로 진입하기";
    if (quest.id == "C207")
    {
        if (!(levelOne.garRecords & 1))
            return levelOne.garChoice == 2 ? "외부 인계실 사본에서 운송 번호 찾기"
                                           : "전초 기록소 인계 색인에서 운송 번호 찾기";
        if (!(levelOne.garRecords & 2))
            return "수도원 장부의 운송 번호와 대조하기";
        return "도시행 운송대장에서 이송자 특징 확인하기";
    }
    if (quest.id == "C203")
    {
        if (!(levelOne.shrineFrames & 1))
        {
            return "서쪽 금빛 봉인틀에서 F로 연결 끊기";
        }
        if (!(levelOne.shrineFrames & 2))
        {
            return "동쪽 금빛 봉인틀에서 F로 연결 끊기";
        }
        if (!levelOne.resolvedBosses.count(StoryEncounter::ShrineGuard))
        {
            return "성흔회 포획기사 제압하기";
        }
        return "종을 듣는 자의 체력을 20% 이하로 낮추기";
    }
    if (quest.id == "C203_RESULT" && controlling)
    {
        return "B로 본체에 돌아와 종 주령 가까이에서 F";
    }
    if (quest.id == "P007")
    {
        return cemeteryEngaged ? "묘역 안의 울음 먹는 자를 격파하기"
                               : "공동묘지 북쪽 봉인문에서 F로 묘역에 진입하기";
    }
    if (quest.id == "P007_SEAL")
    {
        return "묘역 북동쪽 석벽의 봉인 흔적을 조사해 두 파편 확보하기";
    }
    if (quest.id == "C108" && chapter.saintSealUnderstood)
    {
        return "성자 격파 · 기도 중 빛나는 봉인은 F 또는 기억의 결박으로 파훼";
    }
    return quest.objective;
}

int Prototype::DefeatedOrderGuards() const
{
    int defeated = 0;
    for (const auto& guard : StoryLayout::OrderGuards)
    {
        defeated += levelOne.resolvedBosses.count(guard.encounter) != 0;
    }
    return defeated;
}

bool Prototype::UpdateMonasteryEscape(const Chapter::Quest& quest)
{
    if (quest.id == "C200")
    {
        if (chapter.rewardPending)
        {
            return false;
        }
        if (Distance(player, quest.destination) < 85)
        {
            AdvanceChapter();
            autoSaveTime = 30;
        }
        return true;
    }
    if (quest.id == "C201_RAEN" || quest.id == "C201_LEDGER")
    {
        chapter.rewardPending = false;
        return true;
    }
    if (quest.id != "C201")
    {
        return false;
    }
    if (chapter.rewardPending)
    {
        chapter.rewardPending = false;
        Say(quest.title, quest.dialogue);
    }
    if (DefeatedOrderGuards() == int(StoryLayout::OrderGuards.size()))
    {
        AdvanceChapter();
        autoSaveTime = 30;
        return true;
    }
    if (Distance(player, quest.destination) < 380)
    {
        for (const auto& guard : StoryLayout::OrderGuards)
        {
            levelOne.EnsureBoss(guard.encounter,
                                guard.position,
                                player,
                                [this](WorldPoint p)
                                {
                                    return Blocked(p);
                                });
        }
    }
    return true;
}

int Prototype::ShrineProgress() const
{
    int progress = (levelOne.shrineFrames & 1 ? 1 : 0) + (levelOne.shrineFrames & 2 ? 1 : 0);
    progress += levelOne.resolvedBosses.count(StoryEncounter::ShrineGuard) != 0;
    for (const auto& enemy : levelOne.enemies)
    {
        if (enemy.boss == StoryEncounter::BellWraith && enemy.weakened && enemy.health > 0)
        {
            ++progress;
        }
    }
    return progress;
}

bool Prototype::UpdateShrine(const Chapter::Quest& quest)
{
    if (quest.id == "C203_RESULT" || quest.id == "C203_LEDGER")
    {
        chapter.rewardPending = false;
        return true;
    }
    if (quest.id != "C203")
    {
        return false;
    }
    if (chapter.rewardPending)
    {
        chapter.rewardPending = false;
        // A safe snapshot immediately before the authored actors enter combat.
        // If disk saving fails, the existing slot is preserved.
        SaveProgress(false);
    }
    if (Distance(player, StoryLayout::ShrineBell) < 300)
    {
        // Reuse the authored encounter actors. No periodic wave spawning.
        auto blocked = [this](WorldPoint p)
        {
            return Blocked(p);
        };
        levelOne.EnsureBoss(
            StoryEncounter::BellWraith, StoryLayout::ShrineBell, StoryLayout::ShrineGate, blocked);
        levelOne.EnsureBoss(StoryEncounter::ShrineGuard,
                            StoryLayout::ShrineGuard,
                            StoryLayout::ShrineGate,
                            blocked);
    }
    levelOne.RefreshShrineWeakness();
    if (ShrineProgress() == 4)
    {
        AdvanceChapter();
        Say("잔향의 선택",
            "포획기사가 쓰러지고 종소리가 멎었다. 주령 가까이에서 F로 처분을 정하자.");
        autoSaveTime = 30;
    }
    return true;
}

bool Prototype::ShrineChoiceOpen() const
{
    const auto* quest = chapter.Current();
    return chapter.choicePending && quest && quest->id == "C203_RESULT";
}

bool Prototype::NearShrineSpirit() const
{
    const auto* quest = chapter.Current();
    if (controlling || !quest || quest->id != "C203_RESULT")
    {
        return false;
    }
    for (const auto& enemy : levelOne.enemies)
    {
        if (enemy.boss == StoryEncounter::BellWraith && enemy.weakened && enemy.health > 0 &&
            Distance(player, enemy.position) < 110)
        {
            return true;
        }
    }
    return false;
}

bool Prototype::InteractShrine()
{
    const auto* quest = chapter.Current();
    if (!quest || controlling)
    {
        return false;
    }
    if (quest->id == "C203")
    {
        const WorldPoint frames[] = {StoryLayout::ShrineFrameA, StoryLayout::ShrineFrameB};
        for (int i = 0; i < 2; ++i)
        {
            if (Distance(player, frames[i]) < 62 && !(levelOne.shrineFrames & (1 << i)))
            {
                levelOne.shrineFrames |= 1 << i;
                levelOne.RefreshShrineWeakness();
                Say("포획 봉인틀",
                    levelOne.shrineFrames == 3 ? "두 연결이 끊겼다. 종 주령을 약화할 수 있다."
                                               : "연결 하나가 끊겼다. 다른 봉인틀도 해체하자.");
                return true;
            }
        }
    }
    if (NearShrineSpirit())
    {
        ClearInput();
        levelOne.notice.clear();
        chapter.choicePending = true;
        return true;
    }
    return false;
}

void Prototype::ChooseShrine(int disposition)
{
    if (!ShrineChoiceOpen())
    {
        return;
    }
    if (disposition == 4)
    {
        chapter.choicePending = false;
        levelOne.notice.clear();
        ClearInput();
        return;
    }
    if (!NearShrineSpirit() || !levelOne.DisposeShrineSpirit(disposition))
    {
        return;
    }
    chapter.choicePending = false;
    ClearInput();
    levelOne.notice.clear();
    const char* results[] = {"",
                             "종 주령을 봉인했다. 도감의 목 없는 순례자 개체 기록에서 확인하자.",
                             "종에 묶인 원념을 풀어 주령을 정화했다.",
                             "성흔회 연락병에게 주령의 봉인을 인계했다."};
    Say("잔향의 결말", std::string(results[disposition]) + " 종 받침에 남은 인계표를 조사하자.");
    dialoguePending = true;
    AdvanceChapter();
    autoSaveTime = 30;
}

WorldPoint Prototype::GarRecordPosition() const
{
    if (!(levelOne.garRecords & 1))
        return levelOne.garChoice == 2 ? StoryLayout::GarExternal : StoryLayout::GarIndex;
    if (!(levelOne.garRecords & 2))
        return StoryLayout::GarLedger;
    return StoryLayout::GarCopy;
}

bool Prototype::GarChoiceOpen() const
{
    const auto* quest = chapter.Current();
    return chapter.choicePending && quest && quest->id == "C206";
}

bool Prototype::UpdateGar(const Chapter::Quest& quest)
{
    if (quest.id == "C205")
    {
        chapter.rewardPending = false;
        if (levelOne.resolvedBosses.count(StoryEncounter::Gar))
            FinishGarDuel();
        return true;
    }
    if (quest.id == "C206" || quest.id == "C207" || quest.id == "C208")
    {
        chapter.rewardPending = false;
        return true;
    }
    return false;
}

bool Prototype::InteractGar()
{
    const auto* quest = chapter.Current();
    if (!quest || controlling)
        return false;
    if (quest->id == "C205" && !garEngaged && Distance(player, GarCourt::Gate) < 85)
    {
        EnterGar();
        return true;
    }
    if (quest->id == "C206" && Distance(player, StoryLayout::GarPost) < 90)
    {
        ClearInput();
        chapter.choicePending = true;
        return true;
    }
    if (quest->id == "C207" && Distance(player, GarRecordPosition()) < 62)
    {
        const int before = levelOne.garRecords;
        levelOne.garRecords = before == 0 ? 1 : before == 1 ? 3 : 7;
        ClearInput();
        if (before == 0)
            Say("운송 번호",
                levelOne.garChoice == 2
                    ? "후송 인력이 남긴 외부 인계실의 사본에서 사당의 번호와 같은 항목을 찾았다."
                    : "가르가 알려 준 색인에서 사당의 운송 번호를 찾았다.");
        else if (before == 1)
            Say("수도원 장부",
                "번호는 폐수문 중계 창고의 장부와 일치한다. 이어지는 목적지를 확인하자.");
        else
        {
            Say("도시행 운송대장",
                "최근 이송자 기록에 동생의 신체 특징과 일치하는 항목이 있다. 목적지는 죽은 자의 "
                "도시다. 아직 이름은 확인할 수 없다.");
            AdvanceChapter();
        }
        dialoguePending = true;
        autoSaveTime = 30;
        return true;
    }
    if (quest->id == "C208" && Distance(player, StoryLayout::CityGate) < 75)
    {
        StartStoryScene(StoryScene::CityArrival);
        return true;
    }
    return false;
}

void Prototype::ChooseGar(int choice)
{
    if (!GarChoiceOpen() || choice < 1 || choice > 4)
        return;
    chapter.choicePending = false;
    ClearInput();
    if (choice == 4)
        return;
    levelOne.garChoice = choice;
    if (choice == 2)
    {
        AdvanceChapter();
        StartStoryScene(StoryScene::GarEscort);
        return;
    }
    const char* lines[] = {"",
                           "가르: 네가 살려둘 줄은 몰랐다. 일곱 번째 종이 울리면 죽은 자와 산 자의 "
                           "경계가 무너진다. 전초 기록소를 열어 주겠다.",
                           "",
                           "가르: 일곱 번째 종이 울리면 경계가 무너진다. 네 말을 전부 믿지는 "
                           "않겠지만 기록소에서 증거를 확인해라."};
    Say("결투의 결과", lines[choice]);
    dialoguePending = true;
    AdvanceChapter();
    autoSaveTime = 30;
}

void Prototype::EnterGar()
{
    SaveProgress(false);
    ClearInput();
    controlling = false;
    levelOne.damageSpirit = -1;
    player = GarCourt::Entry;
    spirit = {player.x + 30, player.y - 20};
    // Ordinary roaming combatants are not part of the duel. Return them to their homes.
    for (auto& enemy : levelOne.enemies)
    {
        if (enemy.boss == 0 && !enemy.weakened && GarCourt::Contains(enemy.position, 20))
        {
            enemy.position = enemy.home;
            enemy.alerted = enemy.returning = false;
            enemy.phase = 0;
            enemy.trail.assign(1, enemy.home);
        }
    }
    garEngaged = true;
    levelOne.arenaBoss = StoryEncounter::Gar;
    levelOne.EnsureBoss(StoryEncounter::Gar,
                        GarCourt::Boss,
                        player,
                        [this](WorldPoint p)
                        {
                            return Blocked(p);
                        });
    auto* gar = levelOne.ActiveStoryEnemy(StoryEncounter::Gar);
    if (!gar)
    {
        garEngaged = false;
        levelOne.arenaBoss = 0;
        player = spirit = GarCourt::Outside;
        Say("결투장", "전투 장소를 준비하지 못했습니다. 입구에서 다시 시도하세요.");
        return;
    }
    gar->alerted = true;
    StartStoryScene(StoryScene::GarEntrance);
}

void Prototype::FinishGarDuel()
{
    const auto* quest = chapter.Current();
    if (!quest || quest->id != "C205" || !levelOne.resolvedBosses.count(StoryEncounter::Gar))
        return;
    garEngaged = false;
    levelOne.arenaBoss = 0;
    levelOne.orderSeal = {};
    levelOne.health = (std::max)(1.f, levelOne.health);
    controlling = false;
    levelOne.damageSpirit = -1;
    spirit = player;
    skillEffects.clear();
    levelOne.projectiles.erase(std::remove_if(levelOne.projectiles.begin(),
                                              levelOne.projectiles.end(),
                                              [](const LevelOne::Projectile& shot)
                                              {
                                                  return GarCourt::Contains(shot.position, 120);
                                              }),
                               levelOne.projectiles.end());
    ClearInput();
    AdvanceChapter();
    Say("가르", "됐다. 널 실험자의 편이라고 단정할 수는 없겠군. 검을 내려놓겠다.");
    dialoguePending = true;
    autoSaveTime = 30;
}

void Prototype::RetryGar()
{
    if (!garEngaged || levelOne.resolvedBosses.count(StoryEncounter::Gar))
        return;
    garEngaged = false;
    levelOne.arenaBoss = 0;
    levelOne.orderSeal = {};
    levelOne.enemies.erase(std::remove_if(levelOne.enemies.begin(),
                                          levelOne.enemies.end(),
                                          [](const LevelOne::Enemy& enemy)
                                          {
                                              return enemy.boss == StoryEncounter::Gar;
                                          }),
                           levelOne.enemies.end());
    levelOne.projectiles.erase(std::remove_if(levelOne.projectiles.begin(),
                                              levelOne.projectiles.end(),
                                              [](const LevelOne::Projectile& shot)
                                              {
                                                  return GarCourt::Contains(shot.position, 120);
                                              }),
                               levelOne.projectiles.end());
    ResetControls();
    levelOne.health = levelOne.MaximumHealth();
    levelOne.hurtTime = levelOne.invulnerability = levelOne.dodgeTime = levelOne.dodgeCooldown = 0;
    levelOne.cooldown = levelOne.companionCooldown = 0;
    levelOne.damageSpirit = -1;
    for (auto& member : levelOne.collection)
        member.health = 100;
    controlling = false;
    player = spirit = camera = GarCourt::Outside;
    dialoguePending = false;
    ClearInput();
    Say("결투장 입구", "F로 다시 도전하세요. 사용한 물약은 돌아오지 않습니다.");
}

bool Prototype::CanCompareSaintSeal() const
{
    return !chapter.saintSealUnderstood && chapter.step > chapter.Index("C106_RECORD") &&
           chapter.step <= chapter.Index("C108") &&
           levelOne.resolvedBosses.count(StoryEncounter::SaintBoss) == 0;
}

void Prototype::CompareSaintSeal()
{
    chapter.saintSealUnderstood = true;
    tutorials.Discover(Tutorials::SaintSeal);
    ClearInput();
    Say("결박 문양 대조",
        "장부의 봉인 문양이 감금실의 결박과 일치한다. 장부 여백에는 ‘기도가 시작되면 "
        "예배당 앞 봉인으로 주력을 보낼 것’이라고 적혀 있다.\n"
        "빛나는 봉인에 접근해 F로 끊거나, 남겨진 자를 조종해 봉인에 W 기억의 결박을 써 보자.");
    dialoguePending = true;
    autoSaveTime = 30;
}

LevelOne::Enemy* Prototype::SaintAtSeal()
{
    const auto* quest = chapter.Current();
    if (!chapter.saintSealUnderstood || !quest || quest->id != "C108")
    {
        return nullptr;
    }
    const auto saint =
        std::find_if(levelOne.enemies.begin(),
                     levelOne.enemies.end(),
                     [](const LevelOne::Enemy& enemy)
                     {
                         return enemy.boss == StoryEncounter::SaintBoss && enemy.health > 0 &&
                                !enemy.weakened &&
                                Distance(enemy.position, StoryLayout::SaintSeal) <= 360;
                     });
    return saint == levelOne.enemies.end() ? nullptr : &*saint;
}

LevelOne::Enemy* Prototype::NearbySaintSeal()
{
    if (controlling ||
        Distance(player, StoryLayout::SaintSeal) > StoryLayout::SaintSealInteractRange)
    {
        return nullptr;
    }
    auto* saint = SaintAtSeal();
    return saint && !saint->saintSealBroken ? saint : nullptr;
}

void Prototype::BreakSaintSeal(LevelOne::Enemy& saint, WorldPoint source)
{
    if (!saint.SaintSealExposed() || !LevelOne::ClearLine(source,
                                                          StoryLayout::SaintSeal,
                                                          [this](WorldPoint p)
                                                          {
                                                              return Blocked(p);
                                                          }))
    {
        return;
    }
    saint.saintSealBroken = true;
    saint.phase = LevelOne::SaintSealRecoveryPhase;
    saint.phaseTime = LevelOne::SaintSealRecovery;
    Say("봉인 파훼", "성자의 기도가 끊겼다. 지금 공격하면 더 큰 피해를 줄 수 있다.");
    messageTime = LevelOne::SaintSealRecovery;
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
    garEngaged = false;
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
    if (kind == StoryScene::GarEntrance || kind == StoryScene::GarEscort ||
        kind == StoryScene::CityArrival)
    {
        levelOne.projectiles.erase(
            std::remove_if(levelOne.projectiles.begin(),
                           levelOne.projectiles.end(),
                           [kind](const LevelOne::Projectile& shot)
                           {
                               return kind == StoryScene::CityArrival
                                          ? Distance(shot.position, StoryLayout::CityGate) < 420
                                          : GarCourt::Contains(shot.position, 120);
                           }),
            levelOne.projectiles.end());
    }
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
                           : storyScene == StoryScene::GarEntrance ? 5.f
                           : storyScene == StoryScene::GarEscort   ? 4.5f
                           : storyScene == StoryScene::CityArrival ? 9.f
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
    else if (storyScene == StoryScene::GarEntrance)
    {
        focus = GarCourt::Boss;
    }
    else if (storyScene == StoryScene::GarEscort)
    {
        focus = {StoryLayout::GarPost.x + (std::min)(1.f, storySceneTime / 4.5f) * 210,
                 StoryLayout::GarPost.y};
    }
    else if (storyScene == StoryScene::CityArrival)
    {
        focus = storySceneTime < 3.f ? StoryLayout::CityGate : StoryLayout::CitySquare;
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
    else if (finished == StoryScene::GarEntrance)
    {
        levelOne.invulnerability = .5f;
    }
    else if (finished == StoryScene::GarEscort)
    {
        Say("결투의 결과",
            "라엔: 성흔회가 가르를 후송했어. 떨어진 조사 문서에 일곱 번째 종의 경고와 외부 인계실 "
            "위치가 적혀 있다.");
        dialoguePending = true;
        autoSaveTime = 30;
    }
    else if (finished == StoryScene::CityArrival && quest && quest->id == "C208")
    {
        player = StoryLayout::CitySquare;
        spirit = {player.x + 32, player.y + 20};
        levelOne.tickets[1] += 2;
        ++levelOne.stones;
        levelOne.gold += 35;
        AdvanceChapter();
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
                      ? "울음이 사라진 자리. 석벽에 검은 봉인 자국이 드러났다."
                      : "누군가 주령을 이곳에 묶어 두었다. 남은 흔적을 조사하자.";
    }
    else if (storyScene == StoryScene::GarEntrance)
    {
        title = "성흔기사 가르";
        caption =
            storySceneTime < 2.f
                ? "가르: 주령을 부리는 힘에 휘둘리는 자인지, 네 검으로 증명해 보아라."
                : "넓은 검술과 직선 돌진을 피하자. 금빛 봉인 표식에서는 동행 주령을 이동시키자.";
    }
    else if (storyScene == StoryScene::GarEscort)
    {
        title = "성흔회의 개입";
        caption = storySceneTime < 2.f
                      ? "마지막 일격을 가하려는 순간, 성흔회 지원병이 앞을 막아선다."
                      : "지원병은 부상당한 가르를 데리고 기록소 쪽으로 물러난다.";
    }
    else if (storyScene == StoryScene::CityArrival)
    {
        title = "2장 완료 · 사냥당하는 주술사";
        caption = storySceneTime < 3.f ? "운송대장의 번호가 마침내 이 성문을 가리킨다."
                  : storySceneTime < 6.f
                      ? "종이 울리고, 성문 너머의 주령들은 싸울 기색 없이 서 있다."
                      : "라엔: 여기, 백 년 전에 끝났어야 할 도시야.";
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

void Prototype::BindGarCourt()
{
    const auto b = GarCourt::Bounds;
    const auto center = Screen(b.x + b.w / 2, b.y + b.d / 2);
    if (center.x < -400 || center.x > width + 400 || center.y < -300 || center.y > height + 300)
        return;
    scene.Ensure("gar-court", "world");
    auto& floor = scene.Ensure("gar-court/floor", "gar-court");
    floor.layer = ActorLayer::Terrain;
    floor.depthOffset = 1.e13;
    floor.draw = [this, b](const Actor&)
    {
        const Point origin = Screen(b.x, b.y);
        const std::string key = "gar-court-floor-v1";
        if (r.BeginCachedMesh(key, origin))
        {
            Ground(b.x, b.y, b.w, b.d, Color(.21f, .19f, .15f));
            for (int row = 0; row < 7; ++row)
                for (int col = 0; col < 11; ++col)
                    Ground(b.x + 10 + col * 40,
                           b.y + 3 + row * 39,
                           38,
                           37,
                           Color(.36f + (col % 3) * .025f, .32f, .24f));
            const WorldPoint corners[] = {
                {b.x, b.y}, {b.x + b.w, b.y}, {b.x + b.w, b.y + b.d}, {b.x, b.y + b.d}};
            for (int i = 0; i < 4; ++i)
                r.Line(Screen(corners[i].x, corners[i].y, 1),
                       Screen(corners[(i + 1) % 4].x, corners[(i + 1) % 4].y, 1),
                       4,
                       Color(.64f, .53f, .32f));
            r.EndCachedMesh();
        }
        r.DrawCachedMesh(key, origin);
    };
    if (garEngaged)
    {
        auto& barrier = scene.Ensure("gar-court/barrier", "gar-court");
        barrier.layer = ActorLayer::Atmosphere;
        barrier.draw = [this, b](const Actor&)
        {
            const WorldPoint corners[] = {
                {b.x, b.y}, {b.x + b.w, b.y}, {b.x + b.w, b.y + b.d}, {b.x, b.y + b.d}};
            for (int i = 0; i < 4; ++i)
            {
                const auto a = corners[i], c = corners[(i + 1) % 4];
                r.Quad(Screen(a.x, a.y),
                       Screen(c.x, c.y),
                       Screen(c.x, c.y, 45),
                       Screen(a.x, a.y, 45),
                       Color(.78f, .51f, .16f, .12f));
                r.Line(Screen(a.x, a.y, 3), Screen(c.x, c.y, 3), 3, Color(.94f, .74f, .35f, .8f));
            }
        };
    }
    if (storyScene == StoryScene::GarEscort)
    {
        for (int i = 0; i < 3; ++i)
        {
            const float t = (std::max)(0.f, (std::min)(1.f, (storySceneTime - .8f) / 3.7f));
            const WorldPoint at = {StoryLayout::GarPost.x + 210 * t + (i == 0 ? 0 : 18),
                                   StoryLayout::GarPost.y + (i == 1   ? -26
                                                             : i == 2 ? 26
                                                                      : 0)};
            auto& actor =
                scene.Ensure<ModelActor>("gar-court/escort/" + std::to_string(i), "gar-court");
            actor.SetWorldPosition({at.x, at.y, 0});
            actor.layer = ActorLayer::Character;
            actor.footprint = {0, 0, 0, 0, true};
            actor.draw = [this, i](const Actor& node)
            {
                const auto at = node.WorldPosition();
                const auto feet = Screen(at.x, at.y);
                SpriteAnimation walk;
                walk.UpdateWorld(1, 0, 0, true, 1);
                walk.moving = storySceneTime > .8f;
                walk.phase = std::fmod(storySceneTime * 9.f, 8.f);
                const CharacterModel::Parts parts = i == 0   ? CharacterModel::Parts{1, 5, 3, 6}
                                                    : i == 1 ? CharacterModel::Parts{0, 0, 3, 6}
                                                             : CharacterModel::Parts{1, 6, 4, 1};
                r.Ellipse(feet.x, feet.y, 18, 6, Color(0, 0, 0, .3f));
                if (!DrawKeyedModel(npcModel,
                                    feet,
                                    i == 0 ? 94.f : 78.f,
                                    walk,
                                    Color(1, 1, 1),
                                    walk.moving ? CharacterModel::Motion::Walk
                                                : CharacterModel::Motion::Idle,
                                    walk.phase / 8.f,
                                    &static_cast<const ModelActor&>(node).modelState,
                                    &parts))
                    populationWalkSheets[Population::Hunter].Draw(
                        r, feet, i == 0 ? 94.f : 78.f, walk, Color(1, 1, 1), width, height);
            };
        }
    }
}

void Prototype::BindCityEntrance()
{
    if (chapter.step < chapter.Index("C208"))
        return;
    const auto plaza = Screen(StoryLayout::CitySquare.x, StoryLayout::CitySquare.y);
    if (plaza.x < -350 || plaza.x > width + 350 || plaza.y < -300 || plaza.y > height + 300)
        return;
    scene.Ensure("city-preview", "world");
    auto& floor = scene.Ensure("city-preview/floor", "city-preview");
    floor.layer = ActorLayer::Terrain;
    floor.depthOffset = 1.e13;
    floor.draw = [this](const Actor&)
    {
        const double x = StoryLayout::CitySquare.x - 170, y = StoryLayout::CitySquare.y - 70;
        const Point origin = Screen(x, y);
        const std::string key = "city-gateway-floor-v1";
        if (r.BeginCachedMesh(key, origin))
        {
            Ground(x, y, 340, 250, Color(.13f, .17f, .18f));
            for (int row = 0; row < 6; ++row)
                for (int col = 0; col < 8; ++col)
                {
                    const float tone = float((col * 7 + row * 3) % 5) * .012f;
                    Ground(x + 10 + col * 40,
                           y + 5 + row * 40,
                           38,
                           38,
                           Color(.25f + tone, .30f + tone, .31f + tone));
                }
            r.EndCachedMesh();
        }
        r.DrawCachedMesh(key, origin);
    };
    if (storyScene == StoryScene::CityArrival && storySceneTime > 1.5f && storySceneTime < 6.f)
    {
        auto& sound = scene.Ensure("city-preview/bell-echo", "city-preview");
        sound.layer = ActorLayer::Atmosphere;
        sound.draw = [this](const Actor&)
        {
            const auto at = StoryLayout::CityGate;
            const float phase = std::fmod((storySceneTime - 1.5f) * .7f, 1.f);
            for (int ring = 0; ring < 2; ++ring)
            {
                const float t = std::fmod(phase + ring * .5f, 1.f);
                const double radius = 28 + 125 * t;
                for (int i = 0; i < 24; ++i)
                {
                    const double a = i * 6.2831853 / 24, b = (i + 1) * 6.2831853 / 24;
                    r.Line(Screen(at.x + std::cos(a) * radius, at.y + std::sin(a) * radius, 8),
                           Screen(at.x + std::cos(b) * radius, at.y + std::sin(b) * radius, 8),
                           2,
                           Color(.66f, .85f, .93f, (1 - t) * .38f));
                }
            }
        };
    }
    const WorldPoint sites[] = {{2270, -640}, {2430, -645}};
    const int species[] = {SpiritCatalog::Forgotten, SpiritCatalog::Pilgrim};
    for (int i = 0; i < 2; ++i)
    {
        const auto at = sites[i];
        if (!CharacterInView(at, 96))
            continue;
        scene.Ensure("city-preview", "world");
        auto& actor = scene.Ensure<ModelActor>("city-preview/" + std::to_string(i), "city-preview");
        actor.SetWorldPosition({at.x, at.y, 0});
        actor.layer = ActorLayer::Character;
        actor.footprint = {0, 0, 0, 0, true};
        const int kind = species[i];
        actor.draw = [this, kind](const Actor& node)
        {
            const auto pos = node.WorldPosition();
            const auto feet = Screen(pos.x, pos.y);
            SpriteAnimation idle;
            idle.facing = Facing::SouthWest;
            if (!DrawSpiritModel(kind,
                                 feet,
                                 76,
                                 idle,
                                 Color(.85f, 1.f, .95f),
                                 time,
                                 -1,
                                 &static_cast<const ModelActor&>(node).modelState))
                spiritSheets[kind].Draw(r, feet, 76, idle, Color(.85f, 1.f, .95f), width, height);
        };
    }
}

void Prototype::BindCemeteryRoom()
{
    const auto b = CemeteryRoom::Bounds();
    const Point center = Screen(b.x + b.w * .5, b.y + b.d * .5);
    const double horizontal = (b.w + b.d) * .425 + 80;
    const double vertical = (b.w + b.d) * .215;
    if (center.x + horizontal < -96 || center.x - horizontal > width + 96 ||
        center.y + vertical < -96 || center.y - vertical - 130 > height + 96)
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
        const std::string key = "cemetery:floor-v2";
        if (r.BeginCachedMesh(key, origin))
        {
            Ground(b.x, b.y, b.w, b.d, Color(.13f, .17f, .17f));
            for (int row = 0; row < int(b.d / 40); ++row)
            {
                for (int col = 0; col < int(b.w / 40); ++col)
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
            const std::string key = "cemetery:stone-v2:" + std::to_string(i);
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
    const auto opening = CemeteryRoom::GateFootprint();
    auto& gate = scene.Ensure("cemetery/gate", "cemetery");
    gate.SetWorldPosition({opening.x, opening.y, 0});
    gate.layer = ActorLayer::World;
    gate.footprint = {0, 0, opening.w, opening.d, true};
    gate.draw = [this, opening](const Actor&)
    {
        const double y = opening.y + opening.d * .5;
        r.Line(Screen(opening.x - 4, y, opening.h),
               Screen(opening.x + opening.w + 4, y, opening.h),
               7,
               Color(.30f, .23f, .19f));
        if (!CemeteryCleared())
        {
            for (int i = 0; i < 14; ++i)
            {
                const double x = opening.x + 3 + i * (opening.w - 6) / 13;
                r.Line(Screen(x, y, 5), Screen(x, y, opening.h - 8), 3, Color(.24f, .20f, .20f));
            }
            r.Line(Screen(opening.x, y, 24),
                   Screen(opening.x + opening.w, y, 24),
                   4,
                   Color(.35f, .25f, .21f));
            r.Line(Screen(opening.x, y, 60),
                   Screen(opening.x + opening.w, y, 60),
                   4,
                   Color(.35f, .25f, .21f));
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
