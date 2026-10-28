#include "stdafx.h"
#include "CharacterModel.h"
#include "ModelCatalog.h"
#include "AssetFiles.h"
#include <wincodec.h>
#include <wrl/client.h>
#include <tuple>
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")
#include "FrameProfiler.h"
#include "ShaderFiles.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <iostream>
#include <utility>

namespace
{
struct MeshVertex
{
    float position[3], normal[3], color[4], joints[4], weights[4];
};

static_assert(sizeof(MeshVertex) == 72, "Character vertex format must remain packed float32");
static_assert(sizeof(ModelAssets::Vertex) == 176,
              "Imported vertex attributes must match GLSL locations 0..13");

struct Reader
{
    std::vector<unsigned char> bytes;
    size_t offset = 0;

    bool Read(void* destination, size_t size)
    {
        if (size > bytes.size() - offset)
            return false;
        std::memcpy(destination, bytes.data() + offset, size);
        offset += size;
        return true;
    }

    template <class T> bool Value(T& value)
    {
        return Read(&value, sizeof(value));
    }

    bool Open(const wchar_t* relative)
    {
        wchar_t executable[32768] = {};
        GetModuleFileNameW(nullptr, executable, 32768);
        const std::wstring path = executable;
        const auto slash = path.find_last_of(L"\\/");
        const std::wstring folder = slash == std::wstring::npos ? L"" : path.substr(0, slash + 1);
        const std::wstring candidates[] = {
            folder + relative, relative, std::wstring(L"SimpleGame/") + relative};
        for (const auto& candidate : candidates)
        {
            HANDLE file = CreateFileW(candidate.c_str(),
                                      GENERIC_READ,
                                      FILE_SHARE_READ,
                                      nullptr,
                                      OPEN_EXISTING,
                                      FILE_ATTRIBUTE_NORMAL,
                                      nullptr);
            if (file == INVALID_HANDLE_VALUE)
                continue;
            LARGE_INTEGER size = {};
            if (!GetFileSizeEx(file, &size) || size.QuadPart < 24 ||
                size.QuadPart > 64 * 1024 * 1024)
            {
                CloseHandle(file);
                return false;
            }
            bytes.resize(static_cast<size_t>(size.QuadPart));
            DWORD read = 0;
            const bool success =
                ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr) &&
                read == bytes.size();
            CloseHandle(file);
            return success;
        }
        return false;
    }
};

// Restore the 2D batch/HDR target exactly; the model owns only its small color/depth targets.
struct GraphicsState
{
    GLint draw = 0, read = 0, viewport[4] = {}, program = 0, vao = 0, array = 0;
    GLint active = 0, texture = 0, renderbuffer = 0, depthFunction = 0;
    GLint uniformBuffer = 0, jointBuffer = 0;
    GLint sourceRGB = 0, destinationRGB = 0, sourceAlpha = 0, destinationAlpha = 0;
    GLint equationRGB = 0, equationAlpha = 0;
    GLboolean blend = false, depth = false, cull = false, scissor = false, stencil = false;
    GLboolean multisample = false, srgb = false, depthWrite = true, colorWrite[4] = {};
    GLfloat clearColor[4] = {};
    GLdouble clearDepth = 1, depthRange[2] = {};

    GraphicsState()
    {
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read);
        glGetIntegerv(GL_VIEWPORT, viewport);
        glGetIntegerv(GL_CURRENT_PROGRAM, &program);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &array);
        glGetIntegerv(GL_RENDERBUFFER_BINDING, &renderbuffer);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
        glActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
        glGetIntegerv(GL_DEPTH_FUNC, &depthFunction);
        glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &uniformBuffer);
        glGetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, 5, &jointBuffer);
        glGetBooleanv(GL_DEPTH_WRITEMASK, &depthWrite);
        glGetBooleanv(GL_COLOR_WRITEMASK, colorWrite);
        glGetDoublev(GL_DEPTH_RANGE, depthRange);
        glGetDoublev(GL_DEPTH_CLEAR_VALUE, &clearDepth);
        glGetFloatv(GL_COLOR_CLEAR_VALUE, clearColor);
        glGetIntegerv(GL_BLEND_SRC_RGB, &sourceRGB);
        glGetIntegerv(GL_BLEND_DST_RGB, &destinationRGB);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &sourceAlpha);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &destinationAlpha);
        glGetIntegerv(GL_BLEND_EQUATION_RGB, &equationRGB);
        glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &equationAlpha);
        blend = glIsEnabled(GL_BLEND);
        depth = glIsEnabled(GL_DEPTH_TEST);
        cull = glIsEnabled(GL_CULL_FACE);
        scissor = glIsEnabled(GL_SCISSOR_TEST);
        stencil = glIsEnabled(GL_STENCIL_TEST);
        multisample = glIsEnabled(GL_MULTISAMPLE);
        srgb = glIsEnabled(GL_FRAMEBUFFER_SRGB);
    }

    static void Enable(GLenum capability, bool enabled)
    {
        if (enabled)
            glEnable(capability);
        else
            glDisable(capability);
    }

    ~GraphicsState()
    {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, read);
        glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        glUseProgram(program);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, array);
        glBindBufferBase(GL_UNIFORM_BUFFER, 5, jointBuffer);
        glBindBuffer(GL_UNIFORM_BUFFER, uniformBuffer);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glActiveTexture(active);
        glDepthFunc(depthFunction);
        glDepthMask(depthWrite);
        glColorMask(colorWrite[0], colorWrite[1], colorWrite[2], colorWrite[3]);
        glDepthRange(depthRange[0], depthRange[1]);
        glClearDepth(clearDepth);
        glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
        glBlendFuncSeparate(sourceRGB, destinationRGB, sourceAlpha, destinationAlpha);
        glBlendEquationSeparate(equationRGB, equationAlpha);
        Enable(GL_BLEND, blend);
        Enable(GL_DEPTH_TEST, depth);
        Enable(GL_CULL_FACE, cull);
        Enable(GL_SCISSOR_TEST, scissor);
        Enable(GL_STENCIL_TEST, stencil);
        Enable(GL_MULTISAMPLE, multisample);
        Enable(GL_FRAMEBUFFER_SRGB, srgb);
    }
};

bool Finite(const float* values, size_t count, float limit)
{
    for (size_t i = 0; i < count; ++i)
        if (!std::isfinite(values[i]) || std::abs(values[i]) > limit)
            return false;
    return true;
}
} // namespace

CharacterModel::~CharacterModel()
{
    Release();
}

void CharacterModel::Release()
{
    glDeleteBuffers(1, &buffer);
    glDeleteBuffers(1, &paletteBuffer);
    if (!textures.empty())
    {
        glDeleteTextures(static_cast<GLsizei>(textures.size()), textures.data());
    }
    paletteBuffer = 0;
    textures.clear();
    imported.reset();
    importedDraws.clear();
    drawOrderScratch.clear();
    paletteScratch.clear();
    importedAmbient.clear();
    glDeleteVertexArrays(1, &vao);
    glDeleteFramebuffers(1, &framebuffer);
    glDeleteFramebuffers(1, &resolveFramebuffer);
    glDeleteRenderbuffers(1, &colorBuffer);
    glDeleteRenderbuffers(1, &depthBuffer);
    glDeleteTextures(1, &image);
    if (meshProgram)
        glDeleteProgram(meshProgram);
    if (compositeProgram)
        glDeleteProgram(compositeProgram);
    buffer = vao = framebuffer = resolveFramebuffer = colorBuffer = depthBuffer = image = 0;
    meshProgram = compositeProgram = 0;
    vertexCount = 0;
    joints.clear();
    defaultState = State();
    parts.clear();
    framing = 1;
    for (auto& clip : clips)
        clip = {};
}

bool CharacterModel::Ready() const
{
    return vao && buffer && meshProgram && compositeProgram && framebuffer && resolveFramebuffer &&
           image && vertexCount > 0 && (imported ? paletteBuffer != 0 : !joints.empty());
}

bool CharacterModel::Load(const wchar_t* path)
{
    Release();
    const auto start = FrameProfiler::Clock::now();
    Reader reader;
    char magic[8] = {};
    std::uint32_t version = 0, vertices = 0, boneCount = 0, clipCount = 0;
    if (!reader.Open(path) || !reader.Read(magic, sizeof(magic)) ||
        std::memcmp(magic, "CHAR3D01", 8) || !reader.Value(version) ||
        (version != 1 && version != 2) || !reader.Value(vertices) || vertices < 3 ||
        vertices > 600000 || vertices % 3 || !reader.Value(boneCount) || !boneCount ||
        boneCount > 32 || !reader.Value(clipCount) ||
        clipCount != (version == 1 ? 4u : static_cast<unsigned>(clips.size())))
        return false;
    std::vector<MeshPart> loadedParts;
    if (version == 2)
    {
        unsigned count = 0, covered = 0;
        if (!reader.Value(framing) || !std::isfinite(framing) || framing < .05f || framing > 1 ||
            !reader.Value(count) || !count || count > 64)
        {
            return false;
        }
        for (unsigned i = 0; i < count; ++i)
        {
            MeshPart part;
            if (!reader.Value(part.slot) || !reader.Value(part.index) ||
                !reader.Value(part.first) || !reader.Value(part.count) || part.slot < -1 ||
                part.slot > 3 || part.index < 0 || part.index > 63 || part.first != covered ||
                !part.count || part.count % 3 || part.count > vertices - covered)
            {
                return false;
            }
            covered += part.count;
            loadedParts.push_back(part);
        }
        if (covered != vertices)
        {
            return false;
        }
    }
    std::vector<Joint> loadedJoints(boneCount);
    for (size_t i = 0; i < loadedJoints.size(); ++i)
    {
        auto& joint = loadedJoints[i];
        std::int32_t parent = -1;
        if (!reader.Value(parent) || parent < -1 || parent >= int(i) ||
            !reader.Read(joint.inverseBind.data(), sizeof(Matrix)) ||
            !Finite(joint.inverseBind.data(), 16, 100))
            return false;
        joint.parent = parent;
    }
    std::vector<MeshVertex> loadedVertices(vertices);
    if (!reader.Read(loadedVertices.data(), loadedVertices.size() * sizeof(MeshVertex)))
        return false;
    for (const auto& vertex : loadedVertices)
    {
        if (!Finite(vertex.position, 3, 10) || !Finite(vertex.normal, 3, 2) ||
            !Finite(vertex.color, 4, 16))
            return false;
        float total = 0;
        for (int i = 0; i < 4; ++i)
        {
            if (!std::isfinite(vertex.joints[i]) || vertex.joints[i] < 0 ||
                vertex.joints[i] >= boneCount || std::floor(vertex.joints[i]) != vertex.joints[i] ||
                !std::isfinite(vertex.weights[i]) || vertex.weights[i] < 0 || vertex.weights[i] > 1)
                return false;
            total += vertex.weights[i];
        }
        if (std::abs(total - 1) > .001f)
            return false;
    }
    std::array<Clip, static_cast<size_t>(Motion::Count)> loadedClips;
    const char* names[] = {
        "idle", "walk", "run", "cast", "jump", "turn_left", "turn_right", "attack"};
    for (size_t i = 0; i < clipCount; ++i)
    {
        char name[16] = {};
        auto& clip = loadedClips[i];
        std::uint32_t frames = 0;
        if (!reader.Read(name, sizeof(name)) ||
            std::string(name, std::find(name, name + 16, '\0')) != names[i] ||
            !reader.Value(frames) || frames < 2 || frames > 120)
            return false;
        clip.frames = frames;
        clip.poses.resize(size_t(frames) * boneCount);
        if (!reader.Read(clip.poses.data(), clip.poses.size() * sizeof(Pose)))
            return false;
        for (const auto& sample : clip.poses)
        {
            if (!Finite(sample.data(), sample.size(), 10))
                return false;
            const float length = sample[3] * sample[3] + sample[4] * sample[4] +
                                 sample[5] * sample[5] + sample[6] * sample[6];
            if (std::abs(length - 1) > .01f || sample[7] <= 0 || sample[8] <= 0 || sample[9] <= 0)
                return false;
        }
    }
    for (size_t i = clipCount; i < loadedClips.size(); ++i)
    {
        loadedClips[i] = loadedClips[i == static_cast<size_t>(Motion::Attack) ? 3 : 0];
    }
    if (reader.offset != reader.bytes.size())
        return false;
    GraphicsState state;
    if (!CreatePrograms(false))
    {
        Release();
        return false;
    }
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &buffer);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBufferData(GL_ARRAY_BUFFER,
                 loadedVertices.size() * sizeof(MeshVertex),
                 loadedVertices.data(),
                 GL_STATIC_DRAW);
    const int widths[] = {3, 3, 4, 4, 4};
    size_t offset = 0;
    for (GLuint i = 0; i < 5; ++i)
    {
        glEnableVertexAttribArray(i);
        glVertexAttribPointer(i,
                              widths[i],
                              GL_FLOAT,
                              GL_FALSE,
                              sizeof(MeshVertex),
                              reinterpret_cast<void*>(offset * sizeof(float)));
        offset += widths[i];
    }
    joints = std::move(loadedJoints);
    clips = std::move(loadedClips);
    parts = std::move(loadedParts);
    vertexCount = static_cast<GLsizei>(vertices);
    Reset();
    FrameProfiler::Upload(loadedVertices.size() * sizeof(MeshVertex));
    FrameProfiler::Event("character_mesh_loaded",
                         "protagonist",
                         reader.bytes.size(),
                         FrameProfiler::Milliseconds(start));
    return Ready();
}

bool CharacterModel::CreatePrograms(bool gltf)
{
    meshProgram =
        ShaderFiles::Program(gltf ? L"Shaders/ImportedMesh.vs" : L"Shaders/CharacterMesh.vs",
                             gltf ? L"Shaders/ImportedMesh.fs" : L"Shaders/CharacterMesh.fs");
    compositeProgram =
        ShaderFiles::Program(L"Shaders/CharacterComposite.vs", L"Shaders/CharacterComposite.fs");
    if (!meshProgram || !compositeProgram || !CreateTargets())
    {
        return false;
    }
    bonesUniform = glGetUniformLocation(meshProgram, "bones[0]");
    yawUniform = glGetUniformLocation(meshProgram, "heading");
    viewportUniform = glGetUniformLocation(compositeProgram, "viewport");
    feetUniform = glGetUniformLocation(compositeProgram, "feet");
    anchorUniform = glGetUniformLocation(compositeProgram, "imageAnchor");
    tintUniform = glGetUniformLocation(compositeProgram, "tint");
    linearUniform = glGetUniformLocation(compositeProgram, "linearScene");
    framingUniform = glGetUniformLocation(meshProgram, "modelScale");
    drawScaleUniform = glGetUniformLocation(compositeProgram, "drawScale");
    styleUniform = glGetUniformLocation(meshProgram, "fourBand");
    glUseProgram(compositeProgram);
    glUniform1i(glGetUniformLocation(compositeProgram, "characterImage"), 0);
    return true;
}

namespace
{
bool DecodeImage(const ModelAssets::Image& source,
                 std::vector<unsigned char>& pixels,
                 UINT& width,
                 UINT& height)
{
    struct Apartment
    {
        HRESULT status = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

        ~Apartment()
        {
            if (SUCCEEDED(status))
            {
                CoUninitialize();
            }
        }
    } apartment;

    if (FAILED(apartment.status) && apartment.status != RPC_E_CHANGED_MODE)
    {
        return false;
    }
    using Microsoft::WRL::ComPtr;
    ComPtr<IWICImagingFactory> factory;
    ComPtr<IWICStream> stream;
    ComPtr<IWICBitmapDecoder> decoder;
    ComPtr<IWICBitmapFrameDecode> frame;
    ComPtr<IWICFormatConverter> converter;
    if (FAILED(CoCreateInstance(
            CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))) ||
        FAILED(factory->CreateStream(&stream)) ||
        FAILED(stream->InitializeFromMemory(const_cast<BYTE*>(source.encoded.data()),
                                            DWORD(source.encoded.size()))) ||
        FAILED(factory->CreateDecoderFromStream(
            stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, &decoder)) ||
        FAILED(decoder->GetFrame(0, &frame)) || FAILED(frame->GetSize(&width, &height)) || !width ||
        !height || width > 4096 || height > 4096 ||
        FAILED(factory->CreateFormatConverter(&converter)) ||
        FAILED(converter->Initialize(frame.Get(),
                                     GUID_WICPixelFormat32bppRGBA,
                                     WICBitmapDitherTypeNone,
                                     nullptr,
                                     0,
                                     WICBitmapPaletteTypeCustom)))
    {
        return false;
    }
    pixels.resize(size_t(width) * height * 4);
    return SUCCEEDED(converter->CopyPixels(nullptr, width * 4, UINT(pixels.size()), pixels.data()));
}
} // namespace

bool CharacterModel::LoadGltf(const wchar_t* path,
                              const ModelAssets::Catalog* catalog,
                              const std::map<std::string, std::string>* bindings,
                              const std::vector<std::string>* ambientClips)
{
    Release();
    const auto start = FrameProfiler::Clock::now();
    std::string error;
    auto asset = ModelAssets::Asset::Load(path, error);
    if (!asset || (catalog && !catalog->ConfigureParts(*asset, error)))
    {
        std::cerr << "GLB 모델 로드 실패: " << AssetFiles::Utf8(path) << " · " << error << '\n';
        return false;
    }
    GraphicsState restore;
    imported = std::move(asset);
    framing = imported->framing;
    if (!CreatePrograms(true))
    {
        std::cerr << "GLB 셰이더 또는 렌더 타깃 생성 실패: " << AssetFiles::Utf8(path) << '\n';
        Release();
        return false;
    }
    // Skinned primitives sharing skin/material/part can use one draw, independent of mesh node.
    using Key = std::tuple<int, int, int, int, int>;
    std::map<Key, std::vector<size_t>> groups;
    auto bakedColor = [&](int material)
    {
        const auto& m = imported->materials[material];
        return m.texture < 0 && m.alphaMode == 0 && !m.unlit && m.emission[0] == 0 &&
               m.emission[1] == 0 && m.emission[2] == 0;
    };
    for (size_t i = 0; i < imported->primitives.size(); ++i)
    {
        const auto& p = imported->primitives[i];
        groups[{p.skin,
                p.skin < 0 || imported->nodes[p.node].morphCount ? p.node : -1,
                bakedColor(p.material) ? 0 : p.material,
                p.slot,
                p.index}]
            .push_back(i);
    }
    std::vector<ModelAssets::Vertex> vertices;
    vertices.reserve(imported->vertices.size());
    for (const auto& group : groups)
    {
        auto draw = imported->primitives[group.second.front()];
        draw.material = std::get<2>(group.first);
        draw.first = unsigned(vertices.size());
        draw.count = 0;
        for (const auto i : group.second)
        {
            const auto& p = imported->primitives[i];
            vertices.insert(vertices.end(),
                            imported->vertices.begin() + p.first,
                            imported->vertices.begin() + p.first + p.count);
            if (bakedColor(p.material))
            {
                const auto& color = imported->materials[p.material].color;
                for (size_t vertex = vertices.size() - p.count; vertex < vertices.size(); ++vertex)
                {
                    for (size_t channel = 0; channel < 4; ++channel)
                    {
                        vertices[vertex].color[channel] *= color[channel];
                    }
                }
            }
            draw.count += p.count;
        }
        importedDraws.push_back(draw);
    }
    // Material classes do not change with the pose. Only transparent depth order is dynamic.
    std::stable_partition(importedDraws.begin(),
                          importedDraws.end(),
                          [&](const ModelAssets::Primitive& draw)
                          {
                              return imported->materials[draw.material].alphaMode != 2;
                          });
    drawOrderScratch.reserve(importedDraws.size());
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &buffer);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBufferData(GL_ARRAY_BUFFER,
                 vertices.size() * sizeof(ModelAssets::Vertex),
                 vertices.data(),
                 GL_STATIC_DRAW);
    GLint64 uploaded = 0;
    glGetBufferParameteri64v(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &uploaded);
    if (uploaded != static_cast<GLint64>(vertices.size() * sizeof(ModelAssets::Vertex)))
    {
        std::cerr << "GLB 정점 버퍼 할당 실패: " << AssetFiles::Utf8(path) << '\n';
        Release();
        return false;
    }
    const int widths[] = {3, 3, 4, 4, 4, 2, 3, 3, 3, 3, 3, 3, 3, 3};
    size_t offset = 0;
    for (GLuint i = 0; i < 14; ++i)
    {
        glEnableVertexAttribArray(i);
        glVertexAttribPointer(i,
                              widths[i],
                              GL_FLOAT,
                              GL_FALSE,
                              sizeof(ModelAssets::Vertex),
                              reinterpret_cast<void*>(offset * sizeof(float)));
        offset += widths[i];
    }
    vertexCount = static_cast<GLsizei>(vertices.size());
    const GLuint block = glGetUniformBlockIndex(meshProgram, "JointPalette");
    if (block == GL_INVALID_INDEX)
    {
        Release();
        return false;
    }
    glUniformBlockBinding(meshProgram, block, 5);
    glGenBuffers(1, &paletteBuffer);
    glBindBuffer(GL_UNIFORM_BUFFER, paletteBuffer);
    glBufferData(GL_UNIFORM_BUFFER,
                 ModelAssets::MaximumBones * sizeof(ModelAssets::Matrix),
                 nullptr,
                 GL_STREAM_DRAW);
    glGetBufferParameteri64v(GL_UNIFORM_BUFFER, GL_BUFFER_SIZE, &uploaded);
    if (uploaded != static_cast<GLint64>(ModelAssets::MaximumBones * sizeof(ModelAssets::Matrix)))
    {
        std::cerr << "GLB 본 버퍼 할당 실패: " << AssetFiles::Utf8(path) << '\n';
        Release();
        return false;
    }
    objectUniform = glGetUniformLocation(meshProgram, "objectTransform");
    normalizationUniform = glGetUniformLocation(meshProgram, "normalization");
    skinnedUniform = glGetUniformLocation(meshProgram, "skinned");
    materialUniform = glGetUniformLocation(meshProgram, "materialColor");
    emissionUniform = glGetUniformLocation(meshProgram, "emission");
    textureUniform = glGetUniformLocation(meshProgram, "hasTexture");
    alphaUniform = glGetUniformLocation(meshProgram, "alphaMode");
    cutoffUniform = glGetUniformLocation(meshProgram, "alphaCutoff");
    unlitUniform = glGetUniformLocation(meshProgram, "unlit");
    morphUniform = glGetUniformLocation(meshProgram, "morphWeights");
    glUseProgram(meshProgram);
    glUniform1i(glGetUniformLocation(meshProgram, "baseTexture"), 0);
    glActiveTexture(GL_TEXTURE0);
    size_t decodedBytes = 0;
    for (const auto& source : imported->images)
    {
        std::vector<unsigned char> pixels;
        UINT width = 0, height = 0;
        if (!DecodeImage(source, pixels, width, height) ||
            pixels.size() > 256 * 1024 * 1024 - decodedBytes)
        {
            std::cerr << "GLB 텍스처 해독 실패/메모리 한도 초과: " << AssetFiles::Utf8(path)
                      << '\n';
            Release();
            return false;
        }
        decodedBytes += pixels.size();
        GLint maximum = 0;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximum);
        if (width > UINT(maximum) || height > UINT(maximum))
        {
            Release();
            return false;
        }
        GLuint texture = 0;
        glGenTextures(1, &texture);
        textures.push_back(texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D,
                     0,
                     GL_SRGB8_ALPHA8,
                     width,
                     height,
                     0,
                     GL_RGBA,
                     GL_UNSIGNED_BYTE,
                     pixels.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, source.minFilter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, source.magFilter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, source.wrapS);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, source.wrapT);
        glGenerateMipmap(GL_TEXTURE_2D);
        FrameProfiler::Upload(pixels.size());
    }
    const char* names[] = {
        "idle", "walk", "run", "cast", "jump", "turn_left", "turn_right", "attack"};
    for (size_t i = 0; i < importedClips.size(); ++i)
    {
        std::string name = names[i];
        if (bindings)
        {
            const auto binding = bindings->find(name);
            if (binding != bindings->end())
            {
                name = binding->second;
            }
        }
        importedClips[i] = imported->FindClip(name);
    }
    const int idle = importedClips[0];
    for (size_t i = 0; i < importedClips.size(); ++i)
    {
        if (importedClips[i] < 0)
        {
            const int attack = importedClips[static_cast<size_t>(Motion::Attack)];
            importedClips[i] =
                i == static_cast<size_t>(Motion::Cast) && attack >= 0 ? attack : idle;
        }
    }
    if (ambientClips)
    {
        for (const auto& name : *ambientClips)
        {
            const int clip = imported->FindClip(name);
            if (clip >= 0)
            {
                importedAmbient.push_back(clip);
            }
            else
            {
                std::cerr << "GLB 보조 애니메이션을 찾지 못했습니다: " << name << '\n';
            }
        }
    }
    Reset();
    FrameProfiler::Upload(vertices.size() * sizeof(ModelAssets::Vertex));
    FrameProfiler::Event("gltf_model_loaded",
                         AssetFiles::Utf8(path),
                         vertices.size() * sizeof(ModelAssets::Vertex),
                         FrameProfiler::Milliseconds(start));
    // Only static scenery needs a CPU vertex copy after upload.
    std::vector<ModelAssets::Vertex>().swap(imported->vertices);
    for (auto& source : imported->images)
    {
        std::vector<unsigned char>().swap(source.encoded);
    }
    return Ready();
}

bool CharacterModel::Play(
    State& state, const std::string& clip, bool loop, float speed, float fadeSeconds)
{
    if (!imported || !Ready())
    {
        return false;
    }
    if (state.owner != this)
    {
        Reset(state, state.yaw);
    }
    const bool started = imported->Play(state.imported, clip, loop, speed, fadeSeconds);
    state.namedClip = started || state.namedClip;
    return started;
}

void CharacterModel::ResumeLocomotion(State& state)
{
    state.namedClip = false;
    state.transition = 0;
    state.imported.from = state.imported.pose;
    state.imported.fromMorph = state.imported.morph;
}

void CharacterModel::AnimateGltf(State& state, float dt, Motion kind, float phase)
{
    if (state.namedClip)
    {
        imported->Update(state.imported, dt);
        return;
    }
    const int clip = importedClips[static_cast<size_t>(kind)];
    state.imported.ambientSeconds = std::fmod(state.imported.ambientSeconds + dt, 86400.f);
    const bool loop = kind == Motion::Idle || kind == Motion::Walk || kind == Motion::Run;
    const float fraction =
        loop ? phase - std::floor(phase) : (std::max)(0.f, (std::min)(1.f, phase));
    const float seconds = clip < 0 ? 0 : fraction * imported->clips[clip].duration;
    const float blend = state.transition * state.transition * (3 - 2 * state.transition);
    imported->Sample(state.imported, clip, seconds, blend);
}

void CharacterModel::DrawGltf(const State& state, const Parts* selection)
{
    glUniformMatrix4fv(normalizationUniform, 1, GL_FALSE, imported->normalization.data());
    glBindBufferBase(GL_UNIFORM_BUFFER, 5, paletteBuffer);
    glBindBuffer(GL_UNIFORM_BUFFER, paletteBuffer);
    auto& palette = paletteScratch;
    // Opaque and masked geometry writes depth first. Transparent surfaces follow back-to-front.
    auto& order = drawOrderScratch;
    order.clear();
    size_t opaqueCount = 0;
    for (size_t i = 0; i < importedDraws.size(); ++i)
    {
        const auto& draw = importedDraws[i];
        if (draw.slot >= 0 && selection && (*selection)[draw.slot] != draw.index)
        {
            continue;
        }
        order.push_back(i);
        opaqueCount += imported->materials[draw.material].alphaMode != 2;
    }
    auto depth = [&](const ModelAssets::Primitive& p)
    {
        const auto& m = state.imported.global[p.node];
        const float point[] = {m[12], m[13], m[14]};
        const auto world = ModelAssets::Point(imported->normalization, point);
        const float x = world[0] * std::cos(state.yaw) - world[1] * std::sin(state.yaw);
        const float y = world[0] * std::sin(state.yaw) + world[1] * std::cos(state.yaw);
        return .603f * (x + y) + .519f * world[2];
    };
    std::stable_sort(order.begin() + opaqueCount,
                     order.end(),
                     [&](size_t a, size_t b)
                     {
                         return depth(importedDraws[a]) < depth(importedDraws[b]);
                     });
    int lastSkin = -2;
    glActiveTexture(GL_TEXTURE0);
    for (const auto i : order)
    {
        const auto& draw = importedDraws[i];
        const auto& material = imported->materials[draw.material];
        const bool alpha = material.alphaMode == 2;
        GraphicsState::Enable(GL_BLEND, alpha);
        glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
        glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(alpha ? GL_FALSE : GL_TRUE);
        glUniform1i(skinnedUniform, draw.skin >= 0);
        glUniform4fv(morphUniform, 1, state.imported.morph[draw.node].data());
        if (draw.skin >= 0 && draw.skin != lastSkin)
        {
            imported->Palette(state.imported, draw.skin, palette);
            glBufferSubData(
                GL_UNIFORM_BUFFER, 0, palette.size() * sizeof(ModelAssets::Matrix), palette.data());
            FrameProfiler::Upload(palette.size() * sizeof(ModelAssets::Matrix));
            lastSkin = draw.skin;
        }
        glUniformMatrix4fv(objectUniform, 1, GL_FALSE, state.imported.global[draw.node].data());
        glUniform4fv(materialUniform, 1, material.color.data());
        glUniform3fv(emissionUniform, 1, material.emission.data());
        glUniform1i(textureUniform, material.texture >= 0);
        glUniform1i(alphaUniform, material.alphaMode);
        glUniform1f(cutoffUniform, material.alphaCutoff);
        glUniform1i(unlitUniform, material.unlit);
        glBindTexture(GL_TEXTURE_2D, material.texture >= 0 ? textures[material.texture] : 0);
        FrameProfiler::DrawArrays(GL_TRIANGLES, draw.first, draw.count);
    }
}

bool CharacterModel::CreateTargets()
{
    glGenFramebuffers(1, &framebuffer);
    glGenFramebuffers(1, &resolveFramebuffer);
    glGenRenderbuffers(1, &colorBuffer);
    glGenRenderbuffers(1, &depthBuffer);
    glGenTextures(1, &image);
    glBindTexture(GL_TEXTURE_2D, image);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 256, 320, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindFramebuffer(GL_FRAMEBUFFER, resolveFramebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, image, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        return false;
    GLint maximum = 0;
    glGetIntegerv(GL_MAX_SAMPLES, &maximum);
    int samples = (std::max)(1, (std::min)(4, int(maximum)));
    for (;;)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, colorBuffer);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_RGBA16F, 256, 320);
        glFramebufferRenderbuffer(
            GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorBuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH_COMPONENT24, 256, 320);
        glFramebufferRenderbuffer(
            GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthBuffer);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE)
        {
            FrameProfiler::Event("character_msaa", "protagonist", samples);
            return true;
        }
        if (samples == 1)
            return false;
        samples = 1;
    }
}

CharacterModel::Matrix CharacterModel::Multiply(const Matrix& a, const Matrix& b)
{
    Matrix result = {};
    for (int column = 0; column < 4; ++column)
        for (int row = 0; row < 4; ++row)
            for (int k = 0; k < 4; ++k)
                result[column * 4 + row] += a[k * 4 + row] * b[column * 4 + k];
    return result;
}

CharacterModel::Matrix CharacterModel::Transform(const Pose& p)
{
    const float x = p[3], y = p[4], z = p[5], w = p[6];
    return {{(1 - 2 * (y * y + z * z)) * p[7],
             2 * (x * y + z * w) * p[7],
             2 * (x * z - y * w) * p[7],
             0,
             2 * (x * y - z * w) * p[8],
             (1 - 2 * (x * x + z * z)) * p[8],
             2 * (y * z + x * w) * p[8],
             0,
             2 * (x * z + y * w) * p[9],
             2 * (y * z - x * w) * p[9],
             (1 - 2 * (x * x + y * y)) * p[9],
             0,
             p[0],
             p[1],
             p[2],
             1}};
}

CharacterModel::Pose CharacterModel::Blend(const Pose& a, const Pose& b, float weight)
{
    Pose result;
    for (size_t i = 0; i < result.size(); ++i)
        result[i] = a[i] + (b[i] - a[i]) * weight;
    float dot = 0;
    for (int i = 3; i <= 6; ++i)
        dot += a[i] * b[i];
    const float sign = dot < 0 ? -1.f : 1.f;
    dot = (std::min)(1.f, std::abs(dot));
    float from = 1 - weight, to = weight;
    if (dot < .9995f)
    {
        const float angle = std::acos(dot), sine = std::sin(angle);
        from = std::sin((1 - weight) * angle) / sine;
        to = std::sin(weight * angle) / sine;
    }
    float length = 0;
    for (int i = 3; i <= 6; ++i)
    {
        result[i] = a[i] * from + b[i] * to * sign;
        length += result[i] * result[i];
    }
    const float inverse = 1 / std::sqrt((std::max)(.000001f, length));
    for (int i = 3; i <= 6; ++i)
        result[i] *= inverse;
    return result;
}

CharacterModel::Pose CharacterModel::Sample(Motion kind, size_t bone, float phase) const
{
    const auto& clip = clips[static_cast<size_t>(kind)];
    const bool once = kind != Motion::Idle && kind != Motion::Walk && kind != Motion::Run;
    const float sample = once ? (std::max)(0.f, (std::min)(1.f, phase)) * (clip.frames - 1)
                              : (phase - std::floor(phase)) * clip.frames;
    const unsigned first = static_cast<unsigned>(sample);
    const unsigned next = once ? (std::min)(first + 1, clip.frames - 1) : (first + 1) % clip.frames;
    return Blend(clip.poses[size_t(first) * joints.size() + bone],
                 clip.poses[size_t(next) * joints.size() + bone],
                 sample - first);
}

void CharacterModel::UpdateSkin(State& state) const
{
    std::array<Matrix, 32> global;
    state.skin.resize(joints.size());
    for (size_t i = 0; i < joints.size(); ++i)
    {
        const Matrix local = Transform(state.pose[i]);
        global[i] = joints[i].parent < 0 ? local : Multiply(global[joints[i].parent], local);
        state.skin[i] = Multiply(global[i], joints[i].inverseBind);
    }
}

void CharacterModel::Reset(float facing)
{
    Reset(defaultState, facing);
}

void CharacterModel::Reset(State& state, float facing)
{
    state = State();
    state.owner = this;
    state.yaw = facing;
    if (imported)
    {
        imported->Reset(state.imported);
        state.imported.layers = importedAmbient;
        AnimateGltf(state, 0, Motion::Idle, 0);
        return;
    }
    if (joints.empty())
        return;
    state.pose.resize(joints.size());
    for (size_t i = 0; i < state.pose.size(); ++i)
        state.pose[i] = Sample(Motion::Idle, i, 0);
    state.transitionFrom = state.pose;
    UpdateSkin(state);
}

void CharacterModel::Animate(float dt, Motion kind, float phase, float facing)
{
    Animate(defaultState, dt, kind, phase, facing);
}

void CharacterModel::Animate(State& state, float dt, Motion kind, float phase, float facing)
{
    FrameProfiler::Scope timing(FrameProfiler::Stage::AnimationSample);
    if (state.owner != this ||
        (imported ? state.imported.owner != imported.get() : state.pose.size() != joints.size()))
    {
        Reset(state, facing);
    }
    if (!Ready() || !std::isfinite(dt) || dt <= 0 || !std::isfinite(phase) ||
        !std::isfinite(facing) || static_cast<size_t>(kind) >= clips.size())
        return;
    dt = (std::min)(dt, .05f);
    const float difference = std::atan2(std::sin(facing - state.yaw), std::cos(facing - state.yaw));
    if (kind == Motion::Idle)
    {
        if (std::abs(difference) > .5f && state.turnAge >= .35f)
        {
            state.turnAge = 0;
            state.turn = difference > 0 ? Motion::TurnLeft : Motion::TurnRight;
        }
        if (state.turnAge < .35f)
        {
            state.turnAge += dt;
            kind = state.turn;
            phase = (std::min)(1.f, state.turnAge / .35f);
        }
    }
    else
    {
        state.turnAge = 1;
    }
    if (kind != state.motion)
    {
        state.transitionFrom = state.pose;
        if (imported)
        {
            state.imported.from = state.imported.pose;
            state.imported.fromMorph = state.imported.morph;
        }
        state.transition = 0;
        state.motion = kind;
    }
    state.transition =
        (std::min)(1.f, state.transition + dt / (kind == Motion::Cast ? .07f : .12f));
    const float blend = state.transition * state.transition * (3 - 2 * state.transition);
    state.yaw += difference * (1 - std::exp(-24.f * dt));
    state.yaw = std::atan2(std::sin(state.yaw), std::cos(state.yaw));
    if (imported)
    {
        AnimateGltf(state, dt, kind, phase);
        return;
    }
    for (size_t i = 0; i < state.pose.size(); ++i)
        state.pose[i] = Blend(state.transitionFrom[i], Sample(kind, i, phase), blend);
    UpdateSkin(state);
}

bool CharacterModel::Draw(Renderer& renderer, Point feet, Color tint)
{
    return Draw(renderer, feet, tint, defaultState, 90);
}

bool CharacterModel::Draw(Renderer& renderer,
                          Point feet,
                          Color tint,
                          const State& animation,
                          float height,
                          const Parts* selection)
{
    if (!Ready() || animation.owner != this ||
        (imported ? animation.imported.owner != imported.get()
                  : animation.skin.size() != joints.size()) ||
        !std::isfinite(height) || height <= 0)
        return false;
    renderer.Flush();
    FrameProfiler::Scope timing(FrameProfiler::Stage::CharacterDraw);
    GraphicsState state;
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_FRAMEBUFFER_SRGB);
    glDisable(GL_BLEND);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LEQUAL);
    glDepthRange(0, 1);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glViewport(0, 0, 256, 320);
    glClearColor(0, 0, 0, 0);
    glClearDepth(1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(meshProgram);
    glUniform2f(yawUniform, std::cos(animation.yaw), std::sin(animation.yaw));
    glUniform1f(framingUniform, framing);
    glUniform1i(styleUniform, !parts.empty() || imported != nullptr);
    glBindVertexArray(vao);
    if (imported)
    {
        DrawGltf(animation, selection);
    }
    else
    {
        glUniformMatrix4fv(bonesUniform,
                           static_cast<GLsizei>(animation.skin.size()),
                           GL_FALSE,
                           animation.skin[0].data());
        FrameProfiler::Upload(animation.skin.size() * sizeof(Matrix));
        if (parts.empty())
        {
            FrameProfiler::DrawArrays(GL_TRIANGLES, 0, vertexCount);
        }
        else
        {
            for (const auto& part : parts)
            {
                if (part.slot < 0 || !selection || (*selection)[part.slot] == part.index)
                {
                    FrameProfiler::DrawArrays(GL_TRIANGLES, part.first, part.count);
                }
            }
        }
    }
    glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolveFramebuffer);
    glBlitFramebuffer(0, 0, 256, 320, 0, 0, 256, 320, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, state.draw);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, state.read);
    glViewport(state.viewport[0], state.viewport[1], state.viewport[2], state.viewport[3]);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
    glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(compositeProgram);
    glUniform2f(viewportUniform, float(state.viewport[2]), float(state.viewport[3]));
    glUniform2f(feetUniform, feet.x, feet.y);
    glUniform2f(anchorUniform, 64, imported ? 120.f : 140.f);
    glUniform1f(drawScaleUniform, height / (90.f * framing));
    glUniform4f(tintUniform, tint.r, tint.g, tint.b, tint.a);
    glUniform1i(linearUniform, renderer.IsHDRScene());
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, image);
    FrameProfiler::DrawArrays(GL_TRIANGLES, 0, 6);
    return true;
}
