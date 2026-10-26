#include "stdafx.h"
#include "Prototype.h"
#include "EnvironmentGeometry.h"
#include "PrototypeStoryProps.inl"

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

void Prototype::Stone(const Object& o, int variant)
{
    // Tall existing markers become lanterns; broad obstacles remain natural stone.
    // Both use their existing collision footprint and SceneGraph actor.
    auto point = [&](double u, double v, double z)
    {
        return Screen(o.x + o.w * u, o.y + o.d * v, o.h * z);
    };
    auto tier = [&](double inset, double low, double high, Color top)
    {
        const double outerEdge = 1 - inset;
        r.Quad(point(inset, outerEdge, low),
               point(outerEdge, outerEdge, low),
               point(outerEdge, outerEdge, high),
               point(inset, outerEdge, high),
               Color(.24f, .28f, .27f));
        r.Quad(point(outerEdge, inset, low),
               point(outerEdge, outerEdge, low),
               point(outerEdge, outerEdge, high),
               point(outerEdge, inset, high),
               Color(.16f, .20f, .21f));
        r.Quad(point(inset, inset, high),
               point(outerEdge, inset, high),
               point(outerEdge, outerEdge, high),
               point(inset, outerEdge, high),
               top);
    };

    const Point ground = Screen(o.x + o.w * .5, o.y + o.d * .5);
    r.Ellipse(ground.x + 3,
              ground.y + 2,
              float((o.w + o.d) * .46),
              float((o.w + o.d) * .17),
              Color(.035f, .045f, .035f, .22f));
    r.Ellipse(ground.x,
              ground.y,
              float((o.w + o.d) * .32),
              float((o.w + o.d) * .12),
              Color(.025f, .035f, .025f, .42f));
    if (o.w <= 26 && o.d <= 26 && o.h > (std::max)(o.w, o.d) * 1.65)
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

    EnvironmentGeometry::Solid solid;
    using Vertex = EnvironmentGeometry::Vertex;
    constexpr int sides = 10;
    auto vertex = [&](int ring, int i)
    {
        const double a = i * 6.283185307179586 / sides + variant * .31;
        const double radius[] = {.45, .48, .30};
        const double heights[] = {.015, .42, .86};
        const double chip = 1 + .07 * std::sin(i * 2.7 + ring + variant);
        return Vertex{o.x + o.w * (.5 + std::cos(a) * radius[ring] * chip - ring * .025),
                      o.y + o.d * (.5 + std::sin(a) * radius[ring] * chip),
                      o.h * (heights[ring] + (ring ? .045 * std::sin(i * 1.9 + variant) : 0))};
    };
    for (int ring = 0; ring < 2; ++ring)
        for (int i = 0; i < sides; ++i)
        {
            const auto a = vertex(ring, i), b = vertex(ring, i + 1);
            const auto c = vertex(ring + 1, i + 1), d = vertex(ring + 1, i);
            const bool moss = ring == 1 && (i + variant * 2) % 5 <= 1;
            const Color stone =
                moss ? Color(.22f, .30f, .17f) : Color(.37f + variant * .025f, .40f, .37f);
            solid.Quad(a, b, c, d, stone);
            // Inset veins and chips sit on the facet instead of floating above its plane.
            const Vertex p = EnvironmentGeometry::Scale(EnvironmentGeometry::Add(a, d), .5);
            const Vertex q = EnvironmentGeometry::Scale(EnvironmentGeometry::Add(b, c), .5);
            if ((i + ring + variant) % 3 == 0)
            {
                const Vertex toward = EnvironmentGeometry::Scale(
                    EnvironmentGeometry::Add(c, EnvironmentGeometry::Scale(a, -1)), .012);
                solid.Triangle(p, q, EnvironmentGeometry::Add(q, toward), Color(.19f, .23f, .20f));
            }
        }
    const Vertex top = {o.x + o.w * .43, o.y + o.d * .47, o.h * .94};
    for (int i = 0; i < sides; ++i)
        solid.Triangle(top,
                       vertex(2, i),
                       vertex(2, i + 1),
                       (i + variant) % 4 == 0 ? Color(.26f, .34f, .19f) : Color(.44f, .46f, .40f));
    solid.Draw(r,
               [this](Vertex v)
               {
                   return Screen(v.x, v.y, v.z);
               });
    // Small fragments overlap the contact shadow and lock the silhouette to the ground.
    for (int i = 0; i < 4; ++i)
    {
        const double a = i * 1.8 + variant;
        const Point p =
            Screen(o.x + o.w * (.5 + .54 * std::cos(a)), o.y + o.d * (.5 + .54 * std::sin(a)), 1);
        r.Triangle({p.x - 3, p.y}, {p.x, p.y - 3}, {p.x + 4, p.y + 1}, Color(.28f, .32f, .27f));
    }
}

void Prototype::Tree(const Object& o, int variant)
{
    const double cx = o.x + o.w * .5;
    const double cy = o.y + o.d * .5;
    const Point base = Screen(cx, cy);
    const Color bark(.29f, .225f, .145f);
    r.Ellipse(base.x + 11, base.y + 5, 30, 10, Color(.02f, .04f, .03f, .15f));
    r.Ellipse(base.x, base.y + 1, 17, 6, Color(.10f, .12f, .075f, .70f));
    r.Ellipse(base.x, base.y, 11, 4, Color(.02f, .035f, .02f, .42f));
    EnvironmentGeometry::Solid solid;
    using Vertex = EnvironmentGeometry::Vertex;
    const Vertex foot = {cx, cy, 0};
    const Vertex bend = {cx - 2 + variant, cy + 1, o.h * .46};
    const Vertex crown = {cx + 2, cy - 1, o.h * .94};
    solid.Taper(foot, bend, 5.5, 3.3, bark, 12);
    solid.Taper(bend, crown, 3.3, .7, bark, 10);
    for (int i = 0; i < 6; ++i)
    {
        const double a = i * 1.0471975512 + variant * .3;
        solid.Taper({cx + std::cos(a) * 12, cy + std::sin(a) * 12, .6},
                    {cx + std::cos(a) * 2, cy + std::sin(a) * 2, 9},
                    .7,
                    2.5,
                    bark,
                    6);
    }
    for (int i = 0; i < 5; ++i)
    {
        const double a = .16 + i * .29;
        solid.Taper({cx + 5.3 * std::cos(a), cy + 5.3 * std::sin(a), 3},
                    {cx - 1 + 3.3 * std::cos(a), cy + 1 + 3.3 * std::sin(a), o.h * .39},
                    .18,
                    .12,
                    Color(.16f, .12f, .08f),
                    4);
    }
    const bool cedar = variant == 0;
    const int tiers = cedar ? 6 : 4;
    for (int layer = 0; layer < tiers; ++layer)
    {
        const double t = double(layer) / (tiers - 1);
        const double z = o.h * (.43 + t * .48);
        const double spread = cedar ? 25 - t * 18 : 27 - t * 9;
        const double a = layer * 2.39996 + variant;
        const double offset = cedar ? 2 : 12;
        const Vertex tip = {cx + std::cos(a) * offset, cy + std::sin(a) * offset, z};
        solid.Taper({cx - 1, cy, z - 12}, tip, 1.6, .55, bark, 7);
        const Color needles = variant == 2 ? Color(.18f, .28f, .135f) : Color(.12f, .235f, .18f);
        solid.Foliage(tip, spread, spread * .78, cedar ? 13 : 9, layer + variant * 13, needles);
        if (!cedar)
        {
            const Vertex side = {
                tip.x + std::cos(a + 1.6) * 11, tip.y + std::sin(a + 1.6) * 11, z - 3};
            solid.Foliage(side, spread * .65, spread * .52, 6, layer + 9, needles);
        }
    }
    solid.Draw(r,
               [this](Vertex v)
               {
                   return Screen(v.x, v.y, v.z);
               });
    for (int i = 0; i < 6; ++i)
    {
        const double a = i * 1.7 + variant;
        const Point p = Screen(cx + std::cos(a) * 14, cy + std::sin(a) * 12);
        r.Line({p.x - 2, p.y + 1}, {p.x + 3, p.y - 1}, 1, Color(.28f, .25f, .15f));
        r.Triangle({p.x - 2, p.y}, {p.x, p.y - 4}, {p.x + 1, p.y + 1}, Color(.18f, .27f, .13f));
    }
}

void Prototype::DrawPlayer(WorldPoint pos)
{
    Point p = Screen(pos.x, pos.y);
    const float spriteHeight = 78.f;
    if (p.x < -80 || p.x > width + 80 || p.y < -20 || p.y > height + 100)
        return;
    const bool model = useProtagonistModel && protagonistModel.Ready();
    if (model)
    {
        r.Ellipse(p.x + 4, p.y + 3, 20.f, 7, Color(0, 0, 0, .12f));
        r.Ellipse(p.x, p.y + 1, 11.f, 4, Color(0, 0, 0, .32f));
    }
    else
        r.Ellipse(p.x, p.y + 1, 14.f, 5, Color(0, 0, 0, .35f));
    if (!controlling)
        r.Ellipse(p.x, p.y, 18, 7, Color(.75f, .69f, .45f, .20f));
    SpriteAnimation animation = protagonistAnimation;
    SpriteSheet* sheet = protagonistSprinting && protagonistRunSheet.Ready() ? &protagonistRunSheet
                                                                             : &protagonistSheet;
    Color tint = levelOne.dodgeTime > 0  ? Color(.5f, 1.5f, 2.f)
                 : levelOne.hurtTime > 0 ? Color(1.5f, .55f, .55f)
                                         : Color(1, 1, 1);
    if (model && protagonistModel.Draw(r, p, tint))
        return;
    if (levelOne.castTime > 0 && !protagonistAnimation.moving && attackSheet.Ready())
    {
        sheet = &attackSheet;
        animation.facing = levelOne.castFacing;
        animation.moving = true;
        animation.phase = (std::min)(3.f, std::floor((.18f - levelOne.castTime) / .045f));
    }
    if (sheet->Ready())
        sheet->Draw(r,
                    p,
                    spriteHeight,
                    animation,
                    tint,
                    width,
                    height,
                    levelOne.castTime > 0 && !protagonistAnimation.moving ? 1.4f : 0.f);
    else
    {
        // Keep the actor visible if an asset is missing or PNG decoding fails.
        r.Triangle(
            {p.x, p.y - 43}, {p.x - 14, p.y - 6}, {p.x + 14, p.y - 6}, Color(.44f, .49f, .52f));
        r.Ellipse(p.x, p.y - 44, 8, 10, Color(.61f, .53f, .43f));
    }
}

bool Prototype::CharacterInView(WorldPoint position, float bodyHeight) const
{
    const Point p = Screen(position.x, position.y);
    return p.x + 32 > 0 && p.x - 32 < width && p.y >= 0 && p.y - bodyHeight < height;
}

bool Prototype::StoryPersonVisible(const StoryLayout::Site& site, WorldPoint position) const
{
    return site.kind != StoryLayout::Kind::Evidence && StoryLayout::Visible(site, chapter) &&
           site.spriteIndex >= 0 && site.spriteIndex < StoryLayout::NpcCount &&
           (npcModel.Ready() || storyNpcSheets[site.spriteIndex].Ready()) &&
           CharacterInView(position, StoryLayout::PersonHeight(site));
}

void Prototype::StoryPerson(const StoryLayout::Site& site,
                            WorldPoint position,
                            CharacterModel::State* state)
{
    const int index = site.spriteIndex;

    const Point p = Screen(position.x, position.y);
    if (!StoryPersonVisible(site, position))
    {
        return;
    }

    SpriteAnimation animation;
    if (Distance(position, player) < 160)
    {
        const double dx = player.x - position.x, dy = player.y - position.y;
        animation.Update((dx - dy) * .85, (dx + dy) * .43, 0, false);
        animation.moving = false;
    }

    const bool childPortrait = site.kind == StoryLayout::Kind::Child;
    const auto appearance =
        models.npcDirect ? models.catalog.Story(index, site.appearance) : site.appearance;
    r.Ellipse(p.x, p.y + 1, childPortrait ? 10.f : 14.f, 5, Color(0, 0, 0, .35f));
    if (DrawKeyedModel(npcModel,
                       p,
                       78 * appearance.scale,
                       animation,
                       Color(1, 1, 1),
                       CharacterModel::Motion::Idle,
                       time * .75f + index * .11f,
                       state,
                       &appearance.partIndices))
    {
        return;
    }
    storyNpcSheets[index].Draw(
        r, p, StoryLayout::PersonHeight(site), animation, Color(1, 1, 1), width, height);
}

void Prototype::DrawWorldLabel(WorldPoint position,
                               float elevation,
                               const std::string& text,
                               Color color)
{
    if (storyScene != StoryScene::None)
    {
        return;
    }
    const Point feet = Screen(position.x, position.y);
    if (feet.x < 0 || feet.x > width || feet.y < 0 || feet.y - elevation > height)
    {
        return;
    }
    const Point p = Screen(position.x, position.y, elevation);
    if (p.x < -90 || p.x > width + 90 || p.y < -20 || p.y > height + 20)
    {
        return;
    }
    float textWidth = 0;
    for (unsigned char c : text)
    {
        if ((c & 0xc0) != 0x80)
        {
            textWidth += c < 128 ? 8.f : 16.f;
        }
    }
    const float labelWidth = (std::min)(textWidth + 12, float(width) - 16);
    UIRect box = {p.x - labelWidth * .5f, p.y - 17, labelWidth, 24};
    auto overlaps = [](const UIRect& a, const UIRect& b)
    {
        return a.x < b.x + b.w + 4 && a.x + a.w + 4 > b.x && a.y < b.y + b.h + 4 &&
               a.y + a.h + 4 > b.y;
    };
    // Keep a name beside its owner. Never wrap/clamp it into an unrelated screen location.
    const Point preferred = {box.x, box.y};
    const Point offsets[] = {{0, 0}, {-32, 0}, {32, 0}, {0, -28}, {-32, -28}, {32, -28}};
    bool placed = false;
    for (const auto& offset : offsets)
    {
        box.x = preferred.x + offset.x;
        box.y = preferred.y + offset.y;
        if (box.x < 8 || box.x + box.w > width - 8 || box.y < 8 || box.y + box.h > height - 8)
        {
            continue;
        }
        bool blocked = false;
        for (const auto& other : occupiedLabels)
            blocked = blocked || overlaps(box, other);
        if (hudVisible)
        {
            blocked =
                blocked || overlaps(box, {20, 20, 330, 200}) || overlaps(box, MinimapBounds());
        }
        for (const auto& site : StoryLayout::Sites())
        {
            if (!StoryPersonVisible(site, site.position))
                continue;
            const Point body = Screen(site.position.x, site.position.y);
            const float bodyHeight = StoryLayout::PersonHeight(site);
            blocked =
                blocked || overlaps(box, {body.x - 24, body.y - bodyHeight, 48, bodyHeight + 5});
        }
        for (const auto& person : population.people)
        {
            if (!ResidentVisible(person))
                continue;
            const Point body = Screen(person.position.x, person.position.y);
            blocked = blocked || overlaps(box, {body.x - 24, body.y - 67, 48, 72});
        }
        if (!blocked)
        {
            placed = true;
            break;
        }
    }
    if (!placed)
    {
        return;
    }
    occupiedLabels.push_back(box);
    r.Rect(box.x, box.y, box.w, box.h, Color(.015f, .025f, .035f, .92f));
    r.Text(box.x + 6, box.y + 17, text, color);
}

bool Prototype::DrawKeyedModel(CharacterModel& model,
                               Point feet,
                               float size,
                               SpriteAnimation animation,
                               Color tint,
                               CharacterModel::Motion motion,
                               float phase,
                               CharacterModel::State* state,
                               const CharacterModel::Parts* parts)
{
    if (!model.Ready())
    {
        return false;
    }
    CharacterModel::State preview;
    auto& pose = state ? *state : preview;
    const float facing = CharacterFacing(animation.facing);
    if (pose.owner != &model)
    {
        model.Reset(pose, facing);
    }
    const float dt = pose.clock < 0 ? .05f : (std::max)(0.f, (std::min)(.05f, time - pose.clock));
    pose.clock = time;
    model.Animate(pose, pauseMenu || journal ? 0 : dt, motion, phase, facing);
    return model.Draw(r, feet, tint, pose, size, parts);
}

bool Prototype::DrawSpiritModel(int species,
                                Point feet,
                                float size,
                                SpriteAnimation animation,
                                Color tint,
                                float motionClock,
                                float castProgress,
                                CharacterModel::State* state,
                                bool running)
{
    if (!UsingSpiritModel(species))
    {
        return false;
    }
    const bool casting = castProgress >= 0;
    if (spiritMeshes[species].Ready())
    {
        float stridePhase = animation.phase * .25f;
        const auto profile = models.catalog.spirits.find(species);
        if (state && SpiritCatalog::Get(species).gait == 2 &&
            profile != models.catalog.spirits.end() && profile->second.walkStrideRatio > 0)
        {
            auto& model = spiritMeshes[species];
            if (state->owner != &model)
            {
                model.Reset(*state, CharacterFacing(animation.facing));
            }
            const auto& gait = profile->second;
            const float elapsed = state->clock < 0 ? 0 : time - state->clock;
            const double distance = animation.travel - state->gaitTravel;
            if (state->gaitInitialized && elapsed > .0001f && elapsed < .2f && distance >= 0 &&
                distance < 250 && !pauseMenu && !journal)
            {
                const float speed = static_cast<float>(distance / elapsed);
                const float threshold =
                    size * gait.runThresholdRatio * (state->gaitRunning ? .82f : 1.f);
                running = running || (animation.moving && speed > threshold);
                if (animation.moving)
                {
                    const double cycle =
                        size * (running ? gait.runStrideRatio : gait.walkStrideRatio);
                    state->gaitPhase =
                        static_cast<float>(std::fmod(state->gaitPhase + distance / cycle, 1.));
                }
            }
            // Rebase after culling, loading or teleporting; camera motion never adds steps.
            state->gaitTravel = animation.travel;
            state->gaitInitialized = true;
            state->gaitRunning = running;
            stridePhase = state->gaitPhase;
        }
        const auto motion = casting             ? CharacterModel::Motion::Cast
                            : !animation.moving ? CharacterModel::Motion::Idle
                            : running           ? CharacterModel::Motion::Run
                                                : CharacterModel::Motion::Walk;
        const float phase = casting            ? castProgress
                            : animation.moving ? stridePhase
                                               : motionClock * .75f;
        return DrawKeyedModel(
            spiritMeshes[species], feet, size, animation, tint, motion, phase, state);
    }
    const int first = casting            ? SpiritAppearance::CastFirst
                      : animation.moving ? SpiritAppearance::GlideFirst
                                         : SpiritAppearance::IdleFirst;
    const int count = casting            ? SpiritAppearance::CastFrames
                      : animation.moving ? SpiritAppearance::GlideFrames
                                         : SpiritAppearance::IdleFrames;
    if (casting)
    {
        animation.phase = (std::max)(0.f, (std::min)(1.f, castProgress)) * (count - 1);
    }
    else
    {
        // Four old walk phases represent one quadruped stride. Preserve distance-based gait.
        const float phase = species == SpiritCatalog::Hands && animation.moving
                                ? animation.phase * .25f
                                : motionClock / SpiritAppearance::Get(species).loopSeconds;
        animation.phase = (phase - std::floor(phase)) * count;
    }
    animation.moving = true;
    // Float, cloth response and fire are baked in. Do not shear or bob the entire model again.
    spiritModelSheets[species].Draw(
        r, feet, size, animation, tint, width, height, 0, -1, 0, casting ? 1.f : 0.f, first, count);
    return true;
}

void Prototype::Wisp(int type, WorldPoint position, CharacterModel::State* state)
{
    Point p = Screen(position.x, position.y);
    if (p.x < -100 || p.x > width + 100 || p.y < -50 || p.y > height + 130)
        return;
    r.Ellipse(p.x, p.y, 18, 6, Color(0, 0, 0, .3f));
    SpriteAnimation animation;
    if (captured && capturedSpirit == type)
    {
        animation = companionAnimation;
        if (spiritCastPose > 0)
        {
            animation.facing = spiritCastFacing;
            animation.moving = true;
            animation.phase = (1.f - (std::min)(1.f, spiritCastPose / .8f)) * 4.f;
        }
    }
    float castProgress = -1.f;
    if (captured && capturedSpirit == type && spiritCastPose > 0)
    {
        castProgress =
            controlling && PreparingSkill() ? .45f : 1.f - (std::min)(1.f, spiritCastPose / .55f);
    }
    if (DrawSpiritModel(type,
                        p,
                        SpiritCatalog::Get(type).size,
                        animation,
                        Color(1, 1, 1),
                        time,
                        castProgress,
                        state,
                        controlling && requestedSprint))
    {
        return;
    }
    const int motion = SpiritCatalog::Get(type).motion;
    const float bob = motion == 0   ? std::sin(time * 2.f) * 1.5f - 3.f
                      : motion == 1 ? std::sin(time * 1.8f) * .6f
                                    : std::sin(time * 2.f) * .5f;
    const float casting = (captured && capturedSpirit == type)
                              ? std::sin((std::min)(1.f, spiritCastPose / .8f) * 3.14159265f)
                              : 0.f;
    if (spiritSheets[type].Ready())
        spiritSheets[type].Draw(r,
                                {p.x, p.y - 4 + bob},
                                SpiritCatalog::Get(type).size,
                                animation,
                                Color(1, 1, 1),
                                width,
                                height,
                                type == 0 ? 2.f : (type == 1 ? 1.f : .15f),
                                motion,
                                time,
                                casting);
    else
        r.Ellipse(p.x, p.y - 25, 12, 18, Color(.4f, .7f, .8f));
}

void Prototype::DrawEnemy(const EnemyVisual& enemy, CharacterModel::State* state)
{
    Point p = Screen(enemy.position.x, enemy.position.y);
    if (!enemy.weakened && enemy.flash > 0)
    {
        const float reaction = std::sin((1 - enemy.flash / .15f) * 3.14159265f) *
                               enemy.hitStrength * (enemy.boss > 0 ? .4f : 1.f);
        p.x += float(enemy.hitDirection.x - enemy.hitDirection.y) * .85f * reaction;
        p.y +=
            float(enemy.hitDirection.x + enemy.hitDirection.y) * .43f * reaction - reaction * .25f;
    }
    if (p.x < -140 || p.x > width + 140 || p.y < -30 || p.y > height + 160)
    {
        return;
    }

    if (enemy.weakened)
    {
        const float fade = enemy.remnantTime < 0 ? 1.f : (std::min)(1.f, enemy.remnantTime / 10.f);
        const float bob = std::sin(time * 3) * 4;
        r.Ellipse(p.x, p.y, 20, 7, Color(.3f, .8f, .75f, .18f * fade));
        SpriteAnimation remnant = enemy.animation;
        remnant.moving = false;
        if (DrawSpiritModel(enemy.type,
                            {p.x, p.y - 8},
                            55,
                            remnant,
                            Color(.65f, 1, .9f, .6f * fade),
                            time,
                            -1,
                            state))
        {
            return;
        }
        spiritSheets[enemy.type].Draw(r,
                                      {p.x, p.y - 8 + bob},
                                      55,
                                      enemy.animation,
                                      Color(.65f, 1, .9f, .6f * fade),
                                      width,
                                      height,
                                      1.f);
        return;
    }
    const auto& encounter = StoryEncounter::Get(enemy.boss);
    const float size = enemy.boss ? encounter.size : SpiritCatalog::Get(enemy.type).size;
    r.Ellipse(p.x, p.y, 20, 7, Color(0, 0, 0, .3f));
    if (encounter.sprite >= 0)
    {
        if (storyEnemySheets[encounter.sprite].Ready())
            storyEnemySheets[encounter.sprite].Draw(
                r, p, size, enemy.animation, Color(1, 1, 1), width, height);
        else
            r.Text(p.x - 45, p.y - 32, "전용 외형 로드 실패", Color(1, .6f, .5f));
    }
    else if (DrawSpiritModel(enemy.type,
                             p,
                             size,
                             enemy.animation,
                             Color(1, 1, 1),
                             time + float(enemy.position.x * .01),
                             enemy.castPose > 0 ? .25f + .5f * enemy.castPose : -1.f,
                             state,
                             enemy.running))
    {
        // Health bars and alerts below still belong to the existing enemy Actor.
    }
    else if (spiritSheets[enemy.type].Ready())
    {
        SpriteAnimation animation = enemy.animation;
        if (enemy.castPose > 0 && enemy.type != SpiritCatalog::Saint)
        {
            animation.moving = true;
            animation.phase = enemy.castPose * 2.f;
        }
        spiritSheets[enemy.type].Draw(r,
                                      p,
                                      size,
                                      animation,
                                      Color(1, 1, 1),
                                      width,
                                      height,
                                      .4f,
                                      SpiritCatalog::Get(enemy.type).motion,
                                      time + float(enemy.position.x * .01),
                                      enemy.castPose);
    }
    else
    {
        r.Ellipse(p.x, p.y - 25, 16, 24, Color(.4f, .7f, .8f));
    }
    if (!enemy.alerted && !enemy.weakened && enemy.flash <= 0 && enemy.boss == 0)
        return;
    const float barY = p.y - size - 11;
    r.Rect(p.x - 20, barY, 40, 4, Color(.15f, .08f, .09f));
    r.Rect(p.x - 20,
           barY,
           40 * (std::max)(0.f, enemy.health) / enemy.maximum,
           4,
           Color(.9f, .25f, .25f));
}

void Prototype::DrawCombatWarnings()
{
    const auto& combat = levelOne.combat;
    for (const auto& enemy : levelOne.enemies)
    {
        // Include the full ranged telegraph/charge reach, but skip distant NPC battles.
        const Point warningOrigin = Screen(enemy.position.x, enemy.position.y);
        if (warningOrigin.x < -680 || warningOrigin.x > width + 680 || warningOrigin.y < -500 ||
            warningOrigin.y > height + 600)
        {
            continue;
        }
        const int attackType = SpiritCatalog::Get(enemy.type).combatType;
        if (attackType == 3 && enemy.alerted && !enemy.weakened && enemy.health > 0)
        {
            const bool preparing = enemy.phase == 1;
            const float after = combat.saintRecovery - enemy.phaseTime;
            if (preparing || (enemy.phase == 3 && after < .4f))
            {
                const int toll = preparing ? enemy.ritualCount : enemy.ritualCount - 1;
                const bool radial = toll % 3 == 2;
                const WorldPoint target = radial ? enemy.position : enemy.ritualTarget;
                const Point center = Screen(target.x, target.y, 2);
                const float progress =
                    1 - (std::max)(0.f, (std::min)(1.f, enemy.phaseTime / combat.saintWindup));
                const float alpha = preparing ? .2f + progress * .45f : (1 - after / .4f) * .8f;
                const double radius = radial ? 110 : combat.saintRadius;
                for (int i = 0; i < 32; ++i)
                {
                    const double a = i * 6.2831853 / 32, b = (i + 1) * 6.2831853 / 32;
                    r.Line(Screen(target.x + std::cos(a) * radius, target.y + std::sin(a) * radius),
                           Screen(target.x + std::cos(b) * radius, target.y + std::sin(b) * radius),
                           2,
                           Color(.85f, .45f, 1.f, alpha));
                }
                if (radial)
                {
                    const double reach =
                        combat.saintShotBudget * combat.saintShotSpeed / combat.projectileRangeRate;
                    for (int i = 0; i < 8; ++i)
                    {
                        const double angle = i * 6.2831853 / 8;
                        r.Line(Screen(target.x, target.y),
                               Screen(target.x + std::cos(angle) * reach,
                                      target.y + std::sin(angle) * reach),
                               2,
                               Color(.85f, .45f, 1.f, alpha));
                    }
                }
                spiritSpellArt[2].DrawSpellTile(r,
                                                radial ? 3 : 0,
                                                {center.x, center.y - (preparing ? 5 : 30)},
                                                radial ? 130.f : 95.f,
                                                radial ? 130.f : 95.f,
                                                alpha,
                                                time);
            }
            continue;
        }
        if (!enemy.alerted || enemy.weakened || enemy.health <= 0 ||
            (enemy.phase != 1 && enemy.phase != 2))
            continue;
        const float windup = attackType == 0   ? combat.meleeWindup
                             : attackType == 1 ? combat.chargeWindup
                                               : combat.rangedWindup;
        const float progress = 1 - (std::max)(0.f, (std::min)(1.f, enemy.phaseTime / windup));
        const Color warning = enemy.phase == 2 ? Color(2.f, .2f, .1f, .6f)
                                               : Color(1.f, .24f, .07f, .18f + progress * .42f);
        const auto origin = enemy.position;
        if (attackType == 1)
        {
            const double duration =
                enemy.phase == 2 ? (std::max)(0.f, enemy.phaseTime) : combat.chargeDuration;
            const double reach = combat.chargeSpeed * duration * (enemy.slowed > 0 ? .35 : 1.);
            const double nx = -enemy.aim.y * combat.chargeRadius,
                         ny = enemy.aim.x * combat.chargeRadius;
            r.Quad(Screen(origin.x + nx, origin.y + ny),
                   Screen(origin.x - nx, origin.y - ny),
                   Screen(origin.x + enemy.aim.x * reach - nx, origin.y + enemy.aim.y * reach - ny),
                   Screen(origin.x + enemy.aim.x * reach + nx, origin.y + enemy.aim.y * reach + ny),
                   warning);
            // Swept circle contact also reaches around the start and end of the path.
            for (int end = 0; end < 2; ++end)
            {
                const WorldPoint cap = {origin.x + enemy.aim.x * reach * end,
                                        origin.y + enemy.aim.y * reach * end};
                for (int i = 0; i < 16; ++i)
                {
                    const double a = i * 6.2831853 / 16, b = (i + 1) * 6.2831853 / 16;
                    r.Triangle(Screen(cap.x, cap.y),
                               Screen(cap.x + std::cos(a) * combat.chargeRadius,
                                      cap.y + std::sin(a) * combat.chargeRadius),
                               Screen(cap.x + std::cos(b) * combat.chargeRadius,
                                      cap.y + std::sin(b) * combat.chargeRadius),
                               warning);
                }
            }
        }
        else if (attackType == 0)
        {
            const double base = std::atan2(enemy.aim.y, enemy.aim.x);
            const double halfArc = std::acos(combat.meleeFacingDot);
            for (int i = 0; i < 16; ++i)
            {
                const double a = base - halfArc + i * halfArc * 2 / 16;
                const double b = base - halfArc + (i + 1) * halfArc * 2 / 16;
                const Point center = Screen(origin.x, origin.y);
                r.Quad(center,
                       Screen(origin.x + std::cos(a) * combat.meleeReach,
                              origin.y + std::sin(a) * combat.meleeReach),
                       Screen(origin.x + std::cos(b) * combat.meleeReach,
                              origin.y + std::sin(b) * combat.meleeReach),
                       center,
                       warning);
            }
        }
        else
        {
            for (int i = -1; i <= 1; ++i)
            {
                const double a = i * combat.rangedSpread, c = std::cos(a), s = std::sin(a);
                const double reach =
                    combat.rangedReach * combat.rangedSpeed / combat.projectileRangeRate;
                r.Line(Screen(origin.x, origin.y),
                       Screen(origin.x + (enemy.aim.x * c - enemy.aim.y * s) * reach,
                              origin.y + (enemy.aim.x * s + enemy.aim.y * c) * reach),
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
