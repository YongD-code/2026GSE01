// Nebula below actors, sparse drifting lights above; retain the spherical expansion.
void DrawDomain(const SkillEffect& effect, bool behind)
{
    for (auto it = skillEffects.rbegin(); it != skillEffects.rend(); ++it)
    {
        if (it->slot == 3 && it->age >= DomainOpening && it->age < it->duration)
        {
            if (it->id != effect.id)
                return;
            break;
        }
    }
    const float opening =
        (std::min)(1.f, (std::max)(0.f, effect.age - .78f) / (DomainOpening - .78f));
    const float growth = opening * opening * (3 - 2 * opening);
    const float fade =
        (std::max)(0.f, (std::min)(1.f, (effect.duration - effect.age) / DomainFade));
    const float strength = growth * fade;
    if (strength < .001f)
    {
        return;
    }
    const Point feet = Screen(effect.origin.x, effect.origin.y);
    const Point center = {feet.x, feet.y - 35 * growth};
    const float rx = DomainRadius * 1.202f * growth;
    const float ry = DomainRadius * 1.04f * growth;
    auto light = [&](Point p, float x, float y, float angle, Color color, float seed, bool cloud)
    {
        r.EnergyParticle(p, x, y, angle, color, effect.age * .45f + seed, cloud);
    };
    if (behind)
    {
        // Two soft layers replace the opaque disk and its visible geometric edge.
        light(center, rx * 1.6f, ry * 1.6f, 0, Color(.002f, .004f, .02f, strength), 0, false);
        light(center, rx * 1.25f, ry * 1.25f, 0, Color(.002f, .004f, .02f, strength), 0, false);
        for (int i = 0; i < 18; ++i)
        {
            const float angle = i * 2.39996f + effect.age * .055f;
            const float radial = .12f + (i % 6) * .12f;
            const Point p = {center.x + std::cos(angle) * rx * radial,
                             center.y + std::sin(angle) * ry * radial};
            light(p,
                  rx * .32f,
                  ry * .19f,
                  angle + effect.age * .025f,
                  Color(.055f + (i % 3) * .055f, .12f, .52f, strength * .45f),
                  i * 4.f,
                  true);
        }
        for (int i = 0; i < 90; ++i)
        {
            const float angle = i * 2.399963f + effect.age * (.012f + (i % 3) * .008f);
            const float radial = std::sqrt((i + .5f) / 90) * .93f;
            const Point p = {center.x + std::cos(angle) * rx * radial,
                             center.y + std::sin(angle) * ry * radial};
            const float glow = .55f + .25f * std::sin(effect.age * 1.7f + i * 1.3f);
            const float size = i % 9 == 0 ? 5.f : 2.4f;
            light(p, size, size, 0, Color(.65f, .9f, 1.4f, strength * glow), 0, false);
        }
        return;
    }
    // Broken, drifting wisps suggest the shell rather than drawing latitude/longitude lines.
    for (int i = 0; i < 32; ++i)
    {
        const float angle = i * 2.39996f + effect.age * .06f;
        const float ripple = 1 + .025f * std::sin(i * 3.f + effect.age * 2);
        const Point p = {center.x + std::cos(angle) * rx * ripple,
                         center.y + std::sin(angle) * ry * ripple};
        light(p,
              34 * growth,
              12 * growth,
              angle + 1.57f,
              Color(.18f, .5f, 1.1f, strength * .32f),
              float(i),
              true);
    }
    for (int i = 0; i < 22; ++i)
    {
        const float phase = std::fmod(effect.age * .17f + i * .137f, 1.f);
        const float angle = i * 2.39996f + phase * .2f;
        const float radial = .12f + phase * .76f;
        const Point p = {center.x + std::cos(angle) * rx * radial,
                         center.y + std::sin(angle) * ry * radial};
        light(p,
              6 + phase * 9,
              2,
              angle,
              Color(.4f, .75f, 1.4f, strength * std::sin(phase * 3.14159f) * .4f),
              0,
              false);
    }
    // A restrained mist boundary still locates the exact gameplay radius on the ground.
    for (int i = 0; i < 30; ++i)
    {
        const float angle = i * 6.2831853f / 30;
        const Point p = Screen(effect.origin.x + std::cos(angle) * DomainRadius * growth,
                               effect.origin.y + std::sin(angle) * DomainRadius * growth,
                               2);
        light(p,
              22 * growth,
              8 * growth,
              0,
              Color(.15f, .45f, .95f, strength * .22f),
              i + effect.age,
              true);
    }
}

// All passes select the same latest activated realm, so borders and environment cannot disagree.
const SkillEffect* ActiveSpiritRealm() const
{
    if (titleScreen || width < 1 || height < 1)
        return nullptr;
    for (auto it = skillEffects.rbegin(); it != skillEffects.rend(); ++it)
        if (it->slot == 3 && it->age >= DomainOpening && it->age < it->duration)
            return it->set >= 1 && it->set <= 4 ? &*it : nullptr;
    return nullptr;
}

Color SpiritRealmTint(int set) const
{
    const Color colors[] = {Color(.4f, .85f, .95f),
                            Color(.7f, .35f, .24f),
                            Color(.95f, .73f, .32f),
                            Color(.64f, .38f, .92f)};
    return colors[set - 1];
}

float SpiritRealmCollapse(const SkillEffect& effect) const
{
    return (std::max)(0.f, (std::min)(1.f, (effect.age - effect.duration + .65f) / .65f));
}

float SpiritRealmRadius(const SkillEffect& effect, Point center) const
{
    const float elapsed = effect.age - DomainOpening;
    const float t = (std::max)(0.f, (std::min)(1.f, elapsed / .38f));
    const float growth = t * t * (3 - 2 * t);
    const float dx = (std::max)(std::abs(center.x), std::abs(width - center.x));
    const float dy = (std::max)(std::abs(center.y), std::abs(height - center.y));
    const float coverage = std::sqrt(dx * dx + dy * dy * 1.44f) * 1.035f;
    return (12 + coverage * growth) * (1 + SpiritRealmCollapse(effect) * .1f);
}

// The interior grows from the caster as an ellipse, rather than fading a screen-sized panel.
void DrawSpiritRealm()
{
    const auto* realm = ActiveSpiritRealm();
    if (!realm)
        return;
    const auto& effect = *realm;
    const float elapsed = effect.age - DomainOpening;
    const Point center = Screen(effect.origin.x, effect.origin.y, 36);
    const float radius = SpiritRealmRadius(effect, center);
    const float collapse = SpiritRealmCollapse(effect);
    const float phase = collapse > 0 ? -1 - collapse : elapsed;
    spiritRealms[effect.set - 1].DrawRealm(
        r, center, radius, radius / 1.2f, 1 - collapse * .25f, phase);
}

// Sparse foreground cues put the combatants inside the realm; labels and HUD draw afterward.
void DrawSpiritRealmShell()
{
    const auto* realm = ActiveSpiritRealm();
    if (!realm)
        return;
    const auto& effect = *realm;
    const float elapsed = effect.age - DomainOpening;
    const float collapse = SpiritRealmCollapse(effect);
    const float fade = 1 - collapse;
    const float enter = (std::min)(1.f, elapsed / .38f);
    const Point center = Screen(effect.origin.x, effect.origin.y, 36);
    const float radius = SpiritRealmRadius(effect, center);
    const Color tint = SpiritRealmTint(effect.set);
    // The dome's visible edge breaks into wisps, never a latitude/longitude wire frame.
    for (int i = 0; i < 28; ++i)
    {
        const float angle = i * 6.2831853f / 28;
        const Point p = {center.x + std::cos(angle) * radius,
                         center.y + std::sin(angle) * radius / 1.2f};
        r.EnergyParticle(p,
                         24 + enter * 15,
                         8,
                         angle + 1.57f,
                         Color(tint.r, tint.g, tint.b, fade * (.2f + (1 - enter) * .35f)),
                         elapsed + i,
                         true);
    }
    // One bounded particle population, with different motion/material for each inner world.
    for (int i = 0; i < 32; ++i)
    {
        const float phase = std::fmod(i * .137f + elapsed * (effect.set == 1 ? .18f : .1f), 1.f);
        const float x =
            static_cast<float>(std::fmod(i * 197.f + std::sin(elapsed + i) * 18 + width, width));
        const float y = effect.set == 3 ? height * (1 - phase) : height * phase;
        const Point p = {x, y};
        const float dx = (p.x - center.x) / radius;
        const float dy = (p.y - center.y) / (radius / 1.2f);
        if (dx * dx + dy * dy > .96f)
            continue;
        const float alpha = enter * fade * (.18f + .22f * std::sin(phase * 3.14159f));
        if (effect.set == 1)
            r.EnergyParticle(p, 1.6f + i % 3, 1.4f, 0, Color(.8f, .95f, 1, alpha), 0, false);
        else if (effect.set == 2)
        {
            const float swing = std::sin(elapsed * 2 + i) * 3;
            r.Line({x - swing, y - 8}, {x + swing, y + 8}, 2, Color(.9f, .85f, .7f, alpha));
        }
        else if (effect.set == 3)
            r.EnergyParticle(p, 2, 8, .2f, Color(1.f, .8f, .35f, alpha), elapsed + i, false);
        else
            r.Line({x - 3, y - 4}, {x + 2, y + 3}, 2, Color(.7f, .45f, 1, alpha));
    }
    // Releasing barrier fragments move outward during the final .65s.
    if (collapse > 0)
    {
        for (int i = 0; i < 16; ++i)
        {
            const float angle = i * 2.39996f;
            const float spread = 35 + collapse * (120 + i % 4 * 30);
            r.EnergyParticle(
                {center.x + std::cos(angle) * spread, center.y + std::sin(angle) * spread * .65f},
                18 * fade,
                4 * fade,
                angle,
                Color(tint.r, tint.g, tint.b, fade * .35f),
                elapsed + i,
                true);
        }
    }
}

// Screen-space portrait uses the same skill clock as the domain, including pause/reset.
void DrawDomainPortrait()
{
    if (titleScreen)
    {
        return;
    }
    if (const auto* realm = ActiveSpiritRealm())
    {
        const float elapsed = realm->age - DomainOpening;
        const float alpha =
            (std::max)(0.f, (std::min)(1.f, (std::min)(elapsed / .2f, (1.35f - elapsed) / .3f)));
        if (alpha > .001f)
        {
            const char* names[] = {
                "눈에 묻힌 이름", "돌아오지 못한 길", "응답하는 천 개의 손", "기도가 끊긴 종탑"};
            const Color tint = SpiritRealmTint(realm->set);
            const float x = width * .5f - 135, y = height * .17f;
            r.Rect(x, y, 270, 54, Color(.015f, .02f, .035f, alpha * .7f));
            r.Text(x + 103, y + 19, "영역 전개", Color(tint.r, tint.g, tint.b, alpha));
            r.Text(x + 25, y + 41, names[realm->set - 1], Color(.95f, .95f, 1, alpha));
        }
    }
    for (const auto& effect : skillEffects)
    {
        if (effect.slot != 3 || effect.age > DomainOpening + .18f)
        {
            continue;
        }
        SpriteSheet& portrait = effect.set == 0 ? domainPortrait : spiritPortraits[effect.set - 1];
        if (!portrait.Ready())
            continue;
        const float age = effect.age;
        const float enter = (std::min)(1.f, age / .18f);
        const float ease = 1.f - std::pow(1.f - enter, 3.f);
        const float alpha = (std::max)(0.f, (std::min)(enter, (DomainOpening - age) / .24f));
        const float scale = 1.f + .035f * (std::min)(1.f, age);
        const float w = (std::min)(float(width) * .70f, float(height) * 1.12f) * scale;
        const float h = w * (359.f / 640.f);
        const float x = float(width) - w - float(width) * .025f + (1.f - ease) * w;
        const float y = float(height) * .16f - h * (scale - 1.f) * .5f;
        r.Rect(0, 0, float(width), float(height), Color(.015f, .025f, .06f, alpha * .30f));
        r.Rect(x - 4, y - 4, w + 8, h + 8, Color(.06f, .15f, .23f, alpha));
        portrait.DrawPanel(r, x, y, w, h, alpha);
        // Diagonal corner accents leave the face and hand in the original composition.
        r.Triangle({x, y}, {x + w * .12f, y}, {x, y + h * .22f}, Color(.02f, .05f, .09f, alpha));
        r.Triangle({x + w, y + h},
                   {x + w * .88f, y + h},
                   {x + w, y + h * .78f},
                   Color(.02f, .05f, .09f, alpha));
        r.Line({x + w * .12f, y}, {x + w, y}, 2, Color(.3f, .85f, 1, alpha));
        r.Line({x, y + h}, {x + w * .88f, y + h}, 2, Color(.3f, .85f, 1, alpha));
        const float gleam = alpha * (.55f + .45f * std::sin((std::min)(1.f, age / .8f) * 3.14159f));
        for (float eye : {274.f / 640.f, 370.f / 640.f})
        {
            if (effect.set != 0)
                continue;
            const Point p = {x + w * eye, y + h * (191.f / 359.f)};
            r.Ellipse(p.x, p.y, w * .018f, h * .024f, Color(.1f, .8f, 1, gleam * .35f));
            r.Line({p.x - w * .026f, p.y}, {p.x + w * .026f, p.y}, 1.4f, Color(.55f, 1, 1, gleam));
        }
        const float flash = (std::max)(0.f, 1.f - std::abs(age - DomainOpening) / .12f);
        r.Rect(0, 0, float(width), float(height), Color(.55f, .85f, 1, flash * .26f));
        break;
    }
}
