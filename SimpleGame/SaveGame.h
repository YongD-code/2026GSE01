#pragma once
#include "LevelOne.h"
#include "Population.h"
#include <iomanip>
#include <locale>
#include <limits>
#include <shlobj.h>
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uuid.lib")

// Versioned, bounded text snapshots. No GPU handles or native struct layouts are persisted.
class SaveGame
{
  public:
    struct State
    {
        std::uint64_t seed = 0, story = 0;
        WorldPoint player = {0, 0}, spirit = {0, 0}, keeper = {0, 0};
        LevelOne level;
        std::vector<Population::Person> residents;
        Population::Society society;
        int formatVersion = 7;
        Chapter chapter;
        int companion = 0, facing = 0;
        bool controlling = false, dialogue = false;
        bool hud = true, shake = true, post = true, bloom = true;
        std::string speaker, message;
        float messageTime = 0;
        bool spiritEquipped = true;
        float skillTimers[5][4] = {};
    };

    enum class Result
    {
        Missing,
        Loaded,
        Backup,
        Invalid
    };
    static constexpr size_t MaximumBytes = 64 * 1024 * 1024;

    static std::uint64_t Hash(const std::string& bytes)
    {
        std::uint64_t hash = 14695981039346656037ULL;
        for (unsigned char c : bytes)
        {
            hash ^= c;
            hash *= 1099511628211ULL;
        }
        return hash;
    }

    static std::uint64_t StoryHash(const Chapter& chapter)
    {
        std::ostringstream out;
        out.imbue(std::locale::classic());
        out << std::setprecision(17) << "chapter1-structure-v1\n";
        for (const auto& q : chapter.quests)
        {
            // Editorial changes do not invalidate progress; ordering and logic still must match.
            out << std::quoted(q.id) << std::quoted(q.trigger) << q.destination.x << ' '
                << q.destination.y << ' ' << q.amount << '\n';
        }
        return Hash(out.str());
    }

    static std::wstring Path(int slot = 1)
    {
        if (slot < 1 || slot > 3)
            return L"";
        PWSTR folder = nullptr;
        if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &folder)))
        {
            return L"";
        }
        std::wstring path = std::wstring(folder) + L"\\2026GSE01";
        CoTaskMemFree(folder);
        if (!CreateDirectoryW(path.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
        {
            return L"";
        }
        return path +
               (slot == 1 ? L"\\progress.sav" : L"\\progress" + std::to_wstring(slot) + L".sav");
    }

  private:
    struct Archive
    {
        std::stringstream stream;
        bool reading;

        explicit Archive(bool read) : reading(read)
        {
            stream.imbue(std::locale::classic());
            stream << std::setprecision(17);
        }

        template <class T> void Field(T& value)
        {
            if (reading)
            {
                stream >> value;
            }
            else
            {
                stream << value << ' ';
            }
        }

        void Field(std::string& value)
        {
            if (reading)
            {
                stream >> std::quoted(value);
            }
            else
            {
                stream << std::quoted(value) << ' ';
            }
            if (value.size() > 32768)
            {
                stream.setstate(std::ios::failbit);
            }
        }

        template <class... T> void Fields(T&... values)
        {
            int expanded[] = {0, (Field(values), 0)...};
            (void)expanded;
        }

        void Point(WorldPoint& point)
        {
            Fields(point.x, point.y);
        }

        bool Count(size_t& size, size_t limit)
        {
            std::int64_t count = static_cast<std::int64_t>(size);
            Field(count);
            if (!stream || count < 0 || static_cast<std::uint64_t>(count) > limit)
            {
                return false;
            }
            size = static_cast<size_t>(count);
            return true;
        }
    };

    static void TransferNews(Archive& a, Population::News& news)
    {
        a.Fields(news.serial, news.origin, news.created, news.safety, news.jobs, news.wages);
    }

    static bool Transfer(Archive& a, State& s, int version = 7)
    {
        s.formatVersion = version;
        auto& l = s.level;
        auto& c = s.chapter;
        a.Fields(s.seed, s.story);
        a.Point(s.player);
        a.Point(s.spirit);
        a.Point(s.keeper);
        a.Fields(s.companion,
                 s.facing,
                 s.controlling,
                 s.dialogue,
                 s.hud,
                 s.shake,
                 s.post,
                 s.bloom,
                 s.speaker,
                 s.message,
                 s.messageTime);
        a.Fields(l.level,
                 l.experience,
                 l.kills,
                 l.weapon,
                 l.stones,
                 l.spiritRank,
                 l.failedUpgrades,
                 l.health,
                 l.cooldown,
                 l.companionCooldown,
                 l.skillCooldown,
                 l.dodgeCooldown,
                 l.automatic,
                 l.complete,
                 l.captureUnlocked,
                 l.selectedTicket,
                 l.saintRemains,
                 l.skillsUsed);
        if (version >= 5)
        {
            a.Fields(l.gold, l.potions, l.weaponStones, l.spoils);
        }
        for (int& ticket : l.tickets)
        {
            a.Field(ticket);
        }
        a.Fields(c.step,
                 c.baselineKills,
                 c.baselineCaptures,
                 c.rewardPending,
                 c.choicePending,
                 c.empathy,
                 c.indifference,
                 c.greed);
        std::string rng;
        if (!a.reading)
        {
            std::ostringstream engine;
            engine.imbue(std::locale::classic());
            engine << l.random;
            rng = engine.str();
        }
        a.Field(rng);
        if (a.reading)
        {
            std::istringstream engine(rng);
            engine.imbue(std::locale::classic());
            if (!(engine >> l.random))
            {
                return false;
            }
            engine >> std::ws;
            if (!engine.eof())
            {
                return false;
            }
        }
        size_t count = l.collection.size();
        if (!a.Count(count, 100000))
        {
            return false;
        }
        if (a.reading)
        {
            l.collection.resize(count);
        }
        for (auto& spirit : l.collection)
        {
            a.Fields(spirit.species, spirit.rarity, spirit.memory);
            if (version >= 3)
            {
                a.Fields(spirit.health);
            }
        }
        count = l.resolvedBosses.size();
        if (!a.Count(count, 12))
        {
            return false;
        }
        if (a.reading)
        {
            for (size_t i = 0; i < count; ++i)
            {
                int id = 0;
                a.Field(id);
                if (id < 1 || id > 12 || !l.resolvedBosses.insert(id).second)
                {
                    return false;
                }
            }
        }
        else
        {
            for (int id : l.resolvedBosses)
            {
                a.Field(id);
            }
        }
        count = l.camps.size();
        if (!a.Count(count, 500000))
        {
            return false;
        }
        if (a.reading)
        {
            for (size_t i = 0; i < count; ++i)
            {
                World::Key key;
                LevelOne::CampRecord camp;
                a.Fields(key.first, key.second, camp.defeated);
                for (float& hp : camp.health)
                {
                    a.Field(hp);
                }
                if (!a.stream || !l.camps.emplace(key, camp).second)
                {
                    return false;
                }
            }
        }
        else
        {
            for (auto& entry : l.camps)
            {
                auto key = entry.first;
                a.Fields(key.first, key.second, entry.second.defeated);
                for (float& hp : entry.second.health)
                {
                    a.Field(hp);
                }
            }
        }
        count = l.enemies.size();
        if (!a.Count(count, 512))
        {
            return false;
        }
        if (a.reading)
        {
            l.enemies.resize(count);
        }
        for (auto& e : l.enemies)
        {
            a.Point(e.position);
            a.Point(e.home);
            a.Fields(e.type,
                     e.health,
                     e.maximum,
                     e.camp.first,
                     e.camp.second,
                     e.slot,
                     e.weakened,
                     e.boss,
                     e.slowed);
            if (version >= 4)
            {
                a.Fields(e.remnantTime, e.remnantRarity);
            }
            // Snapshots are accepted only outside combat. Resume idle actors without attack phases.
            if (a.reading)
            {
                if (e.boss > 0)
                {
                    e.type = StoryEncounter::Get(e.boss).species;
                    if (e.boss == 10 && e.weakened)
                        e.remnantRarity = (std::max)(3, e.remnantRarity);
                }
                e.trail.push_back(e.home);
                if (LevelOne::Distance(e.home, e.position) > .001)
                {
                    e.trail.push_back(e.position);
                }
            }
        }
        count = l.loot.size();
        if (!a.Count(count, 100000))
        {
            return false;
        }
        if (a.reading)
        {
            l.loot.resize(count);
        }
        for (auto& item : l.loot)
        {
            int kind = static_cast<int>(item.kind);
            a.Point(item.position);
            a.Fields(kind, item.age, item.attracted);
            if (kind < 0 || kind > 4)
            {
                return false;
            }
            item.kind = static_cast<LevelOne::LootKind>(kind);
        }
        if (version >= 2)
        {
            a.Field(s.spiritEquipped);
            for (auto& set : s.skillTimers)
            {
                for (float& timer : set)
                {
                    a.Field(timer);
                }
            }
        }
        else
        {
            for (auto& set : s.skillTimers)
            {
                for (float& timer : set)
                {
                    timer = s.level.skillCooldown;
                }
            }
        }
        if (version >= 6)
        {
            size_t residents = s.residents.size();
            if (!a.Count(residents, version >= 7 ? VillageLayout::ResidentCount : 40))
                return false;
            if (a.reading)
                s.residents.resize(residents);
            for (auto& person : s.residents)
            {
                a.Fields(person.id,
                         person.role,
                         person.district,
                         person.gold,
                         person.earned,
                         person.kills,
                         person.goods,
                         person.health,
                         person.stamina,
                         person.cycle);
                a.Point(person.position);
                a.Point(person.home);
                a.Point(person.workplace);
                if (version >= 7)
                {
                    a.Fields(person.houseId,
                             person.reservedHouse,
                             person.migrationVillage,
                             person.mailId,
                             person.ambition,
                             person.caution,
                             person.sociability,
                             person.migrationWait,
                             person.migrations,
                             person.action,
                             person.nextAction,
                             person.timer);
                    a.Point(person.destination);
                    for (auto& report : person.knowledge)
                    {
                        TransferNews(a, report);
                    }
                }
                else if (a.reading)
                {
                    person.destination = person.position;
                    person.action = Population::Resting;
                    person.timer = 1.f + static_cast<float>(person.id % 6);
                }
            }
        }
        if (version >= 7)
        {
            auto& society = s.society;
            a.Fields(society.clock,
                     society.nextNews,
                     society.nextLetter,
                     society.delivered,
                     society.migrations);
            for (auto& board : society.boards)
            {
                for (auto& report : board)
                {
                    TransferNews(a, report);
                }
            }
            size_t letters = society.letters.size();
            if (!a.Count(letters, 128))
            {
                return false;
            }
            if (a.reading)
            {
                society.letters.resize(letters);
            }
            for (auto& letter : society.letters)
            {
                a.Fields(letter.id, letter.carrier, letter.from, letter.to, letter.delivered);
                TransferNews(a, letter.news);
            }
        }
        return bool(a.stream);
    }

    static bool Number(double value, double low, double high)
    {
        return std::isfinite(value) && value >= low && value <= high;
    }

    static bool Point(WorldPoint p)
    {
        return Number(p.x, -1e12, 1e12) && Number(p.y, -1e12, 1e12);
    }

    static bool Key(World::Key key)
    {
        return key.first >= -2000000000LL && key.first <= 2000000000LL &&
               key.second >= -2000000000LL && key.second <= 2000000000LL;
    }

    static bool ValidateNews(const Population::News& news, const Population::Society& society)
    {
        if (!news.serial)
        {
            return news.origin == -1;
        }
        return news.serial < society.nextNews && Number(news.origin, 0, World::VillageCount - 1) &&
               Number(news.created, 0, society.clock) && Number(news.safety, 0, 1) &&
               Number(news.jobs, 0, 1) && Number(news.wages, 0, 2);
    }

    static bool ValidateSociety(const State& state)
    {
        const auto& society = state.society;
        if (!Number(society.clock, 0, 1e12) || !Number(society.nextNews, 1, 1000000000) ||
            !Number(society.nextLetter, 1, 1000000000) ||
            !Number(society.delivered, 0, 1000000000) || !Number(society.migrations, 0, 1000000000))
        {
            return false;
        }
        std::array<bool, VillageLayout::HomeCount> occupied = {};
        for (int i = 0; i < VillageLayout::StoryResidents; ++i)
        {
            occupied[i] = true;
        }
        for (const auto& person : state.residents)
        {
            const auto* home = VillageLayout::Home(person.houseId);
            if (!home || occupied[person.houseId] || home->village != person.district ||
                !Number(person.ambition, 0, 1) || !Number(person.caution, 0, 1) ||
                !Number(person.sociability, 0, 1) || !Number(person.migrationWait, 0, 1000000) ||
                !Number(person.migrations, 0, 1000000000) ||
                !Number(person.action, 0, Population::MovingHome) ||
                !Number(person.nextAction, 0, Population::MovingHome) ||
                !Number(person.timer, -1000000, 1000000) || !Point(person.destination) ||
                !Number(person.mailId, -1, 1000000000) ||
                !Number(person.migrationVillage, -1, World::VillageCount - 1))
            {
                return false;
            }
            occupied[person.houseId] = true;
            if (person.reservedHouse >= 0)
            {
                const auto* reserved = VillageLayout::Home(person.reservedHouse);
                if (!reserved || occupied[person.reservedHouse] ||
                    reserved->village != person.migrationVillage ||
                    person.migrationVillage == person.district || person.mailId >= 0)
                {
                    return false;
                }
                occupied[person.reservedHouse] = true;
            }
            else if (person.reservedHouse != -1 || person.migrationVillage != -1)
            {
                return false;
            }
            for (int origin = 0; origin < World::VillageCount; ++origin)
            {
                const auto& report = person.knowledge[origin];
                if (!ValidateNews(report, society) || (report.serial && report.origin != origin))
                {
                    return false;
                }
            }
        }
        for (const auto& board : society.boards)
        {
            for (int origin = 0; origin < World::VillageCount; ++origin)
            {
                if (!ValidateNews(board[origin], society) ||
                    (board[origin].serial && board[origin].origin != origin))
                {
                    return false;
                }
            }
        }
        std::set<unsigned> letterIds;
        for (const auto& letter : society.letters)
        {
            if (!letter.id || letter.id >= society.nextLetter ||
                !letterIds.insert(letter.id).second ||
                !Number(letter.carrier, 0, double(state.residents.size()) - 1) ||
                !Number(letter.from, 0, World::VillageCount - 1) ||
                !Number(letter.to, 0, World::VillageCount - 1) || letter.from == letter.to ||
                !letter.news.serial || !ValidateNews(letter.news, society) ||
                (!letter.delivered && state.residents[letter.carrier].mailId != int(letter.id)))
            {
                return false;
            }
        }
        for (const auto& person : state.residents)
        {
            if (person.mailId < 0)
            {
                continue;
            }
            bool found = false;
            for (const auto& letter : society.letters)
            {
                found = found || (!letter.delivered && letter.carrier == person.id &&
                                  int(letter.id) == person.mailId);
            }
            if (!found)
            {
                return false;
            }
        }
        return true;
    }

    static bool Validate(const State& s, const Chapter& authored)
    {
        if (!s.residents.empty() &&
            s.residents.size() !=
                (s.formatVersion >= 7 ? size_t(VillageLayout::ResidentCount) : 40))
            return false;
        for (size_t i = 0; i < s.residents.size(); ++i)
        {
            const auto& person = s.residents[i];
            if (person.id != int(i) || !Number(person.role, 0, 3) ||
                !Number(person.district, 0, s.formatVersion >= 7 ? World::VillageCount - 1 : 4) ||
                !Number(person.gold, 0, 1000000000) || !Number(person.earned, 0, 1000000000) ||
                !Number(person.kills, 0, 1000000000) || !Number(person.goods, 0, 1000000) ||
                !Number(person.health, 0, 100) || !Number(person.stamina, 0, 100) ||
                !Point(person.position) || !Point(person.home) || !Point(person.workplace))
                return false;
        }
        if (s.formatVersion >= 7 && !ValidateSociety(s))
        {
            return false;
        }
        for (const auto& set : s.skillTimers)
        {
            for (float timer : set)
            {
                if (!Number(timer, 0, 1000))
                {
                    return false;
                }
            }
        }
        const auto& l = s.level;
        const auto& c = s.chapter;
        if (authored.quests.empty() || s.story != StoryHash(authored) || !Point(s.player) ||
            !Point(s.spirit) || !Point(s.keeper) || !Number(s.messageTime, 0, 60) ||
            !Number(s.facing, 0, 7) || !Number(l.level, 1, 20) ||
            !Number(l.experience, 0, l.RequiredExperience() - 1) ||
            !Number(l.health, .001, l.MaximumHealth()) || !Number(l.weapon, 0, 20) ||
            !Number(l.gold, 0, 100000000) || !Number(l.potions, 0, 1000000) ||
            !Number(l.weaponStones, 0, 1000000) || !Number(l.spoils, 0, 1000000) ||
            !Number(l.spiritRank, 0, 10) || !Number(l.failedUpgrades, 0, 3) ||
            !Number(l.selectedTicket, 0, 2) || c.step < 0 ||
            static_cast<size_t>(c.step) > authored.quests.size() ||
            !Number(c.baselineKills, 0, l.kills) || c.baselineCaptures < 0 ||
            static_cast<size_t>(c.baselineCaptures) > l.collection.size() || s.companion < 0 ||
            (l.collection.empty() ? s.companion != 0
                                  : static_cast<size_t>(s.companion) >= l.collection.size()) ||
            (s.controlling && l.collection.empty()) ||
            (c.choicePending && c.step != authored.Index("P001_WITNESS")))
        {
            return false;
        }
        for (int count :
             {l.kills, l.stones, l.saintRemains, l.skillsUsed, c.empathy, c.indifference, c.greed})
        {
            if (!Number(count, 0, 1000000000))
            {
                return false;
            }
        }
        for (int ticket : l.tickets)
        {
            if (!Number(ticket, 0, 1000000000))
            {
                return false;
            }
        }
        for (float timer : {l.cooldown, l.companionCooldown, l.skillCooldown, l.dodgeCooldown})
        {
            if (!Number(timer, 0, 1000))
            {
                return false;
            }
        }
        for (const auto& spirit : l.collection)
        {
            if (!Number(spirit.species, 0, SpiritCatalog::Count - 1) ||
                !Number(spirit.rarity, 1, 4) || !Number(spirit.health, 0, 100))
            {
                return false;
            }
        }
        for (const auto& entry : l.camps)
        {
            if (!Key(entry.first) || entry.second.defeated > 15)
            {
                return false;
            }
            for (int i = 0; i < 4; ++i)
            {
                float hp = entry.second.health[i];
                if (!Number(hp, -1, 1000000) || (hp < 0 && hp != -1) ||
                    ((entry.second.defeated & (1u << i)) && hp != 0))
                {
                    return false;
                }
            }
        }
        std::set<std::pair<World::Key, int>> actors;
        std::set<int> bosses;
        for (const auto& e : l.enemies)
        {
            if (!Point(e.position) || !Point(e.home) || !Key(e.camp) ||
                !Number(e.type, 0, SpiritCatalog::Count - 1) || !Number(e.slot, 0, 3) ||
                !Number(e.boss, 0, 12) || !Number(e.maximum, 1, 1000000) ||
                !Number(e.health, .001, e.maximum) || !Number(e.slowed, 0, 1000) ||
                !Number(e.remnantTime, -2, 60) || !Number(e.remnantRarity, 1, 4))
            {
                return false;
            }
            if (e.boss)
            {
                if ((!e.weakened && l.resolvedBosses.count(e.boss)) ||
                    !bosses.insert(e.boss).second)
                {
                    return false;
                }
            }
            else
            {
                auto camp = l.camps.find(e.camp);
                if (camp == l.camps.end() ||
                    (!e.weakened && (camp->second.defeated & (1u << e.slot))) ||
                    !actors.insert({e.camp, e.slot}).second)
                {
                    return false;
                }
            }
        }
        for (const auto& item : l.loot)
        {
            if (!Point(item.position) || !Number(item.age, 0, 90))
            {
                return false;
            }
        }
        return true;
    }

    static bool Read(const std::wstring& path, const Chapter& chapter, State& output)
    {
        std::ifstream file(path.c_str(), std::ios::binary | std::ios::ate);
        if (!file)
        {
            return false;
        }
        const auto size = file.tellg();
        if (size <= 0 || size > static_cast<std::streamoff>(MaximumBytes))
        {
            return false;
        }
        file.seekg(0);
        std::string bytes(static_cast<size_t>(size), '\0');
        if (!file.read(&bytes[0], static_cast<std::streamsize>(bytes.size())))
        {
            return false;
        }
        const auto newline = bytes.find('\n');
        if (newline == std::string::npos || newline > 100)
        {
            return false;
        }
        std::istringstream header(bytes.substr(0, newline));
        std::string magic;
        int version = 0;
        std::uint64_t checksum = 0;
        if (!(header >> magic >> version >> checksum) || magic != "GSE_SAVE" ||
            (version != 1 && version != 2 && version != 3 && version != 4 && version != 5 &&
             version != 6 && version != 7))
        {
            return false;
        }
        header >> std::ws;
        if (!header.eof())
        {
            return false;
        }
        const std::string payload = bytes.substr(newline + 1);
        if (Hash(payload) != checksum)
        {
            return false;
        }
        Archive archive(true);
        archive.stream.str(payload);
        State candidate;
        if (!Transfer(archive, candidate, version))
        {
            return false;
        }
        // Explicit migration from the authored 36-quest layout through patch 1.0.67.
        // Never accept arbitrary mismatches or silently remap numeric quest indices.
        if (candidate.story == 17577236682408322091ULL &&
            StoryHash(chapter) == 12606591398302679102ULL)
        {
            candidate.story = StoryHash(chapter);
            if (candidate.chapter.step == chapter.Index("P006"))
            {
                candidate.level.skillsUsed = 0;
            }
        }
        archive.stream >> std::ws;
        if (!archive.stream.eof() || !Validate(candidate, chapter))
        {
            return false;
        }
        output = std::move(candidate);
        return true;
    }

  public:
    static Result Load(const Chapter& authored, State& output, int slot = 1)
    {
        const std::wstring path = Path(slot);
        if (path.empty())
        {
            return Result::Invalid;
        }
        if (Read(path, authored, output))
        {
            return Result::Loaded;
        }
        if (Read(path + L".bak", authored, output))
        {
            return Result::Backup;
        }
        auto missing = [](const std::wstring& file)
        {
            if (GetFileAttributesW(file.c_str()) != INVALID_FILE_ATTRIBUTES)
            {
                return false;
            }
            const DWORD error = GetLastError();
            return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
        };
        if (missing(path) && missing(path + L".bak"))
        {
            return Result::Missing;
        }
        return Result::Invalid;
    }

    static bool Write(State state, const Chapter& authored, int slot = 1)
    {
        const std::wstring path = Path(slot);
        if (path.empty() || !Validate(state, authored))
        {
            return false;
        }
        Archive archive(false);
        if (!Transfer(archive, state))
        {
            return false;
        }
        const std::string payload = archive.stream.str();
        const std::string bytes = "GSE_SAVE 7 " + std::to_string(Hash(payload)) + "\n" + payload;
        if (bytes.size() > MaximumBytes)
        {
            return false;
        }
        const std::wstring temp = path + L".tmp";
        HANDLE file = CreateFileW(
            temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
        {
            return false;
        }
        DWORD written = 0;
        bool ok =
            WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) &&
            written == bytes.size() && FlushFileBuffers(file);
        CloseHandle(file);
        if (!ok)
        {
            DeleteFileW(temp.c_str());
            return false;
        }
        // Keep the last compatible, verified snapshot as backup, never a corrupt primary.
        State previous;
        if (Read(path, authored, previous) &&
            !CopyFileW(path.c_str(), (path + L".bak").c_str(), FALSE))
        {
            DeleteFileW(temp.c_str());
            return false;
        }
        ok = MoveFileExW(temp.c_str(),
                         path.c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
        if (!ok)
        {
            DeleteFileW(temp.c_str());
        }
        return ok;
    }
};
