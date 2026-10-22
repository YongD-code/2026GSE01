Population population;
SpriteSheet populationSheets[4];
SpriteSheet populationWalkSheets[4];
SpriteSheet gathererSheet;
bool useGathererStudy = false;
int talkingResident = -1;

void ToggleGathererStudy()
{
    if (!gathererSheet.Ready())
    {
        Say("채집꾼 외형", "1차 보행 후보를 읽지 못했습니다. 기존 외형을 사용합니다.");
        return;
    }
    useGathererStudy = !useGathererStudy;
    FrameProfiler::Event("gatherer_render_mode", useGathererStudy ? "volume_atlas" : "original", 0);
    Say("채집꾼 외형",
        useGathererStudy
            ? "이전 채집꾼 보행 원화로 전환했습니다. N으로 조합형 모델과 비교할 수 있습니다."
            : "조합형 NPC 입체 모델로 전환했습니다. 게임 중 N으로 비교할 수 있습니다.");
}

bool ResidentQuestTarget(int id) const
{
    const auto* quest = chapter.Current();
    return quest && quest->trigger == "talk:resident-" + std::to_string(id);
}

bool ResidentNameVisible(const Population::Person& person) const
{
    return ResidentVisible(person) &&
           (ResidentQuestTarget(person.id) || (dialoguePending && talkingResident == person.id));
}

bool ResidentVisible(const Population::Person& person) const
{
    if (person.health <= 0 || person.role < 0 || person.role >= 4 ||
        !CharacterInView(person.position, 67))
    {
        return false;
    }
    return npcModel.Ready() ||
           (useGathererStudy && person.role == Population::Farmer && gathererSheet.Ready()) ||
           (person.animation.moving && populationWalkSheets[person.role].Ready()) ||
           populationSheets[person.role].Ready();
}

const Population::Person* NearbyResident() const
{
    if (controlling)
        return nullptr;
    const Population::Person* nearest = nullptr;
    double reach = 62;
    for (const auto& person : population.people)
    {
        if (person.health <= 0 || person.action == Population::Fighting ||
            person.action == Population::Fleeing || person.action == Population::Recovering)
            continue;
        const double distance = Distance(player, person.position);
        if (distance < 62 && ResidentQuestTarget(person.id))
            return &person;
        if (distance < reach)
        {
            reach = distance;
            nearest = &person;
        }
    }
    return nearest;
}

void TalkResident(int id)
{
    if (id < 0 || id >= int(population.people.size()))
        return;
    auto& person = population.people[id];
    const auto* quest = chapter.Current();
    const bool objective = ResidentQuestTarget(person.id);
    const char* greetings[] = {
        "오늘은 약초와 채소를 거두러 나왔어요. 마을 밖에서는 길을 벗어나지 마세요.",
        "마을을 돌며 물건을 나르고 있소. 낯선 종소리가 들리는 길은 피하는 게 좋겠소.",
        "일감을 마치면 공방으로 돌아갈 거예요. 장비를 손보려면 광장의 대장장이를 찾아가세요.",
        "주령의 흔적을 살피는 중이오. 혼자 깊은 숲에 들어가지는 마시오."};
    std::string line =
        objective ? quest->dialogue
        : person.action == Population::Returning
            ? "오늘 일은 마쳤어요. 이제 집으로 돌아가는 길이에요."
        : person.action == Population::Resting ? "잠깐 쉬는 중이에요. 당신도 무리하지 마세요."
        : person.action == Population::Working ? "손이 조금 바쁘네요. 일을 마치고 다시 이야기해요."
                                               : greetings[person.role];
    if (!objective)
    {
        line += "\n제 집은 " + VillageLayout::Address(person.houseId) + "이에요. " +
                population.HeardNews(person);
        if (person.migrationVillage >= 0)
        {
            line += "\n들은 소식을 믿고 " +
                    std::string(World::VillageName(person.migrationVillage)) +
                    "로 이사하는 중이에요.";
        }
        else if (person.mailId >= 0)
        {
            line += "\n다른 마을에 전할 편지를 맡고 있어요.";
        }
    }
    Say(Population::Name(person.id), line);
    talkingResident = person.id;
    dialoguePending = true;
    person.animation.moving = false;
    person.visualVelocity = {0, 0};
    person.stopTime = .1f;
    SpriteAnimation facing;
    const double dx = player.x - person.position.x, dy = player.y - person.position.y;
    if (std::hypot(dx, dy) > .01)
    {
        facing.Update((dx - dy) * .85, (dx + dy) * .43, 0, false);
        person.animation.facing = facing.facing;
    }
    if (objective)
    {
        AdvanceChapter();
        autoSaveTime = 30;
    }
    ClearInput();
}

void RestoreResidentDialogue()
{
    talkingResident = -1;
    if (!dialoguePending)
        return;
    for (auto& person : population.people)
    {
        if (speaker == Population::Name(person.id) && Distance(player, person.position) < 100)
        {
            talkingResident = person.id;
            person.animation.moving = false;
            person.stopTime = .1f;
            return;
        }
    }
}

void InitializePopulation()
{
    talkingResident = -1;
    population.Initialize(world.seed,
                          [this](WorldPoint p)
                          {
                              return Blocked(p);
                          });
    levelOne.residentCombatants.clear();
    levelOne.encounterObservers.clear();
    levelOne.encounterRegions.clear();
    levelOne.pendingEncounterRegions.clear();
    levelOne.regionLoaded = false;
}

void UpdatePopulation(float dt)
{
    if (population.people.empty() || population.seed != world.seed)
        InitializePopulation();
    // Read combat results from the previous world step before making the next decision.
    for (const auto& result : levelOne.residentCombatants)
    {
        if (result.id >= 0 && result.id < int(population.people.size()))
        {
            auto& p = population.people[result.id];
            p.health = result.health;
            p.invulnerability = result.invulnerability;
        }
    }
    for (auto& person : population.people)
    {
        person.questPinned = ResidentQuestTarget(person.id) || talkingResident == person.id;
        if (person.questPinned && person.migrationVillage >= 0)
        {
            person.reservedHouse = person.migrationVillage = -1;
            population.Travel(person, person.home, Population::Resting, Population::Returning);
        }
    }
    population.Update(
        dt,
        controlling ? spirit : player,
        levelOne,
        [this](WorldPoint p)
        {
            return Blocked(p);
        },
        SimulationRadius());
    levelOne.residentCombatants.clear();
    levelOne.encounterObservers.clear();
    for (const auto& p : population.people)
    {
        if (Distance(p.position, controlling ? spirit : player) > SimulationRadius() + 600)
        {
            continue;
        }
        levelOne.residentCombatants.push_back({p.id, p.position, p.health, p.invulnerability});
        if (p.role == Population::Hunter &&
            Distance(p.position, controlling ? spirit : player) < SimulationRadius() + 300)
        {
            levelOne.encounterObservers.push_back(p.position);
        }
    }
}

void SyncPopulationHealth()
{
    for (const auto& result : levelOne.residentCombatants)
        if (result.id >= 0 && result.id < int(population.people.size()))
        {
            population.people[result.id].health = result.health;
            population.people[result.id].invulnerability = result.invulnerability;
        }
}

std::string HomeOccupant(const VillageLayout::House& house) const
{
    const std::string address =
        std::to_string(house.id % VillageLayout::HomesPerVillage + 1) + "번집 · ";
    const int owner = population.HouseOwner(house.id);
    if (owner == -2)
    {
        for (const auto& site : StoryLayout::Sites())
        {
            if (site.spriteIndex == house.id && site.kind != StoryLayout::Kind::Evidence)
            {
                return address + site.name;
            }
        }
    }
    if (owner >= 0)
    {
        return address + Population::Name(owner) +
               (population.people[owner].houseId == house.id ? "의 집" : " 이사 예약");
    }
    return address + "빈집";
}

void DrawResident(const Population::Person& person, CharacterModel::State* state)
{
    if (!ResidentVisible(person))
    {
        return;
    }
    Point p = Screen(person.position.x, person.position.y);
    const float shade = .88f + (person.id % 4) * .04f;
    r.Ellipse(p.x, p.y, 13, 5, Color(0, 0, 0, .22f));
    const bool study =
        useGathererStudy && gathererSheet.Ready() && person.role == Population::Farmer;
    auto& sheet = study ? gathererSheet
                  : person.animation.moving && populationWalkSheets[person.role].Ready()
                      ? populationWalkSheets[person.role]
                      : populationSheets[person.role];
    SpriteAnimation animation = person.animation;
    if (study)
    {
        // Population's distance-driven cycle has eight phases; this atlas has 32 poses.
        animation.phase *= float(gathererSheet.WalkFrameCount()) / 8.f;
    }
    const bool running = person.action == Population::Fleeing;
    const auto motion = !animation.moving ? CharacterModel::Motion::Idle
                        : running         ? CharacterModel::Motion::Run
                                          : CharacterModel::Motion::Walk;
    const Color tint(shade, 1.f, .94f + (person.id % 3) * .02f);
    if (study || !DrawKeyedModel(npcModel,
                                 p,
                                 67 * person.appearance.scale,
                                 animation,
                                 tint,
                                 motion,
                                 animation.moving ? person.animation.phase / 8.f
                                                  : time * .75f + person.id * .17f,
                                 state,
                                 &person.appearance.partIndices))
    {
        sheet.Draw(r, p, 67, animation, tint, width, height);
    }
    if (person.action == Population::Fighting && person.attackWait > .85f)
    {
        const float glow = (person.attackWait - .85f) / .25f;
        const Point target = Screen(person.destination.x, person.destination.y, 28);
        r.Line({p.x + 12, p.y - 28}, target, 2, Color(.45f, .8f, 1.f, glow * .5f));
        r.EnergyParticle(
            {p.x + 12, p.y - 28}, 22, 6, -.5f, Color(.4f, .85f, 1.f, glow * .7f), time, false);
    }
    if (person.health < 99)
    {
        r.Rect(p.x - 17, p.y - 73, 34, 3, Color(.1f, .12f, .15f));
        r.Rect(p.x - 17, p.y - 73, 34 * person.health / 100.f, 3, Color(.3f, .85f, .6f));
    }
}
