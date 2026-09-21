// Included inside Prototype. Disk state is separate from rendering and input state.
bool CanSaveProgress() const
{
    if (levelOne.Dead() || chapter.quests.empty() || levelOne.dodgeTime > 0 ||
        levelOne.invulnerability > 0 || !levelOne.projectiles.empty())
    {
        return false;
    }
    for (const auto& enemy : levelOne.enemies)
    {
        if (enemy.health > 0 && (enemy.alerted || enemy.returning || enemy.phase != 0) &&
            !enemy.weakened)
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

bool SaveProgress(bool manual)
{
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
        if (enemy.boss == 0)
        {
            state.level.camps[enemy.camp].health[enemy.slot] = enemy.health;
        }
    }
    if (!SaveGame::Write(std::move(state), chapter))
    {
        automaticSaveAllowed = false;
        ProgressNotice("저장 실패 · 기존 기록 유지. 경로·용량을 확인한 뒤 F5로 재시도하세요.");
        return false;
    }
    automaticSaveAllowed = true;
    autoSaveTime = 0;
    ProgressNotice(manual ? "진행을 저장했습니다." : "진행을 자동 저장했습니다.");
    return true;
}

bool LoadProgress(bool startup = false)
{
    if (!startup && !levelOne.Dead() && !CanSaveProgress())
    {
        ProgressNotice("전투가 끝난 뒤 불러오세요. 사망 후에도 F9로 돌아갈 수 있습니다.");
        return false;
    }
    SaveGame::State state;
    const auto result = SaveGame::Load(chapter, state);
    if (result == SaveGame::Result::Missing)
    {
        if (!startup)
        {
            ProgressNotice("아직 저장된 진행이 없습니다.");
        }
        return false;
    }
    if (result == SaveGame::Result::Invalid)
    {
        automaticSaveAllowed = false;
        ProgressNotice(
            "저장을 읽지 못했습니다. 자동 저장 중지 · F5는 현재 진행으로 새로 저장합니다.");
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
    chapter = std::move(state.chapter);
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
    protagonistAnimation.facing = static_cast<Facing>(state.facing);
    companionAnimation = SpriteAnimation();
    moving = false;
    protagonistSprinting = false;
    dodgeHeld = false;
    ClearInput();
    camera = controlling ? spirit : player;
    world.Stream(camera, camera, width, height);
    autoSaveTime = 0;
    automaticSaveAllowed = true;
    ProgressNotice(result == SaveGame::Result::Backup ? "이전 백업에서 진행을 복구했습니다."
                                                      : "저장된 여정을 이어갑니다.");
    return true;
}
