#include "stdafx.h"
#include "ModelCatalog.h"
#include "AssetFiles.h"
#include <cmath>
#include <set>
#include <stdexcept>
#pragma warning(push, 0)
#include "ThirdParty/json.hpp"
#pragma warning(pop)

namespace ModelAssets
{
namespace
{
using Json = nlohmann::json;

Json Read(const std::wstring& path)
{
    std::vector<unsigned char> bytes;
    if (path.empty() || !AssetFiles::Read(path, bytes, 4 * 1024 * 1024))
    {
        throw std::runtime_error("제작 카탈로그를 읽지 못했습니다.");
    }
    auto json = Json::parse(bytes.begin(), bytes.end());
    if (json.at("schemaVersion").get<int>() != 1)
    {
        throw std::runtime_error("지원하지 않는 카탈로그 버전입니다.");
    }
    return json;
}

std::wstring Export(const std::wstring& folder, const std::string& file)
{
    auto path = AssetFiles::Local(folder, file);
    if (path.empty() ||
        (path.size() < 4 || (path.substr(path.size() - 4) != L".glb" &&
                             (path.size() < 5 || path.substr(path.size() - 5) != L".gltf"))))
    {
        throw std::runtime_error("카탈로그 모델 경로는 하위 폴더의 .glb/.gltf여야 합니다.");
    }
    return path;
}

std::map<std::string, std::string> Clips(const Json& json)
{
    std::map<std::string, std::string> result;
    if (json.contains("clips"))
    {
        if (!json["clips"].is_object())
        {
            throw std::runtime_error("clips는 별칭별 객체여야 합니다.");
        }
        for (auto it = json["clips"].begin(); it != json["clips"].end(); ++it)
        {
            result[it.key()] = it.value().is_string() ? it.value().get<std::string>()
                                                      : it.value().value("name", it.key());
        }
    }
    return result;
}
} // namespace

bool Catalog::LoadNpc(const std::wstring& relative, std::string& error)
{
    try
    {
        const auto path = AssetFiles::Resolve(relative);
        const auto json = Read(path);
        const auto folder = AssetFiles::Directory(path);
        Definition model;
        model.key = "npc";
        model.path =
            Export(folder, json.value("riggedPartFile", json.value("partFile", std::string())));
        model.clips = Clips(json);
        auto sceneryFile = Export(folder, json.at("sceneryFile").get<std::string>());
        std::map<std::string, std::pair<int, int>> newParts;
        const char* slots[] = {"body", "hair", "outfit", "accessory"};
        std::array<std::set<int>, 4> available;
        for (int slot = 0; slot < 4; ++slot)
        {
            for (const auto& entry : json.at("parts").at(slots[slot]))
            {
                const auto name = entry.at("mesh").get<std::string>();
                const int index = entry.at("index").get<int>();
                if (name.empty() || index < 0 || index > 4095 ||
                    !available[slot].insert(index).second ||
                    !newParts.emplace(name, std::make_pair(slot, index)).second)
                {
                    throw std::runtime_error(
                        "NPC 파츠 이름/슬롯 인덱스가 중복되거나 잘못됐습니다.");
                }
            }
        }
        std::map<std::string, NpcAppearance::Definition> newPresets;
        for (const auto& entry : json.at("npcs"))
        {
            NpcAppearance::Definition preset;
            const auto id = entry.at("id").get<std::string>();
            preset.scale = entry.value("scale", 1.f);
            if (!std::isfinite(preset.scale) || preset.scale < .1f || preset.scale > 4)
            {
                throw std::runtime_error("NPC 크기 배율은 0.1~4여야 합니다.");
            }
            for (int slot = 0; slot < 4; ++slot)
            {
                const int index = entry.at("partIndices").at(slots[slot]).get<int>();
                if (!(slot == 3 && index == -1) && !available[slot].count(index))
                {
                    throw std::runtime_error("NPC 조합이 존재하지 않는 파츠를 참조합니다.");
                }
                preset.partIndices[slot] = index;
            }
            if (id.empty() || !newPresets.emplace(id, preset).second)
            {
                throw std::runtime_error("NPC 조합 고유 ID가 비어 있거나 중복됐습니다.");
            }
        }
        std::map<std::string, std::string> newScenery;
        for (const auto& entry : json.at("scenery"))
        {
            const auto name = entry.at("mesh").get<std::string>();
            if (!newScenery.emplace(name, name).second)
            {
                throw std::runtime_error("배경 메시 이름이 중복됐습니다.");
            }
        }
        npc = std::move(model);
        sceneryPath = std::move(sceneryFile);
        parts = std::move(newParts);
        presets = std::move(newPresets);
        scenery = std::move(newScenery);
        error.clear();
        return true;
    }
    catch (const std::exception& exception)
    {
        error = exception.what();
        return false;
    }
}

bool Catalog::LoadSpirits(const std::wstring& relative, std::string& error)
{
    try
    {
        const auto path = AssetFiles::Resolve(relative);
        const auto json = Read(path);
        const auto folder = AssetFiles::Directory(path);
        std::map<int, Definition> entries;
        std::set<std::string> keys;
        for (const auto& item : json.at("species"))
        {
            Definition definition;
            definition.id = item.at("id").get<int>();
            definition.key = item.at("key").get<std::string>();
            definition.path = Export(folder, item.at("export").get<std::string>());
            definition.clips = Clips(item);
            definition.ambientClips = item.value("ambientClips", std::vector<std::string>());
            if (definition.id < 0 || definition.id > 65535 || definition.key.empty() ||
                !keys.insert(definition.key).second ||
                !entries.emplace(definition.id, definition).second)
            {
                throw std::runtime_error("주령 고유 ID/키가 중복되거나 잘못됐습니다.");
            }
        }
        spirits = std::move(entries);
        error.clear();
        return true;
    }
    catch (const std::exception& exception)
    {
        error = exception.what();
        return false;
    }
}

bool Catalog::ConfigureParts(Asset& asset, std::string& error) const
{
    std::set<std::string> found;
    for (auto& primitive : asset.primitives)
    {
        const auto part = parts.find(primitive.name);
        if (part == parts.end())
        {
            error = "카탈로그에 없는 NPC 메시입니다: " + primitive.name;
            return false;
        }
        primitive.slot = part->second.first;
        primitive.index = part->second.second;
        found.insert(primitive.name);
    }
    if (found.size() != parts.size())
    {
        error = "카탈로그에 등록한 NPC 파츠 일부가 GLB에 없습니다.";
        return false;
    }
    error.clear();
    return true;
}

NpcAppearance::Definition Catalog::Story(int identity,
                                         const NpcAppearance::Definition& fallback) const
{
    const char* ids[] = {
        "sibling", "parent", "village_merchant", "child", "stranger", "raen", "sealer", "smith"};
    if (identity >= 0 && identity < 8)
    {
        const auto found = presets.find(ids[identity]);
        if (found != presets.end())
        {
            return found->second;
        }
    }
    return fallback;
}
} // namespace ModelAssets
