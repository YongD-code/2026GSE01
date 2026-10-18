// Recognizable spectral faces layered with soft smoke, not projectile blobs.
void DrawGhostFace(Point p, float size, float alpha, float phase)
{
    r.EnergyParticle(p, size, size * 1.2f, 0, Color(.12f, .75f, .6f, alpha * .3f), phase, true);
    if (ghostFace.Ready())
    {
        ghostFace.DrawEffect(r, p, size * 2, size * 2, alpha, phase);
    }
}

void DrawForgottenSkill(const SkillEffect& effect)
{
    const float age = effect.age;
    const float fade = (std::max)(0.f, (std::min)(1.f, (effect.duration - age) * 3));
    const WorldPoint center = effect.slot == 2 && effect.caster == selectedCompanion
                                  ? spirit
                                  : SkillAreaCenter(effect, true);
    const Point p = Screen(center.x, center.y, 30);
    if (effect.slot == 0)
    {
        const double direction =
            std::atan2(effect.target.y - effect.origin.y, effect.target.x - effect.origin.x);
        const float charge = (std::min)(1.f, age / .45f);
        const float appear = (std::min)(1.f, age / .12f);
        const float release = (std::max)(0.f, (std::min)(1.f, (age - .75f) / .35f));
        const float burst = age >= .45f ? std::exp(-(age - .45f) * 12.f) : 0.f;
        Point face = Screen(effect.origin.x, effect.origin.y, 48 + charge * 12 - release * 8);
        face.x += std::sin(age * 77) * burst * 2;
        const float size = 84 + charge * 22 + burst * 12;
        if (ghostScreamPoses.Ready())
        {
            // Closed mouth -> inhale -> scream at the .45s hit -> relaxed jaw.
            float pose = age < .22f   ? age / .22f
                         : age < .45f ? 1 + (age - .22f) / .23f
                                      : 2 + release;
            const int first = (std::min)(3, static_cast<int>(pose));
            const int next = (std::min)(3, first + 1);
            float blend = pose - first;
            blend = blend * blend * (3 - 2 * blend);
            ghostScreamPoses.DrawPoseTile(r, first, face, size, fade * appear * (1 - blend));
            ghostScreamPoses.DrawPoseTile(r, next, face, size, fade * appear * blend);
        }
        else
        {
            DrawGhostFace(face, size * .5f, fade * appear, age * 3);
        }
        r.EnergyParticle({face.x, face.y + size * .14f},
                         12 + burst * 30,
                         7 + burst * 14,
                         0,
                         Color(.4f, 1.f, .92f, burst * .8f),
                         age * 4,
                         true);
        // Wisps converge during the breath; three broken wave fronts leave on the scream.
        for (int i = 0; i < 18 && age < .45f; ++i)
        {
            const float angle = i * 2.39996f + age;
            const float reach = 12 + (1 - charge) * (45 + i % 4 * 10);
            r.EnergyParticle(
                {face.x + std::cos(angle) * reach, face.y + std::sin(angle) * reach * .65f},
                10,
                4,
                angle,
                Color(.2f, .8f, .7f, appear * charge * .45f),
                age * 3 + i,
                true);
        }
        for (int wave = 0; wave < 3; ++wave)
        {
            const float local = age - .45f - wave * .085f;
            if (local < 0 || local > .48f)
            {
                continue;
            }
            const float travel = (std::min)(1.f, local / .28f);
            const float reach = 20 + travel * 300;
            const float opacity = (1 - local / .48f) * fade;
            for (int i = 0; i < 24; ++i)
            {
                const float side = i / 23.f;
                const double angle = direction + (side - .5f) * 1.35;
                const float ripple = 1 + .025f * std::sin(i * 2.1f + age * 31);
                const Point at = Screen(effect.origin.x + std::cos(angle) * reach * ripple,
                                        effect.origin.y + std::sin(angle) * reach * ripple,
                                        28);
                r.EnergyParticle(at,
                                 12 + travel * 12,
                                 5 + travel * 5,
                                 float(angle),
                                 Color(.25f, .95f, .83f, opacity * .52f),
                                 age * 4 + i + wave * 7,
                                 true);
            }
        }
        return;
    }
    const float appear = (std::min)(1.f, age / .22f);
    auto apparition = [&](int variant, Point at, float w, float h, float alpha)
    {
        if (ghostVariants[variant].Ready())
            ghostVariants[variant].DrawEffect(
                r, {at.x, at.y - h * .5f}, w, h, alpha, age + variant);
    };
    if (effect.slot == 1)
    {
        // Grasping hands rise at the target and close inward rather than orbiting faces.
        const float close = (std::min)(1.f, age / .55f);
        apparition(
            0, {p.x, p.y + 24 + (1 - appear) * 25}, 135 - 20 * close, 115 * appear, fade * appear);
        for (int i = 0; i < 18; ++i)
        {
            const float angle = i * 2.39996f - age;
            const float radius = (1 - close) * 65 + 22;
            r.EnergyParticle(
                {p.x + std::cos(angle) * radius, p.y + std::sin(angle) * radius * .45f},
                14,
                6,
                angle,
                Color(.13f, .8f, .65f, fade * .4f),
                age + i,
                true);
        }
    }
    else if (effect.slot == 2)
    {
        // A sparse, trailing profile instead of a ring of identical heads.
        const Point source = Screen(effect.origin.x, effect.origin.y, 30);
        for (int i = 2; i >= 0; --i)
        {
            const float t = (i + 1) / 4.f;
            const Point at = {p.x + (source.x - p.x) * t - 12 * i, p.y + (source.y - p.y) * t + 16};
            apparition(1, at, 92 - i * 12.f, 68 - i * 8.f, fade * appear * (.44f - i * .1f));
        }
    }
    else
    {
        // Different silhouettes and staggered arrivals make this a gathering, not a face wheel.
        for (int i = 0; i < 5; ++i)
        {
            const float local = (std::max)(0.f, age - i * .13f);
            const float rise = (std::min)(1.f, local / .45f);
            const float angle = i * 2.39996f + .25f;
            const Point at = Screen(center.x + std::cos(angle) * 135,
                                    center.y + std::sin(angle) * 135,
                                    10 + rise * 28 + std::sin(age * 1.7f + i) * 5);
            const int variant = i % 3;
            apparition(variant,
                       at,
                       variant == 2 ? 136.f : 84.f,
                       variant == 2 ? 106.f : 94.f,
                       fade * rise * .7f);
        }
        for (int i = 0; i < 24; ++i)
        {
            const float a = i * 2.39996f + age * .2f;
            const Point at = Screen(center.x + std::cos(a) * 155, center.y + std::sin(a) * 155, 8);
            r.EnergyParticle(at, 27, 11, a, Color(.1f, .65f, .6f, fade * .28f), age + i, true);
        }
    }
}

// Each species has four authored silhouettes, animated on its actual gameplay timeline.
void DrawSpiritSkill(const SkillEffect& effect)
{
    if (effect.set == 1)
    {
        DrawForgottenSkill(effect);
        return;
    }
    if (effect.set < 2 || effect.set > 4)
        return;
    const float age = effect.age;
    const float fade = (std::max)(0.f, (std::min)(1.f, (effect.duration - age) / .3f));
    const float enter = (std::min)(1.f, age / .18f);
    const Color colors[] = {Color(.2f, .6f, 1), Color(1, .7f, .18f), Color(.65f, .25f, 1)};
    const Color tint = colors[effect.set - 2];
    auto art = [&](int tile, WorldPoint at, float elevation, float size, float alpha)
    {
        spiritSpellArt[effect.set - 2].DrawSpellTile(r,
                                                     tile,
                                                     Screen(at.x, at.y, elevation),
                                                     size * (1.f + .035f * std::sin(age * 6)),
                                                     size,
                                                     alpha,
                                                     age,
                                                     effect.set == 2 && tile == 0 ? age * 9.f
                                                     : effect.set == 4 && tile == 3
                                                         ? std::sin(age * 6) * .09f
                                                         : 0.f);
    };
    auto mist = [&](WorldPoint at, float radius, float alpha, int count)
    {
        for (int i = 0; i < count; ++i)
        {
            const float a = i * 2.39996f + age * .3f;
            const Point p = Screen(at.x + std::cos(a) * radius,
                                   at.y + std::sin(a) * radius,
                                   12 + std::sin(age * 2 + i) * 5);
            r.EnergyParticle(p, 22, 10, a, Color(tint.r, tint.g, tint.b, alpha), age * 2 + i, true);
        }
    };
    WorldPoint center = SkillAreaCenter(effect, true);
    if (effect.slot == 0)
    {
        if (effect.set == 2)
        {
            const WorldPoint at = SpiritQPosition(effect);
            const float size = age < .18f ? 30 + enter * 45 : 82;
            art(0, at, 28, size, fade);
            // Returning blade leaves a tapered ribbon of particles along recorded travel.
            for (size_t i = 0; i < effect.visualTrail.size(); i += 2)
            {
                const Point p = Screen(effect.visualTrail[i].x, effect.visualTrail[i].y, 25);
                const float t = float(i + 1) / effect.visualTrail.size();
                r.EnergyParticle(
                    p, 15, 5, age, Color(.25f, .6f, 1, t * fade * .45f), age + i, false);
            }
        }
        else if (effect.set == 3)
        {
            const float charge = (std::min)(1.f, age / .45f);
            const float thrust = (std::max)(0.f, (std::min)(1.f, (age - .45f) / .2f));
            const WorldPoint at = {effect.origin.x + (effect.target.x - effect.origin.x) * thrust,
                                   effect.origin.y + (effect.target.y - effect.origin.y) * thrust};
            art(0, at, 35, 65 + charge * 70, fade);
            if (age >= .45f)
                mist(at, 35 + thrust * 40, fade * .4f, 18);
        }
        else
        {
            const double dx = effect.target.x - effect.origin.x,
                         dy = effect.target.y - effect.origin.y;
            const double length = (std::max)(1., std::hypot(dx, dy));
            for (int i = 0; i < 3; ++i)
            {
                const float strike = .55f + i * .25f;
                const float flash =
                    age < strike ? .35f : (std::max)(0.f, 1 - (age - strike) / .35f);
                const WorldPoint at = {effect.target.x - dy / length * (i - 1) * 85,
                                       effect.target.y + dx / length * (i - 1) * 85};
                art(0, at, 18, 80 + flash * 28, fade * flash);
                if (age >= strike && age < strike + .35f)
                    mist(at, (age - strike) * 170, flash * .45f, 10);
            }
        }
    }
    else if (effect.slot == 1)
    {
        // Ground warning and rising binding artwork share the 105-unit hit area.
        mist(center, 105, fade * .2f, 20);
        const float rise = (std::min)(1.f, age / .22f);
        art(1, center, 15 + rise * 25, 80 + rise * 65, fade * enter);
        if (effect.set == 2)
        {
            for (int i = 0; i < 3; ++i)
            {
                const double a = i * 2.094;
                const double reach = 65 * (1 - rise);
                art(1,
                    {center.x + std::cos(a) * reach, center.y + std::sin(a) * reach},
                    12,
                    60,
                    fade * .25f);
            }
        }
    }
    else if (effect.slot == 2)
    {
        if (effect.set == 2)
        {
            const size_t stride = (std::max)(size_t(1), effect.visualTrail.size() / 6);
            for (size_t i = 0; i < effect.visualTrail.size(); i += stride)
                art(2,
                    effect.visualTrail[i],
                    8,
                    60,
                    fade * .5f * float(i + 1) / (std::max)(size_t(1), effect.visualTrail.size()));
            const WorldPoint at = effect.caster == selectedCompanion ? spirit : effect.origin;
            art(2, at, 10, 78, fade * .65f);
        }
        else
        {
            art(2, player, 48, 110, fade * enter);
            mist(player, 25, fade * .3f, 12);
            if (effect.set == 3)
            {
                const WorldPoint at = effect.caster == selectedCompanion ? spirit : effect.origin;
                art(3, at, 35, 100, fade * .35f);
            }
        }
    }
    else
    {
        const float pulse = std::fmod(age, .5f) / .5f;
        if (effect.set == 2)
        {
            art(3, center, 48, 175, fade * enter);
            mist(center, 75, fade * .25f, 16);
        }
        else if (effect.set == 3)
        {
            art(3, center, 70, 245 * enter, fade * .72f);
            mist(center, 180, fade * .35f, 24);
        }
        else
        {
            art(3, center, 95 + std::sin(age * 6) * 5, 200, fade * enter);
            mist(center, 180 * pulse, (1 - pulse) * fade * .5f, 24);
        }
    }
    if (effect.impactAge >= 0 && effect.impactAge < .3f)
    {
        for (const auto& at : effect.impacts)
            mist(at, 8 + effect.impactAge * 65, (1 - effect.impactAge / .3f) * .4f, 6);
    }
}
