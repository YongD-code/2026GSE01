// Included inside Population; news travels with actual residents, never by global knowledge.
static float WageScale(int village)
{
    static const float scale[] = {1.f, 1.35f, 1.2f, .9f, 1.15f, 1.05f, 1.25f};
    return scale[village];
}

static int InitialVillage(int id)
{
    const int first = VillageLayout::PopulationPerVillage - VillageLayout::StoryResidents;
    return id < first ? 0 : 1 + (id - first) / VillageLayout::PopulationPerVillage;
}

int HouseOwner(int houseId) const
{
    if (houseId >= 0 && houseId < VillageLayout::StoryResidents)
    {
        return -2; // Reserved for the story identities, including currently absent people.
    }
    for (const auto& person : people)
    {
        if (person.houseId == houseId || person.reservedHouse == houseId)
        {
            return person.id;
        }
    }
    return -1;
}

int FreeHouse(int village) const
{
    for (const auto& house : VillageLayout::Houses())
    {
        if (house.village == village && HouseOwner(house.id) == -1)
        {
            return house.id;
        }
    }
    return -1;
}

template <class Collision> Person NewPerson(int id, Collision blocked)
{
    Person person;
    person.id = id;
    person.district = InitialVillage(id);
    person.role = id % 10 >= 7 ? Hunter : id % 3;
    person.appearance = NpcAppearance::Resident(person.id, person.role);
    const auto hash = World::Hash(id, person.district, seed);
    person.ambition = float(hash % 101) / 100;
    person.caution = float((hash >> 10) % 101) / 100;
    person.sociability = float((hash >> 20) % 101) / 100;
    person.migrationWait = 90.f + float(hash % 121);
    person.houseId = FreeHouse(person.district);
    const auto* house = VillageLayout::Home(person.houseId);
    person.home = house ? house->entrance : PostOffice(person.district);
    person.workplace = OpenPoint(CampAnchor(person.district, id, true), blocked);
    person.position = person.destination = person.home;
    person.action = Resting;
    person.timer = 1.f + id % 13;
    person.decision = (id % 30) * .037f;
    return person;
}

template <class Collision> void Initialize(std::uint64_t worldSeed, Collision blocked)
{
    seed = worldSeed;
    planningCursor = 0;
    society = Society();
    people.clear();
    people.reserve(VillageLayout::ResidentCount);
    for (int id = 0; id < VillageLayout::ResidentCount; ++id)
    {
        people.push_back(NewPerson(id, blocked));
    }
}

template <class Collision> void RepairPlacement(Collision blocked)
{
    const bool legacy = people.size() != VillageLayout::ResidentCount ||
                        (!people.empty() && people.front().houseId < 0);
    if (legacy)
    {
        auto previous = std::move(people);
        Initialize(seed, blocked);
        for (size_t i = 0; i < previous.size() && i < people.size(); ++i)
        {
            auto& person = people[i];
            const auto& old = previous[i];
            person.role = old.role;
            person.gold = old.gold;
            person.earned = old.earned;
            person.health = old.health;
            person.stamina = old.stamina;
            person.goods = old.goods;
            person.kills = old.kills;
            person.cycle = old.cycle;
        }
    }
    for (auto& person : people)
    {
        person.appearance = NpcAppearance::Resident(person.id, person.role);
        const auto* home = VillageLayout::Home(person.houseId);
        if (home)
        {
            person.home = home->entrance;
            person.district = home->village;
        }
        if (blocked(person.position))
        {
            person.position = person.home;
        }
        person.workplace = OpenPoint(CampAnchor(person.district, person.id, true), blocked);
        person.route.clear();
        person.waypoint = 0;
        person.target = -1;
        person.pathWait = person.simulationTime = 0;
        person.visualVelocity = person.separation = {0, 0};
        person.animation.moving = false;
        person.stopTime = .1f;
    }
}

static void Learn(std::array<News, World::VillageCount>& known, const News& report)
{
    if (report.origin >= 0 && report.origin < World::VillageCount &&
        report.serial > known[report.origin].serial)
    {
        known[report.origin] = report;
    }
}

static std::string NewsText(const News& report)
{
    if (!report.serial || report.origin < 0)
    {
        return "아직 다른 마을에서 온 소식은 듣지 못했어요.";
    }
    return std::string(World::VillageName(report.origin)) +
           (report.safety < .6f  ? " 주변에서 주령을 조심하라는 소식이에요."
            : report.wages > .7f ? "의 품삯이 좋다는 편지가 도착했어요."
                                 : "에서 새 일손과 이웃을 기다린대요.");
}

std::string HeardNews(const Person& person) const
{
    News recent;
    for (const auto& report : person.knowledge)
    {
        if (report.origin != person.district && report.serial > recent.serial)
        {
            recent = report;
        }
    }
    return NewsText(recent);
}

static const char* Personality(const Person& person)
{
    return person.caution > .7f       ? "안전을 중시함"
           : person.ambition > .65f   ? "더 나은 일자리를 찾음"
           : person.sociability > .6f ? "새 이웃과 교류를 좋아함"
                                      : "익숙한 생활을 선호함";
}

float Appeal(const Person& person, const News& report) const
{
    return report.safety * (.35f + person.caution * .55f) +
           report.wages * (.2f + person.ambition * .7f) +
           report.jobs * (.15f + person.sociability * .3f);
}

void ConsiderMigration(Person& person)
{
    if (person.questPinned || person.action != Resting || person.health < 85 ||
        person.stamina < 75 || person.mailId >= 0 || person.migrationVillage >= 0 ||
        person.migrationWait > 0)
    {
        return;
    }
    person.migrationWait = 90.f + person.id % 90;
    const auto& local = person.knowledge[person.district];
    if (!local.serial)
    {
        return;
    }
    int destination = -1;
    float best = Appeal(person, local) + .12f + .07f * person.caution;
    for (int village = 0; village < World::VillageCount; ++village)
    {
        const auto& heard = person.knowledge[village];
        if (village == person.district || !heard.serial || society.clock - heard.created > 600 ||
            FreeHouse(village) < 0)
        {
            continue;
        }
        const float score = Appeal(person, heard);
        if (score > best)
        {
            destination = village;
            best = score;
        }
    }
    if (destination >= 0)
    {
        person.reservedHouse = FreeHouse(destination);
        person.migrationVillage = destination;
        person.migrationWait = 300;
        Travel(person, VillageLayout::Home(person.reservedHouse)->entrance, Resting, MovingHome);
    }
}

template <class Collision> void FinishTravel(Person& person, Collision blocked)
{
    if (person.action == Delivering)
    {
        for (auto& letter : society.letters)
        {
            if (int(letter.id) == person.mailId && !letter.delivered)
            {
                Learn(society.boards[letter.to], letter.news);
                Learn(person.knowledge, letter.news);
                letter.delivered = true;
                ++society.delivered;
                person.gold += 5;
                person.earned += 5;
                break;
            }
        }
        person.mailId = -1;
    }
    if (person.action == MovingHome && person.reservedHouse >= 0)
    {
        const auto* house = VillageLayout::Home(person.reservedHouse);
        if (house)
        {
            person.houseId = person.reservedHouse;
            person.district = house->village;
            person.home = house->entrance;
            person.workplace = OpenPoint(CampAnchor(person.district, person.id, true), blocked);
            ++person.migrations;
            ++society.migrations;
        }
        person.reservedHouse = person.migrationVillage = -1;
    }
}

template <class Level, class Collision>
void UpdateSociety(float dt, Level& level, Collision blocked)
{
    society.clock += dt;
    society.reportWait -= dt;
    society.dispatchWait -= dt;
    society.contactWait -= dt;
    for (auto& person : people)
    {
        person.migrationWait = (std::max)(0.f, person.migrationWait - dt);
    }
    if (society.reportWait <= 0)
    {
        society.reportWait = 30;
        for (int village = 0; village < World::VillageCount; ++village)
        {
            int population = village == 0 ? VillageLayout::StoryResidents : 0;
            int threats = 0;
            const auto at = Hubs()[village];
            for (const auto& person : people)
            {
                population += person.district == village;
            }
            for (const auto& enemy : level.enemies)
            {
                threats += enemy.health > 0 && !enemy.weakened &&
                           Level::Distance(at, enemy.position) < World::VillageHalfSize + 300;
            }
            News report;
            report.serial = society.nextNews++;
            report.origin = village;
            report.created = society.clock;
            report.safety = (std::max)(.1f, .95f - threats * .04f);
            report.jobs = (std::min)(1.f, .5f + (32 - population) * .06f);
            report.wages = WageScale(village) * .6f;
            society.boards[village][village] = report;
            for (auto& person : people)
            {
                if (person.district == village && World::VillageAt(person.position) == village)
                {
                    Learn(person.knowledge, report);
                }
            }
        }
    }
    if (society.contactWait <= 0)
    {
        society.contactWait = 1;
        std::map<World::Key, std::vector<size_t>> contacts;
        for (size_t i = 0; i < people.size(); ++i)
        {
            auto& person = people[i];
            const int village = World::VillageAt(person.position);
            if (village >= 0 && Level::Distance(person.position, PostOffice(village)) < 180)
            {
                for (const auto& report : society.boards[village])
                {
                    Learn(person.knowledge, report);
                }
                for (const auto& report : person.knowledge)
                {
                    Learn(society.boards[village], report);
                }
            }
            const World::Key cell = {World::Coordinate(std::floor(person.position.x / 80)),
                                     World::Coordinate(std::floor(person.position.y / 80))};
            for (int y = -1; y <= 1; ++y)
            {
                for (int x = -1; x <= 1; ++x)
                {
                    const auto neighbors = contacts.find({cell.first + x, cell.second + y});
                    if (neighbors == contacts.end())
                    {
                        continue;
                    }
                    for (const auto other : neighbors->second)
                    {
                        auto& neighbor = people[other];
                        if (Level::Distance(person.position, neighbor.position) < 70 &&
                            person.sociability + neighbor.sociability > .45f)
                        {
                            for (int origin = 0; origin < World::VillageCount; ++origin)
                            {
                                Learn(person.knowledge, neighbor.knowledge[origin]);
                                Learn(neighbor.knowledge, person.knowledge[origin]);
                            }
                        }
                    }
                }
            }
            contacts[cell].push_back(i);
            if (person.migrationVillage >= 0 && person.action == Resting && person.health >= 85 &&
                person.stamina >= 80)
            {
                Travel(person,
                       VillageLayout::Home(person.reservedHouse)->entrance,
                       Resting,
                       MovingHome);
            }
            else if (person.mailId >= 0 && person.action == Resting && person.health >= 85 &&
                     person.stamina >= 80)
            {
                for (const auto& letter : society.letters)
                {
                    if (int(letter.id) == person.mailId && !letter.delivered)
                    {
                        Travel(person, PostOffice(letter.to), Trading, Delivering);
                    }
                }
            }
            ConsiderMigration(person);
        }
    }
    if (society.dispatchWait <= 0)
    {
        society.dispatchWait = 3;
        if (society.letters.size() >= 96)
        {
            society.letters.erase(std::remove_if(society.letters.begin(),
                                                 society.letters.end(),
                                                 [](const Letter& letter)
                                                 {
                                                     return letter.delivered;
                                                 }),
                                  society.letters.end());
        }
        for (auto& person : people)
        {
            const int from = World::VillageAt(person.position);
            if (person.role != Merchant || person.questPinned || from < 0 || person.mailId >= 0 ||
                person.migrationVillage >= 0 || person.health < 85 || person.stamina < 70 ||
                (person.action != Resting && person.action != Trading) ||
                society.letters.size() >= 128)
            {
                continue;
            }
            const int desired = (from + 1 + int(person.cycle % 6)) % World::VillageCount;
            const int to = VillageLayout::NextVillage(from, desired);
            if (to == from)
            {
                continue;
            }
            News report = society.boards[from][(person.cycle + person.id) % World::VillageCount];
            if (!report.serial)
            {
                report = society.boards[from][from];
            }
            Letter letter;
            letter.id = society.nextLetter++;
            letter.carrier = person.id;
            letter.from = from;
            letter.to = to;
            letter.news = report;
            society.letters.push_back(letter);
            person.mailId = int(letter.id);
            ++person.cycle;
            Travel(person, OpenPoint(PostOffice(to), blocked), Trading, Delivering);
            break;
        }
    }
}
