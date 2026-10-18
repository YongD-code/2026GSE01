// NPC services use the same rectangles for drawing and pointer input.
UIRect ServiceButton(int row) const
{
    const float w = (std::min)(600.f, float(width) - 32);
    const float h = (std::min)(36.f, (float(height) - 150) / 6);
    return {(width - w) * .5f, 100 + row * (h + 5), w, h};
}

std::vector<std::string> ServiceOptions() const
{
    if (serviceNpc == "smith")
    {
        return {"대화하기",
                "강화석 구매 · 30금",
                "강화석 1개 판매 · 12금",
                "무기 강화 · 강화석 1개 + " + std::to_string(30 + levelOne.weapon * 15) + "금",
                "전리품 모두 판매 · 개당 5금",
                "종료하기"};
    }
    if (serviceNpc == "sealer")
    {
        return {"대화하기",
                "봉인권 구매 · 25금",
                "고급 봉인권 구매 · 80금",
                "주령석 구매 · 40금",
                "동행 주령 강화 · 주령석 사용",
                "종료하기"};
    }
    return {"대화하기",
            "회복물약 구매 · 20금",
            "회복물약 1개 판매 · 8금",
            "전리품 모두 판매 · 개당 5금",
            "회복물약 사용 · 본체 체력 30%",
            "종료하기"};
}

void ServiceAction(int action)
{
    const auto* site = NearbyStory();
    if (serviceNpc.empty() || !site || site->id != serviceNpc || controlling || levelOne.Dead())
    {
        serviceNpc.clear();
        return;
    }
    auto& l = levelOne;
    auto buy = [&](int& item, int cost)
    {
        if (l.gold < cost || item >= 1000000)
            l.notice = "금화가 부족하거나 보관 한도입니다.";
        else
        {
            l.gold -= cost;
            ++item;
            l.notice = "구매했습니다.";
        }
    };
    auto sell = [&](int& item, int price, bool all)
    {
        if (item <= 0)
        {
            l.notice = "판매할 아이템이 없습니다.";
            return;
        }
        const int amount = all ? item : 1;
        if (l.gold > 100000000 - amount * price)
        {
            l.notice = "금화 보관 한도입니다.";
            return;
        }
        l.gold += amount * price;
        item -= amount;
        l.notice = "판매했습니다.";
    };
    if (action == 5)
    {
        serviceNpc.clear();
        return;
    }
    if (action == 0)
    {
        l.notice =
            serviceNpc == "smith" ? "대장장이: 강화석과 금화를 가져오면 무기를 확실히 강화해 주지."
            : serviceNpc == "sealer"
                ? "봉인사: 봉인권으로 잔영을 포획하고, 주령석으로 동행을 강화하세요."
                : "잡화상: 물약은 가방에 보관해 두었다가 H로 마시면 돼요. 전리품도 사 드립니다.";
    }
    else if (serviceNpc == "smith")
    {
        if (action == 1)
            buy(l.weaponStones, 30);
        if (action == 2)
            sell(l.weaponStones, 12, false);
        if (action == 3)
        {
            const int cost = 30 + l.weapon * 15;
            if (l.weapon >= 20)
                l.notice = "무기가 최대 강화에 도달했습니다.";
            else if (l.weaponStones <= 0 || l.gold < cost)
                l.notice = "강화석 또는 금화가 부족합니다.";
            else
            {
                --l.weaponStones;
                l.gold -= cost;
                ++l.weapon;
                l.notice = "무기 강화 성공!";
            }
        }
        if (action == 4)
            sell(l.spoils, 5, true);
    }
    else if (serviceNpc == "sealer")
    {
        if (action == 1)
            buy(l.tickets[1], 25);
        if (action == 2)
            buy(l.tickets[2], 80);
        if (action == 3)
            buy(l.stones, 40);
        if (action == 4)
            l.Upgrade(captured);
    }
    else
    {
        if (action == 1)
            buy(l.potions, 20);
        if (action == 2)
            sell(l.potions, 8, false);
        if (action == 3)
            sell(l.spoils, 5, true);
        if (action == 4)
            l.UsePotion();
    }
    autoSaveTime = 30;
}

void DrawService()
{
    r.Rect(0, 0, float(width), float(height), Color(.015f, .025f, .04f, .94f));
    const auto box = ServiceButton(0);
    const std::string name = serviceNpc == "smith"    ? "대장장이"
                             : serviceNpc == "sealer" ? "봉인사"
                                                      : "잡화상";
    r.Text(
        box.x, 32, name + " · " + std::to_string(levelOne.gold) + "금", Color(1, .85f, .6f), true);
    WrappedText(box.x,
                58,
                box.w,
                "물약 " + std::to_string(levelOne.potions) + " / 강화석 " +
                    std::to_string(levelOne.weaponStones) + " / 주령석 " +
                    std::to_string(levelOne.stones) + " / 전리품 " +
                    std::to_string(levelOne.spoils),
                Color(.8f, .85f, .9f),
                2);
    const auto options = ServiceOptions();
    for (int i = 0; i < 6; ++i)
    {
        const auto cell = ServiceButton(i);
        UIButton(cell, "     " + std::to_string(i + 1) + "  " + options[i]);
        SpriteAnimation pose;
        if (i == 0)
        {
            const int npc = StoryLayout::NpcSpriteIndex(serviceNpc);
            if (npc >= 0)
                storyNpcSheets[npc].Draw(r,
                                         {cell.x + 23, cell.y + cell.h - 2},
                                         cell.h - 4,
                                         pose,
                                         Color(1, 1, 1),
                                         width,
                                         height);
        }
        else if (i < 5)
        {
            const int icon = serviceNpc == "smith"    ? 1
                             : serviceNpc == "sealer" ? (i < 3 ? 4 : 3)
                                                      : (i == 3 ? 0 : 2);
            lootSheets[icon].Draw(r,
                                  {cell.x + 23, cell.y + cell.h - 2},
                                  cell.h - 5,
                                  pose,
                                  Color(1, 1, 1),
                                  width,
                                  height);
        }
    }
    WrappedText(box.x,
                ServiceButton(5).y + ServiceButton(5).h + 22,
                box.w,
                levelOne.notice,
                Color(.85f, .9f, .8f),
                2);
}
