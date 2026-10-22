#include "stdafx.h"
#include "CharacterModel.h"
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
           image && vertexCount > 0 && !joints.empty();
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
    meshProgram = ShaderFiles::Program(L"Shaders/CharacterMesh.vs", L"Shaders/CharacterMesh.fs");
    compositeProgram =
        ShaderFiles::Program(L"Shaders/CharacterComposite.vs", L"Shaders/CharacterComposite.fs");
    if (!meshProgram || !compositeProgram || !CreateTargets())
    {
        Release();
        return false;
    }
    bonesUniform = glGetUniformLocation(meshProgram, "bones[0]");
    yawUniform = glGetUniformLocation(meshProgram, "heading");
    viewportUniform = glGetUniformLocation(compositeProgram, "viewport");
    feetUniform = glGetUniformLocation(compositeProgram, "feet");
    tintUniform = glGetUniformLocation(compositeProgram, "tint");
    linearUniform = glGetUniformLocation(compositeProgram, "linearScene");
    framingUniform = glGetUniformLocation(meshProgram, "modelScale");
    drawScaleUniform = glGetUniformLocation(compositeProgram, "drawScale");
    styleUniform = glGetUniformLocation(meshProgram, "fourBand");
    glUseProgram(compositeProgram);
    glUniform1i(glGetUniformLocation(compositeProgram, "characterImage"), 0);
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
    if (state.owner != this || state.pose.size() != joints.size())
    {
        Reset(state, facing);
    }
    if (!Ready() || dt <= 0)
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
        state.transition = 0;
        state.motion = kind;
    }
    state.transition =
        (std::min)(1.f, state.transition + dt / (kind == Motion::Cast ? .07f : .12f));
    const float blend = state.transition * state.transition * (3 - 2 * state.transition);
    state.yaw += difference * (1 - std::exp(-24.f * dt));
    state.yaw = std::atan2(std::sin(state.yaw), std::cos(state.yaw));
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
    if (!Ready() || animation.skin.size() != joints.size() || !std::isfinite(height) || height <= 0)
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
    glUniformMatrix4fv(bonesUniform,
                       static_cast<GLsizei>(animation.skin.size()),
                       GL_FALSE,
                       animation.skin[0].data());
    FrameProfiler::Upload(animation.skin.size() * sizeof(Matrix));
    glUniform2f(yawUniform, std::cos(animation.yaw), std::sin(animation.yaw));
    glUniform1f(framingUniform, framing);
    glUniform1i(styleUniform, !parts.empty());
    glBindVertexArray(vao);
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
    glUniform1f(drawScaleUniform, height / (90.f * framing));
    glUniform4f(tintUniform, tint.r, tint.g, tint.b, tint.a);
    glUniform1i(linearUniform, renderer.IsHDRScene());
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, image);
    FrameProfiler::DrawArrays(GL_TRIANGLES, 0, 6);
    return true;
}
