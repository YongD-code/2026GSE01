#include "stdafx.h"
#include "ModelAsset.h"
#include "AssetFiles.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <stdexcept>
#include <unordered_map>
#include <windows.h>
#include <wincrypt.h>
#pragma comment(lib, "crypt32.lib")
#pragma warning(push, 0)
#define CGLTF_IMPLEMENTATION
#include "ThirdParty/cgltf.h"
#pragma warning(pop)

namespace ModelAssets
{
namespace
{
constexpr size_t MaximumFile = 256 * 1024 * 1024;
constexpr size_t MaximumVertices = 2000000;

void Require(bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

bool Finite(const float* values, size_t count, float limit = 1e6f)
{
    for (size_t i = 0; i < count; ++i)
    {
        if (!std::isfinite(values[i]) || std::abs(values[i]) > limit)
        {
            return false;
        }
    }
    return true;
}

cgltf_result ReadBuffer(const cgltf_memory_options*,
                        const cgltf_file_options*,
                        const char* path,
                        cgltf_size* size,
                        void** data)
{
    std::vector<unsigned char> bytes;
    if (!AssetFiles::Read(AssetFiles::Wide(path), bytes, MaximumFile) ||
        (*size && bytes.size() < *size))
    {
        return cgltf_result_io_error;
    }
    *data = std::malloc(bytes.size());
    if (!*data)
    {
        return cgltf_result_out_of_memory;
    }
    std::memcpy(*data, bytes.data(), bytes.size());
    *size = bytes.size();
    return cgltf_result_success;
}

void ReleaseBuffer(const cgltf_memory_options*, const cgltf_file_options*, void* data)
{
    std::free(data);
}

std::wstring Resource(const std::wstring& folder, const char* uri)
{
    std::string decoded = uri ? uri : "";
    if (decoded.empty())
    {
        return {};
    }
    // cgltf decodes percent escapes before opening an external buffer too.
    cgltf_decode_uri(&decoded[0]);
    decoded.resize(std::strlen(decoded.c_str()));
    return AssetFiles::Local(folder, decoded);
}

using Accessors = std::unordered_map<const cgltf_accessor*, std::vector<float>>;

const std::vector<float>& Floats(Accessors& cache, const cgltf_accessor* accessor)
{
    Require(accessor != nullptr, "필수 정점/키프레임 속성이 없습니다.");
    const auto found = cache.find(accessor);
    if (found != cache.end())
    {
        return found->second;
    }
    const size_t width = cgltf_num_components(accessor->type);
    Require(width && accessor->count <= 8000000 / width, "속성 데이터가 로더 한도를 초과했습니다.");
    auto& values = cache[accessor];
    values.resize(accessor->count * width);
    Require(cgltf_accessor_unpack_floats(accessor, values.data(), values.size()) == values.size() &&
                Finite(values.data(), values.size()),
            "정점/키프레임 속성에 잘못된 수치가 있습니다.");
    return values;
}

void Attribute(Accessors& cache,
               const cgltf_accessor* accessor,
               size_t vertex,
               float* out,
               size_t wanted,
               bool required = false)
{
    if (!accessor && !required)
    {
        return;
    }
    const auto& values = Floats(cache, accessor);
    const size_t width = cgltf_num_components(accessor->type);
    Require(width == wanted && vertex < accessor->count, "정점 속성의 형식/개수가 맞지 않습니다.");
    std::copy_n(values.data() + vertex * width, wanted, out);
}

void LoadImages(Asset& result, const cgltf_data& data, const std::wstring& folder)
{
    Require(data.textures_count <= 128, "텍스처 수가 128개를 초과했습니다.");
    size_t total = 0;
    for (size_t i = 0; i < data.textures_count; ++i)
    {
        const auto& texture = data.textures[i];
        Require(texture.image && !texture.has_basisu && !texture.has_webp,
                "PNG/JPEG 기본 텍스처만 지원합니다. KTX2/WebP는 변환 후 내보내세요.");
        Image image;
        if (texture.sampler)
        {
            image.wrapS = texture.sampler->wrap_s ? int(texture.sampler->wrap_s) : 10497;
            image.wrapT = texture.sampler->wrap_t ? int(texture.sampler->wrap_t) : 10497;
            image.minFilter = texture.sampler->min_filter ? int(texture.sampler->min_filter) : 9987;
            image.magFilter = texture.sampler->mag_filter ? int(texture.sampler->mag_filter) : 9729;
        }
        const auto& source = *texture.image;
        if (source.buffer_view)
        {
            Require(source.buffer_view->size <= 32 * 1024 * 1024,
                    "내장 이미지가 32MiB를 초과했습니다.");
            const auto bytes = cgltf_buffer_view_data(source.buffer_view);
            Require(bytes != nullptr, "내장 이미지 버퍼가 없습니다.");
            image.encoded.assign(bytes, bytes + source.buffer_view->size);
        }
        else if (source.uri && std::strncmp(source.uri, "data:", 5) == 0)
        {
            const char* comma = std::strchr(source.uri, ',');
            Require(comma && std::string(source.uri, size_t(comma - source.uri)).find(";base64") !=
                                 std::string::npos,
                    "이미지 data URI는 base64 형식이어야 합니다.");
            DWORD count = 0;
            Require(CryptStringToBinaryA(
                        comma + 1, 0, CRYPT_STRING_BASE64, nullptr, &count, nullptr, nullptr) &&
                        count && count <= 32 * 1024 * 1024,
                    "이미지 base64 형식/크기가 잘못됐습니다.");
            image.encoded.resize(count);
            Require(CryptStringToBinaryA(comma + 1,
                                         0,
                                         CRYPT_STRING_BASE64,
                                         image.encoded.data(),
                                         &count,
                                         nullptr,
                                         nullptr) != 0,
                    "이미지 base64를 읽지 못했습니다.");
        }
        else
        {
            const auto path = Resource(folder, source.uri);
            Require(!path.empty() && AssetFiles::Read(path, image.encoded, 32 * 1024 * 1024),
                    "외부 이미지를 읽지 못했거나 모델 폴더 밖을 참조합니다.");
        }
        total += image.encoded.size();
        Require(total <= 128 * 1024 * 1024, "전체 압축 이미지가 128MiB를 초과했습니다.");
        result.images.push_back(std::move(image));
    }
}

void LoadMaterials(Asset& result, const cgltf_data& data)
{
    result.materials.emplace_back();
    for (size_t i = 0; i < data.materials_count; ++i)
    {
        const auto& source = data.materials[i];
        Material material;
        if (source.has_pbr_metallic_roughness)
        {
            const auto& pbr = source.pbr_metallic_roughness;
            std::copy_n(pbr.base_color_factor, 4, material.color.begin());
            Require(!pbr.base_color_texture.has_transform && pbr.base_color_texture.texcoord == 0,
                    "색상 텍스처는 변환 없는 TEXCOORD_0으로 내보내세요.");
            if (pbr.base_color_texture.texture)
            {
                material.texture = int(pbr.base_color_texture.texture - data.textures);
            }
        }
        std::copy_n(source.emissive_factor, 3, material.emission.begin());
        if (source.has_emissive_strength)
        {
            for (auto& component : material.emission)
            {
                component *= source.emissive_strength.emissive_strength;
            }
        }
        material.unlit = source.unlit != 0;
        material.doubleSided = source.double_sided != 0;
        material.alphaMode = int(source.alpha_mode);
        material.alphaCutoff = source.alpha_cutoff;
        Require(Finite(material.color.data(), 4, 16) &&
                    Finite(material.emission.data(), 3, 10000) &&
                    std::isfinite(material.alphaCutoff),
                "재질 색상/발광 수치가 잘못됐습니다.");
        result.materials.push_back(material);
    }
}

void LoadNodes(Asset& result, const cgltf_data& data)
{
    result.nodes.resize(data.nodes_count);
    for (size_t i = 0; i < data.nodes_count; ++i)
    {
        const auto& source = data.nodes[i];
        auto& node = result.nodes[i];
        node.name = source.name ? source.name : "";
        node.parent = source.parent ? int(source.parent - data.nodes) : -1;
        node.matrixOnly = source.has_matrix != 0;
        if (source.mesh && source.mesh->primitives_count)
        {
            node.morphCount = unsigned(source.mesh->primitives[0].targets_count);
            Require(node.morphCount <= 4, "메시당 모프 타깃은 최대 4개입니다.");
            const auto weights = source.weights ? source.weights : source.mesh->weights;
            const size_t count = source.weights ? source.weights_count : source.mesh->weights_count;
            if (weights)
            {
                Require(count == node.morphCount && Finite(weights, count, 100),
                        "모프 기본 가중치가 잘못됐습니다.");
                std::copy_n(weights, count, node.morph.begin());
            }
        }
        if (node.matrixOnly)
        {
            std::copy_n(source.matrix, 16, node.matrix.begin());
            Require(Finite(node.matrix.data(), 16), "노드 행렬에 잘못된 수치가 있습니다.");
        }
        else
        {
            std::copy_n(source.translation, 3, node.bind.begin());
            std::copy_n(source.rotation, 4, node.bind.begin() + 3);
            std::copy_n(source.scale, 3, node.bind.begin() + 7);
            Require(Finite(node.bind.data(), 10), "노드 변환에 잘못된 수치가 있습니다.");
            float length = 0;
            for (int q = 3; q < 7; ++q)
            {
                length += node.bind[q] * node.bind[q];
            }
            Require(length > .0001f, "노드 회전 쿼터니언이 0입니다.");
            for (int q = 3; q < 7; ++q)
            {
                node.bind[q] /= std::sqrt(length);
            }
            Require(std::abs(node.bind[7] * node.bind[8] * node.bind[9]) > 1e-12f,
                    "크기가 0인 노드는 지원하지 않습니다.");
        }
    }
    // glTF node indices need not place parents before children.
    std::vector<unsigned char> visited(result.nodes.size());
    std::function<void(size_t, int)> visit = [&](size_t i, int depth)
    {
        Require(depth < 256 && visited[i] != 1, "노드 순환 참조/과도한 계층 깊이를 발견했습니다.");
        if (visited[i] == 2)
        {
            return;
        }
        visited[i] = 1;
        if (result.nodes[i].parent >= 0)
        {
            visit(size_t(result.nodes[i].parent), depth + 1);
        }
        visited[i] = 2;
        result.order.push_back(i);
    };
    for (size_t i = 0; i < result.nodes.size(); ++i)
    {
        visit(i, 0);
    }
}

void LoadSkins(Asset& result, const cgltf_data& data, Accessors& cache)
{
    for (size_t i = 0; i < data.skins_count; ++i)
    {
        const auto& source = data.skins[i];
        Require(source.joints_count && source.joints_count <= MaximumBones,
                "스킨당 본 수는 1~128개여야 합니다.");
        Skin skin;
        for (size_t j = 0; j < source.joints_count; ++j)
        {
            skin.nodes.push_back(int(source.joints[j] - data.nodes));
            Matrix inverse = Identity();
            if (source.inverse_bind_matrices)
            {
                const auto& values = Floats(cache, source.inverse_bind_matrices);
                Require(source.inverse_bind_matrices->type == cgltf_type_mat4 &&
                            source.inverse_bind_matrices->count == source.joints_count,
                        "역바인드 행렬 개수가 본 수와 다릅니다.");
                std::copy_n(values.data() + j * 16, 16, inverse.begin());
            }
            skin.inverseBind.push_back(inverse);
        }
        result.skins.push_back(std::move(skin));
    }
}

void LoadGeometry(Asset& result, const cgltf_data& data, Accessors& cache)
{
    std::vector<bool> active(data.nodes_count, false);
    const auto scene = data.scene ? data.scene : data.scenes_count ? &data.scenes[0] : nullptr;
    for (const auto i : result.order)
    {
        if (scene)
        {
            active[i] =
                std::find(scene->nodes, scene->nodes + scene->nodes_count, &data.nodes[i]) !=
                scene->nodes + scene->nodes_count;
            if (result.nodes[i].parent >= 0)
            {
                active[i] = active[i] || active[result.nodes[i].parent];
            }
        }
        else
        {
            active[i] = true;
        }
    }
    for (size_t node = 0; node < data.nodes_count; ++node)
    {
        const auto mesh = data.nodes[node].mesh;
        if (!active[node] || !mesh)
        {
            continue;
        }
        Require(!data.nodes[node].has_mesh_gpu_instancing,
                "GPU 인스턴싱 확장은 기본 메시로 내보내세요.");
        for (size_t p = 0; p < mesh->primitives_count; ++p)
        {
            const auto& source = mesh->primitives[p];
            Require(source.type == cgltf_primitive_type_triangles && source.targets_count <= 4 &&
                        !source.has_draco_mesh_compression,
                    "삼각형 메시와 최대 4개 모프 타깃만 지원합니다. Draco를 해제하고 내보내세요.");
            const auto positions = cgltf_find_accessor(&source, cgltf_attribute_type_position, 0);
            Require(positions && positions->count, "비어 있는 메시입니다.");
            const auto normals = cgltf_find_accessor(&source, cgltf_attribute_type_normal, 0);
            const auto colors = cgltf_find_accessor(&source, cgltf_attribute_type_color, 0);
            const auto uv = cgltf_find_accessor(&source, cgltf_attribute_type_texcoord, 0);
            const auto joints = cgltf_find_accessor(&source, cgltf_attribute_type_joints, 0);
            const auto weights = cgltf_find_accessor(&source, cgltf_attribute_type_weights, 0);
            const auto extraJoints = cgltf_find_accessor(&source, cgltf_attribute_type_joints, 1);
            const auto extraWeights = cgltf_find_accessor(&source, cgltf_attribute_type_weights, 1);
            const size_t count = source.indices ? source.indices->count : positions->count;
            Require(count && count % 3 == 0 && count <= MaximumVertices - result.vertices.size(),
                    "삼각형 정점 수가 잘못됐거나 200만 개를 초과했습니다.");
            Require(!source.indices ||
                        (!source.indices->is_sparse && source.indices->type == cgltf_type_scalar),
                    "희소 인덱스는 일반 인덱스로 내보내세요.");
            Primitive primitive;
            primitive.node = int(node);
            primitive.skin = data.nodes[node].skin ? int(data.nodes[node].skin - data.skins) : -1;
            // Catalog identities refer to Blender object names, not shared datablock names.
            primitive.name = data.nodes[node].name ? data.nodes[node].name
                             : mesh->name          ? mesh->name
                                                   : "";
            primitive.material = source.material ? int(source.material - data.materials) + 1 : 0;
            primitive.first = unsigned(result.vertices.size());
            primitive.count = unsigned(count);
            Require(primitive.skin < 0 || (joints && weights),
                    "스킨 메시의 본 인덱스/가중치가 없습니다.");
            Require((extraJoints == nullptr) == (extraWeights == nullptr),
                    "추가 본 인덱스와 가중치는 쌍으로 있어야 합니다.");
            Require(result.materials[primitive.material].texture < 0 || uv,
                    "텍스처 메시의 UV가 없습니다.");
            for (size_t i = 0; i < count; ++i)
            {
                const size_t index =
                    source.indices ? cgltf_accessor_read_index(source.indices, i) : i;
                Vertex vertex;
                Attribute(cache, positions, index, vertex.position, 3, true);
                Attribute(cache, normals, index, vertex.normal, 3);
                Attribute(cache, uv, index, vertex.uv, 2);
                if (colors)
                {
                    const size_t width = cgltf_num_components(colors->type);
                    Require(width == 3 || width == 4, "정점 색상은 VEC3/VEC4여야 합니다.");
                    Attribute(cache, colors, index, vertex.color, width);
                }
                if (primitive.skin >= 0)
                {
                    float ids[8] = {}, influence[8] = {};
                    Attribute(cache, joints, index, ids, 4, true);
                    Attribute(cache, weights, index, influence, 4, true);
                    Attribute(cache, extraJoints, index, ids + 4, 4);
                    Attribute(cache, extraWeights, index, influence + 4, 4);
                    std::array<int, 8> sorted = {{0, 1, 2, 3, 4, 5, 6, 7}};
                    for (int j = 0; j < 8; ++j)
                    {
                        Require(ids[j] >= 0 && ids[j] < result.skins[primitive.skin].nodes.size() &&
                                    std::floor(ids[j]) == ids[j] && influence[j] >= 0 &&
                                    influence[j] <= 1,
                                "유효하지 않은 본 인덱스/가중치입니다.");
                    }
                    std::stable_sort(sorted.begin(),
                                     sorted.end(),
                                     [&](int a, int b)
                                     {
                                         return influence[a] > influence[b];
                                     });
                    float total = 0;
                    for (int j = 0; j < 4; ++j)
                    {
                        vertex.joints[j] = ids[sorted[j]];
                        vertex.weights[j] = influence[sorted[j]];
                        total += vertex.weights[j];
                    }
                    Require(total > 1e-8f, "본 가중치 합계가 0입니다.");
                    for (auto& weight : vertex.weights)
                    {
                        weight /= total;
                    }
                }

                for (size_t target = 0; target < source.targets_count; ++target)
                {
                    const auto& morph = source.targets[target];
                    for (size_t attribute = 0; attribute < morph.attributes_count; ++attribute)
                    {
                        const auto& item = morph.attributes[attribute];
                        if (item.type == cgltf_attribute_type_position)
                        {
                            Attribute(
                                cache, item.data, index, vertex.morphPosition[target], 3, true);
                        }
                        else if (item.type == cgltf_attribute_type_normal)
                        {
                            Attribute(cache, item.data, index, vertex.morphNormal[target], 3, true);
                        }
                    }
                }
                result.vertices.push_back(vertex);
            }
            if (!normals)
            {
                for (size_t i = primitive.first; i < result.vertices.size(); i += 3)
                {
                    const auto a = result.vertices[i].position, b = result.vertices[i + 1].position,
                               c = result.vertices[i + 2].position;
                    const float u[] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
                    const float v[] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
                    float n[] = {u[1] * v[2] - u[2] * v[1],
                                 u[2] * v[0] - u[0] * v[2],
                                 u[0] * v[1] - u[1] * v[0]};
                    const float length = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
                    for (int j = 0; j < 3; ++j)
                    {
                        for (int axis = 0; axis < 3; ++axis)
                        {
                            result.vertices[i + j].normal[axis] =
                                length > 1e-8f ? n[axis] / length : float(axis == 1);
                        }
                    }
                }
            }
            result.primitives.push_back(std::move(primitive));
        }
    }
    Require(!result.vertices.empty(), "선택된 장면에 표시할 삼각형 메시가 없습니다.");
}

void LoadClips(Asset& result, const cgltf_data& data, Accessors& cache)
{
    size_t totalSamples = 0;
    for (size_t i = 0; i < data.animations_count; ++i)
    {
        const auto& source = data.animations[i];
        Clip clip;
        clip.name = source.name ? source.name : "clip_" + std::to_string(i);
        std::vector<std::array<bool, 4>> seen(data.nodes_count);
        for (size_t j = 0; j < source.channels_count; ++j)
        {
            const auto& channel = source.channels[j];
            Require(channel.target_node && channel.sampler &&
                        (channel.target_path == cgltf_animation_path_type_translation ||
                         channel.target_path == cgltf_animation_path_type_rotation ||
                         channel.target_path == cgltf_animation_path_type_scale ||
                         channel.target_path == cgltf_animation_path_type_weights),
                    "노드 변환/모프 가중치 애니메이션만 지원합니다.");
            Track track;
            track.node = int(channel.target_node - data.nodes);
            track.path =
                channel.target_path == cgltf_animation_path_type_translation
                    ? Track::Path::Translation
                : channel.target_path == cgltf_animation_path_type_rotation ? Track::Path::Rotation
                : channel.target_path == cgltf_animation_path_type_weights  ? Track::Path::Morph
                                                                            : Track::Path::Scale;
            const bool morph = track.path == Track::Path::Morph;
            Require(morph || !result.nodes[track.node].matrixOnly,
                    "행렬 노드를 애니메이션하려면 TRS로 내보내세요.");
            Require(!seen[track.node][int(track.path)],
                    "같은 클립에서 동일 노드/속성을 두 번 제어합니다.");
            seen[track.node][int(track.path)] = true;
            const auto& sampler = *channel.sampler;
            const size_t width = morph ? result.nodes[track.node].morphCount
                                 : track.path == Track::Path::Rotation ? 4
                                                                       : 3;
            const size_t accessorWidth = morph ? 1 : width;
            track.interpolation = sampler.interpolation == cgltf_interpolation_type_step
                                      ? Track::Interpolation::Step
                                  : sampler.interpolation == cgltf_interpolation_type_cubic_spline
                                      ? Track::Interpolation::Cubic
                                      : Track::Interpolation::Linear;
            Require(width && sampler.input && sampler.output &&
                        sampler.input->type == cgltf_type_scalar &&
                        cgltf_num_components(sampler.output->type) == accessorWidth &&
                        sampler.input->count,
                    "키프레임 입출력 형식이 잘못됐습니다.");
            track.times = Floats(cache, sampler.input);
            Require(track.times.front() >= 0 && track.times.back() <= 86400,
                    "키프레임 시간이 범위를 벗어났습니다.");
            for (size_t k = 1; k < track.times.size(); ++k)
            {
                Require(track.times[k] > track.times[k - 1], "키프레임 시간은 증가해야 합니다.");
            }
            const auto& values = Floats(cache, sampler.output);
            const size_t stride = track.interpolation == Track::Interpolation::Cubic ? 3 : 1;
            const size_t samples = track.times.size() * stride;
            Require(values.size() == samples * width, "키프레임 수가 맞지 않습니다.");
            totalSamples += samples;
            Require(totalSamples <= 4000000, "전체 키프레임 수가 로더 한도를 초과했습니다.");
            track.values.resize(samples);
            for (size_t k = 0; k < samples; ++k)
            {
                std::copy_n(values.data() + k * width, width, track.values[k].begin());
                if (track.path == Track::Path::Rotation && (stride == 1 || k % 3 == 1))
                {
                    float length = 0;
                    for (float value : track.values[k])
                    {
                        length += value * value;
                    }
                    Require(length > 1e-8f, "애니메이션 회전 쿼터니언이 0입니다.");
                    for (auto& value : track.values[k])
                    {
                        value /= std::sqrt(length);
                    }
                }
                if (track.path == Track::Path::Scale && (stride == 1 || k % 3 == 1))
                {
                    Require(std::abs(track.values[k][0] * track.values[k][1] * track.values[k][2]) >
                                1e-12f,
                            "크기가 0인 애니메이션 키는 지원하지 않습니다.");
                }
            }
            clip.duration = (std::max)(clip.duration, track.times.back());
            clip.tracks.push_back(std::move(track));
        }
        result.clips.push_back(std::move(clip));
    }
}
} // namespace

std::shared_ptr<Asset> Asset::Load(const std::wstring& path, std::string& error)
{
    error.clear();
    try
    {
        const auto resolved =
            path.find(L':') != std::wstring::npos ? path : AssetFiles::Resolve(path);
        std::vector<unsigned char> bytes;
        Require(!resolved.empty() && AssetFiles::Read(resolved, bytes, MaximumFile),
                "모델 파일을 읽지 못했습니다.");
        cgltf_options options = {};
        options.file.read = ReadBuffer;
        options.file.release = ReleaseBuffer;
        cgltf_data* raw = nullptr;
        Require(cgltf_parse(&options, bytes.data(), bytes.size(), &raw) == cgltf_result_success,
                "glTF 2.0/GLB 구문을 읽지 못했습니다.");
        std::unique_ptr<cgltf_data, decltype(&cgltf_free)> data(raw, cgltf_free);
        Require(data->nodes_count && data->nodes_count <= 8192 && data->skins_count <= 128 &&
                    data->animations_count <= 1024 && data->materials_count <= 4096,
                "모델 구성 요소 수가 로더 한도를 초과했습니다.");
        for (size_t i = 0; i < data->extensions_required_count; ++i)
        {
            const std::string name = data->extensions_required[i];
            Require(name == "KHR_materials_unlit" || name == "KHR_materials_emissive_strength" ||
                        name == "KHR_mesh_quantization",
                    "지원하지 않는 필수 glTF 확장입니다. 압축/특수 재질을 해제하고 내보내세요.");
        }
        const auto folder = AssetFiles::Directory(resolved);
        size_t total = 0;
        for (size_t i = 0; i < data->buffers_count; ++i)
        {
            const auto& buffer = data->buffers[i];
            Require(buffer.size <= MaximumFile - total, "전체 모델 버퍼가 256MiB를 초과했습니다.");
            total += buffer.size;
            if (buffer.uri && std::strncmp(buffer.uri, "data:", 5) != 0)
            {
                Require(!Resource(folder, buffer.uri).empty(),
                        "외부 버퍼가 모델 폴더 밖을 참조합니다.");
            }
        }
        Require(cgltf_load_buffers(&options, data.get(), AssetFiles::Utf8(resolved).c_str()) ==
                    cgltf_result_success,
                "모델 바이너리 버퍼를 읽지 못했습니다.");
        for (size_t i = 0; i < data->buffer_views_count; ++i)
        {
            Require(!data->buffer_views[i].has_meshopt_compression,
                    "Meshopt 압축을 해제하고 내보내세요.");
        }
        Require(cgltf_validate(data.get()) == cgltf_result_success,
                "glTF 참조/접근자 범위가 올바르지 않습니다.");
        auto result = std::make_shared<Asset>();
        Accessors cache;
        LoadNodes(*result, *data);
        LoadSkins(*result, *data, cache);
        LoadImages(*result, *data, folder);
        LoadMaterials(*result, *data);
        LoadGeometry(*result, *data, cache);
        LoadClips(*result, *data, cache);
        State bind;
        result->Reset(bind);
        std::array<float, 3> low, high;
        result->Bounds(bind, "", low, high);
        const float height = high[1] - low[1];
        Require(std::isfinite(height) && height > 1e-6f, "모델의 세로 크기가 0입니다.");
        const float scale = 1 / height;
        const float centerX = (low[0] + high[0]) * .5f, centerZ = (low[2] + high[2]) * .5f;
        // glTF Y-up -> existing Z-up world, feet at zero, height one.
        result->normalization = {{scale,
                                  0,
                                  0,
                                  0,
                                  0,
                                  0,
                                  scale,
                                  0,
                                  0,
                                  -scale,
                                  0,
                                  0,
                                  -centerX * scale,
                                  centerZ * scale,
                                  -low[1] * scale,
                                  1}};
        result->Bounds(bind, "", low, high);
        const float radius = std::sqrt((std::max)(low[0] * low[0], high[0] * high[0]) +
                                       (std::max)(low[1] * low[1], high[1] * high[1]));
        result->framing =
            (std::min)(.70f, 112.f / (std::max)(1.f, radius * 1.414214f * .85f * 180.f));
        return result;
    }
    catch (const std::exception& exception)
    {
        error = exception.what();
        return {};
    }
}
} // namespace ModelAssets
