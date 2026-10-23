#pragma once
#include "ModelAsset.h"
#include "NpcAppearance.h"
#include <map>

namespace ModelAssets
{
struct Definition
{
    int id = -1;
    std::string key;
    std::wstring path;
    std::map<std::string, std::string> clips;
    std::vector<std::string> ambientClips;
};

// A catalog describes export files, not .blend authoring documents or executable code.
class Catalog
{
  public:
    Definition npc;
    std::wstring sceneryPath;
    std::map<int, Definition> spirits;
    std::map<std::string, std::pair<int, int>> parts;
    std::map<std::string, NpcAppearance::Definition> presets;
    std::map<std::string, std::string> scenery;

    bool LoadNpc(const std::wstring& relative, std::string& error);
    bool LoadSpirits(const std::wstring& relative, std::string& error);
    bool ConfigureParts(Asset& asset, std::string& error) const;
    NpcAppearance::Definition Story(int identity, const NpcAppearance::Definition& fallback) const;
};
} // namespace ModelAssets
