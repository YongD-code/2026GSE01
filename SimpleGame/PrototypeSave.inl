// Included inside Prototype. Disk state is separate from rendering and input state.
bool CanSaveProgress() const
{
    if (titleScreen || openingPlaying || levelOne.Dead() || chapter.quests.empty() ||
        levelOne.dodgeTime > 0 || levelOne.invulnerability > 0 || !skillEffects.empty())
    {
        return false;
    }
    for (const auto& shot : levelOne.projectiles)
        if (!shot.residentBattle || Distance(player, shot.position) < 700)
            return false;
    for (const auto& enemy : levelOne.enemies)
    {
        if (enemy.health > 0 && (enemy.alerted || enemy.returning || enemy.phase != 0) &&
            !enemy.weakened && (enemy.boss != 0 || Distance(player, enemy.position) < 700))
        {
            return false;
        }
    }
    return true;
}

void ProgressNotice(const std::string& text)
{
    saveStatus = text;
    saveStatusTime = 6;
}

bool SaveProgress(bool manual, int slot = 0)
{
    if (slot == 0)
        slot = activeSlot;
    if (slot < 1 || slot > 3)
        return false;
    if (!manual && !automaticSaveAllowed)
    {
        return false;
    }
    if (!CanSaveProgress())
    {
        if (manual)
        {
            ProgressNotice("전투·귀환·사망 중에는 저장할 수 없습니다.");
        }
        return false;
    }
    SaveGame::State state;
    state.seed = world.seed;
    state.story = SaveGame::StoryHash(chapter);
    state.player = player;
    state.spirit = spirit;
    state.keeper = keeper;
    state.level = levelOne;
    state.residents = population.people;
    state.society = population.society;
    for (const auto& result : levelOne.residentCombatants)
        if (result.id >= 0 && result.id < int(state.residents.size()))
            state.residents[result.id].health = result.health;
    state.spiritEquipped = controlling;
    for (int set = 0; set < 5; ++set)
    {
        for (int slot = 0; slot < 4; ++slot)
        {
            state.skillTimers[set][slot] = skillTimers[set][slot];
        }
    }
    state.chapter = chapter;
    state.companion = selectedCompanion;
    state.facing = static_cast<int>(protagonistAnimation.facing);
    state.controlling = controlling;
    state.dialogue = dialoguePending;
    state.speaker = speaker;
    state.message = message;
    state.messageTime = messageTime;
    state.hud = hudVisible;
    state.shake = screenShake;
    state.post = r.postProcess.enabled;
    state.bloom = r.postProcess.bloomEnabled;
    auto& enemies = state.level.enemies;
    enemies.erase(std::remove_if(enemies.begin(),
                                 enemies.end(),
                                 [](const LevelOne::Enemy& enemy)
                                 {
                                     return enemy.health <= 0;
                                 }),
                  enemies.end());
    for (const auto& enemy : enemies)
    {
        if (enemy.boss == 0 && !enemy.weakened)
        {
            state.level.camps[enemy.camp].health[enemy.slot] = enemy.health;
        }
    }
    if (!SaveGame::Write(std::move(state), chapter, slot))
    {
        automaticSaveAllowed = false;
        ProgressNotice("저장 실패 · 기존 기록 유지. 경로·용량을 확인한 뒤 F5로 재시도하세요.");
        return false;
    }
    automaticSaveAllowed = true;
    activeSlot = slot;
    autoSaveTime = 0;
    ProgressNotice(manual ? "진행을 저장했습니다." : "진행을 자동 저장했습니다.");
    return true;
}

void RefreshSavedStoryDialogue()
{
    if (chapter.choicePending)
    {
        Say("마을 아이", "아이에게 동생의 행방을 물어볼까요?");
        return;
    }
    if (!dialoguePending)
        return;
    const auto* current = chapter.Current();
    if (current &&
        (speaker == current->title || (current->id == "P005" && speaker == "마지막 일격 대신") ||
         (current->id == "P006" && speaker == "붙잡는 손")))
    {
        speaker = current->title;
        const bool battle = current->trigger == "fight" || current->trigger == "first_capture" ||
                            current->trigger == "survive" || current->trigger == "skill";
        message = battle ? current->dialogue : current->objective;
        return;
    }
    if (chapter.step <= 0 || chapter.step > int(chapter.quests.size()))
        return;
    const auto& previous = chapter.quests[chapter.step - 1];
    for (const auto& site : StoryLayout::Sites())
    {
        if (speaker == site.name &&
            (previous.trigger == "talk:" + site.id || previous.trigger == "inspect:" + site.id ||
             previous.trigger == "choice:" + site.id))
        {
            message = previous.dialogue;
            return;
        }
    }
}

bool LoadProgress(bool startup = false, int slot = 0)
{
    if (slot == 0)
        slot = activeSlot;
    if (!startup && !levelOne.Dead() && !CanSaveProgress())
    {
        ProgressNotice("전투가 끝난 뒤 불러오세요. 사망 후에도 F9로 돌아갈 수 있습니다.");
        return false;
    }
    SaveGame::State state;
    const auto result = SaveGame::Load(chapter, state, slot);
    if (result == SaveGame::Result::Missing)
    {
        ProgressNotice("이 슬롯에는 저장된 진행이 없습니다.");
        return false;
    }
    if (result == SaveGame::Result::Invalid)
    {
        automaticSaveAllowed = false;
        ProgressNotice("퀘스트 구성이 다르거나 손상된 기록입니다. 기존 슬롯을 보존하고 빈 슬롯에서 "
                       "새 여정을 시작하세요.");
        return false;
    }
    // Validate collision using a temporary world before modifying the live session.
    World restoredWorld;
    restoredWorld.seed = state.seed;
    auto blocked = [&](WorldPoint p)
    {
        if (restoredWorld.Blocked(p))
        {
            return true;
        }
        for (const auto& object : villageObjects)
        {
            if (p.x > object.x - 12 && p.x < object.x + object.w + 12 && p.y > object.y - 12 &&
                p.y < object.y + object.d + 12)
            {
                return true;
            }
        }
        return false;
    };
    if (state.formatVersion < 7)
    {
        state.player = Population::OpenPoint(state.player, blocked);
        state.spirit = Population::OpenPoint(state.spirit, blocked);
        for (auto& enemy : state.level.enemies)
        {
            if (enemy.weakened && blocked(enemy.position))
            {
                enemy.position = Population::OpenPoint(enemy.position, blocked);
            }
        }
        // Previous outdoor camps can now lie in a residential block. Keep story encounters
        // and capturable remnants; ordinary dormant camps are streamed again outside villages.
        state.level.enemies.erase(std::remove_if(state.level.enemies.begin(),
                                                 state.level.enemies.end(),
                                                 [](const LevelOne::Enemy& enemy)
                                                 {
                                                     return enemy.boss == 0 && !enemy.weakened &&
                                                            World::Settlement(enemy.position.x,
                                                                              enemy.position.y);
                                                 }),
                                  state.level.enemies.end());
    }
    if (blocked(state.player) || (!state.level.collection.empty() && blocked(state.spirit)))
    {
        automaticSaveAllowed = false;
        ProgressNotice("저장 위치가 현재 지형과 충돌합니다. 기존 기록을 보존합니다.");
        return false;
    }
    state.chapter.quests = chapter.quests;
    state.level.captureRules.Load();
    state.level.LoadCombat();
    world = std::move(restoredWorld);
    levelOne = std::move(state.level);
    population.people = std::move(state.residents);
    population.society = std::move(state.society);
    population.seed = world.seed;
    population.planningCursor = 0;
    population.RepairPlacement(
        [this](WorldPoint p)
        {
            return Blocked(p);
        });
    chapter = std::move(state.chapter);
    storyTrialTime = 0;
    player = state.player;
    spirit = state.spirit;
    keeper = state.keeper;
    selectedCompanion = state.companion;
    captured = !levelOne.collection.empty();
    capturedSpirit = captured ? levelOne.collection[selectedCompanion].species : 0;
    controlling = state.controlling;
    dialoguePending = state.dialogue;
    speaker = std::move(state.speaker);
    message = std::move(state.message);
    messageTime = state.messageTime;
    hudVisible = state.hud;
    screenShake = state.shake;
    r.postProcess.enabled = state.post;
    r.postProcess.bloomEnabled = state.bloom;
    journal = false;
    pauseMenu = false;
    exitConfirm = false;
    exitRequested = false;
    skipExitSave = false;
    collectionPage = 0;
    eventPage = 0;
    protagonistAnimation = SpriteAnimation();
    ResetControls(true);
    dialoguePage = 0;
    RefreshSavedStoryDialogue();
    RestoreResidentDialogue();
    for (int set = 0; set < 5; ++set)
    {
        for (int slotIndex = 0; slotIndex < 4; ++slotIndex)
        {
            skillTimers[set][slotIndex] = state.skillTimers[set][slotIndex];
        }
    }
    movementVelocity = {0, 0};
    movementWasControlling = false;
    protagonistStopTime = .08f;
    protagonistAnimation.facing = static_cast<Facing>(state.facing);
    ResetCharacterPresentation();
    companionAnimation = SpriteAnimation();
    moving = false;
    protagonistSprinting = false;
    dodgeHeld = false;
    ClearInput();
    camera = controlling ? spirit : player;
    world.Stream(camera, camera, width, height);
    autoSaveTime = 0;
    automaticSaveAllowed = true;
    activeSlot = slot;
    titleScreen = false;
    openingPlaying = openingSkipping = false;
    openingPending = false;
    openingTime = openingExitTime = 0;
    openingFade = .7f;
    ProgressNotice(result == SaveGame::Result::Backup ? "이전 백업에서 진행을 복구했습니다."
                                                      : "저장된 여정을 이어갑니다.");
    return true;
}
