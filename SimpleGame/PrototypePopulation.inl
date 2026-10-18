Population population;
SpriteSheet populationSheets[4];

void InitializePopulation()
{
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
        levelOne.residentCombatants.push_back({p.id, p.position, p.health, p.invulnerability});
        if (p.role == Population::Hunter)
            levelOne.encounterObservers.push_back(p.position);
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

void DrawResident(const Population::Person& person)
{
    Point p = Screen(person.position.x, person.position.y);
    if (p.x < -100 || p.x > width + 100 || p.y < -30 || p.y > height + 130)
        return;
    const bool working =
        person.action == Population::Working || person.action == Population::Fighting;
    const float bob = person.animation.moving ? std::sin(time * 4 + person.id) * .6f : 0.f;
    if (working)
        p.x += std::sin(time * 7 + person.id) * 1.2f;
    const float shade = .88f + (person.id % 4) * .04f;
    r.Ellipse(p.x, p.y, 13, 5, Color(0, 0, 0, .22f));
    populationSheets[person.role].Draw(r,
                                       {p.x, p.y + bob},
                                       67,
                                       person.animation,
                                       Color(shade, 1.f, .94f + (person.id % 3) * .02f),
                                       width,
                                       height);
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
