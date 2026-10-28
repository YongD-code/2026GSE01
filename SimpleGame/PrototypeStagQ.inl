// One approved review step: stag Q. W/E/R still use their existing behavior.
const SkillEffect* StagQCast() const
{
    for (auto it = skillEffects.rbegin(); it != skillEffects.rend(); ++it)
    {
        if (it->caster != selectedCompanion || it->species != SpiritCatalog::Stag)
        {
            continue;
        }
        return IsStagQ(*it) && it->age < StagSkill::Animation ? &*it : nullptr;
    }
    return nullptr;
}

void UpdateStagQ(SkillEffect& effect, float dt)
{
    if (effect.age < StagSkill::Charge || effect.age - dt > StagSkill::Charge + StagSkill::Travel)
    {
        return;
    }
    const double length = (std::max)(1., Distance(effect.origin, effect.target));
    const WorldPoint aim = {(effect.target.x - effect.origin.x) / length,
                            (effect.target.y - effect.origin.y) / length};
    auto clear = [this](WorldPoint a, WorldPoint b)
    {
        return LevelOne::ClearLine(a,
                                   b,
                                   [this](WorldPoint p)
                                   {
                                       return Blocked(p);
                                   });
    };
    for (auto& enemy : levelOne.enemies)
    {
        if (enemy.health <= 0 || enemy.weakened || enemy.returning ||
            std::any_of(effect.struck.begin(),
                        effect.struck.end(),
                        [&](WorldPoint home)
                        {
                            return Distance(home, enemy.home) < .01;
                        }))
        {
            continue;
        }
        bool hit = false;
        for (int branch = 0; branch < 3 && !hit; ++branch)
        {
            const float before =
                (std::min)(effect.branchEnds[branch], StagSkill::Growth(effect.age - dt));
            const float after =
                (std::min)(effect.branchEnds[branch], StagSkill::Growth(effect.age));
            if (after <= before)
            {
                continue;
            }
            const WorldPoint a = StagSkill::Point(effect.origin, aim, branch, before);
            const WorldPoint b = StagSkill::Point(effect.origin, aim, branch, after);
            hit = StagSkill::SegmentDistance(enemy.position, a, b) <= StagSkill::Radius &&
                  clear(b, enemy.position);
        }
        if (!hit)
        {
            continue;
        }
        effect.struck.push_back(enemy.home);
        levelOne.DamageEnemy(enemy, levelOne.Damage() * 2.f, effect.origin, true);
        if (effect.impacts.size() < 8)
        {
            effect.impacts.push_back(enemy.position);
        }
        if (enemy.health <= 0 || enemy.weakened)
        {
            continue;
        }
        if (LevelOne::IsCombatBoss(enemy))
        {
            enemy.slowed = (std::max)(enemy.slowed, 1.6f);
        }
        else
        {
            enemy.stagger = (std::max)(enemy.stagger, StagSkill::RootTime);
            enemy.phase = 0;
            enemy.phaseTime = 0;
        }
    }
}

void DrawStagQ(const SkillEffect& effect, bool ground = false)
{
    const float age = effect.age;
    const float charge = (std::min)(1.f, age / StagSkill::Charge);
    const float growth = StagSkill::Growth(age);
    const float fade = (std::max)(0.f, (std::min)(1.f, (effect.duration - age) / StagSkill::Fade));
    const double length = (std::max)(1., Distance(effect.origin, effect.target));
    const WorldPoint aim = {(effect.target.x - effect.origin.x) / length,
                            (effect.target.y - effect.origin.y) / length};
    auto particle =
        [&](WorldPoint at, double height, float x, float y, float angle, Color color, float phase)
    {
        r.EnergyParticle(Screen(at.x, at.y, height), x, y, angle, color, phase, true);
    };
    const WorldPoint crown = {effect.origin.x + aim.x * 22, effect.origin.y + aim.y * 22};
    const float crownFade =
        age < StagSkill::Charge ? charge : (std::max)(0.f, 1 - (age - StagSkill::Charge) / .35f);
    if (!ground)
    {
        particle(crown,
                 69,
                 14 + charge * 9,
                 13 + charge * 6,
                 age,
                 Color(.65f, 1.4f, .55f, crownFade * .65f),
                 age);
        for (int i = 0; i < 14 && age < StagSkill::Charge; ++i)
        {
            const double angle = i * 2.39996323 + age * 2;
            const double radius = 6 + (1 - charge) * (25 + i % 4 * 9);
            const WorldPoint at = {crown.x + std::cos(angle) * radius,
                                   crown.y + std::sin(angle) * radius};
            particle(at,
                     55 + charge * 14,
                     7,
                     3,
                     float(angle),
                     Color(.9f, 1.2f, .35f, charge * .6f),
                     age + i);
        }
    }
    if (ground)
    {
        for (int branch = 0; branch < 3; ++branch)
        {
            const float end = (std::min)(effect.branchEnds[branch], growth);
            WorldPoint previous = effect.origin;
            for (int segment = 1; segment <= StagSkill::Segments; ++segment)
            {
                const float t = (std::min)(end, float(segment) / StagSkill::Segments);
                if (t <= 0)
                {
                    break;
                }
                const WorldPoint next = StagSkill::Point(effect.origin, aim, branch, t);
                r.Line(Screen(previous.x, previous.y, 2),
                       Screen(next.x, next.y, 2),
                       4 * fade * (1 - t * .6f),
                       Color(.14f, .23f, .08f, fade));
                r.Line(Screen(previous.x, previous.y, 4),
                       Screen(next.x, next.y, 4),
                       1.5f * fade,
                       Color(.65f, 1.25f, .3f, fade * .8f));
                if (segment % 6 == 0)
                {
                    const float bloom = (std::min)(1.f, (end - t) * 10 + .2f);
                    particle(next,
                             6,
                             8 * bloom,
                             3 * bloom,
                             float(segment),
                             Color(.25f, .7f, .16f, fade),
                             age + segment);
                }
                if (segment % 12 == 0)
                {
                    const float curlGrowth = (std::min)(1.f, (end - t) * 8 + .05f);
                    WorldPoint curl = next;
                    for (int i = 1; i <= 7; ++i)
                    {
                        const double angle = i * .85;
                        const double radius = i * 1.7 * curlGrowth;
                        const WorldPoint tip = {next.x + std::cos(angle) * radius,
                                                next.y + std::sin(angle) * radius};
                        r.Line(Screen(curl.x, curl.y, 4),
                               Screen(tip.x, tip.y, 4),
                               1.5f,
                               Color(.3f, .8f, .18f, fade * curlGrowth));
                        curl = tip;
                    }
                }
                previous = next;
                if (t >= end)
                {
                    break;
                }
            }
            if (end > .03f)
            {
                particle(
                    previous, 7, 15, 7, age, Color(.45f, 1.4f, .45f, fade * .6f), age + branch);
            }
        }
    }
    if (!ground)
    {
        for (const auto& at : effect.impacts)
        {
            for (int i = 0; i < 18; ++i)
            {
                const double a = i * .7 + age;
                const double b = (i + 1) * .7 + age;
                r.Line(Screen(at.x + std::cos(a) * 16, at.y + std::sin(a) * 16, 2 + i * 2),
                       Screen(at.x + std::cos(b) * 16, at.y + std::sin(b) * 16, 4 + i * 2),
                       2,
                       Color(.45f, .9f, .2f, fade * .7f));
            }
        }
    }
}

void DrawStagBloom(WorldPoint at, int branch, int index, float rise, float local, float fade, float age)
{
    if (rise <= 0 || fade <= 0)
    {
        return;
    }
    const int seed = branch * StagSkill::BloomPoints + index;
    const float height = .72f + .28f * ((index + branch * 2) % 4) / 3.f;
    for (int strand = 0; strand < 2; ++strand)
    {
        auto previous = StagSkill::Stem(at, seed, strand, 0);
        for (int segment = 1; segment <= 6; ++segment)
        {
            const float t = (std::min)(rise, segment / 6.f);
            const auto next = StagSkill::Stem(at, seed, strand, t);
            r.Line(Screen(previous.x, previous.y, previous.z * height + 2),
                   Screen(next.x, next.y, next.z * height + 2),
                   6 * (1 - t * .7f) * fade,
                   Color(.12f, .25f, .06f, fade));
            r.Line(Screen(previous.x, previous.y, previous.z * height + 3),
                   Screen(next.x, next.y, next.z * height + 3),
                   1.8f * fade,
                   Color(.55f, 1.2f, .25f, fade * .85f));
            if (segment == 3)
            {
                r.EnergyParticle(Screen(next.x, next.y, next.z * height + 4),
                                 10,
                                 4,
                                 float(strand + t * 3),
                                 Color(.3f, .8f, .15f, fade * .8f),
                                 age + segment,
                                 true);
            }
            previous = next;
            if (t >= rise)
            {
                break;
            }
        }
        if (rise < 1)
        {
            r.EnergyParticle(Screen(previous.x, previous.y, previous.z * height + 4),
                             10,
                             17,
                             age,
                             Color(.6f, 1.4f, .3f, fade * .65f),
                             age + strand,
                             true);
        }
    }
    const float bloom =
        (std::max)(0.f, (std::min)(1.f, (local - StagSkill::BloomRise * .7f) / .18f));
    const auto tip = StagSkill::Stem(at, seed, 0, rise);
    for (int petal = 0; petal < 5 && bloom > 0; ++petal)
    {
        const float angle = petal * 1.256637061f + branch;
        r.EnergyParticle(Screen(tip.x + std::cos(angle) * 10 * bloom,
                                tip.y + std::sin(angle) * 10 * bloom,
                                tip.z * height + 6),
                         9 * bloom,
                         4 * bloom,
                         angle,
                         Color(1.5f, 1.25f, .35f, fade * bloom * .8f),
                         age + petal,
                         true);
    }
    r.EnergyParticle(Screen(tip.x, tip.y, tip.z * height + 6),
                     6 * bloom,
                     6 * bloom,
                     0,
                     Color(1.5f, 1.8f, .9f, fade * bloom),
                     age,
                     true);
    if (local < .35f)
    {
        const float burst = 1 - local / .35f;
        for (int i = 0; i < 4; ++i)
        {
            const double angle = i * 2.39996323;
            r.EnergyParticle(Screen(at.x + std::cos(angle) * (5 + local * 45),
                                    at.y + std::sin(angle) * (5 + local * 45),
                                    4 + local * 90),
                             6,
                             3,
                             float(angle),
                             Color(.85f, 1.3f, .4f, burst * fade * .6f),
                             age + i,
                             true);
        }
    }
}
