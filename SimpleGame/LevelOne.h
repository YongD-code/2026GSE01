#pragma once
#include "World.h"
#include "SpriteSheet.h"
#include "CaptureRules.h"
#include <algorithm>
#include <random>
#include <string>
#include <vector>

// Session-only training progression. World collision is supplied by the host.
class LevelOne
{
  public:
    struct Enemy
    {
        WorldPoint position;
        int type;
        float health, maximum, flash = 0;
        SpriteAnimation animation;
        WorldPoint home = {0, 0};
        World::Key camp = {0, 0};
        int slot = 0;
        bool alerted = false, returning = false;
        std::vector<WorldPoint> trail;
        bool weakened = false;
        int boss = 0;
        float slowed = 0;
        // 0: approach, 1: committed windup, 2: charge, 3: recovery.
        int phase = 0;
        float phaseTime = 0, stagger = 0, staggerGuard = 0;
        WorldPoint aim = {0, 0};
    };

    struct Projectile
    {
        WorldPoint position, velocity;
        float remaining, damage;
        bool companion;
        bool hostile = false;
        unsigned long long actorId = 0;
    };

    enum class LootKind
    {
        Experience,
        Weapon,
        Healing,
        Stone,
        Ticket
    };

    struct Loot
    {
        WorldPoint position;
        LootKind kind;
        float age = 0;
        bool attracted = false;
        unsigned long long actorId = 0;
    };

    std::vector<Enemy> enemies;
    std::vector<Projectile> projectiles;
    std::vector<Loot> loot;
    int level = 1, experience = 0, kills = 0, weapon = 0, stones = 0, spiritRank = 0;
    int failedUpgrades = 0;
    float health = 100, invulnerability = 0, cooldown = 0, companionCooldown = 0;
    float castTime = 0;
    float dodgeTime = 0, dodgeCooldown = 0, hurtTime = 0;
    WorldPoint dodgeDirection = {0, 1};

    struct CombatTuning
    {
        float dodgeDuration = .22f, dodgeWait = 1.1f, dodgeSpeed = 580;
        float aggro = 270, chaseSpeed = 155, chargeSpeed = 570;
        float meleeWindup = .48f, chargeWindup = .7f, rangedWindup = .8f;
        float meleeDamage = 18, chargeDamage = 28, rangedDamage = 16;
    } combat;

    void LoadCombat()
    {
        wchar_t exe[32768] = {};
        GetModuleFileNameW(nullptr, exe, 32768);
        const std::wstring path = exe;
        const auto slash = path.find_last_of(L"\\/");
        const std::wstring folder = slash == std::wstring::npos ? L"" : path.substr(0, slash + 1);
        const std::wstring paths[] = {
            folder + L"Data/Combat.ini", L"Data/Combat.ini", L"SimpleGame/Data/Combat.ini"};
        std::map<std::string, float*> values = {{"dodge_duration", &combat.dodgeDuration},
                                                {"dodge_wait", &combat.dodgeWait},
                                                {"dodge_speed", &combat.dodgeSpeed},
                                                {"aggro", &combat.aggro},
                                                {"chase_speed", &combat.chaseSpeed},
                                                {"charge_speed", &combat.chargeSpeed},
                                                {"melee_windup", &combat.meleeWindup},
                                                {"charge_windup", &combat.chargeWindup},
                                                {"ranged_windup", &combat.rangedWindup},
                                                {"melee_damage", &combat.meleeDamage},
                                                {"charge_damage", &combat.chargeDamage},
                                                {"ranged_damage", &combat.rangedDamage}};
        for (const auto& candidate : paths)
        {
            std::ifstream file(candidate.c_str());
            if (!file)
                continue;
            std::string line;
            while (std::getline(file, line))
            {
                const auto equal = line.find('=');
                if (equal == std::string::npos)
                    continue;
                const auto found = values.find(line.substr(0, equal));
                if (found == values.end())
                    continue;
                try
                {
                    const float value = std::stof(line.substr(equal + 1));
                    if (std::isfinite(value) && value >= .05f && value <= 1000)
                        *found->second = value;
                }
                catch (...)
                {
                }
            }
            return;
        }
    }

    void Hurt(float damage)
    {
        if (Dead() || invulnerability > 0 || dodgeTime > 0)
            return;
        health = (std::max)(0.f, health - damage);
        invulnerability = .55f;
        hurtTime = .22f;
        if (Dead())
            notice = "불씨가 꺼졌습니다. R로 다시 시작하세요.";
    }

    template <class Collision>
    void Dodge(float dt, WorldPoint& player, WorldPoint direction, bool pressed, Collision blocked)
    {
        dodgeCooldown = (std::max)(0.f, dodgeCooldown - dt);
        const double length = std::hypot(direction.x, direction.y);
        if (length > .001 && dodgeTime <= 0)
            dodgeDirection = {direction.x / length, direction.y / length};
        if (pressed && dodgeCooldown <= 0 && dodgeTime <= 0)
        {
            dodgeTime = combat.dodgeDuration;
            dodgeCooldown = combat.dodgeWait;
        }
        if (dodgeTime > 0)
        {
            const float step = (std::min)(dt, dodgeTime);
            Move(player,
                 {dodgeDirection.x * combat.dodgeSpeed * step,
                  dodgeDirection.y * combat.dodgeSpeed * step},
                 blocked);
            invulnerability = (std::max)(invulnerability, dt + .01f);
            dodgeTime = (std::max)(0.f, dodgeTime - dt);
        }
    }

    bool automatic = false, complete = false;

    struct CampRecord
    {
        std::array<float, 4> health = {{-1, -1, -1, -1}};
        unsigned defeated = 0;
    };

    std::map<World::Key, CampRecord> camps;
    World::Key activeRegion = {0, 0};
    bool regionLoaded = false;
    Facing castFacing = Facing::South;
    std::string notice;
    std::mt19937 random{std::random_device{}()};
    CaptureRules captureRules;
    bool captureUnlocked = false;

    struct SpiritInstance
    {
        int species = 0, rarity = 1;
        bool memory = false;
    };

    std::vector<SpiritInstance> collection;
    std::array<int, 4> tickets = {{0, 0, 0, 0}};
    int selectedTicket = 0, saintRemains = 0;
    float skillCooldown = 0;
    int skillsUsed = 0;
    std::set<int> resolvedBosses;

    int Capturable(WorldPoint player) const
    {
        double best = 110;
        int index = -1;
        for (size_t i = 0; i < enemies.size(); ++i)
        {
            const double distance = Distance(player, enemies[i].position);
            if (enemies[i].health > 0 && enemies[i].weakened && distance < best)
            {
                best = distance;
                index = static_cast<int>(i);
            }
        }
        return index;
    }

    void Resolve(Enemy& enemy, bool purified)
    {
        enemy.health = 0;
        if (enemy.boss > 0)
            resolvedBosses.insert(enemy.boss);
        else
        {
            camps[enemy.camp].defeated |= 1u << enemy.slot;
            camps[enemy.camp].health[enemy.slot] = 0;
        }
        if (!purified)
            return;
        ++kills;
        loot.push_back({enemy.position, LootKind::Experience});
        if (kills == 1 || Roll() < .35f)
            loot.push_back({enemy.position, LootKind::Weapon});
        if (Roll() < .25f)
            loot.push_back({enemy.position, LootKind::Healing});
        if (Roll() < .12f)
            loot.push_back({enemy.position, LootKind::Stone});
        if (Roll() < captureRules.ticketDrop || enemy.boss == 2)
            loot.push_back({enemy.position, LootKind::Ticket});
        if (enemy.boss == 3)
        {
            stones += 2;
            ++tickets[2];
        }
    }

    void Capture(WorldPoint player, bool purify)
    {
        const int index = Capturable(player);
        if (index < 0)
            return;
        Enemy& enemy = enemies[index];
        if (purify)
        {
            Resolve(enemy, true);
            notice = "주령을 정화했습니다. 전리품을 수집하세요.";
            return;
        }
        if (tickets[selectedTicket] <= 0)
        {
            notice = "선택한 봉인권이 없습니다. T로 변경하거나 X로 정화하세요.";
            return;
        }
        if (enemy.boss == 4 && selectedTicket != 2)
        {
            notice = "성자에게는 고급 봉인권이 필요합니다. T로 선택하세요.";
            return;
        }
        --tickets[selectedTicket];
        const bool first = collection.empty();
        const float chance =
            first ? 1.f : (enemy.boss == 4 ? captureRules.bossChance : captureRules.normalChance);
        if (Roll() <= chance)
        {
            const float rarityRoll = Roll();
            int rarity =
                first ? 1
                      : (rarityRoll < captureRules.rareChance
                             ? 3
                             : (rarityRoll < captureRules.rareChance + captureRules.uncommonChance
                                    ? 2
                                    : 1));
            const int species = enemy.boss == 4 ? 3 : (first ? 0 : enemy.type);
            if (enemy.boss == 4)
                rarity = 4;
            collection.push_back({species, rarity, !first && Roll() < .15f});
            Resolve(enemy, false);
            notice = "포획 성공! 별 " + std::to_string(rarity) +
                     " 개체를 기록했습니다. Tab → 주령에서 확인하세요.";
        }
        else if (enemy.boss == 4)
        {
            ++saintRemains;
            Resolve(enemy, false);
            notice = "봉인 실패. 성자의 잔재를 얻었습니다. 성유물은 회수할 수 있습니다.";
        }
        else
        {
            // Keep the encounter available after failure; another ticket or purification is
            // possible.
            notice = "포획 실패. 봉인권 1장을 소모했습니다. 재시도하거나 X로 정화할 수 있습니다.";
        }
    }

    void UseSkill(WorldPoint player)
    {
        if (collection.empty() || skillCooldown > 0 || Dead())
            return;
        for (const Enemy& enemy : enemies)
            if (enemy.boss == 3 && enemy.alerted)
            {
                notice = "눈먼 수도사가 주령 능력을 봉쇄하고 있습니다.";
                return;
            }
        bool affected = false;
        for (Enemy& enemy : enemies)
            if (enemy.health > 0 && !enemy.weakened && Distance(player, enemy.position) < Range())
            {
                enemy.slowed = 3;
                affected = true;
            }
        if (!affected)
        {
            notice = "능력 범위 안에 적이 없습니다.";
            return;
        }
        skillCooldown = 8;
        ++skillsUsed;
        notice = "붙잡는 손 · 주변 적의 움직임을 3초 동안 늦춥니다.";
    }

    template <class Collision> void EnsureBoss(int id, WorldPoint home, Collision blocked)
    {
        if (resolvedBosses.count(id))
            return;
        for (const Enemy& enemy : enemies)
            if (enemy.boss == id)
                return;
        if (blocked(home))
            return;
        Enemy enemy;
        enemy.position = enemy.home = home;
        enemy.trail.push_back(home);
        enemy.type = id == 4 ? 1 : 0;
        enemy.boss = id;
        enemy.maximum = id == 4 ? 420.f : (id == 1 ? 220.f : 160.f);
        enemy.health = enemy.maximum;
        enemies.push_back(enemy);
    }

    int RequiredExperience() const
    {
        return 24 + (level - 1) * 12;
    }

    float MaximumHealth() const
    {
        return 100.f + (level - 1) * 12.f;
    }

    float Damage() const
    {
        return 18.f + (level - 1) * 3.f + weapon * 5.f;
    }

    float Interval() const
    {
        return (std::max)(.22f, .85f - (level - 1) * .035f);
    }

    float Range() const
    {
        return 300.f + (std::min)(level - 1, 10) * 6.f;
    }

    float MagnetRange() const
    {
        return 120.f + (std::min)(level - 1, 10) * 8.f;
    }

    bool Dead() const
    {
        return health <= 0;
    }

    float Roll()
    {
        return std::uniform_real_distribution<float>(0.f, 1.f)(random);
    }

    static double Distance(WorldPoint a, WorldPoint b)
    {
        return std::hypot(a.x - b.x, a.y - b.y);
    }

    template <class Collision> static bool ClearLine(WorldPoint a, WorldPoint b, Collision blocked)
    {
        const int steps = (std::max)(1, static_cast<int>(std::ceil(Distance(a, b) / 6)));
        for (int i = 1; i <= steps; ++i)
        {
            const double t = double(i) / steps;
            if (blocked({a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t}))
                return false;
        }
        return true;
    }

    template <class Collision>
    static void Move(WorldPoint& position,
                     WorldPoint delta,
                     Collision blocked,
                     std::vector<WorldPoint>* trail = nullptr)
    {
        const int steps =
            (std::max)(1, static_cast<int>(std::ceil(std::hypot(delta.x, delta.y) / 5)));
        for (int i = 0; i < steps; ++i)
        {
            WorldPoint next = {position.x + delta.x / steps, position.y};
            if (!blocked(next))
            {
                position = next;
                if (trail)
                    RecordTrail(*trail, position, blocked);
            }
            next = {position.x, position.y + delta.y / steps};
            if (!blocked(next))
            {
                position = next;
                if (trail)
                    RecordTrail(*trail, position, blocked);
            }
        }
    }

    template <class Collision>
    static void RecordTrail(std::vector<WorldPoint>& trail, WorldPoint point, Collision blocked)
    {
        if (!trail.empty() && Distance(trail.back(), point) < .001)
            return;
        if (trail.size() > 1 && ClearLine(trail[trail.size() - 2], point, blocked))
            trail.back() = point;
        else
            trail.push_back(point);
    }

    void Upgrade(bool captured)
    {
        if (Dead())
            return;
        if (!captured)
        {
            notice = "주령석은 보관됩니다. 먼저 E로 주령을 포획하세요.";
            return;
        }
        if (spiritRank >= 10)
        {
            notice = "주령이 최대 강화에 도달했습니다.";
            return;
        }
        if (stones <= 0)
        {
            notice = "강화에 필요한 주령석이 없습니다.";
            return;
        }
        --stones;
        if (failedUpgrades >= 3 || Roll() < .7f)
        {
            ++spiritRank;
            failedUpgrades = 0;
            notice = "주령 강화 성공! 동행 주술의 피해량이 증가했습니다.";
        }
        else
        {
            ++failedUpgrades;
            notice = "강화 실패. 등급은 유지됩니다. 3회 연속 실패 후 다음 강화는 확정 성공입니다.";
        }
    }

    void Collect(LootKind kind)
    {
        switch (kind)
        {
        case LootKind::Experience:
            if (level >= 20)
                break;
            experience += 12;
            while (level < 20 && experience >= RequiredExperience())
            {
                experience -= RequiredExperience();
                ++level;
                health = (std::min)(MaximumHealth(), health + 30.f);
                notice = "레벨 상승! 공격력·최대 체력·흡수 범위 증가, 발사 대기시간 감소.";
            }
            if (level == 20)
                experience = 0;
            break;
        case LootKind::Weapon:
            if (weapon < 20)
                ++weapon;
            notice = "강화 파편 흡수! 무기 피해량이 증가했습니다.";
            break;
        case LootKind::Healing:
            health = (std::min)(MaximumHealth(), health + MaximumHealth() * .3f);
            notice = "생명의 불씨를 흡수해 최대 체력의 30%를 회복했습니다.";
            break;
        case LootKind::Stone:
            ++stones;
            notice = "주령석 획득! G로 동행 주령을 강화하세요. 성공 확률 70%.";
            break;
        case LootKind::Ticket:
            ++tickets[1];
            notice = "봉인권을 획득했습니다.";
            break;
        }
    }

    template <class Collision> bool Shoot(WorldPoint origin, bool companion, Collision blocked)
    {
        Enemy* target = nullptr;
        double nearest = Range();
        for (Enemy& enemy : enemies)
        {
            const double distance = Distance(origin, enemy.position);
            if (companion && !enemy.alerted)
                continue;
            if (!enemy.weakened && !enemy.returning && enemy.health > 0 && distance <= nearest &&
                ClearLine(origin, enemy.position, blocked))
            {
                nearest = distance;
                target = &enemy;
            }
        }
        if (!target)
            return false;
        const double dx = target->position.x - origin.x, dy = target->position.y - origin.y;
        const double length = (std::max)(.001, std::hypot(dx, dy));
        projectiles.push_back({origin,
                               {dx / length * 440, dy / length * 440},
                               Range(),
                               companion ? 10.f + spiritRank * 6.f : Damage(),
                               companion});
        if (!companion)
        {
            SpriteAnimation aim;
            aim.Update((dx - dy) * .85, (dx + dy) * .43, 0, false);
            castFacing = aim.facing;
            castTime = .18f;
        }
        return true;
    }

    template <class Collision>
    void StreamEncounters(WorldPoint player, std::uint64_t seed, Collision blocked)
    {
        const World::Key region = {World::Index(player.x), World::Index(player.y)};
        if (regionLoaded && activeRegion == region)
            return;
        activeRegion = region;
        regionLoaded = true;
        for (const Enemy& enemy : enemies)
            if (enemy.boss == 0)
                camps[enemy.camp].health[enemy.slot] = enemy.health;
        enemies.erase(std::remove_if(enemies.begin(),
                                     enemies.end(),
                                     [&](const Enemy& enemy)
                                     {
                                         return enemy.boss == 0 &&
                                                (std::abs(enemy.camp.first - region.first) > 1 ||
                                                 std::abs(enemy.camp.second - region.second) > 1);
                                     }),
                      enemies.end());
        // A camp belongs to world coordinates, never to the player's position or a timer.
        for (int y = -1; y <= 1; ++y)
            for (int x = -1; x <= 1; ++x)
            {
                const World::Key key = {region.first + x, region.second + y};
                const double baseX = double(key.first) * World::ChunkSize;
                const double baseY = double(key.second) * World::ChunkSize;
                if (World::Village(baseX + 256, baseY + 256))
                    continue;
                CampRecord& record = camps[key];
                const auto hash = World::Hash(key.first, key.second, seed + 712);
                for (int slot = 0; slot < 2 + int(hash % 3); ++slot)
                {
                    if (record.defeated & (1u << slot))
                        continue;
                    bool active = false;
                    for (const Enemy& enemy : enemies)
                        if (enemy.boss == 0 && enemy.camp == key && enemy.slot == slot)
                            active = true;
                    if (active)
                        continue;
                    // Jittered, independent quadrants: no three-unit roadside formations.
                    // Keep margins across chunk borders and a clear exit to the road network.
                    WorldPoint home = {0, 0};
                    bool found = false;
                    for (int attempt = 0; attempt < 24 && !found; ++attempt)
                    {
                        const auto placement = World::Hash(
                            key.first, key.second, seed + 1901 + slot * 97 + attempt * 17);
                        const int quadrant = (slot + int((hash >> 8) % 4)) % 4;
                        home = {baseX + 60 + (quadrant % 2) * 256 + double(placement % 120),
                                baseY + 60 + (quadrant / 2) * 256 +
                                    double((placement >> 16) % 120)};
                        const WorldPoint exits[] = {{baseX + 24, home.y},
                                                    {baseX + 488, home.y},
                                                    {home.x, baseY + 24},
                                                    {home.x, baseY + 488}};
                        if (blocked(home))
                            continue;
                        for (const auto& exit : exits)
                            if (ClearLine(home, exit, blocked))
                                found = true;
                    }
                    if (!found)
                        continue;
                    Enemy enemy;
                    enemy.position = enemy.home = home;
                    enemy.trail.push_back(home);
                    enemy.camp = key;
                    enemy.slot = slot;
                    enemy.type = static_cast<int>((hash >> (slot * 8)) % 3);
                    enemy.maximum = 48.f + enemy.type * 10.f;
                    enemy.health = record.health[slot] < 0 ? enemy.maximum : record.health[slot];
                    enemy.weakened =
                        captureUnlocked && enemy.health <= enemy.maximum * captureRules.threshold;
                    enemies.push_back(enemy);
                }
            }
    }

    template <class Collision>
    void AttackEnemy(Enemy& enemy, WorldPoint player, float dt, Collision blocked)
    {
        enemy.stagger = (std::max)(0.f, enemy.stagger - dt);
        enemy.staggerGuard = (std::max)(0.f, enemy.staggerGuard - dt);
        if (enemy.stagger > 0)
            return;
        enemy.phaseTime -= dt;
        const double distance = Distance(enemy.position, player);
        if (enemy.phase == 3)
        {
            if (enemy.type == 2 && distance > 1)
            {
                const double dx = (player.x - enemy.position.x) / distance,
                             dy = (player.y - enemy.position.y) / distance;
                const double retreat = distance < 200 ? -1. : (distance > 310 ? .5 : 0.);
                const double side = enemy.slot % 2 ? .65 : -.65;
                const double speed = combat.chaseSpeed * dt * (enemy.slowed > 0 ? .35 : 1.);
                Move(enemy.position,
                     {(dx * retreat - dy * side) * speed, (dy * retreat + dx * side) * speed},
                     blocked,
                     &enemy.trail);
            }
            if (enemy.phaseTime <= 0)
                enemy.phase = 0;
            return;
        }
        if (enemy.phase == 2)
        {
            // The charge expired before this simulation step. Do not sweep its
            // hit area one extra frame while transitioning into recovery.
            if (enemy.phaseTime <= 0)
            {
                enemy.phase = 3;
                enemy.phaseTime = 1.05f;
                return;
            }

            const WorldPoint before = enemy.position;
            Move(enemy.position,
                 {enemy.aim.x * combat.chargeSpeed * dt * (enemy.slowed > 0 ? .35 : 1.),
                  enemy.aim.y * combat.chargeSpeed * dt * (enemy.slowed > 0 ? .35 : 1.)},
                 blocked,
                 &enemy.trail);
            // Swept contact prevents a fast charge passing through the player.
            const double vx = enemy.position.x - before.x, vy = enemy.position.y - before.y;
            const double t =
                (std::max)(0.,
                           (std::min)(1.,
                                      ((player.x - before.x) * vx + (player.y - before.y) * vy) /
                                          (vx * vx + vy * vy + .0001)));
            if (Distance(player, {before.x + vx * t, before.y + vy * t}) < 30)
                Hurt(combat.chargeDamage);
            if (enemy.phaseTime <= 0 || Distance(before, enemy.position) < 1)
            {
                enemy.phase = 3;
                enemy.phaseTime = 1.05f;
            }
            return;
        }
        if (enemy.phase == 1)
        {
            if (enemy.phaseTime > 0)
                return;
            if (enemy.type == 1)
            {
                enemy.phase = 2;
                enemy.phaseTime = .48f;
                return;
            }
            if (enemy.type == 0)
            {
                const double dx = player.x - enemy.position.x, dy = player.y - enemy.position.y;
                if (distance < 85 && (dx * enemy.aim.x + dy * enemy.aim.y) >= distance * .35 &&
                    ClearLine(enemy.position, player, blocked))
                    Hurt(combat.meleeDamage);
            }
            else
            {
                for (int i = -1; i <= 1; ++i)
                {
                    const double angle = i * .18, c = std::cos(angle), s = std::sin(angle);
                    projectiles.push_back({enemy.position,
                                           {(enemy.aim.x * c - enemy.aim.y * s) * 440,
                                            (enemy.aim.x * s + enemy.aim.y * c) * 440},
                                           520,
                                           combat.rangedDamage,
                                           false,
                                           true});
                }
            }
            enemy.phase = 3;
            enemy.phaseTime = enemy.type == 0 ? .65f : 1.35f;
            return;
        }
        const double range = enemy.type == 0 ? 72 : (enemy.type == 1 ? 290 : 380);
        if (distance < range && ClearLine(enemy.position, player, blocked))
        {
            const double length = (std::max)(.001, distance);
            enemy.aim = {(player.x - enemy.position.x) / length,
                         (player.y - enemy.position.y) / length};
            enemy.phase = 1;
            enemy.phaseTime = enemy.type == 0
                                  ? combat.meleeWindup
                                  : (enemy.type == 1 ? combat.chargeWindup : combat.rangedWindup);
            return;
        }
        if (distance > 1)
        {
            const double step = (std::min)(
                distance, double(combat.chaseSpeed * dt * (enemy.slowed > 0 ? .35f : 1.f)));
            Move(enemy.position,
                 {(player.x - enemy.position.x) / distance * step,
                  (player.y - enemy.position.y) / distance * step},
                 blocked,
                 &enemy.trail);
        }
    }

    template <class Collision>
    void Update(float dt,
                WorldPoint player,
                WorldPoint companion,
                bool captured,
                bool controlling,
                bool fireHeld,
                std::uint64_t worldSeed,
                Collision blocked)
    {
        if (Dead())
            return;
        invulnerability = (std::max)(0.f, invulnerability - dt);
        hurtTime = (std::max)(0.f, hurtTime - dt);
        castTime = (std::max)(0.f, castTime - dt);
        cooldown = (std::max)(0.f, cooldown - dt);
        companionCooldown = (std::max)(0.f, companionCooldown - dt);
        skillCooldown = (std::max)(0.f, skillCooldown - dt);
        StreamEncounters(player, worldSeed, blocked);
        for (Enemy& enemy : enemies)
        {
            if (enemy.health <= 0 || enemy.weakened)
                continue;
            enemy.slowed = (std::max)(0.f, enemy.slowed - dt);
            enemy.flash = (std::max)(0.f, enemy.flash - dt);
            const WorldPoint previous = enemy.position;
            const double distance = Distance(player, enemy.position);
            if (!enemy.returning && !enemy.alerted && distance < combat.aggro &&
                ClearLine(enemy.position, player, blocked))
                enemy.alerted = true;
            if (enemy.alerted &&
                (Distance(player, enemy.home) > 650 || Distance(enemy.position, enemy.home) > 600 ||
                 Distance(player, {0, 0}) <= 210 || enemy.trail.size() >= 2048))
            {
                enemy.alerted = false;
                enemy.returning = true;
                enemy.phase = 0;
            }
            if (enemy.returning)
            {
                while (enemy.returning && enemy.trail.size() > 1 &&
                       Distance(enemy.position, enemy.trail.back()) < .01)
                    enemy.trail.pop_back();
                const WorldPoint target = enemy.returning ? enemy.trail.back() : player;
                const double travel = Distance(enemy.position, target);
                if (travel > (enemy.returning ? .001 : 24))
                {
                    const double step =
                        (std::min)(travel,
                                   double(enemy.returning ? 90 : 48 + enemy.type * 5) * dt *
                                       (enemy.slowed > 0 ? .35 : 1.));
                    const WorldPoint delta = {(target.x - enemy.position.x) / travel * step,
                                              (target.y - enemy.position.y) / travel * step};
                    if (enemy.returning)
                    {
                        // The trail is composed of collision-checked segments in a static world.
                        enemy.position.x += delta.x;
                        enemy.position.y += delta.y;
                    }
                    else
                        Move(enemy.position, delta, blocked, &enemy.trail);
                }
                if (enemy.returning && Distance(enemy.position, enemy.home) <= 4)
                {
                    enemy.position = enemy.home;
                    enemy.returning = false;
                    enemy.trail.assign(1, enemy.home);
                }
            }

            if (enemy.alerted)
                AttackEnemy(enemy, player, dt, blocked);
            const double ax = enemy.position.x - previous.x, ay = enemy.position.y - previous.y;
            enemy.animation.Update((ax - ay) * .85, (ax + ay) * .43, dt, false);
        }
        if (Dead())
        {
            projectiles.clear();
            return;
        }
        if (dodgeTime <= 0 && !controlling && cooldown <= 0 && (automatic || fireHeld) &&
            Shoot(player, false, blocked))
            cooldown = Interval();
        bool summonBlocked = false;
        for (const Enemy& enemy : enemies)
            if (enemy.boss == 3 && enemy.alerted && enemy.health > 0)
                summonBlocked = true;
        if (!summonBlocked && captured && companionCooldown <= 0 && Shoot(companion, true, blocked))
            companionCooldown = (std::max)(.45f, 1.5f - spiritRank * .08f);
        for (Projectile& shot : projectiles)
        {
            float remainingTime = dt;
            while (remainingTime > 0 && shot.remaining > 0)
            {
                const float step =
                    (std::min)((std::min)(remainingTime, .008f), shot.remaining / 440.f);
                if (step <= 0)
                    break;
                remainingTime -= step;
                shot.remaining = (std::max)(0.f, shot.remaining - 440.f * step);
                shot.position.x += shot.velocity.x * step;
                shot.position.y += shot.velocity.y * step;
                if (blocked(shot.position))
                {
                    shot.remaining = 0;
                    break;
                }
                if (shot.hostile)
                {
                    if (Distance(shot.position, player) < 18)
                    {
                        if (Distance(player, {0, 0}) > 210)
                            Hurt(shot.damage);
                        shot.remaining = 0;
                    }
                    continue;
                }
                for (Enemy& enemy : enemies)
                {
                    if (enemy.weakened || enemy.returning || enemy.health <= 0 ||
                        Distance(shot.position, enemy.position) > 22)
                        continue;
                    enemy.health -= shot.damage;
                    enemy.alerted = true;
                    enemy.flash = .15f;
                    if (enemy.boss == 0 && enemy.phase == 0 && enemy.staggerGuard <= 0)
                    {
                        enemy.stagger = .16f;
                        enemy.staggerGuard = 1.2f;
                    }
                    const bool canCapture = captureUnlocked && (enemy.boss == 0 || enemy.boss == 4);
                    const float threshold =
                        enemy.maximum *
                        (enemy.boss == 4 ? captureRules.bossThreshold : captureRules.threshold);
                    if (canCapture && enemy.health <= threshold)
                    {
                        enemy.health = threshold;
                        enemy.weakened = true;
                        enemy.alerted = false;
                        notice = "주령이 약화되었습니다. 가까이에서 E로 포획하거나 X로 정화하세요.";
                    }
                    shot.remaining = 0;
                    if (enemy.health <= 0)
                        Resolve(enemy, true);
                    break;
                }
            }
        }
        projectiles.erase(std::remove_if(projectiles.begin(),
                                         projectiles.end(),
                                         [](const Projectile& p)
                                         {
                                             return p.remaining <= 0;
                                         }),
                          projectiles.end());
        enemies.erase(std::remove_if(enemies.begin(),
                                     enemies.end(),
                                     [&](const Enemy& e)
                                     {
                                         return e.health <= 0;
                                     }),
                      enemies.end());
        if (Dead())
        {
            projectiles.clear();
            return;
        }
        const WorldPoint collector = controlling ? companion : player;
        for (Loot& item : loot)
        {
            item.age += dt;
            const double distance = Distance(item.position, collector);
            if (distance < MagnetRange())
                item.attracted = true;
            // Pickups are ethereal: attraction passes through scenery to avoid unreachable rewards.
            if (item.attracted && distance > 0)
            {
                const double t = (std::min)(1.0, (180 + item.age * 30) * dt / distance);
                item.position.x += (collector.x - item.position.x) * t;
                item.position.y += (collector.y - item.position.y) * t;
            }
            if (Distance(item.position, collector) < 18)
            {
                Collect(item.kind);
                item.age = 1000;
            }
        }
        loot.erase(std::remove_if(loot.begin(),
                                  loot.end(),
                                  [](const Loot& item)
                                  {
                                      return item.age >= 90;
                                  }),
                   loot.end());
    }
};
