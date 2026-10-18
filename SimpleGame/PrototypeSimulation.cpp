#include "stdafx.h"
#include "Prototype.h"
#include "FrameProfiler.h"

void Prototype::UpdateSimulation(float dt, bool sprint)
{
    if (openingPlaying || titleScreen || slotMode != SlotMode::Closed)
    {
        movementVelocity = {0, 0};
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
    if (serviceNpc.empty() && !pauseMenu && !journal && !chapter.choicePending &&
        !dialoguePending && !levelOne.Dead() && !PreparingSkill())
    {
        WorldPoint actorPosition = controlling ? spirit : player;
        while (routeIndex < moveRoute.size() &&
               Distance(actorPosition, moveRoute[routeIndex]) <
                   (routeIndex + 1 == moveRoute.size() ? 2.0 : 7.0))
        {
            ++routeIndex;
        }
        float sx = 0, sy = 0;
        double remaining = 0;
        if (routeIndex < moveRoute.size())
        {
            const double x = moveRoute[routeIndex].x - actorPosition.x;
            const double y = moveRoute[routeIndex].y - actorPosition.y;
            sx = static_cast<float>((x - y) * .85);
            sy = static_cast<float>((x + y) * .43);
            remaining = std::hypot(sx, sy);
        }
        // Convert the route direction from screen space to world space.
        float dx = sx / .85f + sy / .43f, dy = -sx / .85f + sy / .43f;
        float length = std::sqrt(sx * sx + sy * sy);
        if (movementWasControlling != controlling)
        {
            movementVelocity = {0, 0};
            moveRoute.clear();
            sx = sy = length = 0;
        }
        movementWasControlling = controlling;
        if (dodgePressed)
        {
            const WorldPoint aim = CursorWorld();
            dx = static_cast<float>(aim.x - player.x);
            dy = static_cast<float>(aim.y - player.y);
        }
        if (!controlling)
            levelOne.Dodge(dt,
                           player,
                           {dx, dy},
                           dodgePressed,
                           [this](WorldPoint p)
                           {
                               return Blocked(p);
                           });
        if (controlling || levelOne.dodgeTime <= 0)
        {
            const float pace = sprint ? 1.8f : 1.f;
            float speed = (controlling ? 110.f : 90.f) * pace * (spiritHasteTime > 0 ? 1.5f : 1.f);
            if (!controlling && InsideDomain(player))
            {
                speed *= 1.5f;
            }
            if (routeIndex + 1 >= moveRoute.size())
            {
                speed = (std::min)(speed, static_cast<float>(std::sqrt(2 * 1100 * remaining)));
            }
            const WorldPoint desired = length > 0
                                           ? WorldPoint{sx / length * speed, sy / length * speed}
                                           : WorldPoint{0, 0};
            const WorldPoint before = movementVelocity;
            // Exponential steering keeps velocity continuous through turns and speed changes.
            const double response = length > 0 ? 11.0 : 15.0;
            const double decay = std::exp(-response * dt);
            movementVelocity.x = desired.x + (before.x - desired.x) * decay;
            movementVelocity.y = desired.y + (before.y - desired.y) * decay;
            const double screenX = desired.x * dt + (before.x - desired.x) * (1 - decay) / response;
            const double screenY = desired.y * dt + (before.y - desired.y) * (1 - decay) / response;
            dx = static_cast<float>((screenX / .85 + screenY / .43) * .5);
            dy = static_cast<float>((-screenX / .85 + screenY / .43) * .5);
            const double travel = std::hypot((dx - dy) * .85, (dx + dy) * .43);
            if (travel > remaining && travel > 0)
            {
                dx *= static_cast<float>(remaining / travel);
                dy *= static_cast<float>(remaining / travel);
                movementVelocity = {0, 0};
            }
            WorldPoint& actor = controlling ? spirit : player;
            const WorldPoint start = actor;
            LevelOne::Move(actor,
                           {dx, dy},
                           [this](WorldPoint p)
                           {
                               return Blocked(p);
                           });
            moving = Distance(start, actor) > .001;
            if (!moving && length > 0 && std::hypot(dx, dy) > .01)
            {
                moveRoute.clear();
                movementVelocity = {0, 0};
            }
        }
        else
        {
            movementVelocity = {0, 0};
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
            moveRoute.clear();
            attackHeld = false;
        }
    }
    if (serviceNpc.empty() && !pauseMenu && !journal && !chapter.choicePending && !dialoguePending)
    {
        UpdateChapter();
        if (ControlsActive())
        {
            UpdateSkills(dt);
            UpdateBufferedCommand(inputDt);
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
