#include "stdafx.h"
#include "ModelLibrary.h"
#include "AssetFiles.h"
#include "FrameProfiler.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#pragma warning(push, 0)
#include "ThirdParty/json.hpp"
#pragma warning(pop)

void ModelLibrary::Load()
{
    std::wstring npcCatalog = L"Art/Blender/ModularCollection/catalog.json";
    std::wstring spiritCatalog = L"Art/Blender/MobCollection/mob_catalog.json";
    bool sceneryEnabled = false;
    std::map<std::string, std::vector<std::string>> ambient;
    std::string error;
    try
    {
        auto file = AssetFiles::Resolve(L"Data/Models.json");
        if (file.empty())
        {
            file = AssetFiles::Resolve(L"SimpleGame/Data/Models.json");
        }
        std::vector<unsigned char> bytes;
        if (AssetFiles::Read(file, bytes, 65536))
        {
            const auto config = nlohmann::json::parse(bytes.begin(), bytes.end());
            if (config.at("schemaVersion").get<int>() != 1)
            {
                throw std::runtime_error("모델 배치 설정 버전이 맞지 않습니다.");
            }
            npcCatalog = AssetFiles::Wide(config.at("npcCatalog").get<std::string>());
            spiritCatalog = AssetFiles::Wide(config.at("spiritCatalog").get<std::string>());
            const auto& mapping = config.at("scenery");
            sceneryEnabled = mapping.value("enabled", false);
            house = mapping.at("house").get<std::string>();
            hall = mapping.at("hall").get<std::string>();
            rock = mapping.at("rock").get<std::string>();
            trees = mapping.at("trees").get<std::vector<std::string>>();
            if (config.contains("ambientClips"))
            {
                ambient = config.at("ambientClips")
                              .get<std::map<std::string, std::vector<std::string>>>();
            }
        }
    }
    catch (const std::exception& exception)
    {
        npcCatalog = L"Art/Blender/ModularCollection/catalog.json";
        spiritCatalog = L"Art/Blender/MobCollection/mob_catalog.json";
        ambient.clear();
        sceneryEnabled = false;
        std::cerr << "모델 배치 설정 오류, 기본 카탈로그 사용: " << exception.what() << '\n';
    }
    npcCatalogReady = catalog.LoadNpc(npcCatalog, error);
    if (!npcCatalogReady)
    {
        std::cerr << "NPC 카탈로그 로드 실패: " << error << '\n';
    }
    spiritCatalogReady = catalog.LoadSpirits(spiritCatalog, error);
    for (auto& entry : catalog.spirits)
    {
        const auto layers = ambient.find(entry.second.key);
        if (layers != ambient.end())
        {
            entry.second.ambientClips = layers->second;
        }
    }
    if (!spiritCatalogReady)
    {
        std::cerr << "주령 카탈로그 로드 실패: " << error << '\n';
    }
    scenery.reset();
    scenerySupport.clear();
    if (sceneryEnabled && npcCatalogReady)
    {
        scenery = ModelAssets::Asset::Load(catalog.sceneryPath, error);
        if (scenery)
        {
            scenery->Reset(sceneryBind);
        }
        else
        {
            std::cerr << "배경 GLB 로드 실패, 기존 구조물 사용: " << error << '\n';
        }
    }
}

bool ModelLibrary::DrawScenery(Renderer& renderer,
                               int kind,
                               int variant,
                               Point origin,
                               float width,
                               float depth,
                               float height,
                               float scale)
{
    if (!scenery || width <= 0 || depth <= 0 || height <= 0 || kind < 0 || kind > 2)
    {
        return false;
    }
    // Existing tall marker stones retain their authored lantern/quest appearance.
    if (kind == 2 && height >= 55)
    {
        return false;
    }
    const std::string name = kind == 0 ? (width >= 190 ? hall : house)
                             : kind == 1
                                 ? (trees.empty() ? "" : trees[size_t(variant) % trees.size()])
                                 : rock;
    if (!catalog.scenery.count(name))
    {
        return false;
    }
    auto support = scenerySupport.find(name);
    if (support == scenerySupport.end())
    {
        bool supported = false;
        for (const auto& primitive : scenery->primitives)
        {
            if (primitive.name == name)
            {
                const auto& material = scenery->materials[primitive.material];
                supported = true;
                if (primitive.skin >= 0 || material.texture >= 0 || material.alphaMode != 0)
                {
                    std::cerr << "배경 캐시는 불투명 단색 정적 메시만 지원, 기존 표시 사용: "
                              << name << '\n';
                    supported = false;
                    break;
                }
            }
        }
        support = scenerySupport.emplace(name, supported).first;
    }
    if (!support->second)
    {
        return false;
    }
    const auto key = "gltf:scenery:v1:" + name + ":" + std::to_string(width) + ":" +
                     std::to_string(depth) + ":" + std::to_string(height);
    if (renderer.BeginCachedMesh(key, origin))
    {
        std::array<float, 3> low, high;
        scenery->Bounds(sceneryBind, name, low, high);
        const float extent[] = {(std::max)(.00001f, high[0] - low[0]),
                                (std::max)(.00001f, high[1] - low[1]),
                                (std::max)(.00001f, high[2] - low[2])};

        struct Face
        {
            std::array<std::array<float, 3>, 3> points;
            Color color = Color(1, 1, 1);
            float depth = 0;
        };

        std::vector<Face> faces;
        for (const auto& primitive : scenery->primitives)
        {
            if (primitive.name != name)
            {
                continue;
            }
            const auto transform =
                ModelAssets::Multiply(scenery->normalization, sceneryBind.global[primitive.node]);
            const auto& material = scenery->materials[primitive.material];
            for (size_t i = primitive.first; i < size_t(primitive.first) + primitive.count; i += 3)
            {
                Face face;
                for (size_t corner = 0; corner < 3; ++corner)
                {
                    const auto p =
                        ModelAssets::Point(transform, scenery->vertices[i + corner].position);
                    face.points[corner] = {{(p[0] - low[0]) * width / extent[0],
                                            (p[1] - low[1]) * depth / extent[1],
                                            (p[2] - low[2]) * height / extent[2]}};
                    const auto& world = face.points[corner];
                    face.depth += .603f * (world[0] + world[1]) + .519f * world[2];
                }
                const auto& a = face.points[0];
                const auto& b = face.points[1];
                const auto& c = face.points[2];
                const float u[] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
                const float v[] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
                float n[] = {u[1] * v[2] - u[2] * v[1],
                             u[2] * v[0] - u[0] * v[2],
                             u[0] * v[1] - u[1] * v[0]};
                const float length = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
                if (length < 1e-8f)
                {
                    continue;
                }
                // Double-sided scenery must not let the far wall paint over its front.
                if ((n[0] * .603f + n[1] * .603f + n[2] * .519f) < 0)
                {
                    continue;
                }
                const float diffuse =
                    (-.45f * n[0] - .65f * n[1] + 1.1f * n[2]) / (1.3546217f * length);
                const float lighting = material.unlit   ? 1.f
                                       : diffuse > .72f ? 1.f
                                       : diffuse > .4f  ? .68f
                                       : diffuse > .16f ? .4f
                                                        : .2f;
                auto channel = [&](int axis)
                {
                    const float linear =
                        material.color[axis] * scenery->vertices[i].color[axis] * lighting +
                        material.emission[axis];
                    return linear <= .0031308f ? linear * 12.92f
                                               : 1.055f * std::pow(linear, 1 / 2.4f) - .055f;
                };
                face.color = Color(channel(0), channel(1), channel(2));
                faces.push_back(face);
            }
        }
        std::stable_sort(faces.begin(),
                         faces.end(),
                         [](const Face& a, const Face& b)
                         {
                             return a.depth < b.depth;
                         });
        for (const auto& face : faces)
        {
            auto project = [&](int i) -> Point
            {
                const auto& p = face.points[i];
                return {origin.x + .85f * (p[0] - p[1]), origin.y + .43f * (p[0] + p[1]) - p[2]};
            };
            renderer.Triangle(project(0), project(1), project(2), face.color);
        }
        renderer.EndCachedMesh();
        FrameProfiler::Event("gltf_scenery_cached", name, faces.size() * 3 * 6 * sizeof(float));
    }
    renderer.DrawCachedMesh(key, origin, scale);
    return true;
}
