#pragma once
#include "Renderer.h"
#include "Actor.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

// Builds only when the existing Renderer mesh cache misses. SceneGraph keeps ownership.
namespace EnvironmentGeometry
{
using Vertex = ActorPosition;

inline Vertex Add(Vertex a, Vertex b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

inline Vertex Scale(Vertex a, double scale)
{
    return {a.x * scale, a.y * scale, a.z * scale};
}

inline Vertex Cross(Vertex a, Vertex b)
{
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

inline Vertex Unit(Vertex a)
{
    const double length = std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
    return length > .00001 ? Scale(a, 1 / length) : Vertex{0, 0, 1};
}

class Solid
{
    struct Face
    {
        Vertex a, b, c;
        Color color;
        double depth;
    };

    std::vector<Face> faces;

  public:
    void Triangle(Vertex a, Vertex b, Vertex c, Color color)
    {
        const Vertex cross = Cross(Add(b, Scale(a, -1)), Add(c, Scale(a, -1)));
        if (cross.x * cross.x + cross.y * cross.y + cross.z * cross.z < 1.e-12)
            return;
        const Vertex normal = Unit(cross);
        if (normal.x * .603 + normal.y * .603 + normal.z * .519 <= 0)
            return;
        const float light =
            float((std::max)(0., -normal.x * .30 + normal.y * .25 + normal.z * .92));
        const float shade = .63f + light * .48f;
        color.r *= shade;
        color.g *= shade;
        color.b *= shade;
        const auto center = Scale(Add(Add(a, b), c), 1. / 3);
        faces.push_back({a, b, c, color, center.x * .603 + center.y * .603 + center.z * .519});
    }

    void Quad(Vertex a, Vertex b, Vertex c, Vertex d, Color color)
    {
        Triangle(a, b, c, color);
        Triangle(a, c, d, color);
    }

    void Taper(Vertex start, Vertex end, double bottom, double top, Color color, int sides = 10)
    {
        const Vertex axis = Unit(Add(end, Scale(start, -1)));
        const Vertex u = std::abs(axis.z) > .99 ? Vertex{1, 0, 0} : Unit(Cross(axis, {0, 0, 1}));
        const Vertex v = Unit(Cross(axis, u));
        auto radial = [&](int i)
        {
            const double a = i * 6.283185307179586 / sides;
            return Add(Scale(u, std::cos(a)), Scale(v, std::sin(a)));
        };
        for (int i = 0; i < sides; ++i)
        {
            const auto a = radial(i), b = radial(i + 1);
            const Vertex lowA = Add(start, Scale(a, bottom)), lowB = Add(start, Scale(b, bottom));
            const Vertex highA = Add(end, Scale(a, top)), highB = Add(end, Scale(b, top));
            Quad(lowA, lowB, highB, highA, color);
            Triangle(end, highA, highB, color);
        }
    }

    void Foliage(Vertex center, double rx, double ry, double rz, int seed, Color color)
    {
        constexpr int segments = 12, rings = 5;
        auto point = [&](int ring, int segment)
        {
            const double latitude = -1.5707963267948966 + ring * 3.141592653589793 / rings;
            const double angle = segment * 6.283185307179586 / segments;
            const double edge = 1 + .075 * std::sin(angle * 5 + seed * .8 + ring * .7);
            return Vertex{center.x + std::cos(angle) * std::cos(latitude) * rx * edge,
                          center.y + std::sin(angle) * std::cos(latitude) * ry * edge,
                          center.z + std::sin(latitude) * rz};
        };
        for (int ring = 0; ring < rings; ++ring)
            for (int i = 0; i < segments; ++i)
            {
                Color tint = color;
                const float patch = 1.f + float((i * 7 + ring * 3 + seed) % 5 - 2) * .028f;
                tint.r *= patch;
                tint.g *= patch;
                tint.b *= patch;
                Quad(point(ring, i),
                     point(ring, i + 1),
                     point(ring + 1, i + 1),
                     point(ring + 1, i),
                     tint);
            }
    }

    template <class Projection> void Draw(Renderer& renderer, Projection project)
    {
        std::stable_sort(faces.begin(),
                         faces.end(),
                         [](const Face& a, const Face& b)
                         {
                             return a.depth < b.depth;
                         });
        for (const auto& face : faces)
            renderer.Triangle(project(face.a), project(face.b), project(face.c), face.color);
    }
};
} // namespace EnvironmentGeometry
