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
    bool anchorAtFeet = false;
    bool anchorAtTorso = false;
    bool pingPongWalk = false;
    // Authored Blender sheets share a camera and ground origin across every frame.
    bool fixedCanvas = false;
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

    GLuint texture = 0, program = 0, vao = 0;
    std::vector<Frame> frames;
    SpriteLayout layout;
    int referenceHeight = 1;

  public:
    SpriteSheet() = default;
    SpriteSheet(const SpriteSheet&) = delete;
    SpriteSheet& operator=(const SpriteSheet&) = delete;

    ~SpriteSheet()
    {
        if (texture)
            glDeleteTextures(1, &texture);
        if (program)
            glDeleteProgram(program);
        if (vao)
            glDeleteVertexArrays(1, &vao);
    }

    bool Ready() const
    {
        return texture && program && vao;
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
        program = ShaderFiles::Program(L"Shaders/Sprite.vs", L"Shaders/Sprite.fs");
        if (!program)
            return false;
        glGenVertexArrays(1, &vao);
        return Ready();
    }

    void Draw(Renderer& renderer,
              Point feet,
              float size,
              const SpriteAnimation& animation,
              Color tint,
              int width,
              int height,
              float emission = 0.f)
    {
        if (!Ready())
            return;
        int row = layout.directionRows[static_cast<int>(animation.facing)];
        int column = layout.idleFrame;
        if (animation.moving)
        {
            int step = 0;
            if (layout.pingPongWalk && layout.walkFrames > 1)
            {
                const int period = 2 * (layout.walkFrames - 1);
                step = static_cast<int>(animation.phase) % period;
                if (step >= layout.walkFrames)
                    step = period - step;
            }
            else
                step = static_cast<int>(animation.phase) % layout.walkFrames;
            column = layout.firstWalkFrame + step;
        }
        const Frame& frame = frames[row * layout.columns + column];
        float scale = size / referenceHeight, w = frame.width * scale, h = frame.height * scale;
        if (feet.x + w < 0 || feet.x - w > width || feet.y < 0 || feet.y - h > height)
            return;
        renderer.Flush();
        glUseProgram(program);
        glBindVertexArray(vao);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glUniform1i(glGetUniformLocation(program, "image"), 0);
        glUniform1f(glGetUniformLocation(program, "emission"), emission);
        glUniform2f(glGetUniformLocation(program, "viewport"), float(width), float(height));
        glUniform4f(glGetUniformLocation(program, "rectangle"),
                    feet.x - frame.anchorX * scale,
                    feet.y - frame.anchorY * scale,
                    w,
                    h);
        glUniform4f(
            glGetUniformLocation(program, "uvRect"), frame.u0, frame.v0, frame.u1, frame.v1);
        glUniform4f(glGetUniformLocation(program, "tint"), tint.r, tint.g, tint.b, tint.a);
        glUniform1i(glGetUniformLocation(program, "linearScene"), renderer.IsHDRScene() ? 1 : 0);
        FrameProfiler::DrawArrays(GL_TRIANGLES, 0, 6);
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindVertexArray(0);
        glUseProgram(0);
    }
};
