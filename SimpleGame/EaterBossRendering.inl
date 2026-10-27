// The existing warning Actor owns submission; this file owns no scene objects or textures.
void Prototype::DrawEaterBossWarnings(const LevelOne::Enemy& enemy)
{
    if (enemy.phase == 0)
    {
        return;
    }
    const Point feet = Screen(enemy.position.x, enemy.position.y);
    if (feet.x < -720 || feet.x > width + 720 || feet.y < -540 || feet.y > height + 640)
    {
        return;
    }
    const auto& state = enemy.eater;
    const auto& plan = state.plan;
    const bool preparing = enemy.phase == 1, active = enemy.phase == 2;
    const float charge = EaterBoss::Progress(plan.windup, enemy.phaseTime);
    const float release =
        enemy.phase == 3 ? 1.f : EaterBoss::Progress(plan.active, enemy.phaseTime);
    const float after = plan.recovery - enemy.phaseTime;
    const float fade = enemy.phase == 3 ? (std::max)(0.f, 1 - after / .4f) : 1.f;
    const Color ink(.035f, .07f, .10f, .35f);
    const Color glow =
        state.phaseTwo ? Color(1.25f, .36f, .9f, .7f) : Color(.25f, 1.15f, 1.6f, .7f);
    const Color warning(glow.r, glow.g, glow.b, .1f + charge * .2f);
    auto point = [this](WorldPoint center, double angle, double radius, double height = 2)
    {
        return Screen(
            center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius, height);
    };
    auto ring = [&](WorldPoint center, double radius, double band, Color color, bool fill = false)
    {
        for (int i = 0; i < 32; ++i)
        {
            const double a = i * EaterBoss::Tau / 32, b = (i + 1) * EaterBoss::Tau / 32;
            r.Quad(point(center, a, fill ? 0 : (std::max)(0., radius - band)),
                   point(center, a, radius + band),
                   point(center, b, radius + band),
                   point(center, b, fill ? 0 : (std::max)(0., radius - band)),
                   color);
        }
    };
    if (enemy.phase == 4)
    {
        const float progress = EaterBoss::Progress(EaterBoss::TransitionTime, enemy.phaseTime);
        ring(enemy.position, 28 + progress * 85, 3, Color(glow.r, glow.g, glow.b, 1 - progress));
        for (int i = 0; i < 10; ++i)
        {
            const double angle = i * EaterBoss::Tau / 10 + progress * 2;
            const Point p = point(enemy.position, angle, 60 * (1 - progress), 10 + progress * 90);
            r.EnergyParticle(p, 10, 24, float(angle), Color(.1f, .18f, .25f, .6f), progress, true);
        }
        return;
    }
    if (!state.hasAttack || (!preparing && !active && fade <= 0))
    {
        return;
    }
    if (state.pattern == EaterBoss::Pattern::Sweep)
    {
        const double base = std::atan2(enemy.aim.y, enemy.aim.x);
        for (int i = 0; i < 24; ++i)
        {
            const double a = base - plan.halfArc + i * plan.halfArc * 2 / 24;
            const double b = base - plan.halfArc + (i + 1) * plan.halfArc * 2 / 24;
            if (preparing || active)
            {
                r.Triangle(Screen(state.origin.x, state.origin.y, 2),
                           point(state.origin, a, plan.reach),
                           point(state.origin, b, plan.reach),
                           preparing ? warning : ink);
                r.Line(point(state.origin, a, plan.reach),
                       point(state.origin, b, plan.reach),
                       2,
                       glow);
            }
            if (!preparing)
            {
                const double head = base - plan.halfArc + release * plan.halfArc * 2;
                const double tail = (std::max)(base - plan.halfArc, head - 1.1);
                const double x = tail + (head - tail) * i / 24;
                const double y = tail + (head - tail) * (i + 1) / 24;
                r.Quad(point(state.origin, x, 58, 14),
                       point(state.origin, x, 113, 16),
                       point(state.origin, y, 113, 16),
                       point(state.origin, y, 58, 14),
                       Color(glow.r, glow.g, glow.b, float(i) / 24 * .65f * fade));
            }
        }
        if (!preparing)
        {
            for (int i = 0; i < 6; ++i)
            {
                const double angle = base - plan.halfArc + release * plan.halfArc * 2 - i * .13;
                const Point p = point(state.origin, angle, 100 - i * 5, 18);
                r.EnergyParticle(p,
                                 20 - i * 2,
                                 5,
                                 float(angle),
                                 Color(glow.r, glow.g, glow.b, .6f * fade * (1 - i / 7.f)),
                                 release,
                                 true);
            }
        }
    }
    else if (state.pattern == EaterBoss::Pattern::Rush)
    {
        const auto origin = preparing ? state.origin : enemy.position;
        const double reach = plan.speed *
                             (preparing ? plan.active : (std::max)(0.f, enemy.phaseTime)) *
                             (!preparing && enemy.slowed > 0 ? .35 : 1.);
        const WorldPoint end = {origin.x + enemy.aim.x * reach, origin.y + enemy.aim.y * reach};
        const WorldPoint side = {-enemy.aim.y * plan.radius, enemy.aim.x * plan.radius};
        if (preparing || active)
        {
            r.Quad(Screen(origin.x + side.x, origin.y + side.y, 2),
                   Screen(origin.x - side.x, origin.y - side.y, 2),
                   Screen(end.x - side.x, end.y - side.y, 2),
                   Screen(end.x + side.x, end.y + side.y, 2),
                   preparing ? warning : ink);
            ring(origin, plan.radius, 1.5, glow);
            ring(end, plan.radius, 1.5, glow);
            r.Line(Screen(origin.x + side.x, origin.y + side.y, 2),
                   Screen(end.x + side.x, end.y + side.y, 2),
                   2,
                   glow);
            r.Line(Screen(origin.x - side.x, origin.y - side.y, 2),
                   Screen(end.x - side.x, end.y - side.y, 2),
                   2,
                   glow);
        }
        if (active)
        {
            for (int i = 1; i <= 8; ++i)
            {
                const Point p = Screen(enemy.position.x - enemy.aim.x * i * 11,
                                       enemy.position.y - enemy.aim.y * i * 11,
                                       24 + i % 3 * 6);
                r.EnergyParticle(p,
                                 18 - i,
                                 10,
                                 float(i),
                                 Color(glow.r, glow.g, glow.b, .45f * (1 - i / 9.f)),
                                 release,
                                 true);
            }
        }
    }
    else if (state.pattern == EaterBoss::Pattern::Scream)
    {
        if (preparing)
        {
            ring(state.origin, plan.reach + plan.radius, 1.5, Color(glow.r, glow.g, glow.b, .3f));
            ring(state.origin, 24, 2, glow);
            for (int i = 0; i < 8; ++i)
            {
                const double angle = i * EaterBoss::Tau / 8 + charge;
                const Point p = point(state.origin, angle, 65 * (1 - charge) + 8, 50 + charge * 12);
                r.EnergyParticle(
                    p, 7, 14, float(angle), Color(glow.r, glow.g, glow.b, .5f), charge, true);
            }
        }
        else
        {
            const double radius = EaterBoss::WaveRadius(plan, release);
            ring(state.origin, radius, plan.radius, Color(.02f, .05f, .08f, .5f * fade));
            ring(state.origin, radius + plan.radius, 2, Color(glow.r, glow.g, glow.b, fade));
            ring(state.origin,
                 (std::max)(0., radius - plan.radius),
                 2,
                 Color(glow.r, glow.g, glow.b, .5f * fade));
            for (int i = 0; i < 12; ++i)
            {
                const double angle = i * EaterBoss::Tau / 12;
                const Point p = point(state.origin, angle, radius, 8 + (i % 3) * 5);
                r.EnergyParticle(p,
                                 12,
                                 16,
                                 float(angle),
                                 Color(glow.r, glow.g, glow.b, .55f * fade),
                                 release,
                                 true);
            }
        }
    }
    else
    {
        for (int mark = 0; mark < state.markCount; ++mark)
        {
            const WorldPoint at = state.marks[mark];
            ring(at, plan.radius, 1.5, glow);
            if (preparing)
            {
                ring(at, plan.radius * charge, 0, warning, true);
                const Point p = Screen(at.x, at.y, 6);
                r.EnergyParticle(p, 14, 6, charge * 3, ink, charge, true);
            }
            else
            {
                ring(at, plan.radius, 0, Color(glow.r, glow.g, glow.b, .12f * fade), true);
                for (int i = 0; i < 9; ++i)
                {
                    const double angle = i * 2.39996323;
                    const float rise = (1 - (1 - release) * (1 - release)) * 95;
                    const Point p = point(at, angle, 6 + i * 4, 8 + rise * (1 - i / 12.f));
                    r.EnergyParticle(p,
                                     10 + i % 3 * 3,
                                     24,
                                     float(angle),
                                     Color(glow.r * .4f, glow.g * .4f, glow.b * .4f, .7f * fade),
                                     release,
                                     true);
                }
            }
        }
    }
}
