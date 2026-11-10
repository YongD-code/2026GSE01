WorldPoint StagPlantPosition(WorldPoint origin, int index) const
{
    const auto& plant = StagMeadowMesh::Plants[index];
    return {origin.x + plant.x * 100, origin.y + plant.y * 100};
}

bool StagMeadowReach(WorldPoint origin, WorldPoint at)
{
    return Distance(origin, at) <= 100 && LevelOne::ClearLine(origin, at,
        [this](WorldPoint p) { return Blocked(p); });
}

void UpdateStagW(SkillEffect& effect, float dt)
{
    if (effect.impactAge >= 0)
    {
        effect.impactAge += dt;
    }
    if (effect.stage >= 8 || effect.age < .6f + .5f * (effect.stage + 1))
    {
        return;
    }
    ++effect.stage;
    effect.impactAge = 0;
    effect.impacts.clear();
    auto heal = [&](WorldPoint at, float& health, float maximum)
    {
        if (health > 0 && health < maximum && StagMeadowReach(effect.origin, at))
        {
            health = (std::min)(maximum, health + 3.f);
            if (effect.impacts.size() < 12)
            {
                effect.impacts.push_back(at);
            }
        }
    };
    heal(player, levelOne.health, levelOne.MaximumHealth());
    if (captured && selectedCompanion >= 0 && selectedCompanion < int(levelOne.collection.size()))
    {
        heal(spirit, levelOne.collection[selectedCompanion].health, 100);
    }
    for (auto& resident : levelOne.residentCombatants)
    {
        heal(resident.position, resident.health, 100);
    }
}

void DrawStagPlant(WorldPoint at, int index, float age)
{
    const auto& plant = StagMeadowMesh::Plants[index];
    const float growth = (std::max)(0.f, (std::min)(1.f, (age - plant.delay) / .55f));
    const float fade = (std::max)(0.f, (std::min)(1.f, (5.25f - age) / .65f));
    const float scale = plant.scale * growth * growth * (3 - 2 * growth) * fade;
    const std::string key = "stag-meadow-v1:" + std::to_string(plant.mesh);
    if (r.BeginCachedMesh(key, {0, 0}))
    {
        const auto range = StagMeadowMesh::Ranges[plant.mesh];
        for (int i = range.first; i < range.first + range.count; i += 3)
        {
            const auto& a = StagMeadowMesh::Vertices[i];
            const auto& b = StagMeadowMesh::Vertices[i + 1];
            const auto& c = StagMeadowMesh::Vertices[i + 2];
            r.Triangle({a.x, a.y}, {b.x, b.y}, {c.x, c.y}, Color(a.r, a.g, a.b));
        }
        r.EndCachedMesh();
    }
    r.DrawCachedMesh(key, Screen(at.x, at.y, 1), scale);
}

void DrawStagW(const SkillEffect& effect)
{
    const float age = effect.age;
    if (!CharacterInView(effect.origin, 230))
    {
        return;
    }
    if (age < .6f)
    {
        for (int i = 0; i < 8; ++i)
        {
            const float phase = i * 2.39996f;
            const float spread = 16 * (1 - age / .6f);
            r.EnergyParticle(Screen(effect.origin.x + std::cos(phase) * spread,
                                    effect.origin.y + std::sin(phase) * spread, 66 + i % 4 * 3),
                             2, 1.6f, phase, Color(1.f, .78f, .34f, age), age + i);
        }
        return;
    }
    const float fade = (std::max)(0.f, (std::min)(1.f, (5.25f - age) / .65f));
    for (size_t i = 0; i < effect.meadow.size() && i < 144; i += 3)
    {
        const int index = effect.meadow[i];
        if (age < StagMeadowMesh::Plants[index].delay + .3f)
        {
            continue;
        }
        const auto at = StagPlantPosition(effect.origin, index);
        const float phase = std::fmod(age * .55f + index * .173f, 1.f);
        r.EnergyParticle(Screen(at.x, at.y, 3 + 24 * phase), 2, 1.5f, phase,
                         Color(1.f, .82f, .37f, fade * std::sin(phase * 3.14159f) * .65f),
                         age + index);
    }
    if (effect.impactAge >= 0 && effect.impactAge < .4f)
    {
        const float t = effect.impactAge / .4f;
        for (const auto at : effect.impacts)
        {
            r.EnergyParticle(Screen(at.x, at.y, 12 + 35 * t), 8 - 4 * t, 12, 0,
                             Color(.85f, 1.f, .55f, (1 - t) * .45f), t, true);
        }
    }
}
