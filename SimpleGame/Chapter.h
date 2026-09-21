#pragma once
#include "World.h"
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <windows.h>

// UTF-8, pipe-separated authored objectives. No wall-clock gates: advance by player actions.
class Chapter
{
  public:
    struct Quest
    {
        std::string id, title, objective, trigger, dialogue;
        WorldPoint destination;
        int amount;
    };

    std::vector<Quest> quests;
    int step = 0, baselineKills = 0, baselineCaptures = 0;
    bool rewardPending = true;
    bool choicePending = false;
    int empathy = 0, indifference = 0, greed = 0;

    bool Load()
    {
        wchar_t executable[32768] = {};
        GetModuleFileNameW(nullptr, executable, 32768);
        const std::wstring path = executable;
        const auto slash = path.find_last_of(L"\\/");
        const std::wstring folder = slash == std::wstring::npos ? L"" : path.substr(0, slash + 1);
        const std::wstring candidates[] = {
            folder + L"Data/Chapter1.tsv", L"Data/Chapter1.tsv", L"SimpleGame/Data/Chapter1.tsv"};
        for (const auto& candidate : candidates)
        {
            std::ifstream file(candidate.c_str());
            if (!file)
                continue;
            std::vector<Quest> parsed;
            std::string line;
            while (std::getline(file, line))
            {
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();
                if (line.empty() || line[0] == '#')
                    continue;
                std::istringstream row(line);
                std::vector<std::string> cells;
                std::string cell;
                while (std::getline(row, cell, '|'))
                    cells.push_back(cell);
                if (cells.size() != 8)
                    return false;
                try
                {
                    parsed.push_back({cells[0],
                                      cells[1],
                                      cells[2],
                                      cells[3],
                                      cells[7],
                                      {std::stod(cells[4]), std::stod(cells[5])},
                                      std::stoi(cells[6])});
                }
                catch (...)
                {
                    return false;
                }
            }
            if (parsed.empty())
                return false;
            quests = std::move(parsed);
            return true;
        }
        return false;
    }

    const Quest* Current() const
    {
        return step >= 0 && step < static_cast<int>(quests.size()) ? &quests[step] : nullptr;
    }

    void Advance(int kills, int captures)
    {
        if (!Current())
            return;
        ++step;
        baselineKills = kills;
        baselineCaptures = captures;
        rewardPending = true;
    }
};
