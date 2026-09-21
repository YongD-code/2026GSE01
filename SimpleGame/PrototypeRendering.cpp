#include "stdafx.h"
#include "Prototype.h"

void Prototype::House(const Object& o)
{
    Box(o.x,
        o.y,
        o.w,
        o.d,
        o.h,
        Color(.23f, .25f, .25f),
        Color(.19f, .21f, .22f),
        Color(.12f, .15f, .17f));
    Point a = Screen(o.x - 12, o.y - 12, o.h), b = Screen(o.x + o.w + 12, o.y - 12, o.h);
    Point c = Screen(o.x + o.w + 12, o.y + o.d + 12, o.h),
          d = Screen(o.x - 12, o.y + o.d + 12, o.h);
    Point ridgeA = Screen(o.x - 12, o.y + o.d * .5f, o.h + 65),
          ridgeB = Screen(o.x + o.w + 12, o.y + o.d * .5f, o.h + 65);
    r.Triangle(a, d, ridgeA, Color(.16f, .19f, .20f));
    r.Quad(ridgeA, ridgeB, c, d, Color(.27f, .19f, .18f));
    r.Triangle(b, c, ridgeB, Color(.19f, .13f, .14f));
    r.Line(ridgeA, ridgeB, 3, Color(.38f, .29f, .24f));
    for (int i = 1; i < 5; ++i)
    {
        float u = float(i) / 5;
        r.Line({ridgeA.x + (ridgeB.x - ridgeA.x) * u, ridgeA.y + (ridgeB.y - ridgeA.y) * u},
               {d.x + (c.x - d.x) * u, d.y + (c.y - d.y) * u},
               2,
               Color(.16f, .13f, .14f));
    }
    // Front-wall door and windows lie on the same projected plane as the wall.
    double front = o.y + o.d;
    r.Quad(Screen(o.x + o.w * .42f, front, 0),
           Screen(o.x + o.w * .65f, front, 0),
           Screen(o.x + o.w * .65f, front, 48),
           Screen(o.x + o.w * .42f, front, 48),
           Color(.07f, .09f, .10f));
    r.Quad(Screen(o.x + 18, front, 38),
           Screen(o.x + 40, front, 38),
           Screen(o.x + 40, front, 62),
           Screen(o.x + 18, front, 62),
           Color(2.2f, 1.25f, .45f));
    Box(o.x + o.w * .7f,
        o.y + 18,
        18,
        20,
        o.h + 70,
        Color(.32f, .32f, .29f),
        Color(.23f, .23f, .23f),
        Color(.16f, .17f, .18f));
}

void Prototype::Tree(const Object& o)
{
    Point p = Screen(o.x + o.w * .5f, o.y + o.d * .5f);
    r.Ellipse(p.x, p.y, 27, 11, Color(0, 0, 0, .25f));
    r.Line(p, {p.x - 4, static_cast<float>(p.y - o.h)}, 7, Color(.17f, .16f, .17f));
    for (int i = 0; i < 4; ++i)
    {
        float side = i % 2 ? 1.f : -1.f, level = static_cast<float>(p.y - o.h * .35 - i * 15);
        Point tip = {p.x + side * (23 + i * 3), level - 28};
        r.Line({p.x - 2, level}, tip, 4, Color(.19f, .18f, .19f));
        r.Line(tip, {tip.x + side * 10, tip.y - 21}, 2, Color(.22f, .21f, .22f));
    }
}

void Prototype::Person(WorldPoint pos, Color coat, bool isPlayer, bool childSize)
{
    Point p = Screen(pos.x, pos.y);
    const float spriteHeight = childSize ? 55.f : 78.f;
    if (p.x < -80 || p.x > width + 80 || p.y < -20 || p.y > height + 100)
        return;
    r.Ellipse(p.x, p.y + 1, childSize ? 10.f : 14.f, 5, Color(0, 0, 0, .35f));
    if (isPlayer && !controlling)
        r.Ellipse(p.x, p.y, 18, 7, Color(.75f, .69f, .45f, .20f));
    SpriteAnimation animation = protagonistAnimation;
    SpriteSheet* sheet = isPlayer && protagonistSprinting && protagonistRunSheet.Ready()
                             ? &protagonistRunSheet
                             : &protagonistSheet;
    Color tint = isPlayer && levelOne.dodgeTime > 0  ? Color(.5f, 1.5f, 2.f)
                 : isPlayer && levelOne.hurtTime > 0 ? Color(1.5f, .55f, .55f)
                                                     : Color(1, 1, 1);
    if (isPlayer && levelOne.castTime > 0 && !protagonistAnimation.moving && attackSheet.Ready())
    {
        sheet = &attackSheet;
        animation.facing = levelOne.castFacing;
        animation.moving = true;
        animation.phase = (std::min)(3.f, std::floor((.18f - levelOne.castTime) / .045f));
    }
    if (!isPlayer)
    {
        sheet = childSize ? &childSheet : &keeperSheet;
        // Until a dedicated image is supplied, share the protagonist texture as a placeholder.
        if (!sheet->Ready())
        {
            sheet = &protagonistSheet;
            tint = childSize ? Color(1.f, .78f, .66f) : Color(.72f, .9f, .78f);
        }
        animation = SpriteAnimation();
        if (Distance(pos, player) < 160)
        {
            double dx = player.x - pos.x, dy = player.y - pos.y;
            animation.Update((dx - dy) * .85, (dx + dy) * .43, 0, false);
            animation.moving = false;
        }
        if (childSize ? blenderChild : blenderKeeper)
        {
            // Idle sheets contain a full breathing loop for each facing.
            animation.moving = true;
            animation.phase = std::fmod(time * 4.f + (childSize ? 0.f : 3.f), 8.f);
        }
    }
    if (sheet->Ready())
        sheet->Draw(r,
                    p,
                    spriteHeight,
                    animation,
                    tint,
                    width,
                    height,
                    isPlayer && levelOne.castTime > 0 && !protagonistAnimation.moving ? 1.4f : 0.f);
    else
    {
        // Keep the actor visible if an asset is missing or PNG decoding fails.
        r.Triangle({p.x, p.y - 43}, {p.x - 14, p.y - 6}, {p.x + 14, p.y - 6}, coat);
        r.Ellipse(p.x, p.y - 44, 8, 10, Color(.61f, .53f, .43f));
    }
}

void Prototype::Wisp(int type, WorldPoint position)
{
    Point p = Screen(position.x, position.y);
    if (p.x < -100 || p.x > width + 100 || p.y < -50 || p.y > height + 130)
        return;
    r.Ellipse(p.x, p.y, 18, 6, Color(0, 0, 0, .3f));
    SpriteAnimation animation;
    if (captured && capturedSpirit == type)
        animation = companionAnimation;
    float bob = type % 3 == 2 ? 0.f : std::sin(time * 2 + type) * 3.f;
    if (spiritSheets[type % 3].Ready())
        spiritSheets[type % 3].Draw(r,
                                    {p.x, p.y - 4 + bob},
                                    type == 2 ? 67.f : 72.f,
                                    animation,
                                    Color(1, 1, 1),
                                    width,
                                    height,
                                    type == 0 ? 2.f : (type == 1 ? 1.f : .15f));
    else
        r.Ellipse(p.x, p.y - 25, 12, 18, Color(.4f, .7f, .8f));
}

void Prototype::DrawEnemy(const LevelOne::Enemy& enemy)
{
    const Point p = Screen(enemy.position.x, enemy.position.y);
    r.Ellipse(p.x, p.y, 20, 7, Color(.65f, .08f, .1f, .6f));
    if (spiritSheets[enemy.type].Ready())
    {
        spiritSheets[enemy.type].Draw(
            r,
            p,
            enemy.boss > 0 ? 100.f : 68.f,
            enemy.animation,
            enemy.flash > 0 ? Color(2, 2, 2)
                            : (enemy.returning ? Color(.55f, .55f, .65f) : Color(1.25f, .6f, .65f)),
            width,
            height,
            .4f);
    }
    else
    {
        r.Ellipse(p.x, p.y - 25, 16, 24, Color(.8f, .2f, .3f));
    }
    if (!enemy.alerted && !enemy.weakened && enemy.flash <= 0 && enemy.boss == 0)
        return;
    const float barY = p.y - (enemy.boss > 0 ? 111.f : 79.f);
    r.Rect(p.x - 20, barY, 40, 4, Color(.15f, .08f, .09f));
    r.Rect(p.x - 20,
           barY,
           40 * (std::max)(0.f, enemy.health) / enemy.maximum,
           4,
           Color(.9f, .25f, .25f));
}

void Prototype::DrawCombatWarnings()
{
    for (const auto& enemy : levelOne.enemies)
    {
        if (!enemy.alerted || enemy.weakened || enemy.health <= 0 ||
            (enemy.phase != 1 && enemy.phase != 2))
            continue;
        const Color warning =
            enemy.phase == 2 ? Color(2.f, .2f, .1f, .6f) : Color(1.f, .24f, .07f, .45f);
        const auto origin = enemy.position;
        if (enemy.type == 1)
        {
            const double reach = levelOne.combat.chargeSpeed * .48;
            const double nx = -enemy.aim.y * 30, ny = enemy.aim.x * 30;
            r.Quad(Screen(origin.x + nx, origin.y + ny),
                   Screen(origin.x - nx, origin.y - ny),
                   Screen(origin.x + enemy.aim.x * reach - nx, origin.y + enemy.aim.y * reach - ny),
                   Screen(origin.x + enemy.aim.x * reach + nx, origin.y + enemy.aim.y * reach + ny),
                   warning);
        }
        else if (enemy.type == 0)
        {
            const double base = std::atan2(enemy.aim.y, enemy.aim.x);
            for (int i = 0; i < 16; ++i)
            {
                const double a = base - 1.213 + i * 2.426 / 16;
                const double b = base - 1.213 + (i + 1) * 2.426 / 16;
                const Point center = Screen(origin.x, origin.y);
                r.Quad(center,
                       Screen(origin.x + std::cos(a) * 85, origin.y + std::sin(a) * 85),
                       Screen(origin.x + std::cos(b) * 85, origin.y + std::sin(b) * 85),
                       center,
                       warning);
            }
        }
        else
        {
            for (int i = -1; i <= 1; ++i)
            {
                const double a = i * .18, c = std::cos(a), s = std::sin(a);
                r.Line(Screen(origin.x, origin.y),
                       Screen(origin.x + (enemy.aim.x * c - enemy.aim.y * s) * 520,
                              origin.y + (enemy.aim.x * s + enemy.aim.y * c) * 520),
                       2,
                       warning);
            }
        }
    }
    if (levelOne.dodgeTime > 0)
    {
        const Point p = Screen(player.x, player.y);
        r.Ellipse(p.x, p.y, 25, 10, Color(.25f, 1.3f, 2.f, .7f));
    }
}

void Prototype::Hearth(WorldPoint position)
{
    Point p = Screen(position.x, position.y);
    r.Ellipse(p.x, p.y, 28, 13, Color(.29f, .28f, .26f));
    r.Ellipse(p.x, p.y, 22, 9, Color(.09f, .10f, .11f));
    r.Line({p.x - 15, p.y + 3}, {p.x + 12, p.y - 5}, 6, Color(.36f, .23f, .16f));
    float flicker = std::sin(time * 13) * 3;
    r.Triangle({p.x - 13, p.y},
               {p.x + 11, p.y},
               {p.x + 3, p.y - 40 - flicker},
               Color(2.4f, .9f, .25f, .9f));
    r.Triangle(
        {p.x - 7, p.y}, {p.x + 8, p.y}, {p.x - 2, p.y - 25 + flicker}, Color(3.f, 1.8f, .6f));
    for (int i = 0; i < 7; ++i)
    {
        float t = std::fmod(time * 18 + i * 12.f, 85.f);
        r.Ellipse(
            p.x + std::sin(t * .08f + i) * 12, p.y - t, 1.3f, 2, Color(1, .66f, .30f, 1 - t / 85));
    }
}
