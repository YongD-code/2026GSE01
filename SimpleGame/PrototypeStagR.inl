const SkillEffect* StagSanctuary() const
{
    for (auto it = skillEffects.rbegin(); it != skillEffects.rend(); ++it)
    {
        if (IsStagR(*it) && it->age >= StagSkill::SanctuaryOpening &&
            it->age < StagSkill::SanctuaryOpening + StagSkill::SanctuaryActive)
        {
            return &*it;
        }
    }
    return nullptr;
}

void UpdateStagR(SkillEffect& effect)
{
    if (effect.age < StagSkill::SanctuaryOpening || effect.applied)
    {
        return;
    }
    effect.applied = true;
    domainShake = (std::max)(domainShake, .22f);
}

WorldPoint StagTreePosition(WorldPoint origin, int index) const
{
    static constexpr double offsets[5][2] = {
        {-138, 34}, {-78, 134}, {60, 151}, {148, 47}, {111, -96}};
    return {origin.x + offsets[index][0], origin.y + offsets[index][1]};
}

void DrawStagTree(WorldPoint at, int index, float age)
{
    const float t = (std::max)(0.f, (std::min)(1.f,
        (age - StagSkill::SanctuaryOpening - .09f * index) / .52f));
    const float growth = t * t * (3 - 2 * t);
    const float fade = (std::max)(0.f, (std::min)(1.f,
        (StagSkill::SanctuaryOpening + StagSkill::SanctuaryActive +
         StagSkill::SanctuaryFade - age) / StagSkill::SanctuaryFade));
    if (growth <= .001f || fade <= .001f)
    {
        return;
    }
    const Point base = Screen(at.x, at.y);
    const float height = 148 * growth;
    const float width = 120 * (.32f + .68f * growth);
    stagSanctuaryTrees[index].DrawPanel(r, base.x - width * .5f, base.y - height * .84f,
                                       width, height, fade);
}

void DrawStagRoots(WorldPoint origin, float age)
{
    const float elapsed = age - StagSkill::SanctuaryOpening;
    const float fade = (std::max)(0.f, (std::min)(1.f,
        (StagSkill::SanctuaryActive + StagSkill::SanctuaryFade - elapsed) /
        StagSkill::SanctuaryFade));
    for (int i = 0; i < 5; ++i)
    {
        const auto tip = StagTreePosition(origin, i);
        const double dx = tip.x - origin.x, dy = tip.y - origin.y;
        const float growth = (std::max)(0.f, (std::min)(1.f, (elapsed - .06f * i) / .46f));
        Point previous = Screen(origin.x, origin.y, 1);
        for (int j = 1; j <= 12 && growth > 0; ++j)
        {
            const float t = (std::min)(growth, j / 12.f);
            const double bend = .13 * std::sin(t * 3.14159265) *
                                std::sin(t * 6.4 + i * 2.4);
            const Point next = Screen(origin.x + dx * t - dy * bend,
                                      origin.y + dy * t + dx * bend, 1);
            r.Line(previous, next, 3.f - 1.7f * t, Color(.24f, .41f, .17f, fade * .8f));
            previous = next;
            if (t >= growth)
            {
                break;
            }
        }
    }
}

void DrawStagR(const SkillEffect& effect)
{
    const float age = effect.age;
    if (!CharacterInView(effect.origin, 360))
    {
        return;
    }
    if (age < StagSkill::SanctuaryOpening)
    {
        const float t = age / StagSkill::SanctuaryOpening;
        for (int i = 0; i < 12; ++i)
        {
            const float a = i * 2.39996f + age;
            const float reach = 7 + 25 * (1 - t);
            r.EnergyParticle(Screen(effect.origin.x + std::cos(a) * reach,
                                    effect.origin.y + std::sin(a) * reach, 57 + i % 5 * 5),
                             2.8f, 2, a, Color(1.f, .81f, .35f, t * .65f), age + i);
        }
        return;
    }
    const float elapsed = age - StagSkill::SanctuaryOpening;
    if (elapsed >= StagSkill::SanctuaryActive)
    {
        return;
    }
    const auto inside = [&](WorldPoint at)
    {
        return Distance(at, effect.origin) <= StagSkill::SanctuaryRadius;
    };
    const auto ward = [&](WorldPoint at)
    {
        if (!inside(at) || !CharacterInView(at, 60))
        {
            return;
        }
        for (int i = 0; i < 4; ++i)
        {
            const float a = i * 1.5708f + elapsed * 1.4f;
            r.EnergyParticle(Screen(at.x + std::cos(a) * 12, at.y + std::sin(a) * 12,
                                    12 + i * 6), 4, 9, a,
                             Color(.91f, 1.f, .54f, .42f), elapsed + i);
        }
    };
    ward(player);
    if (captured)
    {
        ward(spirit);
    }
    int residents = 0;
    for (const auto& resident : levelOne.residentCombatants)
    {
        if (resident.health > 0 && inside(resident.position))
        {
            ward(resident.position);
            if (++residents >= 6)
            {
                break;
            }
        }
    }
    int enemies = 0;
    for (const auto& enemy : levelOne.enemies)
    {
        if (enemy.health <= 0 || enemy.weakened ||
            (enemy.boss == 0 && SpiritCatalog::Get(enemy.type).passive) || !inside(enemy.position) ||
            !CharacterInView(enemy.position, 32))
        {
            continue;
        }
        for (int j = 0; j < 3; ++j)
        {
            const float a = j * 2.39996f + elapsed * .7f;
            const auto at = enemy.position;
            Point previous = Screen(at.x, at.y, 1);
            for (int k = 1; k <= 5; ++k)
            {
                const float d = k * 3.6f;
                const float b = a + k * .21f;
                const Point next = Screen(at.x + d * std::cos(b), at.y + d * std::sin(b),
                                          1 + 2.5f * std::sin(k * .63f));
                r.Line(previous, next, 2.7f - .38f * k, Color(.47f, .74f, .34f, .76f));
                previous = next;
            }
        }
        if (++enemies >= 12)
        {
            break;
        }
    }
}
