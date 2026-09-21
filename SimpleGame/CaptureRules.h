#pragma once
#include "Chapter.h"
#include <map>

struct CaptureRules
{
    // TEMP fallbacks; editable tuning is shipped in Data/Capture.ini.
    float threshold = .2f, bossThreshold = .1f;
    float normalChance = .85f, bossChance = .2f, ticketDrop = .15f;
    float rareChance = .05f, uncommonChance = .25f;

    void Load()
    {
        wchar_t exe[32768] = {};
        GetModuleFileNameW(nullptr, exe, 32768);
        const std::wstring path = exe;
        const auto slash = path.find_last_of(L"\\/");
        const std::wstring folder = slash == std::wstring::npos ? L"" : path.substr(0, slash + 1);
        const std::wstring paths[] = {
            folder + L"Data/Capture.ini", L"Data/Capture.ini", L"SimpleGame/Data/Capture.ini"};
        std::map<std::string, float*> values = {{"threshold", &threshold},
                                                {"boss_threshold", &bossThreshold},
                                                {"normal_chance", &normalChance},
                                                {"boss_chance", &bossChance},
                                                {"ticket_drop", &ticketDrop},
                                                {"rare_chance", &rareChance},
                                                {"uncommon_chance", &uncommonChance}};
        for (const auto& candidate : paths)
        {
            std::ifstream file(candidate.c_str());
            if (!file)
                continue;
            std::string line;
            while (std::getline(file, line))
            {
                const auto equal = line.find('=');
                if (line.empty() || line[0] == '#' || equal == std::string::npos)
                    continue;
                const auto found = values.find(line.substr(0, equal));
                if (found == values.end())
                    continue;
                try
                {
                    const float value = std::stof(line.substr(equal + 1));
                    if (std::isfinite(value) && value >= 0 && value <= 1)
                        *found->second = value;
                }
                catch (...)
                {
                }
            }
            return;
        }
    }
};
