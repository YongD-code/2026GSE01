// Spirit Q abilities share telegraph geometry with their hit tests.
WorldPoint SpiritQPosition(const SkillEffect& effect) const
{
    if (effect.set == 1)
    {
        return effect.position;
    }
    const float t = (std::max)(0.f, (std::min)(1.f, (effect.age - .18f) / 1.2f));
    const float travel = t < .5f ? t * 2 : (1 - t) * 2;
    return {effect.origin.x + (effect.target.x - effect.origin.x) * travel,
            effect.origin.y + (effect.target.y - effect.origin.y) * travel};
}

void UpdateSpiritQ(SkillEffect& effect, float dt)
{
    if (IsStagQ(effect))
    {
        UpdateStagQ(effect, dt);
        return;
    }
    auto clear = [this](WorldPoint a, WorldPoint b)
    {
        return LevelOne::ClearLine(a,
                                   b,
                                   [this](WorldPoint p)
                                   {
                                       return Blocked(p);
                                   });
    };
    WorldPoint center = effect.target;
    float radius = 70.f;
    bool strike = false;
    if (effect.set == 1)
    {
        if (effect.applied || effect.age < .45f)
            return;
        effect.applied = true;
        effect.impactAge = 0;
        center = effect.origin;
        radius = 320;
        strike = true;
    }
    else if (effect.set == 2)
    {
        if (effect.age < .18f || effect.age > 1.38f)
        {
            return;
        }
        const int pass = effect.age < .78f ? 0 : 1;
        if (pass != effect.stage)
        {
            effect.stage = pass;
            effect.struck.clear();
        }
        center = SpiritQPosition(effect);
        if (!clear(effect.position, center))
        {
            effect.age = effect.duration;
            return;
        }
        radius = 34;
        strike = true;
    }
    else
    {
        if (effect.age < effect.nextPulse || effect.applied)
        {
            return;
        }
        strike = true;
        effect.applied = effect.set == 3 || effect.stage == 2;
        if (effect.set == 4)
        {
            const double vx = effect.target.x - effect.origin.x;
            const double vy = effect.target.y - effect.origin.y;
            const double length = (std::max)(1., std::hypot(vx, vy));
            const double offset = (effect.stage - 1) * 85.;
            center = {effect.target.x - vy / length * offset,
                      effect.target.y + vx / length * offset};
            ++effect.stage;
            effect.nextPulse += .25f;
            radius = 65;
        }
    }
    for (auto& enemy : levelOne.enemies)
    {
        if (!strike || enemy.health <= 0 || enemy.weakened || enemy.returning)
        {
            continue;
        }
        bool hit = Distance(enemy.position, center) <= radius;
        if (effect.set == 1)
        {
            const double dx = enemy.position.x - effect.origin.x,
                         dy = enemy.position.y - effect.origin.y;
            const double vx = effect.target.x - effect.origin.x,
                         vy = effect.target.y - effect.origin.y;
            const double distance = std::hypot(dx, dy);
            hit = distance <= 320 && (dx * vx + dy * vy) >= distance * std::hypot(vx, vy) * .78;
        }
        if (effect.set == 2)
        {
            const double vx = center.x - effect.position.x, vy = center.y - effect.position.y;
            const double t = (std::max)(0.,
                                        (std::min)(1.,
                                                   ((enemy.position.x - effect.position.x) * vx +
                                                    (enemy.position.y - effect.position.y) * vy) /
                                                       (vx * vx + vy * vy + .0001)));
            hit = Distance(enemy.position,
                           {effect.position.x + vx * t, effect.position.y + vy * t}) < radius;
            for (const auto& home : effect.struck)
            {
                if (Distance(home, enemy.home) < .01)
                {
                    hit = false;
                }
            }
        }
        if (effect.set == 3)
        {
            const double dx = enemy.position.x - effect.origin.x,
                         dy = enemy.position.y - effect.origin.y;
            const double vx = effect.target.x - effect.origin.x,
                         vy = effect.target.y - effect.origin.y;
            const double distance = std::hypot(dx, dy);
            hit = distance < 175 && (dx * vx + dy * vy) >= distance * std::hypot(vx, vy) * .64;
        }
        if (!hit || !clear(effect.set == 2 ? center : effect.origin, enemy.position))
        {
            continue;
        }
        effect.struck.push_back(enemy.home);
        const float factor = effect.set == 1   ? 2.f
                             : effect.set == 2 ? 1.3f
                             : effect.set == 3 ? 2.7f
                                               : 1.1f;
        levelOne.DamageEnemy(enemy, levelOne.Damage() * factor, effect.origin, true);
        if (effect.impactAge > .12f)
        {
            effect.impacts.clear();
        }
        if (effect.impacts.size() < 8)
        {
            effect.impacts.push_back(enemy.position);
            effect.impactAge = 0;
        }
        if (enemy.health <= 0 || enemy.weakened || enemy.boss == 12)
        {
            continue;
        }
        if (effect.set == 2 || effect.set == 4)
        {
            enemy.slowed = (std::max)(enemy.slowed, 1.5f);
        }
        if (effect.set == 3)
        {
            if (levelOne.IsCombatBoss(enemy))
            {
                enemy.slowed = (std::max)(enemy.slowed, 1.f);
            }
            else
            {
                const double distance = (std::max)(1., Distance(effect.origin, enemy.position));
                LevelOne::Move(
                    enemy.position,
                    {(enemy.position.x - effect.origin.x) / distance * 65,
                     (enemy.position.y - effect.origin.y) / distance * 65},
                    [this](WorldPoint p)
                    {
                        return Blocked(p);
                    },
                    &enemy.trail);
                enemy.stagger = (std::max)(enemy.stagger, .6f);
                enemy.phase = 0;
            }
        }
    }
    if (effect.set == 2)
    {
        effect.position = center;
    }
}
