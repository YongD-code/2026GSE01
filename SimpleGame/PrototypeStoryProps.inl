// Authored story props use the existing story actors and cached static meshes.
void Prototype::StoryFacade(const Object& building)
{
    const auto& o = building;
    const Point origin = Screen(o.x, o.y);
    if (origin.x < -600 || origin.x > width + 600 || origin.y < -350 || origin.y > height + 500)
    {
        return;
    }
    const std::string key = "story-facade-v1:" + std::to_string(int(o.x));
    if (r.BeginCachedMesh(key, origin))
    {
        const double front = o.y + o.d - 11;
        const Color dark(.12f, .14f, .15f), wood(.37f, .24f, .13f), paper(.81f, .74f, .55f);
        auto p = [&](double x, double z)
        {
            return Screen(o.x + x, front, z);
        };
        if (o.x == 1900)
        {
            // Detention hall: barred windows and riveted reinforced entrance.
            for (int side = 0; side < 2; ++side)
            {
                const double start = side ? o.w * .70 : o.w * .08;
                for (int bar = 0; bar < 5; ++bar)
                {
                    r.Line(p(start + bar * 6, 25), p(start + bar * 6, 65), 3, dark);
                }
                r.Line(p(start - 2, 35), p(start + 26, 35), 3, dark);
            }
            for (int z = 20; z < 70; z += 18)
            {
                r.Line(p(o.w * .38, z), p(o.w * .62, z), 4, wood);
            }
        }
        else if (o.x == 2440 || o.x == 2740 || o.x == 2200)
        {
            // Shipping depots: crossed braces, loading doors and cargo seals.
            r.Line(p(o.w * .36, 12), p(o.w * .64, o.h * .53), 5, wood);
            r.Line(p(o.w * .64, 12), p(o.w * .36, o.h * .53), 5, wood);
            for (int i = 0; i < 3; ++i)
            {
                const double x = o.w * .12 + i * 12;
                r.Quad(p(x, 50), p(x + 7, 50), p(x + 7, 66), p(x, 66), paper);
                r.Line(p(x + 2, 53), p(x + 5, 63), 1, dark);
            }
        }
        else if (o.x == 3060 || o.x == 1570 || o.x == 940)
        {
            // Sacred hall: braided entrance rope and folded paper wards.
            for (int i = 0; i < 12; ++i)
            {
                const double x = o.w * .2 + o.w * .6 * i / 12;
                const double next = o.w * .2 + o.w * .6 * (i + 1) / 12;
                r.Line(p(x, o.h * .65 - std::sin(i * 3.14159 / 12) * 8),
                       p(next, o.h * .65 - std::sin((i + 1) * 3.14159 / 12) * 8),
                       4,
                       wood);
            }
            for (int i = 0; i < 5; ++i)
            {
                const double x = o.w * .27 + i * o.w * .11, z = o.h * .65 - 8;
                r.Line(p(x, z), p(x - 4, z - 8), 4, paper);
                r.Line(p(x - 4, z - 8), p(x + 3, z - 11), 4, paper);
                r.Line(p(x + 3, z - 11), p(x - 2, z - 18), 4, paper);
            }
        }
        else
        {
            // Roadside shop: fabric doorway curtains and timber signboard.
            for (int i = 0; i < 4; ++i)
            {
                const double x = o.w * .34 + i * o.w * .08;
                r.Quad(p(x, o.h * .59),
                       p(x + o.w * .07, o.h * .59),
                       p(x + o.w * .07, o.h * .42),
                       p(x, o.h * .42),
                       Color(.25f, .33f, .32f));
            }
            r.Line(p(o.w * .15, o.h * .55), p(o.w * .15, o.h * .85), 8, wood);
            r.Line(p(o.w * .15, o.h * .59), p(o.w * .15, o.h * .79), 2, paper);
        }
        r.EndCachedMesh();
    }
    r.DrawCachedMesh(key, origin);
}

void Prototype::StoryProp(const StoryLayout::Site& site, WorldPoint at)
{
    const Point origin = Screen(at.x, at.y);
    const std::string key =
        "story-detail-v6:" + site.id + (StoryCharacterModel(site.id) ? ":workstation" : "") +
        (site.id == "seal" && chapter.step > chapter.Index("P007_SEAL") ? ":collected"
                                                                        : ":present");
    if (r.BeginCachedMesh(key, origin))
    {
        const Color wood(.27f, .18f, .11f), edge(.12f, .10f, .085f), timber(.46f, .32f, .19f);
        const Color stone(.40f, .43f, .43f), ink(.12f, .15f, .17f), paper(.81f, .75f, .57f);
        const Color red(.56f, .15f, .10f), metal(.32f, .35f, .39f);
        auto p = [&](double x, double y, double z = 0.)
        {
            return Screen(at.x + x, at.y + y, z);
        };
        auto line =
            [&](double x, double y, double z, double u, double v, double h, float w, Color c)
        {
            r.Line(p(x, y, z), p(u, v, h), w, c);
        };
        auto box = [&](double x, double y, double w, double d, double h, Color c)
        {
            Box(at.x + x,
                at.y + y,
                w,
                d,
                h,
                c,
                Color(c.r * .8f, c.g * .8f, c.b * .8f),
                Color(c.r * .6f, c.g * .6f, c.b * .6f));
        };
        auto sheet = [&](double x, double y, double z, double w, double d)
        {
            r.Quad(p(x, y, z), p(x + w, y, z), p(x + w, y + d, z), p(x, y + d, z), paper);
            for (int i = 0; i < 4; ++i)
            {
                line(x + 2, y + 2 + i * 2, z + .3, x + w - 2, y + 2 + i * 2, z + .3, 1, ink);
            }
        };
        auto lantern = [&](double x, double y)
        {
            box(x - 7, y - 7, 14, 14, 5, stone);
            line(x, y, 5, x, y, 30, 5, stone);
            const Point light = p(x, y, 37);
            r.Ellipse(light.x, light.y, 6, 8, Color(.86f, .52f, .19f));
            r.Triangle(p(x - 11, y, 43), p(x, y, 52), p(x + 11, y, 43), ink);
        };
        auto desk = [&]()
        {
            box(-23, -35, 46, 23, 26, wood);
            for (int i = 0; i < 5; ++i)
            {
                line(-22 + i * 9, -35, 27, -22 + i * 9, -12, 27, 1, edge);
            }
        };
        if (site.id == "cart")
        {
            box(-28, -18, 56, 34, 12, wood);
            for (int i = 0; i < 6; ++i)
            {
                line(-27 + i * 10, -18, 14, -27 + i * 10, 16, 14, 2, timber);
            }
            for (int side = -1; side <= 1; side += 2)
            {
                for (int rail = 0; rail < 3; ++rail)
                {
                    line(-28, side * 18, 18 + rail * 6, 22, side * 18, 18 + rail * 6, 3, timber);
                }
                const Point wheel = p(0, side * 23, 10);
                r.Ellipse(wheel.x, wheel.y, 15, 16, edge);
                r.Ellipse(wheel.x, wheel.y, 11, 12, timber);
                for (int spoke = 0; spoke < 6; ++spoke)
                {
                    const float a = spoke * 1.0472f;
                    r.Line(
                        wheel, {wheel.x + std::cos(a) * 11, wheel.y + std::sin(a) * 12}, 2, edge);
                }
                line(23, side * 12, 12, 63, side * 15, 3, 4, wood);
            }
            sheet(-12, -8, 15, 18, 14);
            line(10, 4, 16, 25, 20, 5, 3, red);
        }
        else if (site.id == "mark")
        {
            box(-8, -8, 16, 16, 6, stone);
            line(0, 0, 6, 0, 0, 35, 5, wood);
            r.Quad(p(-14, 0, 24), p(10, 0, 24), p(17, 0, 29), p(-14, 0, 33), timber);
            line(-8, 0, 28, 6, 0, 28, 2, paper);
            line(3, 0, 25, 6, 0, 28, 2, paper);
            line(0, 0, 21, 9, 0, 15, 3, red);
        }
        else if (site.id == "belonging")
        {
            const Point bag = p(0, 0, 7);
            r.Ellipse(bag.x, bag.y, 15, 10, Color(.37f, .29f, .48f));
            r.Line({bag.x - 8, bag.y - 6}, {bag.x + 8, bag.y + 3}, 2, paper);
            r.Ellipse(bag.x + 9, bag.y - 5, 4, 4, red);
            line(-8, 4, 2, -18, 12, 1, 3, red);
            sheet(7, 5, 2, 9, 7);
        }
        else if (site.id == "grave")
        {
            box(-19, -15, 38, 30, 7, stone);
            box(-12, -9, 24, 18, 16, stone);
            box(-8, -5, 16, 10, 48, stone);
            for (int i = 0; i < 5; ++i)
            {
                line(-2, 5, 39 - i * 5, 3, 5, 37 - i * 5, 1, ink);
            }
            line(-6, 5, 45, 1, 5, 32, 1, ink);
            for (int i = 0; i < 3; ++i)
            {
                line(-12 + i * 4, 17, 1, -12 + i * 4, 17, 14, 1, wood);
            }
            const Point bowl = p(10, 17, 4);
            r.Ellipse(bowl.x, bowl.y, 6, 3, metal);
        }
        else if (site.id == "seal" || site.id == "relic")
        {
            if (site.id == "relic")
            {
                box(-28, -23, 56, 46, 6, stone);
                box(-22, -18, 44, 36, 13, stone);
                box(-15, -12, 30, 24, 23, stone);
                lantern(-34, -18);
                lantern(34, -18);
            }
            if (site.id == "seal")
            {
                // Keep the story clue on the wall and leave the floor unobstructed.
                const double wall = CemeteryRoom::Bounds().y + 1 - at.y;
                for (int i = 0; i < 4; ++i)
                {
                    line(-12 + i * 8, wall, 23, 10 - i * 7, wall, 57, 2, red);
                }
                line(-17, wall, 24, 17, wall, 24, 2, ink);
                line(-17, wall, 57, 17, wall, 57, 2, ink);
                if (CemeteryCleared())
                {
                    // Small fragments rest at the wall foot, without a raised platform.
                    const bool collected = chapter.step > chapter.Index("P007_SEAL");
                    if (!collected)
                    {
                        for (int i = 0; i < 2; ++i)
                        {
                            const double x = -9 + i * 17;
                            r.Triangle(p(x - 5, -20, 1),
                                       p(x + 2, -20, 13 - i * 3),
                                       p(x + 6, -20, 1),
                                       Color(.17f, .11f, .23f));
                            r.Triangle(p(x + 2, -20, 13 - i * 3),
                                       p(x + 6, -20, 1),
                                       p(x + 3, -14, 1),
                                       Color(.38f, .25f, .44f));
                            line(x, -20, 3, x + 2, -20, 9, 1, Color(.78f, .58f, .87f));
                        }
                    }
                }
            }
            else
            {
                r.Triangle(p(-10, 0, 27), p(0, 0, 62), p(13, 0, 35), Color(.19f, .12f, .26f));
                r.Triangle(p(0, 0, 62), p(13, 0, 35), p(4, 8, 28), Color(.37f, .24f, .45f));
                line(-7, 0, 36, 7, 0, 46, 2, Color(.64f, .45f, .78f));
                for (int i = -1; i <= 1; i += 2)
                {
                    line(i * 19, 4, 15, 0, 0, 38, 2, metal);
                }
            }
        }
        else if (site.id == "cell")
        {
            box(-27, -22, 54, 17, 8, stone);
            for (int i = -3; i <= 3; ++i)
            {
                line(i * 8, -18, 8, i * 8, -18, 62, 3, metal);
            }
            line(-27, -18, 62, 27, -18, 62, 5, edge);
            line(-27, -18, 12, 27, -18, 12, 4, edge);
            for (int side = -1; side <= 1; side += 2)
            {
                for (int link = 0; link < 5; ++link)
                {
                    const Point c = p(side * (15 - link), -10 + link * 3, 37 - link * 5);
                    r.Ellipse(c.x, c.y, 3, 4, metal);
                    r.Ellipse(c.x, c.y, 1.5f, 2, edge);
                }
                const Point cuff = p(side * 10, 5, 9);
                r.Ellipse(cuff.x, cuff.y, 7, 5, metal);
                r.Ellipse(cuff.x, cuff.y, 4, 3, edge);
            }
            sheet(-8, 7, 1, 16, 11);
        }
        else if (site.id == "room")
        {
            box(-20, -14, 40, 28, 20, wood);
            r.Quad(p(-20, -14, 21), p(20, -14, 21), p(20, -22, 40), p(-20, -22, 40), timber);
            for (int side = -1; side <= 1; side += 2)
            {
                line(side * 13, -14, 22, side * 13, 14, 22, 3, metal);
            }
            sheet(-14, -7, 22, 16, 12);
            const Point cloth = p(9, 2, 23);
            r.Ellipse(cloth.x, cloth.y, 9, 5, Color(.37f, .29f, .48f));
            line(-4, 14, 16, 4, 14, 16, 3, metal);
        }
        else if (site.id == "ledger" || site.id == "field_record")
        {
            desk();
            sheet(-19, -32, 28, 18, 17);
            sheet(0, -32, 28, 18, 17);
            line(-1, -32, 29, -1, -15, 29, 2, red);
            const Point pot = p(20, -21, 30);
            r.Ellipse(pot.x, pot.y, 4, 3, ink);
            line(13, -16, 29, 24, -30, 32, 2, edge);
            if (site.id == "field_record")
            {
                line(-23, -35, 26, -23, -35, 61, 4, wood);
                line(23, -35, 26, 23, -35, 61, 4, wood);
                line(-23, -35, 61, 23, -35, 61, 4, wood);
                for (int i = -1; i <= 1; ++i)
                {
                    sheet(i * 13 - 4, -34, 46, 8, 12);
                }
            }
            else
            {
                box(27, -31, 20, 18, 17, wood);
                sheet(29, -28, 18, 15, 11);
            }
        }
        else if (site.id == "merchant" && !merchantModel.Ready())
        {
            desk();
            for (int i = 0; i < 4; ++i)
            {
                const Point goods = p(-15 + i * 10, -22, 32);
                r.Ellipse(goods.x, goods.y, 5, 6, i % 2 ? red : paper);
            }
            line(-27, -36, 0, -27, -36, 66, 4, wood);
            line(27, -36, 0, 27, -36, 66, 4, wood);
            r.Quad(p(-32, -40, 66),
                   p(32, -40, 66),
                   p(32, -9, 58),
                   p(-32, -9, 58),
                   Color(.28f, .34f, .28f));
            for (int i = -2; i <= 2; ++i)
            {
                line(i * 12, -40, 67, i * 12, -9, 59, 2, paper);
            }
        }
        else if (site.id == "smith")
        {
            box(-27, -32, 24, 22, 24, stone);
            const Point fire = p(-15, -20, 27);
            r.Ellipse(fire.x, fire.y, 10, 5, Color(.92f, .36f, .10f));
            if (!smithModel.Ready())
            {
                box(4, -28, 22, 17, 20, wood);
                r.Quad(p(0, -30, 30), p(29, -30, 30), p(25, -10, 30), p(6, -10, 30), metal);
                line(8, -21, 20, 8, -21, 29, 9, metal);
                line(15, -20, 31, 28, -12, 31, 3, wood);
                line(26, -15, 32, 30, -9, 32, 6, metal);
            }
        }
        else if (site.id == "sealer")
        {
            desk();
            sheet(-18, -31, 28, 30, 16);
            lantern(30, -25);
            for (int i = -1; i <= 1; ++i)
            {
                line(i * 8, -18, 29, i * 8 + 4, -27, 29, 2, red);
            }
        }
        else if (site.id.compare(0, 5, "raen_") == 0)
        {
            lantern(-25, -20);
            box(15, -25, 17, 16, 12, wood);
            sheet(17, -23, 13, 12, 11);
        }
        else if (site.id == "parent" || site.id == "sibling")
        {
            box(-22, -24, 44, 14, 11, wood);
            const Point basket = p(16, -17, 17);
            r.Ellipse(basket.x, basket.y, 9, 6, timber);
            for (int i = -2; i <= 2; ++i)
            {
                r.Line({basket.x + i * 3, basket.y - 4}, {basket.x + i * 3, basket.y + 4}, 1, edge);
            }
        }
        else if (site.id == "child")
        {
            const Point ball = p(18, -10, 5);
            r.Ellipse(ball.x, ball.y, 6, 6, red);
            r.Line({ball.x - 5, ball.y}, {ball.x + 5, ball.y}, 1, paper);
            line(-18, -12, 1, -6, -12, 1, 2, paper);
            line(-12, -17, 1, -12, -6, 1, 2, paper);
        }
        else if (site.id == "stranger")
        {
            const Point pack = p(-20, -20, 8);
            r.Ellipse(pack.x, pack.y, 12, 10, wood);
            line(-21, -21, 4, -31, -27, 51, 3, timber);
            sheet(-15, -20, 12, 8, 9);
        }
        r.EndCachedMesh();
    }
    r.DrawCachedMesh(key, origin);
}
