// Protagonist red/blue effects share bounded geometry but have distinct combat timelines.
void UpdatePolarity(SkillEffect& effect, float dt)
{
    const bool red = effect.slot == 1;
    const float start = red ? RedCharge + RedFlight : BlueCharge;
    if (effect.age < (red ? RedCharge : BlueCharge))
    {
        levelOne.castTime = .14f;
    }
    if (effect.age < start || (red && effect.applied) ||
        (!red && effect.age >= BlueCharge + BlueActive))
    {
        return;
    }
    const bool pulse = effect.age >= effect.nextPulse;
    if (pulse)
    {
        effect.applied = true;
        effect.nextPulse += .5f;
        if (red)
        {
            purpleShake = (std::max)(purpleShake, .17f);
        }
    }
    const float activeDt = (std::min)(dt, effect.age - start);
    for (auto& enemy : levelOne.enemies)
    {
        if (enemy.health <= 0 || enemy.weakened || enemy.returning ||
            Distance(enemy.position, effect.target) > (red ? RedRadius : BlueRadius) ||
            !LevelOne::ClearLine(effect.target,
                                 enemy.position,
                                 [this](WorldPoint p)
                                 {
                                     return Blocked(p);
                                 }))
        {
            continue;
        }
        if (pulse)
        {
            levelOne.DamageEnemy(
                enemy, levelOne.Damage() * (red ? 1.2f : .24f), effect.origin, red, red);
        }
        if (enemy.health <= 0 || enemy.weakened || enemy.boss == 12)
        {
            continue;
        }
        double dx = red ? enemy.position.x - effect.origin.x : effect.target.x - enemy.position.x;
        double dy = red ? enemy.position.y - effect.origin.y : effect.target.y - enemy.position.y;
        const double length = std::hypot(dx, dy);
        if (length < .001)
        {
            continue;
        }
        const double strength = LevelOne::IsCombatBoss(enemy) ? .4 : 1.;
        const double travel =
            red ? 110 * strength
                : (std::min)((std::max)(0., length - 10), 180. * activeDt * strength);
        LevelOne::Move(
            enemy.position,
            {dx / length * travel, dy / length * travel},
            [this](WorldPoint p)
            {
                return Blocked(p);
            },
            &enemy.trail);
    }
}

// Inward-flowing filaments and mist share the combat clock without particle allocations.
void DrawPolarity(const SkillEffect& effect)
{
    if (effect.slot == 1)
    {
        DrawRed(effect);
        return;
    }
    const float age = effect.age;
    const float opening = (std::min)(1.f, age / BlueCharge);
    const float fade = (std::max)(0.f, (std::min)(1.f, (effect.duration - age) / BlueFade));
    const float strength = opening * fade;
    const Point core = Screen(effect.target.x, effect.target.y, 24);
    auto mote = [&](Point p, float x, float y, float angle, Color color, float seed, bool cloud)
    {
        r.EnergyParticle(p, x, y, angle, color, age * 2 + seed, cloud);
    };
    mote(core, 78, 66, 0, Color(.025f, .28f, 1.2f, strength * .55f), 0, false);
    // Several staggered streams spiral down to the focal point; their tails taper smoothly.
    for (int i = 0; i < 30; ++i)
    {
        const float phase = std::fmod(age * (.48f + (i % 5) * .035f) + i * .173f, 1.f);
        const float envelope = std::sin(phase * 3.14159f) * strength;
        for (int trail = 0; trail < 3; ++trail)
        {
            const float q = (std::max)(0.f, phase - trail * .027f);
            const float angle = i * 2.39996f + q * 4.8f;
            const float radius = 9 + (1 - q) * BlueRadius * opening;
            const Point p = {core.x + std::cos(angle) * radius * 1.202f,
                             core.y + std::sin(angle) * radius * .608f};
            mote(p,
                 7 + q * 6,
                 2.4f + q * 1.5f,
                 angle + 1.9f,
                 Color(.13f, .65f, 1.4f, envelope * (1 - trail * .28f)),
                 float(i),
                 false);
        }
    }
    for (int i = 0; i < 12; ++i)
    {
        const float angle = i * 2.39996f - age * .9f;
        const float radius = 18 + (i % 4) * 13;
        const Point p = {core.x + std::cos(angle) * radius,
                         core.y + std::sin(angle) * radius * .62f};
        mote(p,
             32 + (i % 3) * 8.f,
             20,
             angle + 1.57f,
             Color(.025f, .25f, .95f, strength * .44f),
             i * 3.f,
             true);
    }
    // Soft dark center creates depth without a hard circular outline.
    mote(core, 27, 24, 0, Color(.002f, .006f, .035f, strength), 0, false);
    for (int i = 0; i < 5; ++i)
    {
        const float angle = age * 2 + i * 1.2566f;
        const Point p = {core.x + std::cos(angle) * 13, core.y + std::sin(angle) * 10};
        mote(p, 10, 3, angle + 1.57f, Color(.4f, 1.1f, 1.5f, strength * .8f), float(i), false);
    }
}
