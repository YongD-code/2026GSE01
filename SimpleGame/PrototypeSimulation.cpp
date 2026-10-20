#include "stdafx.h"
#include "Prototype.h"
#include "FrameProfiler.h"

void Prototype::UpdateSimulation(float dt, bool sprint)
{
    if (openingPlaying || titleScreen || slotMode != SlotMode::Closed)
    {
        StopMovement();
        bufferedCommand.action = -1;
        attackHeld = false;
        time += (std::min)(dt, .05f);
        return;
    }
    dt = (std::max)(0.f, (std::min)(dt, .05f));
    const WorldPoint previousPlayer = player;
    const WorldPoint previousSpirit = spirit;
    time += dt;
    const float inputDt = dt;
    if (ControlsActive())
    {
        levelOne.UpdateHitFeedback(dt);
        const float stopped = (std::min)(dt, levelOne.hitStop);
        levelOne.hitStop -= stopped;
        dt -= stopped;
    }
    else
    {
        bufferedCommand.action = -1;
    }
    saveStatusTime = (std::max)(0.f, saveStatusTime - dt);
    if (!pauseMenu && !journal)
    {
        autoSaveTime += dt;
    }
    if (!dialoguePending && !journal && !pauseMenu)
        messageTime = (std::max)(0.f, messageTime - dt);
    moving = false;
    const bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    const bool dodgePressed = ctrl && !dodgeHeld;
    dodgeHeld = ctrl;
    if (ControlsActive())
    {
        // Release preparation and replay queued input before deciding this frame's movement.
        navigationWait = (std::max)(0.f, navigationWait - dt);
        UpdateSkills(dt);
        UpdateBufferedCommand(inputDt);
    }
    if (ControlsActive() && !PreparingSkill())
    {
        if (movementWasControlling != controlling)
        {
            StopMovement();
        }
        movementWasControlling = controlling;
        UpdateMoveRoute(dt);
        WorldPoint& actor = controlling ? spirit : player;
        WorldPoint dodgeDirection = {0, 0};
        if (routeIndex < moveRoute.size())
        {
            dodgeDirection = {moveRoute[routeIndex].x - actor.x, moveRoute[routeIndex].y - actor.y};
        }
        if (dodgePressed)
        {
            const WorldPoint aim = CursorWorld();
            dodgeDirection = {aim.x - player.x, aim.y - player.y};
        }
        const bool dodging =
            !controlling &&
            (levelOne.dodgeTime > 0 || (dodgePressed && levelOne.dodgeCooldown <= 0 &&
                                        std::hypot(dodgeDirection.x, dodgeDirection.y) > .001));
        if (!controlling)
            levelOne.Dodge(dt,
                           player,
                           dodgeDirection,
                           dodgePressed,
                           [this](WorldPoint p)
                           {
                               return Blocked(p);
                           });
        if (!dodging && routeIndex < moveRoute.size())
        {
            const double x = moveRoute[routeIndex].x - actor.x;
            const double y = moveRoute[routeIndex].y - actor.y;
            const double remaining = std::hypot((x - y) * .85, (x + y) * .43);
            const float pace = sprint ? 1.8f : 1.f;
            float speed = (controlling ? 110.f : World::PlayerMoveSpeed) * pace *
                          (spiritHasteTime > 0 ? 1.5f : 1.f);
            if (!controlling && InsideDomain(player))
            {
                speed *= 1.5f;
            }
            if (moveRouteComplete && routeIndex + 1 == moveRoute.size())
            {
                speed = (std::min)(speed, static_cast<float>(std::sqrt(2 * 1800 * remaining)));
            }
            // Change heading immediately; only speed eases in. Gait still follows actual travel.
            const double before = std::hypot(movementVelocity.x, movementVelocity.y);
            const double decay = std::exp(-40.0 * dt);
            const double currentSpeed = speed + (before - speed) * decay;
            double travel = (std::max)(0.0, speed * dt + (before - speed) * (1 - decay) / 40.0);
            bool collided = false;
            for (int segment = 0; segment < 8 && routeIndex < moveRoute.size(); ++segment)
            {
                const WorldPoint waypoint = moveRoute[routeIndex];
                const double wx = waypoint.x - actor.x, wy = waypoint.y - actor.y;
                const double sx = (wx - wy) * .85, sy = (wx + wy) * .43;
                const double distance = std::hypot(sx, sy);
                if (distance < .25)
                {
                    ++routeIndex;
                    continue;
                }
                movementVelocity = {sx / distance * currentSpeed, sy / distance * currentSpeed};
                if (travel <= 0)
                {
                    break;
                }
                const double step = (std::min)(travel, distance);
                const WorldPoint delta = {wx * step / distance, wy * step / distance};
                const WorldPoint start = actor;
                LevelOne::Move(actor,
                               delta,
                               [this](WorldPoint p)
                               {
                                   return Blocked(p);
                               });
                moving = moving || Distance(start, actor) > .001;
                collided =
                    std::hypot(actor.x - start.x - delta.x, actor.y - start.y - delta.y) > .01;
                if (collided)
                {
                    break;
                }
                travel -= step;
                if (step < distance)
                {
                    break;
                }
                // Spend the remaining distance on the next segment without a pause or speed reset.
                ++routeIndex;
            }
            movementBlockedTime = collided ? movementBlockedTime + dt : 0;
            if (movementBlockedTime >= .12f)
            {
                RetryMovement();
            }
            else if (routeIndex >= moveRoute.size() && moveRouteComplete)
            {
                StopMovement();
            }
        }
        else
        {
            if (!hasMoveDestination || dodging)
            {
                movementVelocity = {0, 0};
            }
        }
        if (captured && !controlling)
        {
            WorldPoint target = {player.x + 105, player.y + 65};
            float t = 1 - std::exp(-dt * 3);
            LevelOne::Move(spirit,
                           {(target.x - spirit.x) * t, (target.y - spirit.y) * t},
                           [this](WorldPoint p)
                           {
                               return Blocked(p);
                           });
        }
    }
    else
    {
        movementVelocity = {0, 0};
        if (!ControlsActive())
        {
            StopMovement();
            attackHeld = false;
        }
    }
    if (serviceNpc.empty() && !pauseMenu && !journal && !chapter.choicePending && !dialoguePending)
    {
        UpdateChapter();
        if (ControlsActive())
        {
            if (attackHeld && bufferedCommand.action < 0)
            {
                BasicAttack();
            }
        }
        if (!dialoguePending && levelOne.storyTrial)
        {
            for (const auto& enemy : levelOne.enemies)
            {
                if (enemy.boss == 12 && enemy.health > 0 && Distance(player, enemy.home) < 250)
                {
                    storyTrialTime += dt;
                    break;
                }
            }
        }
        levelOne.damageSpirit = controlling ? selectedCompanion : -1;
        const float previousCompanionCooldown = levelOne.companionCooldown;
        if (!dialoguePending && !levelOne.Dead())
        {
            FrameProfiler::Scope timing(FrameProfiler::Stage::Population);
            UpdatePopulation(dt);
        }
        if (!dialoguePending)
        {
            FrameProfiler::Scope timing(FrameProfiler::Stage::Combat);
            levelOne.Update(
                dt,
                player,
                spirit,
                captured,
                controlling,
                false,
                world.seed,
                [this](WorldPoint p)
                {
                    return Blocked(p);
                },
                [this](WorldPoint p, int kind)
                {
                    return DomainTimeScale(p, kind);
                },
                SimulationRadius());
        }
        SyncPopulationHealth();
        if (!controlling && levelOne.companionCooldown > previousCompanionCooldown)
        {
            spiritCastPose = .55f;
            spiritCastFacing = companionAnimation.facing;
        }
        if (controlling && levelOne.collection[selectedCompanion].health <= 0)
        {
            controlling = false;
            levelOne.damageSpirit = -1;
            levelOne.invulnerability = .55f;
            spirit = player;
            ClearInput();
            Say("조종 해제",
                "주령이 쓰러져 본체로 돌아왔습니다. 주령은 비조종 상태에서 회복합니다.");
        }
        if (!levelOne.notice.empty())
        {
            Say("기록", levelOne.notice);
            messageTime = 3;
            levelOne.notice.clear();
        }
    }
    double actualX = player.x - previousPlayer.x, actualY = player.y - previousPlayer.y;
    const double screenDistance = std::hypot((actualX - actualY) * .85, (actualX + actualY) * .43);
    const double actualSpeed = dt > 0 ? screenDistance / dt : 0;
    protagonistSprinting = !controlling && levelOne.dodgeTime <= 0 &&
                           actualSpeed > (protagonistSprinting ? 105.0 : 125.0);
    // Full gait cycles follow actual travel; frame count does not change cadence.
    if (serviceNpc.empty() && !pauseMenu && !journal && !chapter.choicePending && !dialoguePending)
    {
        const float stridePhase = protagonistAnimation.phase;
        const SpriteSheet& movementSheet = protagonistSprinting && protagonistRunSheet.Ready()
                                               ? protagonistRunSheet
                                               : protagonistSheet;
        const float pixelsPerFrame = blenderProtagonist ? (protagonistSprinting ? 68.f : 48.f) /
                                                              movementSheet.WalkFrameCount()
                                                        : 13.f;
        protagonistAnimation.Update((actualX - actualY) * .85,
                                    (actualX + actualY) * .43,
                                    dt,
                                    protagonistSprinting,
                                    pixelsPerFrame);
        if (screenDistance < .001)
        {
            protagonistStopTime += dt;
            protagonistAnimation.phase = stridePhase;
            protagonistAnimation.moving = protagonistStopTime < .08f;
        }
        else
        {
            protagonistStopTime = 0;
        }
    }
    UpdateCharacterPresentation(dt, actualX, actualY, screenDistance);
    const double spiritDX = spirit.x - previousSpirit.x, spiritDY = spirit.y - previousSpirit.y;
    double screenDX = (spiritDX - spiritDY) * .85, screenDY = (spiritDX + spiritDY) * .43;
    // Ignore the tiny convergence tail of the following interpolation.
    if (!controlling && screenDX * screenDX + screenDY * screenDY < .0025)
    {
        screenDX = 0;
        screenDY = 0;
    }
    if (serviceNpc.empty() && !pauseMenu && !journal && !chapter.choicePending && !dialoguePending)
        companionAnimation.Update(screenDX,
                                  screenDY,
                                  dt,
                                  controlling && sprint,
                                  capturedSpirit == 0   ? 7.f
                                  : capturedSpirit == 1 ? 11.f
                                                        : 9.f);
    WorldPoint target = controlling ? spirit : player;
    if (Distance(target, camera) > 2000)
        camera = target; // Distant vessel switch.
    float t = 1 - std::exp(-dt * 6);
    camera.x += (target.x - camera.x) * t;
    camera.y += (target.y - camera.y) * t;
    std::vector<WorldPoint> residents;
    for (const auto& npc : population.people)
        residents.push_back(npc.position);
    {
        FrameProfiler::Scope timing(FrameProfiler::Stage::Streaming);
        world.Stream(camera, target, width, height, residents);
    }
    if (!pauseMenu && !journal && automaticSaveAllowed && autoSaveTime >= 30 && CanSaveProgress())
    {
        SaveProgress(false);
    }
}
