#pragma once
#include "World.h"
#include "SpriteSheet.h"
#include "CaptureRules.h"
#include "SpiritCatalog.h"
#include "StoryEncounter.h"
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
        WorldPoint hitDirection = {0, 0};
        float hitStrength = 0;
        SpriteAnimation animation;
        WorldPoint home = {0, 0};
        World::Key camp = {0, 0};
        int slot = 0;
        bool alerted = false, returning = false;
        std::vector<WorldPoint> trail;
        bool weakened = false;
        float remnantTime = -2;
        int remnantRarity = 1;
        int boss = 0;
        float slowed = 0;
        // 0: approach, 1: committed windup, 2: charge, 3: recovery.
        int phase = 0;
        float phaseTime = 0, stagger = 0, staggerGuard = 0;
        WorldPoint aim = {0, 0};
        WorldPoint ritualTarget = {0, 0};
        int npcTarget = -1;
        int residentTargetIndex = -1;
        float targetDecisionTime = 0;
        float simulationTime = 0;
        float saintSightTime = 0;
        bool saintSightClear = false;
        WorldPoint saintSightTarget = {0, 0};
        int ritualCount = 0;
    };

    struct Projectile
    {
        WorldPoint position, velocity;
        float remaining, damage;
        bool companion;
        bool hostile = false;
        unsigned long long actorId = 0;
        bool residentBattle = false;
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

    struct ResidentCombatant
    {
        int id;
        WorldPoint position;
        float health, invulnerability;
    };

    std::vector<ResidentCombatant> residentCombatants;
    std::vector<WorldPoint> encounterObservers;
    std::set<World::Key> encounterRegions;
    std::vector<World::Key> pendingEncounterRegions;
    int damageResident = -1;

    struct HitFeedback
    {
        WorldPoint position, direction;
        float age = 0;
        bool heavy = false;
    };

    std::vector<HitFeedback> hitFeedback;
    float hitStop = 0, hitStopGuard = 0;

    void UpdateHitFeedback(float dt)
    {
        hitStopGuard = (std::max)(0.f, hitStopGuard - dt);
        for (auto& hit : hitFeedback)
            hit.age += dt;
        hitFeedback.erase(std::remove_if(hitFeedback.begin(),
                                         hitFeedback.end(),
                                         [](const HitFeedback& hit)
                                         {
                                             return hit.age >= .24f;
                                         }),
                          hitFeedback.end());
    }

    std::vector<Projectile> projectiles;
    std::vector<Loot> loot;
    int level = 1, experience = 0, kills = 0, weapon = 0, stones = 0, spiritRank = 0;
    int failedUpgrades = 0;
    int gold = 0, potions = 0, weaponStones = 0, spoils = 0;
    float health = 100, invulnerability = 0, cooldown = 0, companionCooldown = 0;
    float castTime = 0;
    int damageSpirit = -1;
    float spiritInvulnerability = 0;
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
        if (damageResident >= 0 && damageResident < int(residentCombatants.size()))
        {
            auto& person = residentCombatants[damageResident];
            if (person.invulnerability <= 0 && person.health > 0)
            {
                person.health = (std::max)(0.f, person.health - damage);
                person.invulnerability = .55f;
            }
            return;
        }
        if (damageSpirit >= 0 && damageSpirit < static_cast<int>(collection.size()))
        {
            if (spiritInvulnerability <= 0)
            {
                auto& spirit = collection[damageSpirit];
                spirit.health = (std::max)(0.f, spirit.health - damage);
                spiritInvulnerability = .55f;
            }
            return;
        }
        if (Dead() || invulnerability > 0 || dodgeTime > 0)
            return;
        health = (std::max)(storyTrial ? 1.f : 0.f, health - damage);
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
    bool storyTrial = false;

    struct SpiritInstance
    {
        int species = 0, rarity = 1;
        bool memory = false;
        float health = 100;
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
        if (enemy.weakened && enemy.remnantTime >= -1)
        {
            if (enemy.boss > 0)
            {
                resolvedBosses.insert(enemy.boss);
            }
            return;
        }
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
        gold += 8;
        ++spoils;
        const size_t firstDrop = loot.size();
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
        for (size_t i = firstDrop; i < loot.size(); ++i)
        {
            const double itemIndex = static_cast<double>(i - firstDrop);
            const double angle = itemIndex * 2.39996;
            const double radius = 18.0 + itemIndex * 5.0;
            loot[i].position.x += std::cos(angle) * radius;
            loot[i].position.y += std::sin(angle) * radius;
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
            if (enemy.boss == 5)
            {
                notice = "라엔: 첫 봉인이야. 낡은 봉인권을 선택하고 F로 포획해.";
                return;
            }
            Resolve(enemy, true);
            notice = "잔영을 흩어 보냈습니다. 처치 보상은 이미 지급되었습니다.";
            return;
        }
        const bool first = enemy.boss == 5;
        if (first)
        {
            // The mandatory lesson always consumes an old ticket and cannot be stranded.
            selectedTicket = 0;
            tickets[0] = (std::max)(1, tickets[0]);
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
        const float chance =
            first ? 1.f : (enemy.boss == 4 ? captureRules.bossChance : captureRules.normalChance);
        if (Roll() <= chance)
        {
            int rarity = first ? 1 : enemy.remnantRarity;
            const int species = enemy.boss == 4 ? SpiritCatalog::Saint : enemy.type;
            if (species == SpiritCatalog::Saint)
            {
                rarity = (std::max)(rarity, 3);
            }
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

    void DamageEnemy(Enemy& enemy,
                     float damage,
                     WorldPoint source = {0, 0},
                     bool heavy = false,
                     bool feedback = true,
                     bool residentOwned = false)
    {
        if (enemy.weakened || enemy.health <= 0)
        {
            return;
        }
        if (feedback && damage > 0 && enemy.flash <= .05f)
        {
            const double dx = enemy.position.x - source.x, dy = enemy.position.y - source.y;
            const double length = (std::max)(.001, std::hypot(dx, dy));
            enemy.hitDirection = {dx / length, dy / length};
            enemy.hitStrength = heavy ? 9.f : 4.f;
            if (hitFeedback.size() >= 48)
                hitFeedback.erase(hitFeedback.begin());
            hitFeedback.push_back({enemy.position, enemy.hitDirection, 0, heavy});
            if (heavy && hitStopGuard <= 0)
            {
                hitStop = .045f;
                hitStopGuard = .18f;
            }
        }
        enemy.health -= enemy.boss == 12 ? .05f : damage;
        if (enemy.boss == 12)
        {
            enemy.health = (std::max)(1.f, enemy.health);
        }
        enemy.alerted = true;
        if (feedback)
            enemy.flash = .15f;
        if (enemy.boss == 0 && enemy.phase == 0 && enemy.staggerGuard <= 0)
        {
            enemy.stagger = .16f;
            enemy.staggerGuard = 1.2f;
        }
        const bool canCapture =
            !residentOwned && captureUnlocked && StoryEncounter::Get(enemy.boss).capturable;
        if (enemy.health <= 0)
        {
            Resolve(enemy, !residentOwned);
            if (canCapture)
            {
                const float roll = Roll();
                enemy.remnantRarity = enemy.boss == 4                      ? 4
                                      : enemy.boss == 10                   ? 3
                                      : enemy.type == SpiritCatalog::Saint ? 3
                                      : roll < captureRules.rareChance     ? 3
                                      : roll < captureRules.rareChance + captureRules.uncommonChance
                                          ? 2
                                          : 1;
                enemy.weakened = true;
                enemy.health = 1;
                enemy.alerted = enemy.returning = false;
                enemy.phase = 0;
                enemy.animation.moving = false;
                enemy.remnantTime =
                    enemy.boss == 5 ? -1.f
                                    : (enemy.boss > 0 || enemy.remnantRarity >= 3 ? 60.f : 30.f);
                if (enemy.boss == 5)
                {
                    resolvedBosses.erase(5);
                }
                notice = "주령을 처치했습니다. 남은 잔영에 가까이 가서 F로 포획할 수 있습니다.";
            }
        }
    }

    static bool IsCombatBoss(const Enemy& enemy)
    {
        return (enemy.boss >= 1 && enemy.boss <= 4) || enemy.boss == 12;
    }

    template <class Collision>
    bool ClearSpawn(WorldPoint home, WorldPoint observer, Collision blocked) const
    {
        if (blocked(home) || !ClearLine(observer, home, blocked))
        {
            return false;
        }
        // A clear center alone can still bury the sprite or immediately block its attacks.
        for (int i = 0; i < 16; ++i)
        {
            const double angle = i * 6.283185307 / 16;
            if (!ClearLine(
                    home, {home.x + std::cos(angle) * 64, home.y + std::sin(angle) * 64}, blocked))
            {
                return false;
            }
        }
        for (const auto& enemy : enemies)
        {
            if (enemy.health > 0 && Distance(home, enemy.position) < 64)
            {
                return false;
            }
        }
        return true;
    }

    template <class Collision>
    void EnsureBoss(int id, WorldPoint home, WorldPoint observer, Collision blocked)
    {
        if (resolvedBosses.count(id))
            return;
        for (const Enemy& enemy : enemies)
            if (enemy.boss == id)
                return;
        const WorldPoint intended = home;
        bool found = ClearSpawn(home, observer, blocked);
        for (int ring = 1; ring <= 10 && !found; ++ring)
        {
            for (int direction = 0; direction < 16 && !found; ++direction)
            {
                const double angle = direction * 6.283185307 / 16;
                home = {intended.x + std::cos(angle) * ring * 16,
                        intended.y + std::sin(angle) * ring * 16};
                found = ClearSpawn(home, observer, blocked);
            }
        }
        if (!found)
        {
            return;
        }
        Enemy enemy;
        enemy.position = enemy.home = home;
        enemy.trail.push_back(home);
        enemy.type = StoryEncounter::Get(id).species;
        enemy.boss = id;
        enemy.maximum = id == 4 ? 420.f : (id == 1 ? 220.f : 160.f);
        if (id >= 5)
        {
            enemy.maximum = id == 12 ? 200.f : 55.f;
        }
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
            const WorldPoint diagonal = {position.x + delta.x / steps,
                                         position.y + delta.y / steps};
            if (ClearLine(position, diagonal, blocked))
            {
                position = diagonal;
                if (trail)
                {
                    RecordTrail(*trail, position, blocked);
                }
                continue;
            }
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
            notice = "주령석은 보관됩니다. 먼저 F로 주령을 포획하세요.";
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

    void UsePotion()
    {
        if (Dead() || health >= MaximumHealth() || potions <= 0)
        {
            notice =
                potions <= 0 ? "회복물약이 없습니다." : "지금은 회복물약을 사용할 수 없습니다.";
            return;
        }
        --potions;
        health = (std::min)(MaximumHealth(), health + MaximumHealth() * .3f);
        notice = "회복물약 사용 · 본체 체력 30% 회복";
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
            ++weaponStones;
            notice = "강화석 획득 · 대장장이에게서 사용·판매할 수 있습니다.";
            break;
        case LootKind::Healing:
            ++potions;
            notice = "회복물약 획득 · H로 필요할 때 사용하세요.";
            break;
        case LootKind::Stone:
            ++stones;
            notice = "주령석 획득! 봉인사에게서 동행 주령을 강화하세요. 성공 확률 70%.";
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
        std::set<World::Key> needed;
        std::vector<WorldPoint> observers = encounterObservers;
        observers.push_back(player);
        for (const auto& observer : observers)
            for (int y = -1; y <= 1; ++y)
                for (int x = -1; x <= 1; ++x)
                    needed.insert({World::Index(observer.x) + x, World::Index(observer.y) + y});
        const bool changed = !regionLoaded || encounterRegions != needed;
        if (!changed && pendingEncounterRegions.empty())
            return;
        if (changed)
        {
            pendingEncounterRegions.assign(needed.rbegin(), needed.rend());
        }
        encounterRegions = needed;
        activeRegion = region;
        regionLoaded = true;
        for (const Enemy& enemy : enemies)
            if (enemy.boss == 0 && !enemy.weakened)
                camps[enemy.camp].health[enemy.slot] = enemy.health;
        enemies.erase(std::remove_if(enemies.begin(),
                                     enemies.end(),
                                     [&](const Enemy& enemy)
                                     {
                                         return enemy.boss == 0 && !enemy.weakened &&
                                                needed.count(enemy.camp) == 0;
                                     }),
                      enemies.end());
        // Camps stay attached to world coordinates; only clear initial sightlines are accepted.
        // At most two camps per frame, including the initial remote population load.
        for (int budget = 0; budget < 2 && !pendingEncounterRegions.empty(); ++budget)
        {
            const auto key = pendingEncounterRegions.back();
            pendingEncounterRegions.pop_back();
            const double baseX = double(key.first) * World::ChunkSize;
            const double baseY = double(key.second) * World::ChunkSize;
            if (World::Village(baseX + 256, baseY + 256) ||
                World::Settlement(baseX + 256, baseY + 256) ||
                (baseX >= 512 && baseX <= 3584 && baseY >= -512 && baseY <= 0))
                continue;
            CampRecord& record = camps[key];
            const auto hash = World::Hash(key.first, key.second, seed + 712);
            for (int slot = 0; slot < 2 + int(hash % 3); ++slot)
            {
                if (enemies.size() >= 480)
                    return;
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
                    const auto placement =
                        World::Hash(key.first, key.second, seed + 1901 + slot * 97 + attempt * 17);
                    const int quadrant = (slot + int((hash >> 8) % 4)) % 4;
                    home = {baseX + 60 + (quadrant % 2) * 256 + double(placement % 120),
                            baseY + 60 + (quadrant / 2) * 256 + double((placement >> 16) % 120)};
                    if (World::Settlement(home.x, home.y))
                    {
                        continue;
                    }
                    const WorldPoint exits[] = {{baseX + 24, home.y},
                                                {baseX + 488, home.y},
                                                {home.x, baseY + 24},
                                                {home.x, baseY + 488}};
                    WorldPoint nearestObserver = player;
                    for (const auto& observer : observers)
                        if (Distance(home, observer) < Distance(home, nearestObserver))
                            nearestObserver = observer;
                    if (!ClearSpawn(home, nearestObserver, blocked))
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
                // One rare saint in selected outlying camps; village/tutorial stays unchanged.
                if (slot == 0 && hash % 7 == 0 && std::hypot(home.x, home.y) > 1200)
                {
                    enemy.type = SpiritCatalog::Saint;
                }
                enemy.maximum = SpiritCatalog::Get(enemy.type).health;
                enemy.health = record.health[slot] < 0 ? enemy.maximum : record.health[slot];
                enemy.weakened = false;
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
        if (enemy.type == SpiritCatalog::Saint)
        {
            if (enemy.phase == 3)
            {
                if (enemy.phaseTime <= 0)
                {
                    enemy.phase = 0;
                    enemy.saintSightTime = 0;
                }
                return;
            }
            if (enemy.phase == 1)
            {
                if (enemy.phaseTime > 0)
                {
                    return;
                }
                // A locked ground seal can be dodged; every third toll is a radial volley.
                if (enemy.ritualCount % 3 != 2)
                {
                    if (Distance(player, enemy.ritualTarget) < 78 &&
                        ClearLine(enemy.position, player, blocked))
                    {
                        Hurt(combat.rangedDamage * 1.35f);
                    }
                }
                else
                {
                    for (int i = 0; i < 8; ++i)
                    {
                        const double angle = i * 6.2831853 / 8;
                        projectiles.push_back({enemy.position,
                                               {std::cos(angle) * 220, std::sin(angle) * 220},
                                               420,
                                               combat.rangedDamage,
                                               false,
                                               true});
                    }
                }
                ++enemy.ritualCount;
                enemy.phase = 3;
                enemy.phaseTime = 1.25f;
                return;
            }
            enemy.saintSightTime -= dt;
            if (distance < 360 &&
                (enemy.saintSightTime <= 0 || Distance(player, enemy.saintSightTarget) > 48))
            {
                enemy.saintSightClear = ClearLine(enemy.position, player, blocked);
                enemy.saintSightTarget = player;
                enemy.saintSightTime = .2f;
            }
            if (distance < 360 && enemy.saintSightClear)
            {
                enemy.ritualTarget = player;
                enemy.phase = 1;
                enemy.phaseTime = 1.1f;
                return;
            }
            if (distance > 280)
            {
                const double step = (std::min)(
                    distance - 280,
                    double(combat.chaseSpeed * .65f * dt * (enemy.slowed > 0 ? .35f : 1.f)));
                Move(enemy.position,
                     {(player.x - enemy.position.x) / distance * step,
                      (player.y - enemy.position.y) / distance * step},
                     blocked,
                     &enemy.trail);
            }
            return;
        }
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
        if (distance > 62)
        {
            const double step = (std::min)(
                distance - 62, double(combat.chaseSpeed * dt * (enemy.slowed > 0 ? .35f : 1.f)));
            Move(enemy.position,
                 {(player.x - enemy.position.x) / distance * step,
                  (player.y - enemy.position.y) / distance * step},
                 blocked,
                 &enemy.trail);
        }
    }

    template <class Collision>
    int ResidentTarget(Enemy& enemy, WorldPoint player, float dt, Collision blocked)
    {
        int previous = enemy.residentTargetIndex;
        if (enemy.npcTarget >= 0 && (previous < 0 || previous >= int(residentCombatants.size()) ||
                                     residentCombatants[previous].id != enemy.npcTarget))
        {
            previous = -1;
            for (size_t i = 0; i < residentCombatants.size(); ++i)
            {
                if (residentCombatants[i].id == enemy.npcTarget)
                {
                    previous = int(i);
                    break;
                }
            }
            enemy.residentTargetIndex = previous;
        }
        const bool valid = previous >= 0 && previous < int(residentCombatants.size()) &&
                           residentCombatants[previous].id == enemy.npcTarget &&
                           residentCombatants[previous].health > 0 &&
                           Distance(residentCombatants[previous].position, enemy.position) < 600;
        // A committed attack keeps its victim. Cancel it if that resident has fallen.
        if (enemy.boss == 0 && (enemy.phase == 1 || enemy.phase == 2))
        {
            if (valid)
                return previous;
            if (enemy.npcTarget >= 0)
            {
                enemy.phase = 3;
                enemy.phaseTime = .45f;
            }
            enemy.npcTarget = enemy.residentTargetIndex = -1;
            return -1;
        }
        if (enemy.boss != 0 || Distance(player, enemy.position) <= 380)
        {
            enemy.npcTarget = enemy.residentTargetIndex = -1;
            enemy.targetDecisionTime = 0;
            return -1;
        }
        enemy.targetDecisionTime -= dt;
        if (enemy.targetDecisionTime > 0)
        {
            if (valid)
                return previous;
            if (enemy.npcTarget < 0)
                return -1;
        }
        enemy.targetDecisionTime = .35f + enemy.slot * .03f;
        int chosen = -1;
        double nearest = combat.aggro;
        if (valid && Distance(residentCombatants[previous].position, enemy.position) < 340 &&
            ClearLine(enemy.position, residentCombatants[previous].position, blocked))
        {
            chosen = previous;
            // A new resident must be substantially closer before changing victims.
            nearest = Distance(residentCombatants[previous].position, enemy.position) * .75;
        }
        for (size_t i = 0; i < residentCombatants.size(); ++i)
        {
            if (int(i) == chosen || residentCombatants[i].health <= 0)
                continue;
            const double distance = Distance(residentCombatants[i].position, enemy.position);
            if (distance < nearest &&
                ClearLine(enemy.position, residentCombatants[i].position, blocked))
            {
                nearest = distance;
                chosen = int(i);
            }
        }
        enemy.residentTargetIndex = chosen;
        enemy.npcTarget = chosen >= 0 ? residentCombatants[chosen].id : -1;
        return chosen;
    }

    template <class Collision, class TimeScale>
    void Update(float dt,
                WorldPoint player,
                WorldPoint companion,
                bool captured,
                bool controlling,
                bool fireHeld,
                std::uint64_t worldSeed,
                Collision blocked,
                TimeScale timeScale,
                double fullRateRadius = 1400)
    {
        if (Dead())
            return;
        invulnerability = (std::max)(0.f, invulnerability - dt);
        hurtTime = (std::max)(0.f, hurtTime - dt);
        castTime = (std::max)(0.f, castTime - dt);
        cooldown = (std::max)(0.f, cooldown - dt);
        companionCooldown = (std::max)(0.f, companionCooldown - dt);
        skillCooldown = (std::max)(0.f, skillCooldown - dt);
        spiritInvulnerability = (std::max)(0.f, spiritInvulnerability - dt);
        for (int i = 0; i < static_cast<int>(collection.size()); ++i)
        {
            if (!controlling || i != damageSpirit)
            {
                collection[i].health = (std::min)(100.f, collection[i].health + 5.f * dt);
            }
        }
        const WorldPoint combatTarget = controlling ? companion : player;
        StreamEncounters(combatTarget, worldSeed, blocked);
        for (Enemy& enemy : enemies)
        {
            if (enemy.weakened)
            {
                if (enemy.remnantTime == -2)
                {
                    // Upgrade legacy weakened actors once, awarding their pending kill.
                    enemy.weakened = false;
                    DamageEnemy(enemy, enemy.maximum + 1, enemy.position, false, false);
                }
                if (enemy.remnantTime > 0)
                {
                    enemy.remnantTime = (std::max)(0.f, enemy.remnantTime - dt);
                    if (enemy.remnantTime == 0)
                    {
                        enemy.health = 0;
                    }
                }
                continue;
            }
            if (enemy.health <= 0)
            {
                continue;
            }
            enemy.slowed = (std::max)(0.f, enemy.slowed - dt);
            enemy.flash = (std::max)(0.f, enemy.flash - dt);
            const float scaledDt = dt * timeScale(enemy.position, IsCombatBoss(enemy) ? 1 : 0);
            if (scaledDt <= 0)
            {
                // Frozen time must never be replayed after a domain releases its targets.
                enemy.simulationTime = 0;
                enemy.animation.moving = false;
                continue;
            }
            enemy.simulationTime += scaledDt;
            const bool distant =
                enemy.boss == 0 && Distance(enemy.position, combatTarget) > fullRateRadius;
            const float interval = enemy.alerted || enemy.returning ? .1f : .25f;
            if (distant && enemy.simulationTime < interval)
                continue;
            const float enemyDt = enemy.simulationTime;
            enemy.simulationTime = 0;
            const WorldPoint previous = enemy.position;
            WorldPoint enemyTarget = combatTarget;
            damageResident = ResidentTarget(enemy, combatTarget, enemyDt, blocked);
            if (damageResident >= 0)
            {
                enemyTarget = residentCombatants[damageResident].position;
            }
            const double distance = Distance(enemyTarget, enemy.position);
            if (!enemy.returning && !enemy.alerted && distance < combat.aggro &&
                ClearLine(enemy.position, enemyTarget, blocked))
                enemy.alerted = true;
            if (enemy.alerted &&
                (Distance(enemyTarget, enemy.home) > 650 ||
                 Distance(enemy.position, enemy.home) > 600 ||
                 Distance(enemyTarget, {0, 0}) <= 210 || enemy.trail.size() >= 2048))
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
                const WorldPoint target = enemy.returning ? enemy.trail.back() : combatTarget;
                const double travel = Distance(enemy.position, target);
                if (travel > (enemy.returning ? .001 : 24))
                {
                    const double step =
                        (std::min)(travel,
                                   double(enemy.returning ? 90 : 48 + enemy.type * 5) * enemyDt *
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

            const size_t previousProjectiles = projectiles.size();
            if (enemy.alerted)
                AttackEnemy(enemy, enemyTarget, enemyDt, blocked);
            if (damageResident >= 0)
                for (size_t i = previousProjectiles; i < projectiles.size(); ++i)
                    projectiles[i].residentBattle = true;
            const double ax = enemy.position.x - previous.x, ay = enemy.position.y - previous.y;
            const float authoredStride = StoryEncounter::Get(enemy.boss).walkFrameDistance;
            enemy.animation.Update((ax - ay) * .85,
                                   (ax + ay) * .43,
                                   enemyDt,
                                   false,
                                   authoredStride > 0 ? authoredStride
                                   : enemy.type == 0  ? 7.f
                                   : enemy.type == 1  ? 11.f
                                                      : 9.f);
        }
        damageResident = -1;
        if (Dead())
        {
            projectiles.clear();
            return;
        }
        if (dodgeTime <= 0 && !controlling && cooldown <= 0 && fireHeld &&
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
                const float scale = timeScale(shot.position, 2);
                const float step =
                    (std::min)((std::min)(remainingTime, .008f), shot.remaining / (440.f * scale));
                if (step <= 0)
                    break;
                remainingTime -= step;
                const float flightTime = step * scale;
                shot.remaining = (std::max)(0.f, shot.remaining - 440.f * flightTime);
                shot.position.x += shot.velocity.x * flightTime;
                shot.position.y += shot.velocity.y * flightTime;
                if (blocked(shot.position))
                {
                    shot.remaining = 0;
                    break;
                }
                if (shot.hostile)
                {
                    for (size_t i = 0; i < residentCombatants.size(); ++i)
                    {
                        if (residentCombatants[i].health > 0 &&
                            Distance(shot.position, residentCombatants[i].position) < 18)
                        {
                            damageResident = int(i);
                            Hurt(shot.damage);
                            damageResident = -1;
                            shot.remaining = 0;
                            break;
                        }
                    }
                    if (shot.remaining <= 0)
                        break;
                    if (Distance(shot.position, combatTarget) < 18)
                    {
                        if (Distance(combatTarget, {0, 0}) > 210)
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
                    DamageEnemy(
                        enemy,
                        shot.damage,
                        {shot.position.x - shot.velocity.x, shot.position.y - shot.velocity.y});
                    shot.remaining = 0;
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
            // Every controlled character attracts nearby loot, even during spirit suppression.
            item.attracted = distance < MagnetRange();
            // Pickups are ethereal: attraction passes through scenery to avoid unreachable rewards.
            if (item.attracted && distance > 0)
            {
                const double t = (std::min)(1.0, (180 + item.age * 30) * dt / distance);
                item.position.x += (collector.x - item.position.x) * t;
                item.position.y += (collector.y - item.position.y) * t;
            }
            if (item.attracted && Distance(item.position, collector) < 18)
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
