// Included inside LevelOne. Only encounter 1 uses this state machine.
template <class Collision>
void PrepareEaterAttack(Enemy& enemy, WorldPoint target, Collision blocked, bool followup = false)
{
    auto& state = enemy.eater;
    state.plan = EaterBoss::Describe(state.pattern);
    state.plan.windup *= (std::max)(.65f, (std::min)(2.f, combat.eaterWindupScale));
    if (followup)
    {
        state.plan.windup *= .85f;
    }
    state.plan.damage *= combat.eaterDamageScale;
    state.origin = enemy.position;
    state.hit = false;
    state.hasAttack = true;
    const double distance = Distance(enemy.position, target);
    // Keep a valid facing even when a controlled spirit overlaps the boss.
    if (distance > .001)
    {
        enemy.aim = {(target.x - enemy.position.x) / distance,
                     (target.y - enemy.position.y) / distance};
    }
    else if (std::hypot(enemy.aim.x, enemy.aim.y) < .001)
    {
        enemy.aim = {0, 1};
    }
    state.markCount = 0;
    if (state.pattern == EaterBoss::Pattern::Grief)
    {
        const int count = state.phaseTwo ? 3 : 2;
        for (int i = 0; i < count; ++i)
        {
            const double side = i == 0 ? 0 : i == 1 ? 150 : -150;
            WorldPoint mark = {target.x - enemy.aim.y * side, target.y + enemy.aim.x * side};
            if (arenaBoss == EaterBoss::Encounter)
            {
                const auto room = World::CemeteryBounds();
                mark.x = (std::max)(room.x + 76, (std::min)(room.x + room.w - 76, mark.x));
                mark.y = (std::max)(room.y + 76, (std::min)(room.y + room.d - 76, mark.y));
            }
            bool separated = true;
            for (int existing = 0; existing < state.markCount; ++existing)
            {
                if (Distance(mark, state.marks[existing]) < state.plan.radius * 2 + 18)
                {
                    separated = false;
                    break;
                }
            }
            if (separated && !blocked(mark) && ClearLine(enemy.position, mark, blocked))
            {
                state.marks[state.markCount++] = mark;
            }
        }
    }
    enemy.phase = 1;
    enemy.phaseTime = state.plan.windup;
}

void RecoverEater(Enemy& enemy)
{
    enemy.phase = 3;
    enemy.phaseTime = enemy.eater.plan.recovery;
}

template <class Collision>
void AttackEaterBoss(Enemy& enemy, WorldPoint target, float dt, Collision blocked)
{
    auto& state = enemy.eater;
    if (enemy.phase == 4)
    {
        enemy.phaseTime = (std::max)(0.f, enemy.phaseTime - dt);
        if (enemy.phaseTime <= 0)
        {
            enemy.phase = 3;
            enemy.phaseTime = .8f;
            state.plan.recovery = .8f;
        }
        return;
    }
    if (enemy.phase == 3)
    {
        enemy.phaseTime = (std::max)(0.f, enemy.phaseTime - dt);
        if (enemy.phaseTime <= 0)
        {
            enemy.phase = 0;
        }
        return;
    }
    if (enemy.phase == 1)
    {
        // Red/blue can displace bosses during preparation; keep the warning on the caster.
        state.origin = enemy.position;
        enemy.phaseTime = (std::max)(0.f, enemy.phaseTime - dt);
        if (enemy.phaseTime <= 0)
        {
            enemy.phase = 2;
            enemy.phaseTime = state.plan.active;
        }
        return;
    }
    if (enemy.phase == 2)
    {
        if (state.pattern == EaterBoss::Pattern::Rush && Distance(state.origin, enemy.position) > 2)
        {
            // Forced displacement interrupts a rush rather than opening an unannounced lane.
            RecoverEater(enemy);
            return;
        }
        if (state.pattern == EaterBoss::Pattern::Sweep)
        {
            state.origin = enemy.position;
        }
        // Clip movement and swept damage to the remaining active interval.
        const float step = (std::min)(dt, (std::max)(0.f, enemy.phaseTime));
        const float beforeProgress = EaterBoss::Progress(state.plan.active, enemy.phaseTime);
        enemy.phaseTime = (std::max)(0.f, enemy.phaseTime - step);
        const float progress = EaterBoss::Progress(state.plan.active, enemy.phaseTime);
        bool contact = false, stopped = false;
        WorldPoint damageOrigin = state.origin;
        if (step > 0)
        {
            if (state.pattern == EaterBoss::Pattern::Rush)
            {
                const auto before = enemy.position;
                const double travel = state.plan.speed * step * (enemy.slowed > 0 ? .35 : 1.);
                const int slices = (std::max)(1, static_cast<int>(std::ceil(travel / 5)));
                for (int i = 0; i < slices; ++i)
                {
                    const WorldPoint next = {enemy.position.x + enemy.aim.x * travel / slices,
                                             enemy.position.y + enemy.aim.y * travel / slices};
                    // Rushes stop at walls rather than sliding out of their warning lane.
                    if (blocked(next))
                    {
                        stopped = true;
                        break;
                    }
                    enemy.position = next;
                }
                RecordTrail(enemy.trail, enemy.position, blocked);
                state.origin = enemy.position;
                damageOrigin = enemy.position;
                contact =
                    EaterBoss::SegmentDistance(target, before, enemy.position) <= state.plan.radius;
            }
            else if (state.pattern == EaterBoss::Pattern::Sweep)
            {
                contact = EaterBoss::ConeContains(state.origin, enemy.aim, target, state.plan);
            }
            else if (state.pattern == EaterBoss::Pattern::Scream)
            {
                const double distance = Distance(state.origin, target);
                contact =
                    distance >=
                        EaterBoss::WaveRadius(state.plan, beforeProgress) - state.plan.radius &&
                    distance <= EaterBoss::WaveRadius(state.plan, progress) + state.plan.radius;
            }
            else
            {
                for (int i = 0; i < state.markCount && !contact; ++i)
                {
                    contact = Distance(target, state.marks[i]) <= state.plan.radius;
                    if (contact)
                    {
                        damageOrigin = state.marks[i];
                    }
                }
            }
        }
        if (contact && !state.hit && ClearLine(damageOrigin, target, blocked))
        {
            Hurt(state.plan.damage);
            // A dodge consumes this strike too; the same area cannot hit again on exit.
            state.hit = true;
        }
        if (enemy.phaseTime <= 0 || stopped)
        {
            if (state.followups > 0 && !stopped)
            {
                --state.followups;
                // Reacquire only here, with a fresh, fully visible warning before the next hit.
                PrepareEaterAttack(enemy, target, blocked, true);
            }
            else
            {
                RecoverEater(enemy);
            }
        }
        return;
    }
    // Finish the committed attack and its recovery before switching phases.
    if (!state.phaseTwo && enemy.health <= enemy.maximum * .5f)
    {
        state.phaseTwo = true;
        state.hasAttack = false;
        state.followups = 0;
        state.origin = enemy.position;
        enemy.phase = 4;
        enemy.phaseTime = EaterBoss::TransitionTime;
        return;
    }
    const double distance = Distance(enemy.position, target);
    if (distance <= 420 && ClearLine(enemy.position, target, blocked))
    {
        state.pattern = EaterBoss::Select(state, distance);
        state.lastUse[static_cast<int>(state.pattern)] = state.sequence++;
        state.followups = state.phaseTwo && (state.pattern == EaterBoss::Pattern::Sweep ||
                                             state.pattern == EaterBoss::Pattern::Scream)
                              ? 1
                              : 0;
        PrepareEaterAttack(enemy, target, blocked);
        return;
    }
    if (distance > 132)
    {
        const double step = (std::min)(
            distance - 132, double(combat.chaseSpeed * dt * (enemy.slowed > 0 ? .35 : 1.)));
        Move(enemy.position,
             {(target.x - enemy.position.x) / distance * step,
              (target.y - enemy.position.y) / distance * step},
             blocked,
             &enemy.trail);
    }
}
