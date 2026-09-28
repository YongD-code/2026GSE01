#include "stdafx.h"
#include "Prototype.h"

void Prototype::House(const Object& o)
{
    // Keep solid details inside the existing collision footprint.
    // Dimensions select the style so identical cached meshes remain reusable.
    const bool hall = o.w >= 190;
    const bool tall = !hall && o.h >= 105;
    const double x = o.x;
    const double y = o.y;
    const double front = y + o.d;
    const double wallFront = front - 12;
    const double base = 10;
    const double eave = o.h;
    const double rise = hall ? 58 : (tall ? 44 : 34);
    const double overhang = hall ? 12 : 8;
    const Color wood(.15f, .12f, .105f);
    const Color woodLight(.29f, .23f, .17f);
    const Color plaster = hall ? Color(.43f, .42f, .36f) : Color(.32f, .34f, .32f);

    auto frontPanel = [&](double left, double right, double low, double high, Color color)
    {
        r.Quad(Screen(left, wallFront, low),
               Screen(right, wallFront, low),
               Screen(right, wallFront, high),
               Screen(left, wallFront, high),
               color);
    };

    // Stone plinth, recessed facade and a shallow timber veranda.
    Box(x,
        y,
        o.w,
        o.d,
        base,
        Color(.32f, .34f, .34f),
        Color(.22f, .24f, .25f),
        Color(.16f, .19f, .21f));
    Box(x + 3, y + 3, o.w - 6, o.d - 15, eave, plaster, plaster, Color(.23f, .26f, .26f));
    for (double offset = 16; offset < o.w; offset += 22)
    {
        r.Line(Screen(x + offset, front, 1),
               Screen(x + offset, front, base - 1),
               1,
               Color(.12f, .15f, .16f));
    }
    for (double offset = 4; offset < o.w - 4; offset += 9)
    {
        r.Line(Screen(x + offset, wallFront, base + 1),
               Screen(x + offset, front, base + 1),
               1,
               woodLight);
    }

    const int bays = hall ? 7 : (o.w >= 100 ? 5 : 3);
    const double bayWidth = (o.w - 12) / bays;
    for (int bay = 0; bay < bays; ++bay)
    {
        const double left = x + 6 + bay * bayWidth;
        const double right = left + bayWidth;
        const bool door = bay == bays / 2;
        const double low = door ? base + 1 : base + 17;
        const double high = tall ? eave * .53 : eave - 16;
        frontPanel(left + 2, right - 2, low, high, Color(.055f, .075f, .085f));
        frontPanel(left + 4,
                   right - 4,
                   low + 3,
                   high - 3,
                   door ? Color(.20f, .18f, .14f) : Color(.64f, .46f, .25f));
        for (int slat = 1; slat < 4; ++slat)
        {
            const double sx = left + (right - left) * slat / 4;
            r.Line(Screen(sx, wallFront, low), Screen(sx, wallFront, high), 1, wood);
        }
        r.Line(Screen(left + 2, wallFront, (low + high) * .5),
               Screen(right - 2, wallFront, (low + high) * .5),
               1,
               wood);
        if (tall)
        {
            frontPanel(left + 3, right - 3, eave * .65, eave - 14, Color(.12f, .16f, .17f));
            r.Line(Screen((left + right) * .5, wallFront, eave * .65),
                   Screen((left + right) * .5, wallFront, eave - 14),
                   2,
                   woodLight);
        }
    }
    for (int post = 0; post <= bays; ++post)
    {
        const double px = x + 6 + post * bayWidth;
        frontPanel(px - 2, px + 2, base, eave, wood);
    }
    frontPanel(x + 3, x + o.w - 3, base + 2, base + 7, wood);
    frontPanel(x + 3, x + o.w - 3, eave - 9, eave - 3, woodLight);
    if (tall)
    {
        frontPanel(x + 3, x + o.w - 3, eave * .57, eave * .62, wood);
    }

    // Side-wall framing is drawn on the actual side plane.
    for (int post = 0; post <= 4; ++post)
    {
        const double py = y + 4 + (o.d - 17) * post / 4;
        r.Line(Screen(x + o.w - 3, py, base), Screen(x + o.w - 3, py, eave), 3, wood);
    }
    r.Line(Screen(x + o.w - 3, y + 3, eave * .55),
           Screen(x + o.w - 3, wallFront, eave * .55),
           3,
           woodLight);

    // Hip roof: two bent slope bands, raised ridge and individual tile courses.
    const double midY = y + (o.d - 12) * .5;
    const double hip = o.w * .18;
    auto roofPoint = [&](double u, double t, bool nearSide)
    {
        const double ridgeX = x + hip + (o.w - 2 * hip) * u;
        const double edgeX = x - overhang + (o.w + 2 * overhang) * u;
        const double edgeY = nearSide ? front + overhang : y - overhang;
        const double height = eave + rise * (1 - t) + 7 * t * t * t;
        return Screen(ridgeX + (edgeX - ridgeX) * t, midY + (edgeY - midY) * t, height);
    };
    for (int side = 0; side < 2; ++side)
    {
        const bool nearSide = side == 1;
        for (int band = 0; band < 2; ++band)
        {
            const double t0 = band * .5;
            const double t1 = t0 + .5;
            r.Quad(roofPoint(0, t0, nearSide),
                   roofPoint(1, t0, nearSide),
                   roofPoint(1, t1, nearSide),
                   roofPoint(0, t1, nearSide),
                   nearSide ? Color(.23f + band * .025f, .27f, .29f) : Color(.12f, .16f, .19f));
        }
    }
    r.Triangle(roofPoint(1, 0, true),
               roofPoint(1, 1, false),
               roofPoint(1, 1, true),
               Color(.16f, .20f, .23f));
    const int tiles = hall ? 22 : (o.w >= 100 ? 16 : 8);
    for (int tile = 0; tile <= tiles; ++tile)
    {
        const double u = double(tile) / tiles;
        r.Line(roofPoint(u, 0, true), roofPoint(u, .5, true), 1, Color(.34f, .38f, .39f));
        r.Line(roofPoint(u, .5, true), roofPoint(u, 1, true), 1, Color(.29f, .33f, .35f));
    }
    for (int course = 1; course <= 6; ++course)
    {
        const double t = double(course) / 6;
        r.Line(roofPoint(0, t, true), roofPoint(1, t, true), 1, Color(.12f, .16f, .18f));
    }
    r.Line(roofPoint(0, 0, true), roofPoint(1, 0, true), hall ? 6.f : 4.f, Color(.39f, .41f, .39f));
    r.Line(roofPoint(0, 1, true), roofPoint(1, 1, true), 4, wood);
    r.Line(roofPoint(1, 1, false), roofPoint(1, 1, true), 3, wood);

    // Entrance canopy and steps stay within the original footprint.
    const double entryLeft = x + o.w * .34;
    const double entryRight = x + o.w * .66;
    const double canopy = tall ? eave * .55 : eave * .67;
    r.Quad(Screen(entryLeft, wallFront, canopy + 7),
           Screen(entryRight, wallFront, canopy + 7),
           Screen(entryRight + 3, front, canopy),
           Screen(entryLeft - 3, front, canopy),
           Color(.23f, .27f, .28f));
    r.Line(
        Screen(entryLeft - 3, front, canopy), Screen(entryRight + 3, front, canopy), 3, woodLight);
    for (int side = 0; side < 2; ++side)
    {
        const double px = side == 0 ? entryLeft : entryRight;
        r.Line(Screen(px, front - 2, base), Screen(px, front - 2, canopy), 3, wood);
    }
    r.Quad(Screen(entryLeft, front - 5, 5),
           Screen(entryRight, front - 5, 5),
           Screen(entryRight, front, 5),
           Screen(entryLeft, front, 5),
           Color(.37f, .38f, .35f));
}

void Prototype::Stone(const Object& o)
{
    // Tall existing markers become lanterns; broad obstacles remain natural stone.
    // Both use their existing collision footprint and SceneGraph actor.
    auto point = [&](double u, double v, double z)
    {
        return Screen(o.x + o.w * u, o.y + o.d * v, o.h * z);
    };
    auto tier = [&](double inset, double low, double high, Color top)
    {
        const double far = 1 - inset;
        r.Quad(point(inset, far, low),
               point(far, far, low),
               point(far, far, high),
               point(inset, far, high),
               Color(.24f, .28f, .27f));
        r.Quad(point(far, inset, low),
               point(far, far, low),
               point(far, far, high),
               point(far, inset, high),
               Color(.16f, .20f, .21f));
        r.Quad(point(inset, inset, high),
               point(far, inset, high),
               point(far, far, high),
               point(inset, far, high),
               top);
    };

    if (o.h > (std::max)(o.w, o.d) * 1.65)
    {
        tier(0, 0, .10, Color(.39f, .42f, .38f));
        tier(.10, .10, .18, Color(.32f, .36f, .33f));
        tier(.34, .18, .57, Color(.37f, .40f, .36f));
        tier(.12, .54, .62, Color(.42f, .44f, .39f));
        tier(.22, .62, .84, Color(.29f, .33f, .30f));
        r.Quad(point(.30, .785, .65),
               point(.70, .785, .65),
               point(.70, .785, .79),
               point(.30, .785, .79),
               Color(.85f, .53f, .23f));
        r.Line(point(.50, .79, .65), point(.50, .79, .79), 1, Color(.16f, .20f, .19f));
        r.Triangle(
            point(0, 1, .84), point(1, 1, .84), point(.50, .50, .98), Color(.40f, .44f, .40f));
        r.Triangle(
            point(1, 0, .84), point(1, 1, .84), point(.50, .50, .98), Color(.24f, .29f, .28f));
        r.Line(point(0, 1, .84), point(1, 1, .84), 2, Color(.16f, .23f, .19f));
        r.Line(point(.50, .50, .96), point(.50, .50, 1), 3, Color(.45f, .47f, .40f));
        return;
    }

    const Point a = point(.06, .28, .15);
    const Point b = point(.28, .06, .35);
    const Point c = point(.80, .12, .26);
    const Point d = point(.98, .65, .08);
    const Point e = point(.64, .98, .04);
    const Point f = point(.08, .82, .07);
    const Point peak = point(.43, .42, 1);
    const Point shoulder = point(.77, .56, .72);
    r.Triangle(a, b, peak, Color(.34f, .38f, .37f));
    r.Triangle(b, c, peak, Color(.43f, .46f, .42f));
    r.Triangle(c, shoulder, peak, Color(.36f, .40f, .38f));
    r.Triangle(c, d, shoulder, Color(.22f, .27f, .28f));
    r.Triangle(d, e, shoulder, Color(.17f, .22f, .24f));
    r.Triangle(e, f, peak, Color(.29f, .34f, .32f));
    r.Triangle(e, peak, shoulder, Color(.25f, .30f, .29f));
    r.Triangle(f, a, peak, Color(.32f, .37f, .34f));
    // Moss follows the rock facets rather than floating above the silhouette.
    r.Triangle(a, b, point(.36, .35, .79), Color(.22f, .32f, .20f));
    r.Triangle(f, point(.20, .65, .37), point(.34, .87, .20), Color(.19f, .28f, .18f));
    r.Line(peak, point(.52, .68, .46), 1, Color(.16f, .21f, .21f));
    r.Line(point(.52, .68, .46), e, 1, Color(.19f, .24f, .23f));
}

void Prototype::Tree(const Object& o)
{
    const double cx = o.x + o.w * .5;
    const double cy = o.y + o.d * .5;
    const Point base = Screen(cx, cy);
    const bool cedar = static_cast<int>(o.h) % 3 == 0;
    const Color bark(.22f, .19f, .15f);
    r.Ellipse(base.x, base.y, 22, 9, Color(.035f, .065f, .055f, .45f));
    const Point bend = Screen(cx - 4, cy + 2, o.h * .53);
    const Point crown = Screen(cx + 3, cy, o.h * .90);
    r.Line(base, bend, 7, bark);
    r.Line(bend, crown, 4, Color(.29f, .25f, .19f));
    r.Line(Screen(cx + 2, cy, 3), Screen(cx - 2, cy + 2, o.h * .50), 1, Color(.42f, .36f, .25f));
    for (int root = 0; root < 3; ++root)
    {
        r.Line(base, Screen(o.x + o.w * root * .5, o.y + o.d, 0), 3, bark);
    }

    // Layered cedar crowns and asymmetric pine boughs share the same tree actor.
    for (int layer = 0; layer < 4; ++layer)
    {
        const double height = o.h * (.40 + layer * .16);
        const double side = layer % 2 == 0 ? -1 : 1;
        const double spread = cedar ? 30 - layer * 5 : 27 - layer * 3;
        const Point center = Screen(cx + (cedar ? 0 : side * 10), cy, height);
        const Point trunk = Screen(cx, cy, height - 8);
        r.Line(trunk, {center.x + float(side * spread * .6), center.y}, 3, bark);
        const Point left = {center.x - float(spread), center.y + 3};
        const Point right = {center.x + float(spread), center.y + 3};
        const Point top = {center.x + float(side * 4), center.y - float(cedar ? 27 : 13)};
        r.Triangle(left, right, top, Color(.085f, .17f, .14f));
        r.Triangle(left, {center.x, center.y + 8}, right, Color(.055f, .12f, .105f));
        r.Triangle(
            {left.x + 5, left.y - 1}, {right.x - 9, right.y - 3}, top, Color(.15f, .24f, .18f));
        for (int needle = 0; needle < 5; ++needle)
        {
            const float nx = center.x - float(spread * .7) + float(needle * spread * .32);
            r.Line({nx, center.y}, {nx - 3, center.y - 5}, 1, Color(.25f, .32f, .22f));
        }
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
    const std::string key = "hearth:stone-basin:environment-v3";
    if (r.BeginCachedMesh(key, p))
    {
        r.Ellipse(p.x, p.y + 3, 32, 14, Color(.045f, .06f, .055f, .6f));
        r.Ellipse(p.x, p.y, 28, 12, Color(.30f, .33f, .30f));
        r.Ellipse(p.x, p.y - 3, 23, 9, Color(.055f, .075f, .075f));
        for (int stone = 0; stone < 12; ++stone)
        {
            const double angle = stone * 6.283185307179586 / 12;
            const float sx = p.x + float(std::cos(angle) * 26);
            const float sy = p.y + float(std::sin(angle) * 10);
            r.Quad({sx - 5, sy},
                   {sx, sy - 4},
                   {sx + 6, sy - 2},
                   {sx + 3, sy + 3},
                   stone % 3 == 0 ? Color(.24f, .32f, .22f) : Color(.41f, .43f, .37f));
        }
        r.Line({p.x - 15, p.y + 3}, {p.x + 12, p.y - 5}, 6, Color(.26f, .18f, .12f));
        r.Line({p.x - 11, p.y - 5}, {p.x + 10, p.y + 3}, 4, Color(.33f, .23f, .14f));
        r.EndCachedMesh();
    }
    r.DrawCachedMesh(key, p);
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
