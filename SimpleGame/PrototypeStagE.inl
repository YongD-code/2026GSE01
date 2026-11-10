void StartStagE(SkillEffect& effect)
{
    const auto origin = effect.origin;
    const double length = Distance(origin, effect.target);
    const double reach = (std::min)(length, double(StagSkill::LeapReach));
    const int steps = (std::max)(1, int(std::ceil(reach / 4)));
    const auto aim = effect.target;
    effect.target = origin;
    for (int i = 1; i <= steps && length > .001; ++i)
    {
        const double t = reach * i / (length * steps);
        const WorldPoint at = {origin.x + (aim.x - origin.x) * t,
                               origin.y + (aim.y - origin.y) * t};
        if (Blocked(at))
        {
            break;
        }
        effect.target = at;
    }
    effect.position = origin;
    effect.duration = StagSkill::LeapDuration;
}

const SkillEffect* StagECast() const
{
    for (auto it = skillEffects.rbegin(); it != skillEffects.rend(); ++it)
    {
        if (IsStagE(*it) && it->caster == selectedCompanion &&
            it->age < StagSkill::LeapAnimation)
        {
            return &*it;
        }
    }
    return nullptr;
}

void UpdateStagE(SkillEffect& effect)
{
    if (effect.applied)
    {
        return;
    }
    if (!controlling || !captured || selectedCompanion != effect.caster ||
        capturedSpirit != SpiritCatalog::Stag)
    {
        effect.age = effect.duration;
        return;
    }
    const float t = (std::max)(0.f, (std::min)(1.f, (effect.age - StagSkill::LeapCharge) /
                            (StagSkill::LeapLand - StagSkill::LeapCharge)));
    const WorldPoint at = {effect.origin.x + (effect.target.x - effect.origin.x) * t,
                           effect.origin.y + (effect.target.y - effect.origin.y) * t};
    auto blocked = [this](WorldPoint p)
    {
        return Blocked(p);
    };
    if (LevelOne::ClearLine(spirit, at, blocked))
    {
        spirit = at;
    }
    if (t < 1)
    {
        return;
    }
    effect.applied = true;
    effect.position = effect.target = spirit;
    for (auto& enemy : levelOne.enemies)
    {
        if (enemy.health > 0 && !enemy.weakened && !enemy.returning &&
            Distance(spirit, enemy.position) <= StagSkill::LeapRadius &&
            LevelOne::ClearLine(spirit, enemy.position, blocked))
        {
            enemy.slowed = (std::max)(enemy.slowed, 1.f);
        }
    }
}

void DrawStagE(const SkillEffect& effect)
{
    if (!effect.applied || !CharacterInView(effect.position, 100))
    {
        return;
    }
    const float t = (std::max)(0.f, (std::min)(1.f, (effect.age - StagSkill::LeapLand) /
                             (StagSkill::LeapDuration - StagSkill::LeapLand)));
    for (int i = 0; i < 29; ++i)
    {
        const float angle = i * 2.39996f + .17f * std::sin(i * 3.1f);
        const float distance = (15 + (i * 13 % 48)) * t;
        const float height = 3 + (4 + i % 12) * 4 * t * (1 - t);
        const Point p = Screen(effect.position.x + std::cos(angle) * distance,
                               effect.position.y + std::sin(angle) * distance, height);
        const float length = (4 + i % 5) * (1 - .45f * t);
        const float ax = std::cos(angle + t) * length, ay = std::sin(angle + t) * length * .7f;
        const float w = i < 18 ? .23f : .38f;
        const Point tip = {p.x + ax, p.y + ay - 2};
        const Point ridge = {p.x + ax * .48f, p.y + ay * .48f - 2};
        const Point left = {ridge.x - ay * w, ridge.y + ax * w + 1};
        const Point right = {ridge.x + ay * w, ridge.y - ax * w + 1};
        const float alpha = 1 - t * t;
        const Color light = i < 18 ? Color(.34f, .52f, .15f, alpha)
                                   : Color(1.f, .85f, .46f, alpha);
        const Color dark = Color(light.r * .72f, light.g * .78f, light.b * .7f, alpha);
        r.Triangle(p, left, ridge, dark);
        r.Triangle(ridge, left, tip, dark);
        r.Triangle(p, ridge, right, light);
        r.Triangle(ridge, tip, right, light);
    }
}
