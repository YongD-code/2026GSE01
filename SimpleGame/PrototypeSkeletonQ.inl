WorldPoint SkeletonQAim(const SkillEffect& effect) const
{
    const double length = (std::max)(.001, Distance(effect.origin, effect.target));
    return {(effect.target.x - effect.origin.x) / length,
            (effect.target.y - effect.origin.y) / length};
}

void StartSkeletonQ(SkillEffect& effect)
{
    effect.duration = SkeletonSkill::Duration;
    effect.nextPulse = SkeletonSkill::Charge;
    const auto aim = SkeletonQAim(effect);
    for (int shard = 0; shard < 3; ++shard)
    {
        const auto direction = SkeletonSkill::Direction(aim, shard);
        WorldPoint previous = effect.origin;
        for (int segment = 0; segment <= 48; ++segment)
        {
            const auto next = SkeletonSkill::Point(effect.origin, aim, shard, segment / 48.f);
            bool clear = true;
            for (int edge = -1; edge <= 1 && clear; ++edge)
            {
                const WorldPoint offset = {-direction.y * SkeletonSkill::Radius * edge,
                                           direction.x * SkeletonSkill::Radius * edge};
                clear = LevelOne::ClearLine({previous.x + offset.x, previous.y + offset.y},
                                            {next.x + offset.x, next.y + offset.y},
                                            [this](WorldPoint at)
                                            {
                                                return Blocked(at);
                                            });
            }
            if (!clear)
            {
                effect.branchEnds[shard] = (std::max)(0.f, (segment - 1) / 48.f);
                break;
            }
            previous = next;
        }
    }
}

const SkillEffect* SkeletonQCast() const
{
    for (auto it = skillEffects.rbegin(); it != skillEffects.rend(); ++it)
    {
        if (it->caster != selectedCompanion || it->species != SpiritCatalog::Skeleton)
        {
            continue;
        }
        return IsSkeletonQ(*it) && it->age < SkeletonSkill::Animation ? &*it : nullptr;
    }
    return nullptr;
}

void UpdateSkeletonQ(SkillEffect& effect, float dt)
{
    if (effect.age < SkeletonSkill::Charge ||
        effect.age - dt >= SkeletonSkill::Charge + SkeletonSkill::Travel)
    {
        return;
    }
    const auto aim = SkeletonQAim(effect);
    for (int shard = 0; shard < 3; ++shard)
    {
        const float before =
            (std::min)(effect.branchEnds[shard], SkeletonSkill::Progress(effect.age - dt));
        const float after =
            (std::min)(effect.branchEnds[shard], SkeletonSkill::Progress(effect.age));
        if (after <= before)
        {
            continue;
        }
        const auto a = SkeletonSkill::Point(effect.origin, aim, shard, before);
        const auto b = SkeletonSkill::Point(effect.origin, aim, shard, after);
        LevelOne::Enemy* nearest = nullptr;
        double contact = 2;
        for (auto& enemy : levelOne.enemies)
        {
            if (enemy.health <= 0 || enemy.weakened || enemy.returning)
            {
                continue;
            }
            const double t = SkeletonSkill::Contact(enemy.position, a, b);
            if (t >= 0 && t < contact &&
                LevelOne::ClearLine(
                    a,
                    enemy.position,
                    [this](WorldPoint at)
                    {
                        return Blocked(at);
                    }))
            {
                contact = t;
                nearest = &enemy;
            }
        }
        if (!nearest)
        {
            continue;
        }
        effect.branchEnds[shard] = before + (after - before) * static_cast<float>(contact);
        // A second shard still breaks on the same enemy, but cannot deal damage twice.
        if (std::any_of(effect.struck.begin(),
                        effect.struck.end(),
                        [&](WorldPoint home)
                        {
                            return Distance(home, nearest->home) < .01;
                        }))
        {
            continue;
        }
        effect.struck.push_back(nearest->home);
        levelOne.DamageEnemy(
            *nearest, levelOne.Damage() * SkeletonSkill::Damage, effect.origin, true);
        if (nearest->health > 0 && !nearest->weakened && nearest->boss == 0)
        {
            nearest->stagger = (std::max)(nearest->stagger, SkeletonSkill::Stagger);
            nearest->phase = 0;
            nearest->phaseTime = 0;
        }
    }
}

void DrawSkeletonShard(
    WorldPoint at, WorldPoint direction, int shard, float elevation, float scale, float alpha)
{
    if (alpha <= 0 || !CharacterInView(at, 100))
    {
        return;
    }

    struct Face
    {
        Point a, b, c;
        float depth;
        Color color = Color(1, 1, 1);
    };

    std::array<Face, SkeletonShardMesh::MaxFaces> faces;
    const auto range = SkeletonShardMesh::Ranges[shard];
    size_t count = 0;
    for (int index = range.first; index < range.first + range.count; index += 3)
    {
        WorldPoint world[3];
        float height[3];
        Point screen[3];
        for (int corner = 0; corner < 3; ++corner)
        {
            const auto& vertex = SkeletonShardMesh::Vertices[index + corner];
            world[corner] = {at.x + (direction.x * vertex.x - direction.y * vertex.y) * scale,
                             at.y + (direction.y * vertex.x + direction.x * vertex.y) * scale};
            height[corner] = elevation + vertex.z * scale;
            screen[corner] = Screen(world[corner].x, world[corner].y, height[corner]);
        }
        const double ux = world[1].x - world[0].x, uy = world[1].y - world[0].y;
        const double uz = height[1] - height[0];
        const double vx = world[2].x - world[0].x, vy = world[2].y - world[0].y;
        const double vz = height[2] - height[0];
        const double nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
        const double normalLength = (std::max)(.000001, std::sqrt(nx * nx + ny * ny + nz * nz));
        const float light =
            .62f + .38f * static_cast<float>(
                              (std::max)(0., (-nx * .35 - ny * .45 + nz * .82) / normalLength));
        const auto& vertex = SkeletonShardMesh::Vertices[index];
        faces[count++] = {screen[0],
                          screen[1],
                          screen[2],
                          static_cast<float>((world[0].x + world[0].y + height[0] * 1.5 +
                                              world[1].x + world[1].y + height[1] * 1.5 +
                                              world[2].x + world[2].y + height[2] * 1.5) /
                                             3),
                          Color(vertex.r * light, vertex.g * light, vertex.b * light, alpha)};
    }
    std::sort(faces.begin(),
              faces.begin() + count,
              [](const Face& a, const Face& b)
              {
                  return a.depth < b.depth;
              });
    for (size_t face = 0; face < count; ++face)
    {
        r.Triangle(faces[face].a, faces[face].b, faces[face].c, faces[face].color);
    }
}

void DrawSkeletonQShard(WorldPoint origin, WorldPoint aim, int shard, float age, float end)
{
    const auto direction = SkeletonSkill::Direction(aim, shard);
    const float progress = (std::min)(end, SkeletonSkill::Progress(age));
    const float stopAge = SkeletonSkill::Charge + end * SkeletonSkill::Travel;
    const auto at = SkeletonSkill::Point(origin, aim, shard, progress);
    if (!CharacterInView(at, 100))
    {
        return;
    }
    if (age < stopAge)
    {
        const float gather = (std::min)(1.f, age / SkeletonSkill::Charge);
        const float scale = .55f + .45f * gather;
        const float elevation = SkeletonSkill::Height + (shard - 1) * 5 * (1 - progress);
        r.EnergyParticle(Screen(at.x, at.y, elevation),
                         14 * scale,
                         5 * scale,
                         0,
                         Color(.16f, .72f, .78f, gather * .18f),
                         age + shard,
                         true);
        if (age >= SkeletonSkill::Charge)
        {
            for (int trail = 1; trail <= 4; ++trail)
            {
                const float previous = (std::max)(0.f, progress - trail * .022f);
                const auto behind = SkeletonSkill::Point(origin, aim, shard, previous);
                r.EnergyParticle(Screen(behind.x, behind.y, elevation),
                                 9 - trail,
                                 3,
                                 0,
                                 Color(.22f, .55f, .58f, (5 - trail) * .035f),
                                 age * 3 + trail,
                                 true);
            }
        }
        DrawSkeletonShard(at, direction, shard, elevation, scale, gather);
        return;
    }
    const float burst = (age - stopAge) / SkeletonSkill::Fade;
    if (burst >= 1)
    {
        return;
    }
    const float alpha = 1 - burst;
    for (int chip = 0; chip < 6; ++chip)
    {
        const double angle = chip * 2.39996 + shard;
        const float travel = 7 + burst * (18 + chip * 2);
        const WorldPoint broken = {at.x + std::cos(angle) * travel,
                                   at.y + std::sin(angle) * travel};
        const float elevation =
            SkeletonSkill::Height + std::sin(angle) * travel * .6f - burst * burst * 24;
        const Point p = Screen(broken.x, broken.y, elevation);
        const float size = (2 + chip % 3) * alpha;
        r.Triangle({p.x - size, p.y + size * .4f},
                   {p.x + size * .6f, p.y - size},
                   {p.x + size, p.y + size * .6f},
                   Color(.78f, .72f, .57f, alpha));
        r.EnergyParticle(p,
                         4,
                         2,
                         static_cast<float>(angle),
                         Color(.2f, .78f, .84f, alpha * .28f),
                         age + chip,
                         true);
    }
}

void DrawSkeletonQ(const SkillEffect& effect)
{
    if (effect.age >= SkeletonSkill::Charge || !CharacterInView(effect.origin, 110))
    {
        return;
    }
    const auto aim = SkeletonQAim(effect);
    const float charge = effect.age / SkeletonSkill::Charge;
    for (int wisp = 0; wisp < 7; ++wisp)
    {
        const double angle = wisp * 2.39996 + effect.age * 2;
        const float radius = (1 - charge) * 19 + 3;
        const WorldPoint at = {effect.origin.x + aim.x * 15 + std::cos(angle) * radius,
                               effect.origin.y + aim.y * 15 + std::sin(angle) * radius};
        r.EnergyParticle(Screen(at.x, at.y, SkeletonSkill::Height + std::sin(angle) * radius),
                         5,
                         2,
                         static_cast<float>(angle),
                         Color(.2f, .75f, .8f, charge * .32f),
                         effect.age + wisp,
                         true);
    }
}
