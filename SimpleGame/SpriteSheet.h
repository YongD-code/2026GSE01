#pragma once
#include "FrameProfiler.h"
#include "Renderer.h"
#include "ShaderFiles.h"
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <array>
#include <cmath>
#include <algorithm>
#include <iostream>
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")

// Screen-space facing, clockwise from down. Diagonals are first-class movement states.
enum class Facing
{
    South,
    SouthWest,
    West,
    NorthWest,
    North,
    NorthEast,
    East,
    SouthEast
};

struct SpriteLayout
{
    int columns = 4, rows = 4;
    std::array<int, 8> directionRows = {{0, 1, 1, 1, 3, 2, 2, 2}};
    int idleFrame = 0, firstWalkFrame = 1, walkFrames = 3;
    std::array<int, 8> directionIdleColumns = {{-1, -1, -1, -1, -1, -1, -1, -1}};
    bool anchorAtFeet = false;
    bool anchorAtTorso = false;
    bool pingPongWalk = false;
    bool blendWalkFrames = true, normalizeFrameHeight = false;
    // Authored Blender sheets share a camera and ground origin across every frame.
    bool fixedCanvas = false;
    bool detectLocomotionColumns = false;
    float canvasAnchorY = .845f, canvasCharacterHeight = .78f;
};

struct SpriteAnimation
{
    Facing facing = Facing::South;
    float phase = 0;
    bool moving = false;

    void Update(double x, double y, float dt, bool sprint, float pixelsPerFrame = 0.f)
    {
        moving = std::abs(x) + std::abs(y) > .00001;
        if (!moving)
        {
            phase = 0;
            return;
        }
        const double length = std::sqrt(x * x + y * y);
        int sx = x / length > .3 ? 1 : (x / length < -.3 ? -1 : 0);
        int sy = y / length > .3 ? 1 : (y / length < -.3 ? -1 : 0);
        if (sy > 0)
            facing = sx < 0 ? Facing::SouthWest : (sx > 0 ? Facing::SouthEast : Facing::South);
        else if (sy < 0)
            facing = sx < 0 ? Facing::NorthWest : (sx > 0 ? Facing::NorthEast : Facing::North);
        else
            facing = sx < 0 ? Facing::West : Facing::East;
        // Distance-driven playback keeps footsteps in sync with collision-limited movement.
        const float advance = pixelsPerFrame > 0.f ? static_cast<float>(length) / pixelsPerFrame
                                                   : dt * (sprint ? 11.f : 7.f);
        phase = std::fmod(phase + advance, 1024.f);
    }
};

class SpriteSheet
{
    struct Frame
    {
        float u0, v0, u1, v1;
        int width, height;
        float anchorX, anchorY;
    };

    GLuint texture = 0;
    std::vector<Frame> frames;
    SpriteLayout layout;
    float imageAspect = 1;
    int referenceHeight = 1;

  public:
    SpriteSheet() = default;
    SpriteSheet(const SpriteSheet&) = delete;
    SpriteSheet& operator=(const SpriteSheet&) = delete;

    ~SpriteSheet()
    {
        if (texture)
            glDeleteTextures(1, &texture);
    }

    bool Ready() const
    {
        return texture && !frames.empty();
    }

    // Full image panel, independent of character-frame anchoring.
    void DrawPanel(Renderer& renderer, float x, float y, float w, float h, float alpha)
    {
        if (Ready())
        {
            renderer.Sprite(texture, x, y, w, h, 0, 0, 1, 1, Color(1, 1, 1, alpha));
        }
    }

    // Crop UVs rather than stretching or drawing outside the cinematic frame.
    void DrawCinematic(Renderer& renderer,
                       float x,
                       float y,
                       float w,
                       float h,
                       float zoom,
                       float panX,
                       float panY,
                       float alpha = 1.f)
    {
        if (!Ready() || w <= 0 || h <= 0)
            return;
        const float aspect = w / h;
        const float scale = (std::max)(1.f, zoom);
        const float cropX = (aspect < imageAspect ? aspect / imageAspect : 1.f) / scale;
        const float cropY = (aspect > imageAspect ? imageAspect / aspect : 1.f) / scale;
        const float u = (std::max)(cropX * .5f, (std::min)(1 - cropX * .5f, .5f + panX));
        const float v = (std::max)(cropY * .5f, (std::min)(1 - cropY * .5f, .5f + panY));
        renderer.Sprite(texture,
                        x,
                        y,
                        w,
                        h,
                        u - cropX * .5f,
                        v - cropY * .5f,
                        u + cropX * .5f,
                        v + cropY * .5f,
                        Color(1, 1, 1, alpha));
    }

    void DrawEffect(Renderer& renderer, Point center, float w, float h, float alpha, float phase)
    {
        if (Ready() && alpha > .001f)
        {
            renderer.Sprite(texture,
                            center.x - w * .5f,
                            center.y - h * .5f,
                            w,
                            h,
                            0,
                            0,
                            1,
                            1,
                            Color(1, 1, 1, alpha),
                            .5f,
                            0,
                            phase,
                            true);
        }
    }

    // Four padded spell illustrations in a 2x2 atlas; inset avoids neighboring-cell bleed.
    void DrawSpellTile(Renderer& renderer,
                       int tile,
                       Point center,
                       float w,
                       float h,
                       float alpha,
                       float phase = 0,
                       float rotation = 0)
    {
        if (!Ready() || tile < 0 || tile > 3 || alpha <= .001f)
            return;
        const float u = (tile % 2) * .5f, v = (tile / 2) * .5f;
        renderer.Sprite(texture,
                        center.x - w * .5f,
                        center.y - h * .5f,
                        w,
                        h,
                        u + .001f,
                        v + .001f,
                        u + .499f,
                        v + .499f,
                        Color(1, 1, 1, alpha),
                        .7f,
                        0,
                        phase,
                        true,
                        rotation);
    }

    // Authored facial poses retain their anatomy instead of using UV noise distortion.
    void DrawPoseTile(Renderer& renderer,
                      int tile,
                      Point center,
                      float size,
                      float alpha,
                      float aspect = 1.f,
                      float emission = .45f)
    {
        if (!Ready() || tile < 0 || tile > 3 || alpha <= .001f)
        {
            return;
        }
        const float u = (tile % 2) * .5f, v = (tile / 2) * .5f;
        renderer.Sprite(texture,
                        center.x - size * .5f,
                        center.y - size / aspect * .5f,
                        size,
                        size / aspect,
                        u + .001f,
                        v + .001f,
                        u + .499f,
                        v + .499f,
                        Color(1, 1, 1, alpha),
                        emission);
    }

    // A single textured dome; the shader clips/refracts its edge and dissolves on collapse.
    void DrawRealm(Renderer& renderer, Point center, float rx, float ry, float alpha, float phase)
    {
        if (!Ready() || rx < 1 || ry < 1 || alpha <= .001f)
            return;
        const float aspect = rx / ry;
        const float cropX = aspect < imageAspect ? aspect / imageAspect : 1.f;
        const float cropY = aspect > imageAspect ? imageAspect / aspect : 1.f;
        renderer.Sprite(texture,
                        center.x - rx,
                        center.y - ry,
                        rx * 2,
                        ry * 2,
                        .5f - cropX * .5f,
                        .5f - cropY * .5f,
                        .5f + cropX * .5f,
                        .5f + cropY * .5f,
                        Color(1, 1, 1, alpha),
                        .12f,
                        0,
                        phase,
                        false,
                        0,
                        4);
    }

    int WalkFrameCount() const
    {
        return layout.walkFrames;
    }

    bool Load(const std::wstring& relativePath, const SpriteLayout& definition = SpriteLayout())
    {
        using Microsoft::WRL::ComPtr;
        layout = definition;
        if (layout.columns <= 0 || layout.rows <= 0 || layout.walkFrames <= 0 ||
            layout.firstWalkFrame < 0 ||
            layout.firstWalkFrame + layout.walkFrames > layout.columns || layout.idleFrame < 0 ||
            layout.idleFrame >= layout.columns)
            return false;
        for (int row : layout.directionRows)
            if (row < 0 || row >= layout.rows)
                return false;
        for (int column : layout.directionIdleColumns)
        {
            if (column < -1 || column >= layout.columns)
            {
                return false;
            }
        }
        HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

        struct ComScope
        {
            bool owned;

            ~ComScope()
            {
                if (owned)
                    CoUninitialize();
            }
        } comScope = {SUCCEEDED(initialized)};

        if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE)
            return false;
        ComPtr<IWICImagingFactory> factory;
        if (FAILED(CoCreateInstance(CLSID_WICImagingFactory,
                                    nullptr,
                                    CLSCTX_INPROC_SERVER,
                                    IID_PPV_ARGS(factory.GetAddressOf()))))
            return false;
        wchar_t executable[32768] = {};
        GetModuleFileNameW(nullptr, executable, 32768);
        std::wstring folder = executable;
        size_t slash = folder.find_last_of(L"\\/");
        folder = slash == std::wstring::npos ? L"" : folder.substr(0, slash + 1);
        const std::wstring candidates[] = {
            folder + relativePath, relativePath, L"SimpleGame/" + relativePath};
        ComPtr<IWICBitmapDecoder> decoder;
        for (const auto& path : candidates)
        {
            if (SUCCEEDED(factory->CreateDecoderFromFilename(path.c_str(),
                                                             nullptr,
                                                             GENERIC_READ,
                                                             WICDecodeMetadataCacheOnLoad,
                                                             decoder.ReleaseAndGetAddressOf())))
                break;
        }
        if (!decoder)
            return false;
        ComPtr<IWICBitmapFrameDecode> source;
        if (FAILED(decoder->GetFrame(0, source.GetAddressOf())))
            return false;
        ComPtr<IWICFormatConverter> converter;
        if (FAILED(factory->CreateFormatConverter(converter.GetAddressOf())))
            return false;
        if (FAILED(converter->Initialize(source.Get(),
                                         GUID_WICPixelFormat32bppRGBA,
                                         WICBitmapDitherTypeNone,
                                         nullptr,
                                         0,
                                         WICBitmapPaletteTypeCustom)))
            return false;
        UINT width = 0, height = 0;
        converter->GetSize(&width, &height);
        imageAspect = height > 0 ? float(width) / height : 1.f;
        if (layout.detectLocomotionColumns)
        {
            if (height == 0 || height % layout.rows != 0)
            {
                return false;
            }
            const UINT cellSize = height / layout.rows;
            if (width % cellSize != 0)
            {
                return false;
            }
            const UINT columns = width / cellSize;
            if (columns != 9 && columns != 17)
            {
                return false;
            }
            layout.columns = static_cast<int>(columns);
            layout.walkFrames = layout.columns - layout.firstWalkFrame;
        }
        if (width < static_cast<UINT>(layout.columns) || height < static_cast<UINT>(layout.rows) ||
            width > 8192 || height > 8192)
            return false;
        std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 4);
        if (FAILED(converter->CopyPixels(
                nullptr, width * 4, static_cast<UINT>(pixels.size()), pixels.data())))
            return false;
        // Integer boundary division handles sheets whose dimensions are not divisible by four.
        for (int row = 0; row < layout.rows; ++row)
            for (int col = 0; col < layout.columns; ++col)
            {
                int left = col * width / layout.columns, right = (col + 1) * width / layout.columns;
                int top = row * height / layout.rows, bottom = (row + 1) * height / layout.rows;
                int minX = right, minY = bottom, maxX = left - 1, maxY = top - 1;
                for (int y = top; y < bottom; ++y)
                    for (int x = left; x < right; ++x)
                    {
                        if (pixels[(static_cast<size_t>(y) * width + x) * 4 + 3] > 24)
                        {
                            minX = (std::min)(minX, x);
                            maxX = (std::max)(maxX, x);
                            minY = (std::min)(minY, y);
                            maxY = (std::max)(maxY, y);
                        }
                    }
                if (maxX < minX)
                {
                    minX = left;
                    maxX = left;
                    minY = top;
                    maxY = top;
                }
                if (layout.fixedCanvas)
                {
                    minX = left;
                    maxX = right - 1;
                    minY = top;
                    maxY = bottom - 1;
                }
                int w = maxX - minX + 1, h = maxY - minY + 1;
                float anchor = w * .5f;
                if (layout.anchorAtFeet)
                {
                    double sum = 0, weight = 0;
                    for (int y = maxY - h / 8; y <= maxY; ++y)
                        for (int x = minX; x <= maxX; ++x)
                        {
                            unsigned char alpha =
                                pixels[(static_cast<size_t>(y) * width + x) * 4 + 3];
                            if (alpha > 24)
                            {
                                sum += (x - minX) * double(alpha);
                                weight += alpha;
                            }
                        }
                    if (weight > 0)
                        anchor = static_cast<float>(sum / weight);
                }
                if (layout.anchorAtTorso)
                {
                    // Coattails and outstretched boots must not pull the body sideways.
                    // Use the opaque torso rather than the changing silhouette bounds.
                    double sum = 0, weight = 0;
                    for (int y = minY + h * 3 / 10; y <= minY + h / 2; ++y)
                        for (int x = minX; x <= maxX; ++x)
                        {
                            unsigned char alpha =
                                pixels[(static_cast<size_t>(y) * width + x) * 4 + 3];
                            if (alpha > 128)
                            {
                                sum += (x - minX) * double(alpha);
                                weight += alpha;
                            }
                        }
                    if (weight > 0)
                        anchor = static_cast<float>(sum / weight);
                }
                frames.push_back({(minX + .5f) / width,
                                  (minY + .5f) / height,
                                  (maxX + .5f) / width,
                                  (maxY + .5f) / height,
                                  w,
                                  h,
                                  anchor,
                                  layout.fixedCanvas ? h * layout.canvasAnchorY : float(h)});
                referenceHeight =
                    (std::max)(referenceHeight,
                               layout.fixedCanvas ? int(h * layout.canvasCharacterHeight) : h);
            }
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(
            GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
        return Ready();
    }

    void Draw(Renderer& renderer,
              Point feet,
              float size,
              const SpriteAnimation& animation,
              Color tint,
              int width,
              int height,
              float emission = 0.f,
              int spiritMotion = -1,
              float motionTime = 0.f,
              float casting = 0.f,
              int clipFirst = -1,
              int clipFrames = 0)
    {
        if (!Ready())
            return;
        const int firstWalkFrame = clipFirst >= 0 ? clipFirst : layout.firstWalkFrame;
        const int walkFrames = clipFrames > 0 ? clipFrames : layout.walkFrames;
        if (clipFirst >= 0 && (walkFrames <= 0 || firstWalkFrame + walkFrames > layout.columns))
        {
            return;
        }
        int row = layout.directionRows[static_cast<int>(animation.facing)];
        int column = layout.idleFrame;
        const int directionColumn = layout.directionIdleColumns[static_cast<int>(animation.facing)];
        if (!animation.moving && directionColumn >= 0)
        {
            column = directionColumn;
        }
        if (animation.moving)
        {
            int step = 0;
            if (layout.pingPongWalk && walkFrames > 1)
            {
                const int period = 2 * (walkFrames - 1);
                step = static_cast<int>(animation.phase) % period;
                if (step >= walkFrames)
                    step = period - step;
            }
            else
                step = static_cast<int>(animation.phase) % walkFrames;
            column = firstWalkFrame + step;
            // Single-pose directional atlases still need their direction-specific column.
            if (walkFrames == 1 && directionColumn >= 0)
                column = directionColumn;
        }
        // Smooth only locomotion, not discrete cast poses. Fractional phase survives between
        // frames.
        if (layout.blendWalkFrames && animation.moving && casting <= 0 && emission < 1.f &&
            walkFrames > 1)
        {
            const float fraction = animation.phase - std::floor(animation.phase);
            int next = static_cast<int>(animation.phase) + 1;
            if (layout.pingPongWalk)
            {
                const int period = 2 * (walkFrames - 1);
                next %= period;
                if (next >= walkFrames)
                    next = period - next;
            }
            else
                next %= walkFrames;
            const Frame& from = frames[row * layout.columns + column];
            const Frame& to = frames[row * layout.columns + firstWalkFrame + next];
            const float mix = fraction * fraction * (3 - 2 * fraction);
            auto pose = [&](const Frame& f, float alpha)
            {
                if (alpha < .001f)
                    return;
                const float scale =
                    size / (layout.normalizeFrameHeight ? f.height : referenceHeight);
                renderer.Sprite(texture,
                                feet.x - f.anchorX * scale,
                                feet.y - f.anchorY * scale,
                                f.width * scale,
                                f.height * scale,
                                f.u0,
                                f.v0,
                                f.u1,
                                f.v1,
                                Color(tint.r, tint.g, tint.b, tint.a * alpha),
                                emission);
            };
            pose(from, 1 - mix);
            pose(to, mix);
            return;
        }
        const Frame& frame = frames[row * layout.columns + column];
        float scale = size / (layout.normalizeFrameHeight ? frame.height : referenceHeight);
        float w = frame.width * scale, h = frame.height * scale;
        if (feet.x + w < 0 || feet.x - w > width || feet.y < 0 || feet.y - h > height)
            return;
        if (spiritMotion >= 0)
        {
            const float pace = animation.moving ? animation.phase * 1.5707963f : motionTime * 2;
            const float pulse = std::sin(pace);
            const float squash =
                spiritMotion == 2 ? .045f * pulse - casting * .12f : .025f * pulse + casting * .06f;
            h *= 1 + squash;
            const float left = feet.x - frame.anchorX * scale;
            const float top = feet.y - frame.anchorY * scale * (1 + squash);
            auto offset = [&](float t)
            {
                const float wave = spiritMotion == 0   ? std::sin(pace - t * 4) * t * 4
                                   : spiritMotion == 1 ? pulse * (1 - t) * 4
                                                       : std::sin(pace + t * 2) * 2;
                return wave + casting * (1 - t) * (spiritMotion == 2 ? 9 : -5);
            };
            // Contiguous sheared strips bend the sprite without changing source artwork.
            for (int strip = 0; strip < 8; ++strip)
            {
                const float a = strip / 8.f, b = (strip + 1) / 8.f;
                renderer.Sprite(texture,
                                left + offset(a),
                                top + h * a,
                                w,
                                h / 8,
                                frame.u0,
                                frame.v0 + (frame.v1 - frame.v0) * a,
                                frame.u1,
                                frame.v0 + (frame.v1 - frame.v0) * b,
                                tint,
                                emission,
                                offset(b) - offset(a));
            }
            return;
        }
        renderer.Sprite(texture,
                        feet.x - frame.anchorX * scale,
                        feet.y - frame.anchorY * scale,
                        w,
                        h,
                        frame.u0,
                        frame.v0,
                        frame.u1,
                        frame.v1,
                        tint,
                        emission);
    }
};
