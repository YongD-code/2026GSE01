// Analytic particles: bounded count, reproducible motion, no per-frame allocation.
void DrawRed(const SkillEffect& effect)
{
    const Point origin = Screen(effect.origin.x, effect.origin.y, 35);
    const Point target = Screen(effect.target.x, effect.target.y, 24);
    const float age = effect.age;
    const float impact = RedCharge + RedFlight;
    auto random = [&](int i)
    {
        const float v = std::sin(float(i) * 127.1f + float(effect.id % 1024) * 31.7f) * 43758.5453f;
        return v - std::floor(v);
    };
    auto particle =
        [&](Point p, float rx, float ry, float angle, Color tint, float seed, bool smoke)
    {
        r.EnergyParticle(p, rx, ry, angle, tint, age * 3 + seed, smoke);
    };
    if (age < RedCharge)
    {
        const float t = age / RedCharge;
        particle(origin, 48, 42, 0, Color(1, .025f, .045f, .18f + .22f * t), 0, false);
        for (int i = 0; i < 28; ++i)
        {
            const float seed = random(i);
            const float travel = std::fmod(t * .9f + seed, 1.f);
            const float angle = i * 2.39996f + travel * 1.4f;
            const float radius = 8 + (1 - travel) * (35 + random(i + 50) * 45);
            const Point p = {origin.x + std::cos(angle) * radius,
                             origin.y + std::sin(angle) * radius * .7f};
            particle(p,
                     3 + travel * 6,
                     2 + travel * 2,
                     angle,
                     Color(1.3f, .12f, .08f, std::sin(travel * 3.14159f) * t),
                     seed * 13,
                     false);
        }
        for (int i = 0; i < 5; ++i)
        {
            const float a = age * 9 + i * 2.4f;
            const Point p = {origin.x + std::cos(a) * 5, origin.y + std::sin(a) * 5};
            particle(p, 13 + 10 * t, 10 + 8 * t, a, Color(1.2f, .055f, .08f, .5f), float(i), true);
        }
        particle(origin, 5 + 5 * t, 5 + 5 * t, 0, Color(1.5f, .65f, .48f, t), 0, false);
        return;
    }
    // A short tapered wake continues to dissipate after the projectile hits.
    if (age < impact + .22f)
    {
        const float head = (std::min)(1.f, (age - RedCharge) / RedFlight);
        const float tailFade = (std::max)(0.f, (std::min)(1.f, 1 - (age - impact) / .22f));
        const float angle = std::atan2(target.y - origin.y, target.x - origin.x);
        for (int i = 0; i < 22; ++i)
        {
            const float back = i / 21.f;
            const float t = (std::max)(0.f, head - back * .55f);
            const float jitter = std::sin(i * 2.4f + age * 23) * back * 5;
            const Point p = {origin.x + (target.x - origin.x) * t - std::sin(angle) * jitter,
                             origin.y + (target.y - origin.y) * t + std::cos(angle) * jitter};
            particle(p,
                     26 - back * 17,
                     13 - back * 9,
                     angle,
                     Color(1.3f, .06f + .24f * (1 - back), .07f, (1 - back) * .65f * tailFade),
                     float(i),
                     i % 3 == 0);
        }
        if (age < impact)
        {
            const Point p = {origin.x + (target.x - origin.x) * head,
                             origin.y + (target.y - origin.y) * head};
            particle(p, 14, 11, angle, Color(1.6f, .85f, .65f, 1), 0, false);
            return;
        }
    }
    const float elapsed = age - impact;
    const float t = (std::min)(1.f, elapsed / RedFade);
    const float fade = (1 - t) * (1 - t);
    const float expansion = 1 - std::exp(-elapsed * 9);
    // Uneven plumes replace solid rings; they broaden and cool into translucent smoke.
    for (int i = 0; i < 16; ++i)
    {
        const float seed = random(i + 100);
        const float angle = i * 2.39996f + seed;
        const float reach = expansion * RedRadius * (.35f + seed * .65f);
        const Point p = {target.x + std::cos(angle) * reach * 1.202f,
                         target.y + std::sin(angle) * reach * .608f - elapsed * (12 + seed * 20)};
        const float size = 18 + expansion * (18 + seed * 22);
        particle(p,
                 size,
                 size * .75f,
                 angle + elapsed,
                 Color(1.15f - t * .8f, .045f + t * .02f, .06f, fade * .6f),
                 seed * 30,
                 true);
    }
    for (int i = 0; i < 30; ++i)
    {
        const float seed = random(i + 200);
        const float life = .3f + seed * .6f;
        if (elapsed >= life)
        {
            continue;
        }
        const float angle = i * 2.39996f + seed;
        const float reach = (1 - std::exp(-elapsed * 4)) * (45 + seed * 100);
        const Point p = {target.x + std::cos(angle) * reach,
                         target.y + std::sin(angle) * reach * .65f + elapsed * elapsed * 35};
        particle(p,
                 4 + (1 - t) * 6,
                 1.8f,
                 angle,
                 Color(1.5f, .22f + seed * .3f, .08f, (1 - elapsed / life) * fade),
                 seed,
                 false);
    }
    particle(target,
             55 + expansion * 25,
             45 + expansion * 20,
             0,
             Color(1.4f, .06f, .025f, fade * .35f),
             0,
             false);
    const float flash = std::exp(-elapsed * 24);
    particle(target, 36, 29, 0, Color(1.8f, 1.1f, .8f, flash), 0, false);
}
