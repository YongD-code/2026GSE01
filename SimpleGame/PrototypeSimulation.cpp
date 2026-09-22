#include "stdafx.h"
#include "Prototype.h"

void Prototype::UpdateSimulation(float dt, bool sprint)
{
    if (titleScreen || slotMode != SlotMode::Closed)
    {
        time += (std::min)(dt, .05f);
        return;
    }
    dt = (std::max)(0.f, (std::min)(dt, .05f));
    const WorldPoint previousPlayer = player;
    const WorldPoint previousSpirit = spirit;
    time += dt;
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
    if (!pauseMenu && !journal && !chapter.choicePending && !dialoguePending && !levelOne.Dead())
    {
        float sx = float(keys['d'] || arrows[2]) - float(keys['a'] || arrows[0]);
        float sy = float(keys['s'] || arrows[3]) - float(keys['w'] || arrows[1]);
        // Invert the isometric projection so keys correspond to screen directions.
        float dx = sx / .85f + sy / .43f, dy = -sx / .85f + sy / .43f;
        float length = std::sqrt(sx * sx + sy * sy);
        if (!controlling)
            levelOne.Dodge(dt,
                           player,
                           {dx, dy},
                           dodgePressed,
                           [this](WorldPoint p)
                           {
                               return Blocked(p);
                           });
        if (length > 0 && (controlling || levelOne.dodgeTime <= 0))
        {
            const float pace = sprint ? 1.8f : 1.f;
            float speed = (controlling ? 110.f : 90.f) * pace;
            dx = dx * .5f / length * speed * dt;
            dy = dy * .5f / length * speed * dt;
            WorldPoint& actor = controlling ? spirit : player;
            LevelOne::Move(actor,
                           {dx, dy},
                           [this](WorldPoint p)
                           {
                               return Blocked(p);
                           });
            moving = true;
            walk += dt * 10 * pace;
        }
        if (captured && !controlling)
        {
            WorldPoint target = {player.x + 35, player.y + 20};
            float t = 1 - std::exp(-dt * 3);
            LevelOne::Move(spirit,
                           {(target.x - spirit.x) * t, (target.y - spirit.y) * t},
                           [this](WorldPoint p)
                           {
                               return Blocked(p);
                           });
        }
    }
    if (!pauseMenu && !journal && !chapter.choicePending && !dialoguePending)
    {
        UpdateChapter();
        if (!dialoguePending)
            levelOne.Update(dt,
                            player,
                            spirit,
                            captured,
                            controlling,
                            keys[' '],
                            keys['z'],
                            captured && capturedSpirit == 0,
                            world.seed,
                            [this](WorldPoint p)
                            {
                                return Blocked(p);
                            });
        if (!levelOne.notice.empty())
        {
            Say("기록", levelOne.notice);
            messageTime = 3;
            levelOne.notice.clear();
        }
    }
    double actualX = player.x - previousPlayer.x, actualY = player.y - previousPlayer.y;
    protagonistSprinting = sprint && !controlling && (std::abs(actualX) + std::abs(actualY) > .001);
    // One pose per 13 screen pixels: sprinting and wall sliding follow actual travel.
    // Keep the stride phase when switching between walking and sprinting.
    if (!pauseMenu && !journal && !chapter.choicePending && !dialoguePending)
        protagonistAnimation.Update((actualX - actualY) * .85,
                                    (actualX + actualY) * .43,
                                    dt,
                                    sprint,
                                    blenderProtagonist ? (sprint ? 7.f : 5.f) : 13.f);
    const double spiritDX = spirit.x - previousSpirit.x, spiritDY = spirit.y - previousSpirit.y;
    double screenDX = (spiritDX - spiritDY) * .85, screenDY = (spiritDX + spiritDY) * .43;
    // Ignore the tiny convergence tail of the following interpolation.
    if (!controlling && screenDX * screenDX + screenDY * screenDY < .0025)
    {
        screenDX = 0;
        screenDY = 0;
    }
    if (!pauseMenu && !journal && !chapter.choicePending && !dialoguePending)
        companionAnimation.Update(screenDX, screenDY, dt, controlling && sprint);
    WorldPoint target = controlling ? spirit : player;
    if (Distance(target, camera) > 2000)
        camera = target; // Distant vessel switch.
    float t = 1 - std::exp(-dt * 6);
    camera.x += (target.x - camera.x) * t;
    camera.y += (target.y - camera.y) * t;
    world.Stream(camera, target, width, height);
    if (!pauseMenu && !journal && automaticSaveAllowed && autoSaveTime >= 30 && CanSaveProgress())
    {
        SaveProgress(false);
    }
}
