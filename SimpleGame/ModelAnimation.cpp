#include "stdafx.h"
#include "ModelAsset.h"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <limits>

namespace ModelAssets
{
Matrix Identity()
{
    return {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
}

Matrix Multiply(const Matrix& a, const Matrix& b)
{
    Matrix result = {};
    for (int column = 0; column < 4; ++column)
    {
        for (int row = 0; row < 4; ++row)
        {
            for (int k = 0; k < 4; ++k)
            {
                result[column * 4 + row] += a[k * 4 + row] * b[column * 4 + k];
            }
        }
    }
    return result;
}

Matrix Transform(const Pose& p)
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

static void Normalize(float* q)
{
    float length = 0;
    for (int i = 0; i < 4; ++i)
    {
        length += q[i] * q[i];
    }
    if (length < 1e-12f)
    {
        q[0] = q[1] = q[2] = 0;
        q[3] = 1;
        return;
    }
    const float inverse = 1 / std::sqrt((std::max)(1e-12f, length));
    for (int i = 0; i < 4; ++i)
    {
        q[i] *= inverse;
    }
}

Pose Blend(const Pose& a, const Pose& b, float weight)
{
    Pose result;
    weight = (std::max)(0.f, (std::min)(1.f, weight));
    for (size_t i = 0; i < result.size(); ++i)
    {
        result[i] = a[i] + (b[i] - a[i]) * weight;
    }
    float dot = 0;
    for (int i = 3; i < 7; ++i)
    {
        dot += a[i] * b[i];
    }
    const float sign = dot < 0 ? -1.f : 1.f;
    dot = (std::min)(1.f, std::abs(dot));
    float from = 1 - weight, to = weight;
    if (dot < .9995f)
    {
        const float angle = std::acos(dot), sine = std::sin(angle);
        from = std::sin((1 - weight) * angle) / sine;
        to = std::sin(weight * angle) / sine;
    }
    for (int i = 3; i < 7; ++i)
    {
        result[i] = a[i] * from + b[i] * to * sign;
    }
    Normalize(result.data() + 3);
    return result;
}

std::array<float, 3> Point(const Matrix& matrix, const float* point)
{
    std::array<float, 3> result;
    for (int row = 0; row < 3; ++row)
    {
        result[row] = matrix[row] * point[0] + matrix[4 + row] * point[1] +
                      matrix[8 + row] * point[2] + matrix[12 + row];
    }
    return result;
}

static std::string Lower(std::string text)
{
    for (auto& c : text)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return text;
}

void Asset::PrepareAnimation()
{
    locomotionRoots.clear();
    for (size_t i = 0; i < nodes.size(); ++i)
    {
        if (Lower(nodes[i].name) == "root")
        {
            locomotionRoots.push_back(i);
        }
    }
    turnClips.clear();
    for (const auto& clip : clips)
    {
        turnClips.push_back(Lower(clip.name).find("turn_") != std::string::npos);
    }
}

int Asset::FindClip(const std::string& name) const
{
    const auto wanted = Lower(name);
    // Prefer exact names, then the standard suffix (NPC_WALK / MOB_STAG_WALK).
    for (size_t i = 0; i < clips.size(); ++i)
    {
        if (Lower(clips[i].name) == wanted)
        {
            return int(i);
        }
    }
    const auto suffix = "_" + wanted;
    for (size_t i = 0; i < clips.size(); ++i)
    {
        const auto candidate = Lower(clips[i].name);
        if (candidate.size() >= suffix.size() &&
            candidate.compare(candidate.size() - suffix.size(), suffix.size(), suffix) == 0)
        {
            return int(i);
        }
    }
    return -1;
}

void Asset::Globals(State& state) const
{
    state.global.resize(nodes.size());
    for (const auto i : order)
    {
        const auto& node = nodes[i];
        const auto local = node.matrixOnly ? node.matrix : Transform(state.pose[i]);
        state.global[i] = node.parent < 0 ? local : Multiply(state.global[node.parent], local);
    }
}

void Asset::Reset(State& state) const
{
    state = State();
    state.owner = this;
    for (const auto& node : nodes)
    {
        state.pose.push_back(node.bind);
        state.morph.push_back(node.morph);
    }
    state.from = state.pose;
    state.fromMorph = state.morph;
    Globals(state);
}

bool Asset::Play(
    State& state, const std::string& name, bool loop, float speed, float fadeSeconds) const
{
    const int index = FindClip(name);
    if (index < 0 || !std::isfinite(speed) || speed <= 0 || speed > 100 ||
        !std::isfinite(fadeSeconds) || fadeSeconds < 0)
    {
        return false;
    }
    if (state.owner != this)
    {
        Reset(state);
    }
    state.from = state.pose;
    state.fromMorph = state.morph;
    state.clip = index;
    state.seconds = 0;
    state.speed = speed;
    state.loop = loop;
    state.finished = false;
    state.fadeSeconds = fadeSeconds;
    state.fade = fadeSeconds > 0 ? 0.f : 1.f;
    Sample(state, index, 0, state.fade);
    return true;
}

static std::array<float, 4> Evaluate(const Track& track, float seconds)
{
    const bool cubic = track.interpolation == Track::Interpolation::Cubic;
    const size_t stride = cubic ? 3 : 1;
    const auto upper = std::upper_bound(track.times.begin(), track.times.end(), seconds);
    const size_t first = upper == track.times.begin() ? 0 : size_t(upper - track.times.begin() - 1);
    const size_t next = (std::min)(first + 1, track.times.size() - 1);
    auto a = track.values[first * stride + (cubic ? 1 : 0)];
    if (first == next || seconds <= track.times.front() ||
        track.interpolation == Track::Interpolation::Step)
    {
        return a;
    }
    const auto& b = track.values[next * stride + (cubic ? 1 : 0)];
    const float span = track.times[next] - track.times[first];
    const float t = (std::max)(0.f, (std::min)(1.f, (seconds - track.times[first]) / span));
    if (cubic)
    {
        const auto& out = track.values[first * 3 + 2];
        const auto& in = track.values[next * 3];
        const float t2 = t * t, t3 = t2 * t;
        for (int i = 0; i < 4; ++i)
        {
            a[i] = (2 * t3 - 3 * t2 + 1) * a[i] + (t3 - 2 * t2 + t) * span * out[i] +
                   (-2 * t3 + 3 * t2) * b[i] + (t3 - t2) * span * in[i];
        }
        if (track.path == Track::Path::Rotation)
        {
            Normalize(a.data());
        }
        return a;
    }
    if (track.path == Track::Path::Rotation)
    {
        Pose qa = {{0, 0, 0, a[0], a[1], a[2], a[3], 1, 1, 1}};
        Pose qb = {{0, 0, 0, b[0], b[1], b[2], b[3], 1, 1, 1}};
        const auto result = Blend(qa, qb, t);
        return {{result[3], result[4], result[5], result[6]}};
    }
    for (int i = 0; i < 4; ++i)
    {
        a[i] += (b[i] - a[i]) * t;
    }
    return a;
}

void Asset::Sample(State& state, int clip, float seconds, float fade, bool inPlace) const
{
    seconds = std::isfinite(seconds) ? (std::max)(0.f, seconds) : 0;
    fade = std::isfinite(fade) ? (std::max)(0.f, (std::min)(1.f, fade)) : 1;
    if (state.owner != this)
    {
        Reset(state);
    }
    for (size_t i = 0; i < nodes.size(); ++i)
    {
        state.pose[i] = nodes[i].bind;
        state.morph[i] = nodes[i].morph;
    }
    auto apply = [&](const Clip& animation, float at)
    {
        for (const auto& track : animation.tracks)
        {
            const auto value = Evaluate(track, at);
            if (track.path == Track::Path::Morph)
            {
                state.morph[track.node] = value;
            }
            else
            {
                const int offset = track.path == Track::Path::Translation ? 0
                                   : track.path == Track::Path::Rotation  ? 3
                                                                          : 7;
                std::copy_n(value.begin(),
                            track.path == Track::Path::Rotation ? 4 : 3,
                            state.pose[track.node].begin() + offset);
            }
        }
    };
    if (clip >= 0 && size_t(clip) < clips.size())
    {
        apply(clips[clip], seconds);
        if (inPlace)
        {
            const bool turn = turnClips[clip];
            for (const size_t i : locomotionRoots)
            {
                // Gameplay owns horizontal travel and heading. Keep the keyed jump height.
                state.pose[i][0] = nodes[i].bind[0];
                state.pose[i][2] = nodes[i].bind[2];
                if (turn)
                {
                    std::copy_n(nodes[i].bind.begin() + 3, 4, state.pose[i].begin() + 3);
                }
            }
        }
    }
    if (fade < 1)
    {
        for (size_t i = 0; i < nodes.size(); ++i)
        {
            state.pose[i] = Blend(state.from[i], state.pose[i], fade);
            for (size_t target = 0; target < 4; ++target)
            {
                state.morph[i][target] =
                    state.fromMorph[i][target] +
                    (state.morph[i][target] - state.fromMorph[i][target]) * fade;
            }
        }
    }
    for (const int layer : state.layers)
    {
        if (layer >= 0 && size_t(layer) < clips.size() && clips[layer].duration > 0)
        {
            apply(clips[layer], std::fmod(state.ambientSeconds, clips[layer].duration));
        }
    }
    Globals(state);
}

void Asset::Update(State& state, float dt) const
{
    if (state.owner != this)
    {
        Reset(state);
    }
    if (!std::isfinite(dt) || dt <= 0 || state.clip < 0)
    {
        return;
    }
    const float duration = clips[state.clip].duration;
    state.seconds += dt * state.speed;
    state.ambientSeconds = std::fmod(state.ambientSeconds + dt, 86400.f);
    if (duration > 0)
    {
        if (state.loop)
        {
            state.seconds = std::fmod(state.seconds, duration);
        }
        else
        {
            state.finished = state.seconds >= duration;
            state.seconds = (std::min)(duration, state.seconds);
        }
    }
    state.fade = state.fadeSeconds > 0 ? (std::min)(1.f, state.fade + dt / state.fadeSeconds) : 1.f;
    const float smooth = state.fade * state.fade * (3 - 2 * state.fade);
    Sample(state, state.clip, state.seconds, smooth);
}

void Asset::Palette(const State& state, int skin, std::vector<Matrix>& matrices) const
{
    const auto& skeleton = skins[skin];
    matrices.resize(skeleton.nodes.size());
    for (size_t i = 0; i < matrices.size(); ++i)
    {
        matrices[i] = Multiply(state.global[skeleton.nodes[i]], skeleton.inverseBind[i]);
    }
}

void Asset::Bounds(const State& state,
                   const std::string& mesh,
                   std::array<float, 3>& low,
                   std::array<float, 3>& high) const
{
    low.fill((std::numeric_limits<float>::max)());
    high.fill((std::numeric_limits<float>::lowest)());
    std::vector<Matrix> palette;
    for (const auto& primitive : primitives)
    {
        if (!mesh.empty() && primitive.name != mesh)
        {
            continue;
        }
        if (primitive.skin >= 0)
        {
            Palette(state, primitive.skin, palette);
        }
        for (size_t i = primitive.first; i < size_t(primitive.first) + primitive.count; ++i)
        {
            const auto& vertex = vertices[i];
            auto local =
                std::array<float, 3>{{vertex.position[0], vertex.position[1], vertex.position[2]}};
            for (size_t target = 0; target < nodes[primitive.node].morphCount; ++target)
            {
                for (int axis = 0; axis < 3; ++axis)
                {
                    local[axis] +=
                        vertex.morphPosition[target][axis] * state.morph[primitive.node][target];
                }
            }
            auto position = Point(state.global[primitive.node], local.data());
            if (primitive.skin >= 0)
            {
                position.fill(0);
                for (int joint = 0; joint < 4; ++joint)
                {
                    const auto p = Point(palette[size_t(vertex.joints[joint])], local.data());
                    for (int axis = 0; axis < 3; ++axis)
                    {
                        position[axis] += p[axis] * vertex.weights[joint];
                    }
                }
            }
            position = Point(normalization, position.data());
            for (int axis = 0; axis < 3; ++axis)
            {
                low[axis] = (std::min)(low[axis], position[axis]);
                high[axis] = (std::max)(high[axis], position[axis]);
            }
        }
    }
}
} // namespace ModelAssets
